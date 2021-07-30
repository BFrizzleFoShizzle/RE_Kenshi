#include <vector>


namespace Settings
{
	void Init();

	void SetUseHeightmapCompression(bool value);
	bool UseHeightmapCompression();
	// TODO remove after testing
	void SetPreloadHeightmap(bool value);
	bool PreloadHeightmap();
	int GetAttackSlots();
	void SetAttackSlots(int num);

	const std::vector<float> GetGameSpeeds();
	void SetGameSpeeds(std::vector<float> gameSpeeds);
}