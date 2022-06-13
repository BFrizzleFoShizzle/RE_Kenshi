#include "Sound.h"
#include "Escort.h"
#include "Kenshi/Kenshi.h"
#include "Settings.h"
#include <strstream>
#include <unordered_set>
#include <stdint.h>

enum AKRESULT
{
	UNKNOWN,
	SUCCESS = 1
};

unsigned long int (*AK_SoundEngine_GetIDFromString_orig)(char const* __ptr64);
unsigned long int (*AK_SoundEngine_GetIDFromString2_orig)(wchar_t const* __ptr64);
enum AKRESULT (*AK_SoundEngine_LoadBank_orig)(char const* __ptr64, long int, unsigned long int&);
enum AKRESULT (*AK_SoundEngine_SetState_orig)(char const* __ptr64, char const* __ptr64);
enum AKRESULT (*AK_SoundEngine_SetSwitch_orig)(char const* __ptr64, char const* __ptr64, unsigned long long(unsigned __int64));
long int (*AK_SoundEngine_PostEvent_orig)(char const* __ptr64, unsigned long long, unsigned long int, void(__cdecl*)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64, unsigned long int, struct AkExternalSourceInfo* __ptr64, unsigned long int);
enum AKRESULT(*AK_SoundEngine_RegisterGameObj_orig)(unsigned long long, char const* __ptr64, unsigned long int);

std::unordered_set<std::string> IDs;
std::unordered_set<std::string> events;
std::unordered_set<std::string> switches;
std::unordered_set<std::string> states;
std::unordered_set<std::string> gameObjNames;

// The game loads banks before mods are loaded - we defer these till after our mod config is run
// so we can load our banks before the games, allowing us to override sounds
std::vector<std::string> queuedBanks;
bool soundInitialized = false;
unsigned long int fakeBankID = 1;

bool alwaysLog = false;

void Sound::SetAlwaysLog(bool log)
{
	alwaysLog = log;
}

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook(char const* __ptr64 str)
{
	unsigned long int ret = AK_SoundEngine_GetIDFromString_orig(str);
	if (Settings::GetLogAudio() && (alwaysLog || IDs.find(str) == IDs.end()))
	{
		std::stringstream out;
		out << "ID: " << str << " ";
		IDs.emplace(str);
		DebugLog(out.str());
	}
	return ret;
}

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook2(wchar_t const* __ptr64 str)
{
	std::wstring wstr = str;
	std::string sstr = std::string(wstr.begin(), wstr.end());
	if (Settings::GetLogAudio() && (alwaysLog || IDs.find(sstr) == IDs.end()))
	{
		std::stringstream out;
		IDs.emplace(sstr);
		out << "ID (w): " << str << " ";
		DebugLog(out.str());
	}
	unsigned long int ret = AK_SoundEngine_GetIDFromString2_orig(str);
	return ret;
}


enum AKRESULT AK_SoundEngine_SetState_hook(char const* __ptr64 unk1, char const* __ptr64 unk2)
{
	std::string combinedID = unk1;
	combinedID += " " + std::string(unk2);
	AKRESULT ret = AK_SoundEngine_SetState_orig(unk1, unk2);
	if (Settings::GetLogAudio() && (alwaysLog || states.find(combinedID) == states.end()))
	{
		std::stringstream out;
		out << "State: " << unk1 << " " << unk2;
		states.emplace(combinedID);
		DebugLog(out.str());
	}
	return ret;
}

enum AKRESULT AK_SoundEngine_SetSwitch_hook(char const* __ptr64 unk1, char const* __ptr64 unk2, unsigned long long unk3(unsigned __int64))
{
	std::string combinedID = unk1;
	combinedID += " " + std::string(unk2);
	AKRESULT ret = AK_SoundEngine_SetSwitch_orig(unk1, unk2, unk3);
	if (Settings::GetLogAudio() && (alwaysLog || switches.find(combinedID) == switches.end()))
	{
		std::stringstream out;
		out << "Switch: " << unk1 << " " << unk2;
		switches.emplace(combinedID);
		DebugLog(out.str());
	}
	return ret;
}

long int __cdecl AK_SoundEngine_PostEvent_hook(char const* __ptr64 in_pszEventName, unsigned long long in_gameObjectID , unsigned long int in_uFlags, void(__cdecl* in_pfnCallback)(enum AkCallbackType, struct AkCallbackInfo* __ptr64) , void* __ptr64 in_pCookie, unsigned long int in_cExternals, struct AkExternalSourceInfo* __ptr64 in_pExternalSources, unsigned long int in_PlayingID)
{
	if (Settings::GetLogAudio() && (alwaysLog || events.find(in_pszEventName) == events.end()))
	{
		std::stringstream out;
		out << "Event: " << in_pszEventName;
		events.emplace(in_pszEventName);
		DebugLog(out.str());
	}
	return AK_SoundEngine_PostEvent_orig(in_pszEventName, in_gameObjectID, in_uFlags, in_pfnCallback, in_pCookie, in_cExternals, in_pExternalSources, in_PlayingID);
}

