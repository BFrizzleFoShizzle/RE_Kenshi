#include "SIC.h"
#include "zstd/zstd.h"

#include <vector>
#include <iostream>
#include <string>
#include <cmath>

#pragma comment(lib, "thirdparty/zstd/libzstd_static.lib")

static inline size_t GetPixelSize(SIC::EncodeType type)
{
	if (type == SIC::EncodeType::BGR)
		return 3;
	else if (type == SIC::EncodeType::BGRA)
		return 4;
	else if (type == SIC::EncodeType::R8)
		return 1;
	else if (type == SIC::EncodeType::R16)
		return 2;
	else
		return 0;
}

size_t SIC::GetMaxCompressedSize(size_t width, size_t height, SIC::EncodeType type)
{
	size_t numBytes = width * height * GetPixelSize(type);

	// invalid type
	if (numBytes == 0)
		return 0;

	size_t zstdSize = ZSTD_compressBound(numBytes);

	if (zstdSize == 0)
		return 0;

	return zstdSize  + sizeof(SIC::SICHeader);
}

SIC::SICHeader SIC::GetHeader(const void* inData, size_t inSize)
{
	// return error
	if (inSize < sizeof(SIC::SICHeader))
		return SIC::SICHeader();

	return *(SIC::SICHeader*)inData;
}
size_t SIC::GetDecompressedSize(const void* data, size_t size)
{
	// return error
	if (size < sizeof(SICHeader))
		return 0;

	SIC::SICHeader header = *(SIC::SICHeader*)data;

	if(memcmp(header.magic, SIC::MAGIC, sizeof(SIC::MAGIC) != 0))
		return 0;

	// return error
	if (header.encodeType >= EncodeType::COUNT
		|| header.filterType >= FilterType::COUNT
		|| header.rearrangeType >= RearrangeType::COUNT)
		return 0;

	size_t pixelSize = header.width * header.height * GetPixelSize((EncodeType)header.encodeType);

	// return error
	if (pixelSize == 0)
		return 0;

	char* compressedPtr = (char*)data;
	compressedPtr += sizeof(SIC::SICHeader);


	size_t outSize = ZSTD_getDecompressedSize(compressedPtr, size - sizeof(SIC::SICHeader));

	// return error
	if (ZSTD_isError(outSize))
		return 0;

	// return error
	if (pixelSize != outSize)
		return 0;

	return outSize;
}

static void ApplyDeltaCodingRGB(uint8_t* __restrict memory, size_t size)
{
	uint8_t lastValues[3] = { memory[0], memory[1], memory[2] };
	for (size_t i = 3; i < size; i += 3)
	{
		uint8_t newValue;
		newValue = memory[i] - lastValues[0];
		lastValues[0] = memory[i];
		memory[i] = newValue;
		if (i + 1 >= size)
			break;
		newValue = memory[i + 1] - lastValues[1];
		lastValues[1] = memory[i + 1];
		memory[i + 1] = newValue;

		if (i + 2 >= size)
			break;

		newValue = memory[i + 2] - lastValues[2];
		lastValues[2] = memory[i + 2];
		memory[i + 2] = newValue;
	}
}

static void ReverseDeltaCodingRGB(uint8_t* __restrict memory, size_t size)
{
	for (size_t i = 3; i < size; ++i)
	{
		// no conditional logic because we do in-place so memory[i-3] is actual decoded value
		memory[i] = memory[i] + memory[i - 3];
	}
}

static void ApplyDeltaCodingR8(uint8_t* __restrict  memory, size_t size)
{
	uint8_t lastValue = memory[0];
	for (size_t i = 1; i < size; ++i)
	{
		uint8_t newValue;
		newValue = memory[i] - lastValue;
		lastValue = memory[i];
		memory[i] = newValue;
	}
}

static void ReverseDeltaCodingR8(uint8_t* __restrict  memory, size_t size)
{
	for (size_t i = 1; i < size; ++i)
	{
		memory[i] = memory[i] + memory[i - 1];
	}
}

static void ApplyDeltaCodingR16(uint16_t* __restrict memory, size_t size)
{
	uint16_t lastValue = memory[0];
	for (size_t i = 1; i < size; ++i)
	{
		uint16_t newValue;
		newValue = memory[i] - lastValue;
		lastValue = memory[i];
		memory[i] = newValue;
	}
}

static void ReverseDeltaCodingR16(uint16_t* __restrict memory, size_t size)
{
	for (size_t i = 1; i < size; ++i)
	{
		memory[i] = memory[i] + memory[i - 1];
	}
}

// TODO try SIMD
static void ApplyDeltaCodingRGBA(uint8_t* __restrict memory, size_t size)
{
	uint8_t lastValues[4] = { memory[0], memory[1], memory[2], memory[3] };
	for (size_t i = 4; i < size; i += 4)
	{
		uint8_t newValue;
		newValue = memory[i] - lastValues[0];
		lastValues[0] = memory[i];
		memory[i] = newValue;

		if (i + 1 >= size)
			break;

		newValue = memory[i + 1] - lastValues[1];
		lastValues[1] = memory[i + 1];
		memory[i + 1] = newValue;

		if (i + 2 >= size)
			break;

		newValue = memory[i + 2] - lastValues[2];
		lastValues[2] = memory[i + 2];
		memory[i + 2] = newValue;

		if (i + 3 >= size)
			break;

		newValue = memory[i + 3] - lastValues[3];
		lastValues[3] = memory[i + 3];
		memory[i + 3] = newValue;
	}
}

