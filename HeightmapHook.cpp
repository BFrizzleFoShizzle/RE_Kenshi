#include "HeightmapHook.h"
#include "Escort.h"

#include <ogre/OgreVector3.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <sstream>
#include <boost/filesystem.hpp>

#include "Debug.h"
#include <CompressToolsLib.h>
#include "Settings.h"
#include "io.h"
#include <Release_Assert.h>
#include <core/Functions.h>


// pretty sure this macro is defined in C++11? so we just NOP it out
#define __func__ ""
#define TINY_DNG_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "tiny_dng_loader.h"

// NOTE: historic discrepancies for this are handled in the settings file rebinding code
static const std::string HEIGHTMAP_CIF_PATH = "data\\newland/land\\fullmap.cif";
static const std::string HEIGHTMAP_TIF_PATH = "data\\newland/land\\fullmap.tif"; 
CompressToolsLib::CompressedImageFileHdl heightmapHandle = nullptr;

// TODO named after reversed behaviour, no idea what it actutally is
struct TerrainBounds
{
	char unk1[8];
	int mapMaxX;
	int mapMaxY;
};

struct Terrain
{
	char unk1[0x1B8];
	TerrainBounds* bounds;
	// 0x1C0
	char unk2[0x10];
	// 0x1D0
	// 0.1495384127
	float heightScale;
	float worldToTextureOffset;
	float worldToTextureScale;
	// 0x1DC
	char unk3[0x84];
	// 0x260
	// THIS IS WRONG - don't know where I got it from
	//int mapMinX;
	//int mapMinY;

};

// used for mmap'd TIF loading, which is faster on SSDs
// adapted from tinydng::LoadDNGFromMemory
static bool ParseDNGFromMemory(const char* mem, unsigned int size,
	std::vector<tinydng::FieldInfo>& custom_fields,
	std::vector<tinydng::DNGImage>* images)
{
	if (mem == nullptr || size < 32 || images == nullptr) {
		ErrorLog("Invalid argument. Argument is null or invalid.");
		return false;
	}

	bool is_dng_big_endian = false;

	const unsigned short magic = *(reinterpret_cast<const unsigned short*>(mem));

	if (magic == 0x4949) 
	{
		// might be TIFF(DNG).
	}
	else if (magic == 0x4d4d) 
	{
		// might be TIFF(DNG, bigendian).
		is_dng_big_endian = true;
		TINY_DNG_DPRINTF("DNG is big endian\n");
	}
	else 
	{
		std::stringstream ss;
		ss << "Seems the data is not a DNG format." << std::endl;
		ErrorLog(ss.str());
		return false;
	}

	const bool swap_endian = (is_dng_big_endian && (!tinydng::IsBigEndian()));
	tinydng::StreamReader sr(reinterpret_cast<const uint8_t*>(mem), size, swap_endian);

	char header[32];

	if (32 != sr.read(32, 32, reinterpret_cast<unsigned char*>(header))) 
	{
		ErrorLog("Error reading header");
		return false;
	}

	// skip magic header
	if (!sr.seek_set(4)) 
	{
		ErrorLog("Failed to seek to offset 4.");
		return false;
	}

	std::string warn, err;
	bool ret = ParseDNGFromMemory(sr, custom_fields, images, &warn, &err);

	if (!ret) 
	{
		ErrorLog(err + "Failed to parse DNG data.");
		return false;
	}

	if (images->size() != 1)
	{
		ErrorLog("wrong number of images in heightmap TIFF");
		return false;
	}

	tinydng::DNGImage* image = &images->at(0);
	image->bits_per_sample = image->bits_per_sample_original;

	if (image->bits_per_sample != 16)
	{
		ErrorLog("wrong number of bits per sample in heightmap TIFF");
		return false;
	}

	return true;
}

static uint16_t* mappedHeightmapPixels = nullptr;
static const uint64_t heightmapDim = 16385;