enum AKRESULT AK_SoundEngine_RegisterGameObj_hook(unsigned long long in_gameObjectID, char const* __ptr64 in_pszObjName, unsigned long int in_uListenerMask)
{
	if (Settings::GetLogAudio() && (alwaysLog || gameObjNames.find(in_pszObjName) == gameObjNames.end()))
	{
		std::stringstream out;
		out << "GameObj: " << in_pszObjName;
		gameObjNames.emplace(in_pszObjName);
		DebugLog(out.str());
	}
	return AK_SoundEngine_RegisterGameObj_orig(in_gameObjectID, in_pszObjName, in_uListenerMask);
}

bool loadedBank = false;

// typical args: ("bank.bnk", -1, ID (out))
AKRESULT __cdecl AK_SoundEngine_LoadBankHook(char const* __ptr64 bankName, long int unk1, unsigned long int& unk2)
{
	std::stringstream out;
	
	if (!soundInitialized)
	{
		// as far as I can tell, the game doesn't actually use these IDs, so hopefully this is safe
		unk2 = fakeBankID;
		++fakeBankID;
		out << "Deferred load: " << bankName << " " << unk1 << " " << unk2;
		DebugLog(out.str());
		queuedBanks.push_back(bankName);

		return AKRESULT::SUCCESS;
	}
	
	AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankName, unk1, unk2);
	out << "Bank load: " << bankName << " " << unk1 << " " << unk2 << " ";
	out << std::hex << ret;
	DebugLog(out.str());
	return ret;
}


void Sound::LoadQueuedBanks()
{
	// load init bank
	std::stringstream out;
	unsigned long ID;
	out << "Loading init bank " << queuedBanks[0].c_str() << " " << std::dec << AK_SoundEngine_LoadBank_orig(queuedBanks[0].c_str(), -1, ID);
	DebugLog(out.str());

	std::vector<std::string>* modSoundBanks = Settings::GetModSoundBanks();
	for (int i = 0; i < modSoundBanks->size(); ++i)
	{
		unsigned long ID;
		std::stringstream out;
		const char* bankName = modSoundBanks->at(i).c_str();
		AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankName, -1, ID);
		out << "Mod bank load: " << bankName << " " << -1 << " " << ID << " ";
		out << std::hex << ret;
		DebugLog(out.str());
	}

	for (int i = 1; i < queuedBanks.size(); ++i)
	{
		unsigned long ID;
		std::stringstream out;
		const char* bankName = queuedBanks[i].c_str();
		AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankName, -1, ID);
		out << "Vanilla bank load: " << bankName << " " << -1 << " " << ID << " ";
		out << std::hex << ret;
		DebugLog(out.str());
	}
	queuedBanks.clear();
	loadedBank = true;
}

void Sound::Init()
{
	AK_SoundEngine_GetIDFromString_orig = Escort::JmpReplaceHook<unsigned long int(char const* __ptr64)>(Kenshi::GetSoundEngineGetIDFromString(), AK_SoundEngine_GetIDFromStringHook);
	AK_SoundEngine_GetIDFromString2_orig = Escort::JmpReplaceHook<unsigned long int(wchar_t const* __ptr64)>(Kenshi::GetSoundEngineGetIDFromString2(), AK_SoundEngine_GetIDFromStringHook2);
	AK_SoundEngine_LoadBank_orig = Escort::JmpReplaceHook<AKRESULT(char const* __ptr64, long int, unsigned long int&)>(Kenshi::GetSoundEngineLoadBank(), AK_SoundEngine_LoadBankHook);
	AK_SoundEngine_SetState_orig = Escort::JmpReplaceHook<enum AKRESULT(char const* __ptr64, char const* __ptr64)>(Kenshi::GetSoundEngineSetState(), AK_SoundEngine_SetState_hook);
	AK_SoundEngine_SetSwitch_orig = Escort::JmpReplaceHook<enum AKRESULT(char const* __ptr64, char const* __ptr64, unsigned long long (unsigned __int64))>(Kenshi::GetSoundEngineSetSwitch(), AK_SoundEngine_SetSwitch_hook);
	AK_SoundEngine_PostEvent_orig = Escort::JmpReplaceHook<long int (char const* __ptr64, unsigned long long, unsigned long int, void(__cdecl*)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64, unsigned long int, struct AkExternalSourceInfo* __ptr64, unsigned long int)>(Kenshi::GetSoundEnginePostEvent(), AK_SoundEngine_PostEvent_hook);
	//AK_SoundEngine_RegisterGameObj_orig = Escort::JmpReplaceHook< enum AKRESULT(unsigned long long, char const* __ptr64, unsigned long int)>(Kenshi::GetSoundEngineRegisterGameObj(), AK_SoundEngine_RegisterGameObj_hook);
}