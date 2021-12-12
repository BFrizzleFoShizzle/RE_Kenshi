#include <vector>
#include <unordered_map>

namespace Settings
{
	void Init();
	void LoadModOverrides();

	void SetUseHeightmapCompression(bool value);
	bool UseHeightmapCompression();
	// TODO remove after testing
	void SetPreloadHeightmap(bool value);
	bool PreloadHeightmap();
	int GetAttackSlots();
	void SetAttackSlots(int num);
	void SetLogFileIO(bool value);
	bool GetLogFileIO();
	bool GetCheckUpdates();
	void SetCheckUpdates(bool value);

	const std::unordered_map<std::string, std::string> *GetFileOverrides();
	std::string ResolvePath(std::string path);

	const std::vector<float> GetGameSpeeds();
	void SetGameSpeeds(std::vector<float> gameSpeeds);
}