// TODO try SIMD
static void ReverseDeltaCodingRGBA(uint8_t* __restrict memory, size_t size)
{
	for (size_t i = 4; i < size; ++i)
	{
		// no conditional logic because we do in-place so memory[i-4] is actual decoded value
		memory[i] = memory[i] + memory[i - 4];
	};
}

// Adapted from https://stackoverflow.com/questions/10566668/lossless-rgb-to-ycbcr-transformation
static inline void ForwardLift(uint8_t& x, uint8_t& y, uint8_t& diff, uint8_t& average)
{
	diff = y - x;
	average = x + (diff >> 1);
}

static inline void ReverseLift(uint8_t& average, uint8_t& diff, uint8_t& x, uint8_t& y)
{
	x = average - (diff >> 1);
	y = x + diff;
}

// TODO merge with delta so we don't process twice
static void BGRToYCgCo(uint8_t* memory, size_t size, size_t elementSize)
{
	for (size_t i = elementSize; i < size; i += elementSize)
	{
		uint8_t temp;
		uint8_t Y;
		uint8_t Co;
		uint8_t Cg;
		ForwardLift(memory[i + 2], memory[i], temp, Co);
		ForwardLift(memory[i + 1], temp, Y, Cg);
		memory[i] = Y;
		// this can be optimized out
		memory[i + 1] = Co;
		memory[i + 2] = Cg;
	}
}

template<typename inttype>
static void ApplyXORDeltaCoding(inttype* __restrict memory, size_t size, size_t channels)
{
	std::vector<inttype> newValues;
	newValues.resize(size);
	// special case - first pixel
	for (int i = 0; i < channels; ++i)
		newValues[i] = memory[i];
	for (int i = channels; i < size; ++i)
	{
		newValues[i] = memory[i - channels] ^ memory[i];
	}
	memcpy(memory, &newValues[0], size * sizeof(inttype));
}

template<typename inttype>
static void ReverseXORDeltaCoding(inttype* __restrict memory, size_t size, size_t channels)
{
	for (int i = channels; i < size; ++i)
	{
		memory[i] = memory[i - channels] ^ memory[i];
	}
}

template<typename inttype>
static void Apply2AvgDeltaCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	std::vector<inttype> newValues;
	newValues.resize(width * height * channels);

	// special case - first pixel
	for (int i = 0; i < channels; ++i)
		newValues[i] = memory[i];

	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = channels; x < width * channels; ++x)
	{
		newValues[x] = memory[x] - memory[x - channels];
	}

	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * channels;
		const size_t rowStart = y * rowSize;
		const inttype* __restrict readPtr = &memory[rowStart];
		inttype* __restrict writePtr = &newValues[rowStart];
		inttype* rowEnd = &memory[rowStart + rowSize];

		// 1st element in row, special case
		for (int elem = 0; elem < channels; ++elem)
		{
			writePtr[elem] = readPtr[elem] - readPtr[elem - rowSize];
		}

		writePtr += channels;
		readPtr += channels;

		while (readPtr != rowEnd)
		{
			inttype predicted = (inttype)((((uint64_t)readPtr[-rowSize]) + readPtr[-channels]) / 2);
			*writePtr = *readPtr - predicted;
			++writePtr;
			++readPtr;
		}
	}
	memcpy(memory, &newValues[0], width * height * channels * sizeof(inttype));
}

template<typename inttype>
static void Reverse2AvgDeltaCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = channels; x < width * channels; ++x)
	{
		memory[x] = memory[x] + memory[x - channels];
	}

	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * channels;
		const size_t rowStart = y * rowSize;
		inttype* __restrict writePtr = &memory[rowStart];
		inttype* rowEnd = &memory[rowStart + rowSize];

		// 1st element in row, special case
		for (int elem = 0; elem < channels; ++elem)
		{
			writePtr[elem] = writePtr[elem - rowSize] + writePtr[elem];
		}

		writePtr += channels;

		while (writePtr != rowEnd)
		{
			inttype predicted = (inttype)((((uint64_t)writePtr[-rowSize]) + writePtr[-channels]) / 2);
			*writePtr = *writePtr + predicted;
			++writePtr;
		}
	}
}

