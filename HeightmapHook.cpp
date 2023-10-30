#include "HeightmapHook.h"
#include "Escort.h"

#include <ogre/OgreVector3.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <sstream>
#include <boost/filesystem.hpp>

#include "CompressTools/CompressToolsLib.h"
#include "Debug.h"
#include "Settings.h"
#include "io.h"

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
	// TODO is it L or LE
	if (pixelX <= thisPtr->bounds->mapMaxX
		&& pixelY <= thisPtr->bounds->mapMaxY
		&& pixelX >= 0
		&& pixelY >= 0)
	{
		uint16_t height = 0;
		if (heightmapMode == HeightmapHook::COMPRESSED)
			height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
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
		// TODO
		return 0 * thisPtr->heightScale;
	}
}

unsigned __int64 (*Terrain_getRawData_orig)(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out);
unsigned __int64 Terrain_getRawData_hook(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out)
{
	// this MUST be cached here for safety reasons, explained in the below note
	HeightmapHook::HeightmapMode heightmapMode = heightmapModeCache;

	if (heightmapMode == HeightmapHook::VANILLA)
		return Terrain_getRawData_orig(thisPtr, x, y, w, h, out);

	uint16_t *shortPtr = reinterpret_cast<uint16_t*>(out);
	uint64_t written = 0;
	for (int i = 0; i < h; ++i)
	{
		for (int j = 0; j < w; ++j)
		{
			int pixelX = x + j;
			int pixelY = y + i;
			uint16_t height = 0;
			// Note: using useCompressedHeightmapCache directly here would be unsafe if the setting is 
			if (heightmapMode == HeightmapHook::COMPRESSED)
				height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
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
	/*
	if (!Settings::UseHeightmapCompression() || !CompressedHeightmapFileExists())
		// early return if heightmap compression is disabled
		return;

	CompressToolsLib::ImageMode mode = CompressToolsLib::ImageMode::Streaming;

	if (Settings::PreloadHeightmap())
		mode = CompressToolsLib::ImageMode::Preload;

	DebugLog("Loading heightmap...");
	heightmapHandle = CompressToolsLib::OpenImage(Settings::ResolvePath("data/newland/land/fullmap.cif").c_str(), mode);
	DebugLog("Heightmap loaded!");
	*/
}

// bytes before modification
//uint8_t Terrain_getHeightOld[15];
//uint8_t Terrain_getRawDataOld[15];

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
	}

	// mangled symbol for protected Terrain::getHeight()
	// protected: float __cdecl Terrain::getHeight(class Ogre::Vector3 const & __ptr64,int) __ptr64
	void* Terrain_getHeight_ptr = Escort::GetFuncAddress("Plugin_Terrain_x64.dll", "?getHeight@Terrain@@IEAAMAEBVVector3@Ogre@@H@Z");
	Terrain_getHeight_orig = Escort::JmpReplaceHook<float(Terrain* thisPtr, class Ogre::Vector3 const& vec, int unk)>(Terrain_getHeight_ptr, Terrain_getHeight_hook);
	/*
	// backup bytes
	memcpy(Terrain_getHeightOld, Terrain_getHeightPtr, 15);
	// TODO update
	Escort::PushRetHookASM(Terrain_getHeightPtr, Terrain_getHeight, 15);
	*/
	// mangled symbol for Terrain::getRawData()
	// public: unsigned __int64 __cdecl Terrain::getRawData(int,int,int,int,char * __ptr64)const __ptr64
	// backup bytes
	//memcpy(Terrain_getRawDataOld, Terrain_getRawDataPtr, 15);
	//Escort::PushRetHookASM(Terrain_getRawDataPtr, Terrain_getRawData, 15);
	void* Terrain_getRawData_ptr = Escort::GetFuncAddress("Plugin_Terrain_x64.dll", "?getRawData@Terrain@@QEBA_KHHHHPEAD@Z");
	Terrain_getRawData_orig = Escort::JmpReplaceHook<uint64_t(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out)>(Terrain_getRawData_ptr, Terrain_getRawData_hook);

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
				heightmapHandle = CompressToolsLib::OpenImage(Settings::ResolvePath("data/newland/land/fullmap.cif").c_str(), mode);
				DebugLog("Compressed heightmap loaded!");
			}
		}
		else
		{
			ErrorLog("Heightmap mode set to compressed, but the compressed heightmap doesn't exist!");
			newMode = VANILLA;
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
		if (boost::filesystem::exists(Settings::ResolvePath("data/newland/land/fullmap.cif")))
			compressedFilemapExists = EXISTS;
		else
			compressedFilemapExists = DOESNT_EXIST;
	}
	return compressedFilemapExists == EXISTS;
}

HeightmapHook::HeightmapMode HeightmapHook::GetRecommendedHeightmapMode()
{
	// if the compressed heightmap exists and the user is loading off an HDD, that's probably the best setting
	if (IO::GetDriveStorageType(Settings::ResolvePath("data/newland/land/fullmap.cif")) == IO::HDD)
	{
		return HeightmapHook::COMPRESSED;
	}
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