static uint16_t* MMapTIFF(std::string path)
{
	if (!boost::filesystem::exists(path))
	{
		ErrorLog("Heightmap TIFF file doesn't exist!");
		return nullptr;
	}
	size_t fileSize = boost::filesystem::file_size(path);
	// TODO is sequential scan useful here?
	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		ErrorLog("Error opening TIFF file");
		return nullptr;
	}
	HANDLE hMappingObject = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hMappingObject == NULL)
	{
		ErrorLog("Error mapping TIFF file");
		CloseHandle(hFile);
		return nullptr;
	}
	char* fileAddr = (char*)MapViewOfFile(hMappingObject, FILE_MAP_READ, 0, 0, 0);
	if (fileAddr == NULL)
	{
		ErrorLog("Error mapping view of TIFF file");
		CloseHandle(hFile);
		CloseHandle(hMappingObject);
		return nullptr;
	}

	std::vector<tinydng::FieldInfo> custom_fields;
	std::vector<tinydng::DNGImage> images;
	// tinydng::LoadDNGFromMemory actually loads all bytes into RAM, which we don't want to do...
	bool loaded = ParseDNGFromMemory(fileAddr, fileSize, custom_fields, &images);

	if (!loaded)
	{
		ErrorLog("ParseDNGFromMemory failed loading " + path);
		return nullptr;
	}

	tinydng::DNGImage* image = &images[0];
	const size_t data_offset = (image->offset > 0) ? image->offset : image->tile_offset;

	const size_t len = size_t(image->samples_per_pixel) *
		size_t(image->width) * size_t(image->height) *
		size_t(image->bits_per_sample) / size_t(8);

	if (data_offset + len > fileSize)
	{
		ErrorLog("TIFF pixel buffer overruns" + path);
		return nullptr;
	}

	if (len != heightmapDim * heightmapDim * sizeof(uint16_t))
	{
		ErrorLog("TIFF pixel buffer size unexpected, got " + std::to_string(len) + " expected " + std::to_string(heightmapDim * heightmapDim * sizeof(uint16_t)));
		return nullptr;
	}

	uint16_t* pixels = (uint16_t*)&fileAddr[data_offset];
	return pixels;

	/*
	// here's GC code, but there's no reason to close the heightmap
	UnmapViewOfFile(fileAddr);
	CloseHandle(hFile);
	CloseHandle(hMappingObject);
	*/
}

// These exist so we don't have to re-read this setting a million times in hot loops
// These don't need to be thread-safe since incorrectly using the old value
// only results in a performance loss, not a crash
static HeightmapHook::HeightmapMode heightmapModeCache = HeightmapHook::VANILLA;

float (*Terrain_getHeight_orig)(Terrain* thisPtr, class Ogre::Vector3 const& vec, int unk);
float Terrain_getHeight_hook(Terrain* thisPtr, class Ogre::Vector3 const& vec, int unk)
{
	HeightmapHook::HeightmapMode heightmapMode = heightmapModeCache;
	if (heightmapMode == HeightmapHook::VANILLA)
		return Terrain_getHeight_orig(thisPtr, vec, unk);

	// transform into terrain space
	// reverse-engineered
	float x_transformed = vec.x - thisPtr->worldToTextureOffset;
	x_transformed *= thisPtr->worldToTextureScale;

	float z_transformed = vec.z - thisPtr->worldToTextureOffset;
	z_transformed = z_transformed * thisPtr->worldToTextureScale;

	int pixelX = int(x_transformed);
	int pixelY = int(z_transformed);

	// in-game bounds checks
	// Tested: DEFINITELY L/G NOT LE/GE
	if (pixelX < thisPtr->bounds->mapMaxX
		&& pixelY < thisPtr->bounds->mapMaxY
		&& pixelX >= 0
		&& pixelY >= 0)
	{
		uint16_t height = 0;
		if (heightmapMode == HeightmapHook::COMPRESSED)
			height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
		else if (heightmapMode == HeightmapHook::FAST_UNCOMPRESSED)
			height = mappedHeightmapPixels[pixelX + (pixelY * thisPtr->bounds->mapMaxX)];
		else
		{
			ErrorLog("Heightmap mode is invalid! Resetting to vanilla.");
			Settings::SetHeightmapMode(HeightmapHook::VANILLA);
			heightmapModeCache = HeightmapHook::VANILLA;
			return Terrain_getHeight_orig(thisPtr, vec, unk);
		}
		return height * thisPtr->heightScale;
	}
	else
	{
		// from my tests, this always returns 0
		// TODO re-check why the function call here is broken
		return 0.0f;// Terrain_getHeight_orig(thisPtr, vec, unk);
	}
}