template<typename inttype>
// This isn't used by any vanilla files - did I mess it up, or is it just not useful?
static void Apply3AvgDeltaCoding(inttype* memory, size_t width, size_t height, size_t channels)
{

	std::vector<inttype> newValues;
	newValues.resize(width * height * channels);

	// special case - first pixel
	for (int i = 0; i < channels; ++i)
		newValues[i] = memory[i];

	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = channels; x < width * channels; ++x)
	{
		newValues[x] = memory[x] - memory[x - channels];
	}

	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * channels;
		const size_t rowStart = y * rowSize;
		const inttype* __restrict readPtr = &memory[rowStart];
		inttype* __restrict writePtr = &newValues[rowStart];
		inttype* rowEnd = &memory[rowStart + rowSize];

		// 1st element in row, special case
		for (int elem = 0; elem < channels; ++elem)
		{
			writePtr[elem] = readPtr[elem] - readPtr[elem - rowSize];
		}

		writePtr += channels;
		readPtr += channels;

		while (readPtr != rowEnd)
		{
			inttype predicted = (inttype)(((((uint64_t)readPtr[-rowSize]) + readPtr[-channels]) + readPtr[(-channels) - rowSize]) / 3);
			*writePtr = *readPtr - predicted;
			++writePtr;
			++readPtr;
		}
	}
	memcpy(memory, &newValues[0], width * height * channels * sizeof(inttype));
}

template<typename inttype>
static void Reverse3AvgDeltaCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = channels; x < width * channels; ++x)
	{
		memory[x] = memory[x] + memory[x - channels];
	}

	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * channels;
		const size_t rowStart = y * rowSize;
		inttype* __restrict writePtr = &memory[rowStart];
		inttype* rowEnd = &memory[rowStart + rowSize];

		// 1st element in row, special case
		for (int elem = 0; elem < channels; ++elem)
		{
			writePtr[elem] = writePtr[elem - rowSize] + writePtr[elem];
		}

		writePtr += channels;

		while (writePtr != rowEnd)
		{
			inttype predicted = (inttype)(((((uint64_t)writePtr[-rowSize]) + writePtr[-channels]) + writePtr[(-channels) - rowSize])/ 3);
			*writePtr = *writePtr + predicted;
			++writePtr;
		}
	}
}

template<typename inttype>
static void ApplyPaethPredictionCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	std::vector<inttype> newValues;
	newValues.resize(width * height * channels);
	//std::cout << "Body begin: " << (1 * width * channels) + (1 * channels) << std::endl;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int elem = 0; elem < channels; ++elem)
			{

				inttype predicted;
				if (x == 0 && y == 0)
				{
					newValues[elem] = memory[elem];
					continue;
				}
				else if (x > 0 && y == 0)
					predicted = memory[((x - 1) * channels) + elem];
				else if (x == 0 && y > 0)
					predicted = memory[((y - 1) * width * channels) + elem];
				else // (x > 0 && y > 0)
				{
					inttype BL = memory[(y * width * channels) + ((x - 1) * channels) + elem];
					inttype TR = memory[((y - 1) * width * channels) + (x * channels) + elem];
					inttype TL = memory[((y - 1) * width * channels) + ((x - 1) * channels) + elem];
					int64_t base = ((int64_t)BL + TR) - TL;
					predicted = BL;
					if(std::abs(base - TR) < std::abs(base - BL))
						predicted = TR;
					if (std::abs(base - TL) < std::abs(base - BL) && std::abs(base - TL) < std::abs(base - TR))
						predicted = TL;
					/*
					size_t position = (y * width * channels) + (x * channels) + elem;
					if (position == 1801)
					{
						std::cout << "Coord " << x << " " << y << " " << elem << std::endl;
						std::cout << (int)memory[position] << " " << (int)predicted << " " << (int)memory[position] + predicted << std::endl;
						std::cout << (int)BL << " " << (int)TR << " " << (int)TL << std::endl;
					}
					*/
				}
				
				newValues[(y * width * channels) + (x * channels) + elem] = memory[(y * width * channels) + (x * channels) + elem] - predicted;
			}
		}
	}
	memcpy(memory, &newValues[0], width * height * channels * sizeof(inttype));
}

template<typename inttype>
// TODO see if this can be optimized more - this costs about as much as the Zstandard decode
static void ReversePaethPredictionCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = channels; x < width * channels; ++x)
	{
		memory[x] = memory[x - channels] + memory[x];
	}
	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * channels;
		const size_t rowStart = y * rowSize;
		inttype* __restrict writePtr = &memory[rowStart];
		inttype* rowEnd = &memory[rowStart + rowSize];

		// 1st element in row, special case
		for (int elem = 0; elem < channels; ++elem)
		{
			writePtr[elem] = writePtr[elem - rowSize] + writePtr[elem];
		}

		writePtr += channels;

		while (writePtr != rowEnd)
		{
			// I tried optimizing this arithmetic, but it didn't affect performance and this is more readable
			// I think this is the best a non-SIMD version can get
			inttype BL = writePtr[-channels];
			inttype TR = writePtr[-rowSize];
			inttype TL = writePtr[(- rowSize) - channels];
			int64_t base = ((int64_t)BL + TR) - TL;
			inttype predicted = BL;
			if (std::abs(base - TR) < std::abs(base - predicted))
				predicted = TR;
			if (std::abs(base - TL) < std::abs(base - predicted))
				predicted = TL;
			
			*writePtr = predicted + *writePtr;
			++writePtr;
		}
	}
}

