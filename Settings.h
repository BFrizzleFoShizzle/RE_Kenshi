#pragma once

#include <vector>
#include <unordered_map>

#include "HeightmapHook.h"

namespace Settings
{
	void Init();
	void LoadModOverrides();
	// whether mod overrides have been loaded
	bool GetModOverridesLoaded();

	void SetHeightmapMode(HeightmapHook::HeightmapMode value);
	HeightmapHook::HeightmapMode GetHeightmapMode();
	// TODO remove after testing
	void SetPreloadHeightmap(bool value);
	bool PreloadHeightmap();
	void SetProfileLoads(bool value);
	bool GetProfileLoads();
	int GetAttackSlots();
	void SetAttackSlots(int num);
	bool GetOverrideAttackSlots();
	void SetOverrideAttackSlots(bool value);
	int GetMaxFactionSize();
	void SetMaxFactionSize(int num);
	bool GetOverrideMaxFactionSize();
	void SetOverrideMaxFactionSize(bool value);
	int GetMaxSquadSize();
	void SetMaxSquadSize(int num);
	bool GetOverrideMaxSquadSize();
	void SetOverrideMaxSquadSize(bool value);
	int GetMaxSquads();
	void SetMaxSquads(int num);
	bool GetOverrideMaxSquads();
	void SetOverrideMaxSquads(bool value);
	bool GetFixRNG();
	void SetFixRNG(bool value);
	// TODO remove after dropping support for old versions
	bool GetFixFontSize();
	void SetFixFontSize(bool value);
	void SetLogFileIO(bool value);
	bool GetLogFileIO();
	bool GetCheckUpdates();
	void SetCheckUpdates(bool value);
	std::string GetSkippedVersion();
	void SetSkippedVersion(std::string version);
	bool GetOpenSettingsOnStart();
	void SetOpenSettingsOnStart(bool value);
	bool GetIncreaseMaxCameraDistance();
	void SetIncreaseMaxCameraDistance(bool value);
	bool GetCacheShaders();
	void SetCacheShaders(bool value);
	bool GetLogAudio();
	void SetLogAudio(bool value);
	bool GetEnableEmergencySaves();
	void SetEnableEmergencySaves(bool value);

	const std::unordered_map<std::string, std::string> *GetFileOverrides();
	std::vector<std::string> *GetModSoundBanks();
	std::string ResolvePath(std::string path);

	bool GetUseCustomGameSpeeds();
	void SetUseCustomGameSpeeds(bool value);
	const std::vector<float> GetGameSpeeds();
	void SetGameSpeeds(std::vector<float> gameSpeeds);
}