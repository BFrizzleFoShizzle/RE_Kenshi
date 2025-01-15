#include <stdint.h>
#include <string.h>

// "Shitty Image Codec" or "Stupid Image Codec"
namespace SIC
{
	static char MAGIC[4] = { 'S', 'h', 'I', 't' };
	struct SICHeader
	{
		inline SICHeader()
			: width(0), height(0), encodeType(EncodeType::COUNT), filterType(FilterType::COUNT), rearrangeType(RearrangeType::COUNT)
		{
			memcpy(magic, MAGIC, sizeof(MAGIC));
		}
		// C++03 is ass
		struct EncodeType {
			enum Enum
			{
				BGR,
				BGRA,
				R8,
				R16,
				COUNT
			};
		};
		struct FilterType {
			enum Enum
			{
				NONE,
				LEFT,
				TWO_AVERAGE,
				THREE_AVERAGE,
				PAETH,
				// use extrapolation as prediction instead of neighbour value closest to extrapolation
				PAETH_NONEAREST,
				XOR,
				COUNT
				/*
				BGR_ZST,
				BGRA_ZST,
				BGR_DELTA_ZST,
				BGRA_DELTA_ZST,
				// YCoCg - decodes to BGR
				YCoCg_DELTA_ZST,
				YCoCgA_DELTA_ZST,
				COUNT
				*/
			};
		};
		struct RearrangeType
		{
			enum Enum
			{
				NONE,
				SEPARATE_CHANNELS,
				COUNT
			};
		};
		char magic[4];
		uint32_t width;
		uint32_t height;
		uint8_t encodeType;
		uint8_t filterType;
		uint8_t rearrangeType;
		uint8_t reserved; // makes alignment obvious
	};

	typedef SICHeader::EncodeType::Enum EncodeType;
	typedef SICHeader::FilterType::Enum FilterType;
	typedef SICHeader::RearrangeType::Enum RearrangeType;

	size_t Compress(void* outData, size_t outSize, const void* inData, size_t width, size_t height, EncodeType encodeType, int compressionLevel = 1);
	size_t Compress(void* outData, size_t outSize, const void* inData, size_t width, size_t height, EncodeType encodeType, FilterType filterType, RearrangeType rearrangeType, int compressionLevel = 1);
	SICHeader GetHeader(const void* inData, size_t inSize);
	size_t GetMaxCompressedSize(size_t width, size_t height, SIC::EncodeType type);
	size_t GetDecompressedSize(const void* inData, size_t inSize);
	size_t Decompress(void* outData, size_t outSize, const void* inData, size_t inSize);
}