template<typename inttype>
static void ApplyPaethNoNearestPredictionCoding(inttype* memory, size_t width, size_t height, size_t channels)
{
	std::vector<inttype> newValues;
	newValues.resize(width * height * channels);
	//std::cout << "Body begin: " << (1 * width * channels) + (1 * channels) << std::endl;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int elem = 0; elem < channels; ++elem)
			{

				inttype predicted;
				if (x == 0 && y == 0)
				{
					newValues[elem] = memory[elem];
					continue;
				}
				else if (x > 0 && y == 0)
					predicted = memory[((x - 1) * channels) + elem];
				else if (x == 0 && y > 0)
					predicted = memory[((y - 1) * width * channels) + elem];
				else // (x > 0 && y > 0)
				{
					inttype BL = memory[(y * width * channels) + ((x - 1) * channels) + elem];
					inttype TR = memory[((y - 1) * width * channels) + (x * channels) + elem];
					inttype TL = memory[((y - 1) * width * channels) + ((x - 1) * channels) + elem];
					predicted = (inttype)std::min(std::max(0ll, ((int64_t)BL + TR) - TL), (int64_t)((1 << (sizeof(inttype) * 8)) - 1));
				}

				newValues[(y * width * channels) + (x * channels) + elem] = memory[(y * width * channels) + (x * channels) + elem] - predicted;
			}
		}
	}
	memcpy(memory, &newValues[0], width * height * channels * sizeof(inttype));
}

template<typename inttype>
static void ReversePaethNoNearestPredictionCoding(inttype* memory, size_t width, size_t height, size_t elementSize)
{
	// 1st row (special case)
	// 1st element is unmodified so we skip it
	for (int x = elementSize; x < width * elementSize; ++x)
	{
		memory[x] = memory[x - elementSize] + memory[x];
	}
	for (int y = 1; y < height; ++y)
	{
		const size_t rowSize = width * elementSize;
		const size_t rowStart = y * rowSize;
		inttype* __restrict rowptr = &memory[rowStart];

		// 1st element in row, special case
		for (int elem = 0; elem < elementSize; ++elem)
			memory[(y * rowSize) + elem] = memory[((y - 1) * rowSize) + elem] + memory[(y * rowSize) + elem];

		for (int x = elementSize; x < width * elementSize; ++x)
		{
			inttype BL = rowptr[x - elementSize];
			inttype TR = rowptr[x - rowSize];
			inttype TL = rowptr[(x - rowSize) - elementSize];
			inttype predicted = (inttype)std::min(std::max(0ll, ((int64_t)BL + TR) - TL), (int64_t)((1 << (sizeof(inttype) * 8)) - 1));

			rowptr[x] = predicted + rowptr[x];
		}
	}
}

// TODO merge with delta so we don't process twice
static void YCgCoToBGR(uint8_t* memory, size_t size, size_t elementSize)
{
	for (size_t i = elementSize; i < size; i += elementSize)
	{
		uint8_t temp;
		uint8_t R;
		uint8_t G;
		uint8_t B;
		ReverseLift(memory[i], memory[i + 2], G, temp);
		ReverseLift(temp, memory[i + 1], R, B);
		// R + B can be optimized out
		memory[i] = B;
		memory[i + 1] = G;
		memory[i + 2] = R;
	}
}

static void SeparateChannels(uint8_t* memory, const size_t size, const size_t numChannels)
{
	std::vector<uint8_t> separated;
	separated.resize(size);
	int channelSize = size / numChannels;
	for (int i = 0; i < size; i++)
	{
		int channelIdx = i % numChannels;
		int pixelIndex = i / numChannels;
		int writePos = (channelIdx * channelSize) + pixelIndex;
		separated[writePos] = memory[i];
	}
	memcpy(memory, &separated[0], size);
	// this does a *shitload* of random accesses, but runs in-place
	/*
	int channelSize = size / numChannels;
	// we skip the first + last element since they don't move, also we exit at size-1 swaps
	int swaps = 2;
	// this is cooked AF
	for (int startReadPos = 1; swaps < size; startReadPos += 2)
	{
		std::cout << "swaps " << swaps << " " << size <<  " " << startReadPos << std::endl;
		uint8_t value = memory[startReadPos];
		int channelIdx = startReadPos % numChannels;
		int pixelIndex = startReadPos / numChannels;
		int writePos = (channelIdx * channelSize) + pixelIndex;

		if (writePos != startReadPos)
		{
			std::cout << startReadPos << " ";
			while (writePos != startReadPos)
			{
				std::cout << writePos << " ";
				std::swap(value, memory[writePos]);
				channelIdx = writePos % numChannels;
				pixelIndex = writePos / numChannels;
				writePos = (channelIdx * channelSize) + pixelIndex;
				++swaps;
			}
			std::swap(value, memory[writePos]);
			std::cout << std::endl;
		}
		// this also accounts for identity swaps
		++swaps;
		std::cout << "swaps " << swaps << " " << size << " " << startReadPos << std::endl;
	}
	*/
}

// TODO can this be done faster?
// scratch has to be at least `size` large
static void CombineChannels(uint8_t* memory, uint8_t* scratch, const size_t size, const size_t numChannels)
{
	int channelSize = size / numChannels;
	for (int i = 0; i < size; i++)
	{
		int channelIdx = i % numChannels;
		int pixelIndex = i / numChannels;
		int readPos = (channelIdx * channelSize) + pixelIndex;
		scratch[i] = memory[readPos];
	}
	memcpy(memory, scratch, size);
}

