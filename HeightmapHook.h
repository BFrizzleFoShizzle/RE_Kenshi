#pragma once

#include <vector>
#include <stdint.h>
#include "mygui/MyGUI_Canvas.h"

namespace HeightmapHook
{
	enum HeightmapMode
	{
		// Auto = update setting with the recommended setting at runtime
		AUTO = -1,
		VANILLA = 0,
		COMPRESSED = 1,
		FAST_UNCOMPRESSED = 2
	};

	void Preload();
	void Init();

	void UpdateHeightmapSettings();
	bool CompressedHeightmapFileExists();
	HeightmapMode GetRecommendedHeightmapMode();

	//debug functions
	bool CompressedHeightmapIsLoaded();
	std::vector<uint8_t> GetBlockLODs();
	void WriteBlockLODsToCanvas(MyGUI::Canvas* canvas);
	uint32_t GetBlocksWidth();
	uint32_t GetBlocksHeight();
}