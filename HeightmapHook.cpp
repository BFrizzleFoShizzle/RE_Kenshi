#include "HeightmapHook.h"
#include "Escort.h"

#include <ogre/OgreVector3.h>

#include <Windows.h>
#include <string>
#include <sstream>

#include "CompressTools/CompressToolsLib.h"
#include "Debug.h"
#include "Settings.h"

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
	int mapMinX;
	int mapMinY;

};

void* GetFuncAddress(std::string moduleName, std::string functionName)
{
	HMODULE hMod = GetModuleHandleA(moduleName.c_str());
	std::string debugStr = "DLL handle: " + std::to_string((uint64_t)hMod);
	//MessageBoxA(0, debugStr.c_str(), "Debug", MB_OK);
	return GetProcAddress(hMod, functionName.c_str());
}

float __cdecl Terrain_getHeight(Terrain* thisPtr, class Ogre::Vector3 const& vec, int unk)
{
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
		&& pixelX >= thisPtr->mapMinX
		&& pixelY >= thisPtr->mapMinY)
	{
		uint16_t height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
		return height * thisPtr->heightScale;
	}
	else
	{
		// TODO
		return 0 * thisPtr->heightScale;
	}
}

unsigned __int64 __cdecl Terrain_getRawData(Terrain* thisPtr, int x, int y, int w, int h, char* __ptr64 out)
{
	uint16_t *shortPtr = reinterpret_cast<uint16_t*>(out);
	uint64_t written = 0;

	for (int i = 0; i < h; ++i)
	{
		for (int j = 0; j < w; ++j)
		{
			int pixelX = x + j;
			int pixelY = y + i;

			uint16_t height = CompressToolsLib::ReadHeightValue(heightmapHandle, pixelX, pixelY);
			shortPtr[i * w + j] = height;
			++written;
		}
	}
	return written;
}

void HeightmapHook::Preload()
{
	// early return if heightmap compression is disabled
	if (!Settings::UseHeightmapCompression())
		return;

	CompressToolsLib::ImageMode mode = CompressToolsLib::ImageMode::Streaming;

	if (Settings::PreloadHeightmap())
		mode = CompressToolsLib::ImageMode::Preload;

	DebugLog("Loading heightmap...");
	heightmapHandle = CompressToolsLib::OpenImage("data/newland/land/fullmap.cif", mode);
	DebugLog("Heightmap loaded!");
}

void HeightmapHook::Init()
{
	// early return if heightmap compression is disabled
	if (!Settings::UseHeightmapCompression())
		return;

	EnableCompressedHeightmap();
}

// debugging functions
std::vector<uint8_t> HeightmapHook::GetBlockLODs()
{
	uint32_t width = GetBlocksWidth();
	uint32_t height = GetBlocksHeight();
	std::vector<uint8_t> blockLODs;
	blockLODs.resize(width * height);

	CompressToolsLib::GetBlockLODs(heightmapHandle, &blockLODs[0]);

	return std::move(blockLODs);
}

bool test = false;

void HeightmapHook::WriteBlockLODsToCanvas(MyGUI::Canvas* canvas)
{
	// return if heightmap not loaded
	if (!HeightmapIsLoaded())
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
			pixels[y * textureWidth + x] = 220 - max(blockLODs[y * width + x] * 40, 0);

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

bool HeightmapHook::HeightmapIsLoaded()
{
	return heightmapHandle != nullptr;
}

// bytes before modification
uint8_t Terrain_getHeightOld[15];
uint8_t Terrain_getRawDataOld[15];

void HeightmapHook::EnableCompressedHeightmap()
{
	// mangled symbol for protected Terrain::getHeight()
	// protected: float __cdecl Terrain::getHeight(class Ogre::Vector3 const & __ptr64,int) __ptr64
	void* Terrain_getHeightPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getHeight@Terrain@@IEAAMAEBVVector3@Ogre@@H@Z");
	// backup bytes
	memcpy(Terrain_getHeightOld, Terrain_getHeightPtr, 15);
	Escort::PushRetHookASM(Terrain_getHeightPtr, Terrain_getHeight, 15);


	// mangled symbol for Terrain::getRawData()
	// public: unsigned __int64 __cdecl Terrain::getRawData(int,int,int,int,char * __ptr64)const __ptr64
	void* Terrain_getRawDataPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getRawData@Terrain@@QEBA_KHHHHPEAD@Z");
	// backup bytes
	memcpy(Terrain_getRawDataOld, Terrain_getRawDataPtr, 15);
	Escort::PushRetHookASM(Terrain_getRawDataPtr, Terrain_getRawData, 15);

	DebugLog("Heightmap hooks installed...");
}

void HeightmapHook::DisableCompressedHeightmap()
{
	// mangled symbol for protected Terrain::getHeight()
	// protected: float __cdecl Terrain::getHeight(class Ogre::Vector3 const & __ptr64,int) __ptr64
	void* Terrain_getHeightPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getHeight@Terrain@@IEAAMAEBVVector3@Ogre@@H@Z");
	// Restore backup
	Escort::WriteProtected(Terrain_getHeightPtr, Terrain_getHeightOld, 15);


	// mangled symbol for Terrain::getRawData()
	// public: unsigned __int64 __cdecl Terrain::getRawData(int,int,int,int,char * __ptr64)const __ptr64
	void* Terrain_getRawDataPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getRawData@Terrain@@QEBA_KHHHHPEAD@Z");
	// Restore backup
	Escort::WriteProtected(Terrain_getRawDataPtr, Terrain_getRawDataOld, 15);

	DebugLog("Heightmap hooks uninstalled...");
}