// in bits
double GetEntropy(uint8_t* data, size_t size)
{
	std::vector<size_t> symbolCounts;
	symbolCounts.resize(256);
	memset(&symbolCounts[0], 0, sizeof(size_t) * 256);
	for (int i = 0; i < size; ++i)
	{
		++symbolCounts[data[i]];
	}
	double totalEntropy = 0;
	for (int i = 0; i < 256; ++i)
		if(symbolCounts[i] > 0)
			totalEntropy += -log((double)symbolCounts[i] / size) * symbolCounts[i] / log(2.0);

	return totalEntropy;
}

// finds best filter for each row
// this results in larger files lmao
//  it might be possible to improve this by using Zstd dictionaries but early results are not promising
/*
size_t RowCompress(void* outData, size_t outSize, const void* inData, size_t width, size_t height, SIC::EncodeType encodeType, int compressionLevel)
{
	using namespace SIC;

	int rowCombine = 16;

	if (height % rowCombine != 0)
	{
		rowCombine = 1;

		std::cout << "disabling row combine" << std::endl;
	}

	if (encodeType == EncodeType::R16)
		return 0;

	size_t maxSize = GetMaxCompressedSize(width, height, encodeType);

	// return error - incorrect max. out size causes performance issues
	if (maxSize > outSize)
		return 0;


	char* compresedWritePos = (char*)outData;
	compresedWritePos += sizeof(SICHeader);

	size_t pixelSize = width * height * GetPixelSize(encodeType);

	size_t totalSize = 0;

	// TODO optimize away
	std::vector<uint8_t> finalDeltas;
	finalDeltas.resize(pixelSize);
	// stuff to compress
	std::vector<uint8_t> encodeBuffer;
	encodeBuffer.resize(rowCombine * width * GetPixelSize(encodeType) * 2);
	// stores the best delta coding for the current row
	std::vector<uint8_t> bestRow;
	bestRow.resize(rowCombine * width * GetPixelSize(encodeType));
	// buffer for storing temporary compressed stuff
	std::vector<uint8_t> compressedTemp;
	compressedTemp.resize(ZSTD_compressBound(encodeBuffer.size()));
	// best filter for each row
	std::vector<uint8_t> rowFilters;
	rowFilters.resize(height);

	// first row - special case
	memcpy(&bestRow[0], inData, rowCombine * width * GetPixelSize(encodeType));
	// NONE
	size_t bestEncodeSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &bestRow[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	rowFilters[0] = FilterType::NONE;
	// TODO REMOVE avoid none
	//bestEncodeSize += 10000000;
	// LEFT
	memcpy(&encodeBuffer[0], inData, rowCombine * width * GetPixelSize(encodeType));
	if (encodeType == EncodeType::BGR)
		ApplyDeltaCodingRGB(&encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	else if (encodeType == EncodeType::BGRA)
		ApplyDeltaCodingRGBA(&encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	else if (encodeType == EncodeType::R8)
		ApplyDeltaCodingR8(&encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	size_t encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::LEFT;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// 2AVG
	memcpy(&encodeBuffer[0], inData, rowCombine * width * GetPixelSize(encodeType));
	Apply2AvgDeltaCoding(&encodeBuffer[0], width, rowCombine, GetPixelSize(encodeType));
	encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::TWO_AVERAGE;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// 3AVG
	memcpy(&encodeBuffer[0], inData, rowCombine * width * GetPixelSize(encodeType));
	Apply3AvgDeltaCoding(&encodeBuffer[0], width, rowCombine, GetPixelSize(encodeType));
	encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::THREE_AVERAGE;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// 3AVG
	memcpy(&encodeBuffer[0], inData, rowCombine * width * GetPixelSize(encodeType));
	ApplyPaethPredictionCoding(&encodeBuffer[0], width, rowCombine, GetPixelSize(encodeType));
	encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::PAETH;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// PAETH NO NEAREST
	memcpy(&encodeBuffer[0], inData, rowCombine * width * GetPixelSize(encodeType));
	ApplyPaethNoNearestPredictionCoding(&encodeBuffer[0], width, rowCombine, GetPixelSize(encodeType));
	encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::PAETH_NONEAREST;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// XOR
	memcpy(&encodeBuffer[0], inData, rowCombine * width* GetPixelSize(encodeType));
	ApplyXORDeltaCoding(&encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), GetPixelSize(encodeType));
	encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType), compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		bestEncodeSize = encodedSize;
		rowFilters[0] = FilterType::XOR;
		// copy deltas to buffer for reuse later
		memcpy(&bestRow[0], &encodeBuffer[0], rowCombine * width * GetPixelSize(encodeType));
	}
	// copy to final deltas
	memcpy(&finalDeltas[0], &bestRow[0], rowCombine * width * GetPixelSize(encodeType));
	std::cout << "Best filter: " << std::to_string((uint64_t)rowFilters[0]) << " " << bestEncodeSize << std::endl;
	// all other rows

	totalSize = bestEncodeSize;

	for (int section = 1; section < height / rowCombine; ++section)
	{
		int y = section * rowCombine;

		// includes previous row
		const uint8_t* currentPixels = (uint8_t*)inData;
		currentPixels += (y - rowCombine) * width * GetPixelSize(encodeType);
		uint8_t* rowEncodeBufferPos = &encodeBuffer[rowCombine * width * GetPixelSize(encodeType)];
		const uint8_t* lastDeltas = &finalDeltas[(y - rowCombine) * width * GetPixelSize(encodeType)];

		// copy new values
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		// NONE
		size_t bestEncodeSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		rowFilters[section] = FilterType::NONE;
		// TODO REMOVE avoid none
		//bestEncodeSize += 10000000;
		
		// LEFT
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		if (encodeType == EncodeType::BGR)
			ApplyDeltaCodingRGB(&encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType));
		else if (encodeType == EncodeType::BGRA)
			ApplyDeltaCodingRGBA(&encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType));
		else if (encodeType == EncodeType::R8)
			ApplyDeltaCodingR8(&encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		size_t encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::LEFT;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		
		// 2AVG
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		Apply2AvgDeltaCoding(&encodeBuffer[0], width, 2 * rowCombine, GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::TWO_AVERAGE;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		// 3AVG
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		Apply3AvgDeltaCoding(&encodeBuffer[0], width, 2 * rowCombine, GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::THREE_AVERAGE;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		// PAETH
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		ApplyPaethPredictionCoding(&encodeBuffer[0], width, 2 * rowCombine, GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::PAETH;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		
		// PAETH NO NEAREST
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		ApplyPaethNoNearestPredictionCoding(&encodeBuffer[0], width, 2 * rowCombine, GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::PAETH_NONEAREST;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		// XOR
		// copy values for delta coding (last row + this row)
		memcpy(&encodeBuffer[0], currentPixels, 2 * rowCombine * width * GetPixelSize(encodeType));
		ApplyXORDeltaCoding(&encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), GetPixelSize(encodeType));
		// copy best encoding of last row in (used to partially initialize Zstandard state)
		memcpy(&encodeBuffer[0], lastDeltas, rowCombine * width * GetPixelSize(encodeType));
		encodedSize = ZSTD_compress(&compressedTemp[0], compressedTemp.size(), &encodeBuffer[0], 2 * rowCombine * width * GetPixelSize(encodeType), compressionLevel);
		if (encodedSize < bestEncodeSize)
		{
			bestEncodeSize = encodedSize;
			rowFilters[section] = FilterType::XOR;
			// copy deltas to buffer for reuse later
			memcpy(&bestRow[0], rowEncodeBufferPos, rowCombine * width * GetPixelSize(encodeType));
		}
		
		// copy to final deltas
		memcpy(&finalDeltas[y * width * GetPixelSize(encodeType)], &bestRow[0], rowCombine * width * GetPixelSize(encodeType));
		
		std::cout << "Best filter: " << std::to_string((uint64_t)rowFilters[section]) << " " << bestEncodeSize << std::endl;
		totalSize += bestEncodeSize;
	}

	// we should have generated the full image
	size_t finalBytes = ZSTD_compress(outData, outSize, &finalDeltas[0], width * height * GetPixelSize(encodeType), compressionLevel);
	std::cout << "Final size: " << finalBytes << " " << (totalSize / 2) << std::endl;

	return finalBytes;
}
*/

