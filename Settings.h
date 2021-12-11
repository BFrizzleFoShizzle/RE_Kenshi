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

	// May block for a while
	bool IsCurrentVersion();
	// "x.x.x"
	std::string GetCurrentVersion();
	// version text to display in main menu
	std::string GetDisplayVersion();

	const std::unordered_map<std::string, std::string> *GetFileOverrides();
	std::string ResolvePath(std::string path);

	const std::vector<float> GetGameSpeeds();
	void SetGameSpeeds(std::vector<float> gameSpeeds);
}