unsigned __int64 (*Terrain_getRawData_orig)(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out);
unsigned __int64 Terrain_getRawData_hook(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out)
{
	// this MUST be cached here for safety reasons, explained in the below note
	HeightmapHook::HeightmapMode heightmapMode = heightmapModeCache;

	if (heightmapMode == HeightmapHook::VANILLA)
		return Terrain_getRawData_orig(thisPtr, x, y, w, h, out);

	// this check should never fail, if it does, the following code will segfault
	if (heightmapMode != HeightmapHook::COMPRESSED)
		assert_release(mappedHeightmapPixels != nullptr);

	uint16_t *shortPtr = reinterpret_cast<uint16_t*>(out);
	uint64_t written = 0;

	const int mapXBound = thisPtr->bounds->mapMaxX - 1;
	const int mapYBound = thisPtr->bounds->mapMaxY - 1;
	for (int i = 0; i < h; ++i)
	{
		for (int j = 0; j < w; ++j)
		{
			const int pixelX = std::min(std::max(0, x + j), mapXBound);
			const int pixelY = std::min(std::max(0, y + i), mapYBound);
			uint16_t height = 0;
			// Note: using useCompressedHeightmapCache directly here would be unsafe if the setting is 
			// toggled part-way through the function and UseFastUncompressedHeightmap() is false
			// (the mmap read would trigger a segfault)
			if (heightmapMode == HeightmapHook::COMPRESSED)
				height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
			else
				height = mappedHeightmapPixels[pixelX + (pixelY * heightmapDim)];
			shortPtr[i * w + j] = height;
			++written;
		}
	}
	return written;
}

static void CompressToolsDebugLog(const char* message)
{
	DebugLog(message);
}

static void CompressToolsErrorLog(const char* message)
{
	ErrorLog(message);
}

void HeightmapHook::Preload()
{
	// this is called before mods are set up, so doing anything else here is unsafe
	CompressToolsLib::SetLoggers(CompressToolsDebugLog, CompressToolsErrorLog);
}

void HeightmapHook::Init()
{
	// first call to this on a drive takes ~250ms, so we do it while the main menu is loading
	// that way, lookups on Kenshi's install drive won't cause stalls when they're actually needed
	HeightmapHook::HeightmapMode compressedHeightmapExists = GetRecommendedHeightmapMode();
	if (Settings::GetHeightmapMode() == HeightmapHook::AUTO)
	{
		if (compressedHeightmapExists == COMPRESSED
			&& CompressedHeightmapFileExists())
		{
			Settings::SetHeightmapMode(COMPRESSED);
		}
		else
		{
			Settings::SetHeightmapMode(FAST_UNCOMPRESSED);
		}
	}

	// mangled symbol for protected Terrain::getHeight()
	// protected: float __cdecl Terrain::getHeight(class Ogre::Vector3 const & __ptr64,int) __ptr64
	void* Terrain_getHeight_ptr = Escort::GetFuncAddress("Plugin_Terrain_x64.dll", "?getHeight@Terrain@@IEAAMAEBVVector3@Ogre@@H@Z");
	KenshiLib::AddHook(Terrain_getHeight_ptr, Terrain_getHeight_hook, &Terrain_getHeight_orig);

	// mangled symbol for Terrain::getRawData()
	// public: unsigned __int64 __cdecl Terrain::getRawData(int,int,int,int,char * __ptr64)const __ptr64
	void* Terrain_getRawData_ptr = Escort::GetFuncAddress("Plugin_Terrain_x64.dll", "?getRawData@Terrain@@QEBA_KHHHHPEAD@Z");
	KenshiLib::AddHook(Terrain_getRawData_ptr, Terrain_getRawData_hook, &Terrain_getRawData_orig);

	DebugLog("Heightmap hooks installed...");

	UpdateHeightmapSettings();
}

void HeightmapHook::UpdateHeightmapSettings()
{
	// update settings cache
	// cache to avoid race condtitions
	HeightmapMode newMode = Settings::GetHeightmapMode();

	// if we're now using the compressed heightmap, it isn't loaded, and the file exists, load it
	if (newMode == COMPRESSED)
	{
		if (CompressedHeightmapFileExists())
		{
			if (heightmapHandle == nullptr)
			{
				CompressToolsLib::ImageMode mode = CompressToolsLib::ImageMode::Streaming;

				if (Settings::PreloadHeightmap())
					mode = CompressToolsLib::ImageMode::Preload;

				DebugLog("Loading compressed heightmap...");
				heightmapHandle = CompressToolsLib::OpenImage(Settings::ResolvePath(HEIGHTMAP_CIF_PATH).c_str(), mode);
				if (!heightmapHandle)
				{
					ErrorLog("Could not open compressed heightmap, reverting setting to vanilla");
					Settings::SetHeightmapMode(HeightmapMode::VANILLA);
				}
				else
				{
					DebugLog("Compressed heightmap loaded!");
				}
			}
		}
		else
		{
			ErrorLog("Heightmap mode set to compressed, but the compressed heightmap doesn't exist!");
			Settings::SetHeightmapMode(VANILLA);
		}
	}
	else if (newMode == FAST_UNCOMPRESSED && mappedHeightmapPixels == nullptr)
	{
		DebugLog("Opening fast heightmap...");
		mappedHeightmapPixels = MMapTIFF(Settings::ResolvePath(HEIGHTMAP_TIF_PATH));
		if (mappedHeightmapPixels == nullptr)
		{
			ErrorLog("Error initializing fast heightmap loader");
			Settings::SetHeightmapMode(VANILLA);
		}
		else
		{
			DebugLog("Fast heightmap mapped!");
		}
	}

	// it's now safe to update these as all required files will be open
	heightmapModeCache = Settings::GetHeightmapMode();
}