size_t SIC::Compress(void* outData, size_t outSize, const void* inData, size_t width, size_t height, SIC::EncodeType encodeType, SIC::FilterType filterType, SIC::RearrangeType rearrangeType, int compressionLevel)
{
	size_t maxSize = GetMaxCompressedSize(width, height, encodeType);

	// return error - incorrect max. out size causes performance issues
	if (maxSize > outSize)
		return 0;

	// return error
	if (encodeType >= EncodeType::COUNT
		|| filterType >= FilterType::COUNT
		|| rearrangeType >= RearrangeType::COUNT)
		return 0;

	char* compresedWritePos = (char*)outData;
	compresedWritePos += sizeof(SIC::SICHeader);

	size_t pixelSize = width * height * GetPixelSize(encodeType);

	// copy data if we need to modify it
	std::vector<uint8_t> dataCopy;
	if (filterType != FilterType::NONE|| rearrangeType != RearrangeType::NONE)
	{
		dataCopy.resize(pixelSize);
		memcpy(&dataCopy[0], inData, pixelSize);
		inData = &dataCopy[0];
	}

	// filter
	if (encodeType == EncodeType::R16)
	{
		if (filterType == FilterType::LEFT)
			// pixelSize is in bytes, so halve since one value is 2 bytes
			ApplyDeltaCodingR16((uint16_t*)&dataCopy[0], pixelSize / 2);
		else if (filterType == FilterType::TWO_AVERAGE)
			Apply2AvgDeltaCoding((uint16_t*)&dataCopy[0], width, height, 1);
		else if (filterType == FilterType::THREE_AVERAGE)
			Apply3AvgDeltaCoding((uint16_t*)&dataCopy[0], width, height, 1);
		else if (filterType == FilterType::PAETH)
			ApplyPaethPredictionCoding((uint16_t*)&dataCopy[0], width, height, 1);
		else if (filterType == FilterType::PAETH_NONEAREST)
			ApplyPaethNoNearestPredictionCoding((uint16_t*)&dataCopy[0], width, height, 1);
		else if (filterType == FilterType::XOR)
			// pixelSize is in bytes, so halve since one value is 2 bytes
			ApplyXORDeltaCoding((uint16_t*)&dataCopy[0], pixelSize / 2, 1);
	}
	else
	{
		if (filterType == FilterType::LEFT && encodeType == EncodeType::BGR)
			ApplyDeltaCodingRGB(&dataCopy[0], pixelSize);
		else if (filterType == FilterType::LEFT && encodeType == EncodeType::BGRA)
			ApplyDeltaCodingRGBA(&dataCopy[0], pixelSize);
		else if (filterType == FilterType::LEFT && encodeType == EncodeType::R8)
			ApplyDeltaCodingR8(&dataCopy[0], pixelSize);
		else if (filterType == FilterType::TWO_AVERAGE)
			Apply2AvgDeltaCoding(&dataCopy[0], width, height, GetPixelSize(encodeType));
		else if (filterType == FilterType::THREE_AVERAGE)
			Apply3AvgDeltaCoding(&dataCopy[0], width, height, GetPixelSize(encodeType));
		else if (filterType == FilterType::PAETH)
			ApplyPaethPredictionCoding(&dataCopy[0], width, height, GetPixelSize(encodeType));
		else if (filterType == FilterType::PAETH_NONEAREST)
			ApplyPaethNoNearestPredictionCoding(&dataCopy[0], width, height, GetPixelSize(encodeType));
		else if (filterType == FilterType::XOR)
			// pixelSize is in bytes, so halve since one value is 2 bytes
			ApplyXORDeltaCoding(&dataCopy[0], pixelSize, GetPixelSize(encodeType));
	}

	// rearrange (only available if more than 1 channel)
	if(encodeType != EncodeType::R8 && encodeType != EncodeType::R16 && rearrangeType == RearrangeType::SEPARATE_CHANNELS)
		SeparateChannels(&dataCopy[0], pixelSize, GetPixelSize(encodeType));

	size_t compressedSize = ZSTD_compress(compresedWritePos, outSize - sizeof(SIC::SICHeader), inData, pixelSize, compressionLevel);

	// return error
	if (ZSTD_isError(compressedSize))
		return 0;

	// write header
	SIC::SICHeader header;
	header.width = width;
	header.height = height;
	header.encodeType = encodeType;
	header.filterType = filterType;
	header.rearrangeType = rearrangeType;

	memcpy(outData, &header, sizeof(header));

	// success
	return compressedSize + sizeof(header);
}

