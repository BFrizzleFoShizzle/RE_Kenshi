#include "HeightmapHook.h"
#include "Escort.h"

#include <ogre/OgreVector3.h>

#include <Windows.h>
#include <string>
#include <sstream>

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
	// transform into map space
	// reverse-engineered
	float x_transformed = vec.x - thisPtr->worldToTextureOffset;
	x_transformed *= thisPtr->worldToTextureScale;

	float z_transformed = vec.z - thisPtr->worldToTextureOffset;
	z_transformed = z_transformed * thisPtr->worldToTextureScale;

	int pixelX = int(x_transformed);
	int pixelY = int(z_transformed);

	/*
	std::stringstream debugStr;
	debugStr << "Heightmap pixel: " << pixelX << ", " << pixelY << std::endl;
	debugStr << "Map max: " << thisPtr->bounds->mapMaxX << " " << thisPtr->bounds->mapMaxY << std::endl;
	debugStr << "Map min: " << thisPtr->mapMinX << " " << thisPtr->mapMinY << std::endl;
	MessageBoxA(0, debugStr.str().c_str(), "Debug", MB_OK);
	*/
	
	// in-game bounds checks
	// TODO is it L or LE
	if (pixelX <= thisPtr->bounds->mapMaxX
		&& pixelY <= thisPtr->bounds->mapMaxY
		&& pixelX > thisPtr->mapMinX
		&& pixelY > thisPtr->mapMinY)
	{
		return 1000 * thisPtr->heightScale;
	}
	else
	{
		// TODO
		return 1000 * thisPtr->heightScale;
	}
}

unsigned __int64 __cdecl Terrain_getRawData(void* thisPtr, int x, int y, int w, int h, char* __ptr64 out)
{
	uint16_t *shortPtr = reinterpret_cast<uint16_t*>(out);
	uint64_t written = 0;
	for (int i = 0; i < h; ++i)
	{
		for (int j = 0; j < w; ++j)
		{
			int x_out = x + j;
			int y_out = y + i;
			shortPtr[i * w + j] = 1000;
			++written;
		}
	}
	return written;
}

void HeightmapHook::Init()
{
	// mangled symbol for protected Terrain::getHeight()
	// protected: float __cdecl Terrain::getHeight(class Ogre::Vector3 const & __ptr64,int) __ptr64
	void* Terrain_getHeightPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getHeight@Terrain@@IEAAMAEBVVector3@Ogre@@H@Z");
	//std::string debugStr = "Address: " + std::to_string((uint64_t)Terrain_getHeightPtr);
	//MessageBoxA(0, debugStr.c_str(), "Debug", MB_OK);
	Escort::PushRetHookASM(Terrain_getHeightPtr, Terrain_getHeight, 15);
	
	
	// mangled symbol for Terrain::getRawData()
	// public: unsigned __int64 __cdecl Terrain::getRawData(int,int,int,int,char * __ptr64)const __ptr64
	void* Terrain_getRawDataPtr = GetFuncAddress("Plugin_Terrain_x64.dll", "?getRawData@Terrain@@QEBA_KHHHHPEAD@Z");
	Escort::PushRetHookASM(Terrain_getRawDataPtr, Terrain_getRawData, 15);


	MessageBoxA(0, "Installed...", "Debug", MB_OK);

}