#pragma once

#include <vector>
#include <stdint.h>
#include "mygui/MyGUI_Canvas.h"

namespace HeightmapHook
{
	void Preload();
	void Init();
	std::vector<uint8_t> GetBlockLODs();
	void WriteBlockLODsToCanvas(MyGUI::Canvas* canvas);
	uint32_t GetBlocksWidth();
	uint32_t GetBlocksHeight();

	void EnableCompressedHeightmap();
	void DisableCompressedHeightmap();

	bool HeightmapIsLoaded();
}