size_t SIC::Compress(void* outData, size_t outSize, const void* inData, size_t width, size_t height, SIC::EncodeType type, int compressionLevel)
{
	size_t maxSize = GetMaxCompressedSize(width, height, type);

	// return error - incorrect max. out size causes performance issues
	if (maxSize > outSize)
		return 0;

	char* compresedWritePos = (char*)outData;
	compresedWritePos += sizeof(SIC::SICHeader);

	size_t pixelSize = width * height * GetPixelSize(type);
	/*
	std::vector<uint8_t> test;
	test.resize(32);
	for (int i = 0; i < 32; ++i)
		test[i] = i;
	SeparateChannels(&test[0], 16, 4);
	for (int i = 0; i < 32; ++i)
		std::cout << (int)test[i] << " ";
	std::cout << std::endl;
	*/
	
	// rearrage definitely reduces size with CL 1 but makes almost no difference at CL19
	// SLOW AS FUCK JUST USE CL19 AND GET OVER IT
	// actually, I think this might knock an average of 5% more off vs None
	RearrangeType rearrangeType = RearrangeType::NONE; // compressionLevel < 5 ? RearrangeType::SEPARATE_CHANNELS : RearrangeType::NONE;

	// TODO try per-scanline filtering like PNG does
	std::vector<uint8_t> bytes;
	bytes.resize(outSize);

	//RowCompress(&bytes[0], outSize, inData, width, height, type, compressionLevel);

	// NONE wins often enough to be useful
	size_t bestEncodeSize = Compress(outData, outSize, inData, width, height, type, FilterType::NONE, rearrangeType, compressionLevel);
	// TODO rearrange from fastest to slowest and only switch codecs if space saved is >x%
	size_t startEncodeSize = bestEncodeSize;
	size_t encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::LEFT, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::TWO_AVERAGE, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::PAETH, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::PAETH_NONEAREST, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	// XOR pretty much never wins (once in vanilla)
	encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::XOR, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	// for some reason, 3-avg never seems to get used
	encodedSize = Compress(&bytes[0], outSize, inData, width, height, type, FilterType::THREE_AVERAGE, rearrangeType, compressionLevel);
	if (encodedSize < bestEncodeSize)
	{
		memcpy(outData, &bytes[0], encodedSize);
		bestEncodeSize = encodedSize;
	}
	//std::cout << "Final size 2: " << bestEncodeSize << std::endl;
	// success
	return bestEncodeSize;
}

