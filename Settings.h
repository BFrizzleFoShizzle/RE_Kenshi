#include <vector>


namespace Settings
{
	void Init();

	bool UseHeightmapcompression();
	// TODO remove after testing
	bool PreloadHeightmap();
	const std::vector<int> GetGameSpeeds();
}