enum ExistsState
{
	UNSET,
	// Windows defines TRUE and FALSE macros, so we use these instead
	EXISTS,
	DOESNT_EXIST
};

static ExistsState compressedFilemapExists = UNSET;

bool HeightmapHook::CompressedHeightmapFileExists()
{
	if (compressedFilemapExists == UNSET)
	{
		if (boost::filesystem::exists(Settings::ResolvePath(HEIGHTMAP_CIF_PATH)))
			compressedFilemapExists = EXISTS;
		else
			compressedFilemapExists = DOESNT_EXIST;
	}
	return compressedFilemapExists == EXISTS;
}

HeightmapHook::HeightmapMode HeightmapHook::GetRecommendedHeightmapMode()
{
	// if the compressed heightmap exists and the user is loading off an HDD, that's probably the best setting
	if (IO::GetDriveStorageType(Settings::ResolvePath(HEIGHTMAP_CIF_PATH)) == IO::HDD)
	{
		return HeightmapHook::COMPRESSED;
	}
	// For pretty much all other configurations, "fast uncompressed" should be fastest
	return HeightmapHook::FAST_UNCOMPRESSED;
}

/******** debugging functions ************/

bool HeightmapHook::CompressedHeightmapIsLoaded()
{
	// TODO update?
	return heightmapHandle != nullptr;
}

std::vector<uint8_t> HeightmapHook::GetBlockLODs()
{
	uint32_t width = GetBlocksWidth();
	uint32_t height = GetBlocksHeight();
	std::vector<uint8_t> blockLODs;
	blockLODs.resize(width * height);

	CompressToolsLib::GetBlockLODs(heightmapHandle, &blockLODs[0]);

	return std::move(blockLODs);
}

void HeightmapHook::WriteBlockLODsToCanvas(MyGUI::Canvas* canvas)
{
	// return if heightmap not loaded
	if (!CompressedHeightmapIsLoaded())
		return;


	// Resize if needed
	uint32_t width = GetBlocksWidth();
	uint32_t height = GetBlocksHeight();

	// MyGUI will round width + height up to power of 2
	if (!canvas->isTextureCreated() || canvas->getTextureRealWidth() < width || canvas->getTextureRealHeight() < height)
	{
		canvas->createTexture(width, height, MyGUI::Canvas::TextureResizeMode::TRM_PT_CONST_SIZE, MyGUI::TextureUsage::Default, MyGUI::PixelFormat::L8);
		if (!canvas->isTextureCreated())
			DebugLog("Error: Canvas texture not created!");
		// set to black
		uint8_t* pixels = reinterpret_cast<uint8_t*>(canvas->lock());
		int textureWidth = canvas->getTextureRealWidth();
		int textureHeight = canvas->getTextureRealHeight();
		for (int y = 0; y < textureHeight; ++y)
			for (int x = 0; x < textureWidth; ++x)
				pixels[y * textureWidth + x] = 0;
		canvas->unlock();
		canvas->updateTexture();
	}

	uint8_t* pixels = reinterpret_cast<uint8_t*>(canvas->lock());

	std::vector<uint8_t> blockLODs = GetBlockLODs();
	int textureWidth = canvas->getTextureRealWidth();

	// multiply values to useful range
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
			pixels[y * textureWidth + x] = 220 - std::max(blockLODs[y * width + x] * 40, 0);

	canvas->unlock();
	canvas->updateTexture();
}

uint32_t HeightmapHook::GetBlocksWidth()
{
	return CompressToolsLib::GetImageWidthInBlocks(heightmapHandle);
}

uint32_t HeightmapHook::GetBlocksHeight()
{
	return CompressToolsLib::GetImageHeightInBlocks(heightmapHandle);
}