size_t SIC::Decompress(void* outData, size_t outDataSize, const void* inData, size_t inSize)
{
	// return error - incorrect max. out size causes performance issues
	if (inSize < sizeof(SIC::SICHeader))
		return 0;

	SICHeader header = *(SIC::SICHeader*)inData;

	if (memcmp(header.magic, SIC::MAGIC, sizeof(SIC::MAGIC) != 0))
		return 0;

	size_t pixelsSize = header.width * header.height * GetPixelSize((EncodeType)header.encodeType);

	// return error
	if (pixelsSize == 0)
		return 0;

	// return error
	if (outDataSize < pixelsSize)
		return 0;

	char* compressedPtr = (char*)inData;
	compressedPtr += sizeof(SIC::SICHeader);

	size_t outSize = ZSTD_getDecompressedSize(compressedPtr, inSize - sizeof(SIC::SICHeader));

	// return error
	if (ZSTD_isError(outSize))
		return 0;

	// return error
	if (outDataSize < outSize)
		return 0;

	size_t finalOutSize = ZSTD_decompress(outData, outDataSize, compressedPtr, inSize - sizeof(SIC::SICHeader));

	if (ZSTD_isError(finalOutSize))
		std::cout << ZSTD_getErrorName(finalOutSize) << std::endl;

	// return error
	if (ZSTD_isError(finalOutSize))
		return 0;

	// return error
	if (finalOutSize % GetPixelSize((EncodeType)header.encodeType) != 0)
		return 0;

	// at this point, decode is successful, just need to reformat pixel data

	// allocate scratch if needed
	std::vector<uint8_t> scratch;
	if (header.filterType != FilterType::NONE || header.rearrangeType != RearrangeType::NONE)
		scratch.resize(finalOutSize);

	// rearrange (only available if more than 1 channel)
	if (header.encodeType != EncodeType::R8 && header.encodeType != EncodeType::R16 && header.rearrangeType == RearrangeType::SEPARATE_CHANNELS)
		CombineChannels((uint8_t*)outData, &scratch[0], finalOutSize, GetPixelSize((EncodeType)header.encodeType));

	// unfilter
	if (header.encodeType == EncodeType::R16)
	{
		if (header.filterType == FilterType::LEFT)
			// finalOutSize is in bytes, so halve since one value is 2 bytes
			ReverseDeltaCodingR16((uint16_t*)outData, finalOutSize / 2);
		else if (header.filterType == FilterType::TWO_AVERAGE)
			Reverse2AvgDeltaCoding((uint16_t*)outData, header.width, header.height, 1);
		else if (header.filterType == FilterType::THREE_AVERAGE)
			Reverse3AvgDeltaCoding((uint16_t*)outData, header.width, header.height, 1);
		else if (header.filterType == FilterType::PAETH)
			ReversePaethPredictionCoding((uint16_t*)outData, header.width, header.height, 1);
		else if (header.filterType == FilterType::PAETH_NONEAREST)
			ReversePaethNoNearestPredictionCoding((uint16_t*)outData, header.width, header.height, 1);
		else if (header.filterType == FilterType::XOR)
			// pixelSize is in bytes, so halve since one value is 2 bytes
			ReverseXORDeltaCoding((uint16_t*)outData, finalOutSize / 2, 1);
		else if (header.filterType != FilterType::NONE)
			return 0;
	}
	else
	{
		if (header.filterType == FilterType::LEFT && header.encodeType == EncodeType::BGR)
			ReverseDeltaCodingRGB((uint8_t*)outData, finalOutSize);
		else if (header.filterType == FilterType::LEFT && header.encodeType == EncodeType::BGRA)
			ReverseDeltaCodingRGBA((uint8_t*)outData, finalOutSize);
		else if (header.filterType == FilterType::LEFT && header.encodeType == EncodeType::R8)
			ReverseDeltaCodingR8((uint8_t*)outData, finalOutSize);
		else if (header.filterType == FilterType::TWO_AVERAGE)
			Reverse2AvgDeltaCoding((uint8_t*)outData, header.width, header.height, GetPixelSize((EncodeType)header.encodeType));
		else if (header.filterType == FilterType::THREE_AVERAGE)
			Reverse3AvgDeltaCoding((uint8_t*)outData, header.width, header.height, GetPixelSize((EncodeType)header.encodeType));
		else if (header.filterType == FilterType::PAETH)
			ReversePaethPredictionCoding((uint8_t*)outData, header.width, header.height, GetPixelSize((EncodeType)header.encodeType));
		else if (header.filterType == FilterType::PAETH_NONEAREST)
			ReversePaethNoNearestPredictionCoding((uint8_t*)outData, header.width, header.height, GetPixelSize((EncodeType)header.encodeType));
		else if (header.filterType == FilterType::XOR)
			// pixelSize is in bytes, so halve since one value is 2 bytes
			ReverseXORDeltaCoding((uint8_t*)outData, finalOutSize, GetPixelSize((EncodeType)header.encodeType));
		else if (header.filterType != FilterType::NONE)
			return 0;
	}

	return finalOutSize;
}