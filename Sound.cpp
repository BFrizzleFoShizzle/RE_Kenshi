#include "Sound.h"
#include "Escort.h"
#include "Kenshi/Kenshi.h"
#include "Settings.h"
#include <strstream>
#include <unordered_set>
#include <stdint.h>
#include <boost/signals2/mutex.hpp>
#include <boost/filesystem.hpp>

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
long int (*AK_SoundEngine_PostEvent_orig2)(wchar_t const* __ptr64, unsigned long long, unsigned long int, void(__cdecl*)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64, unsigned long int, struct AkExternalSourceInfo* __ptr64, unsigned long int);
enum AKRESULT(*AK_SoundEngine_RegisterGameObj_orig)(unsigned long long, char const* __ptr64, unsigned long int);

std::unordered_set<std::string> IDs;
std::unordered_set<std::string> events;
std::unordered_set<std::string> switches;
std::unordered_set<std::string> states;
std::unordered_set<std::string> gameObjNames;

boost::signals2::mutex soundLock;

// The game loads banks before mods are loaded - we defer these till after our mod config is run
// so we can load our banks before the games, allowing us to override sounds
std::vector<std::string> queuedBanks;
bool soundInitialized = false;
unsigned long int fakeBankID = 1;

bool alwaysLog = false;
bool queuedBootUpGame = false;

void Sound::SetAlwaysLog(bool log)
{
	alwaysLog = log;
}

const char* NULLPTR_ID = "[NULLPTR/EMPTY]";
const wchar_t* NULLPTR_ID_W = L"[NULLPTR/EMPTY]";

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook(char const* __ptr64 str)
{
	unsigned long int ret = AK_SoundEngine_GetIDFromString_orig(str);
	if (str == nullptr)
		str = NULLPTR_ID;
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
	unsigned long int ret = AK_SoundEngine_GetIDFromString2_orig(str);

	if (str == nullptr)
		str = NULLPTR_ID_W;

	std::wstring wstr = str;
	std::string sstr = std::string(wstr.begin(), wstr.end());

	if (Settings::GetLogAudio() && (alwaysLog || IDs.find(sstr) == IDs.end()))
	{
		std::stringstream out;
		IDs.emplace(sstr);
		out << "ID (w): " << sstr << " ";
		DebugLog(out.str());
	}
	return ret;
}


enum AKRESULT AK_SoundEngine_SetState_hook(char const* __ptr64 unk1, char const* __ptr64 unk2)
{
	AKRESULT ret = AK_SoundEngine_SetState_orig(unk1, unk2);

	if (unk1 == nullptr)
		unk1 = NULLPTR_ID;
	if (unk2 == nullptr)
		unk2 = NULLPTR_ID;

	std::string combinedID = unk1;
	combinedID += " " + std::string(unk2);
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
	AKRESULT ret = AK_SoundEngine_SetSwitch_orig(unk1, unk2, unk3);

	if (unk1 == nullptr)
		unk1 = NULLPTR_ID;
	if (unk2 == nullptr)
		unk2 = NULLPTR_ID;

	std::string combinedID = "";
	combinedID += " " + std::string(unk2);
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

long int __cdecl AK_SoundEngine_PostEvent_hook2(wchar_t const* __ptr64 in_pszEventName, unsigned long long in_gameObjectID, unsigned long int in_uFlags, void(__cdecl* in_pfnCallback)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64 in_pCookie, unsigned long int in_cExternals, struct AkExternalSourceInfo* __ptr64 in_pExternalSources, unsigned long int in_PlayingID)
{
	if (in_pszEventName == nullptr)
		in_pszEventName = NULLPTR_ID_W;

	std::wstring wstr = in_pszEventName;
	std::string sstr = std::string(wstr.begin(), wstr.end());

	if ((!soundInitialized) && wstr == L"Boot_Up_Game")
	{
		queuedBootUpGame = true;
		DebugLog("Queueing Boot_Up_Game event");
		if (in_gameObjectID != 100)
			ErrorLog("Error: unexpected gameObjectID for Boot_Up_Game event");
	}

	if (Settings::GetLogAudio() && (alwaysLog || events.find(sstr) == events.end()))
	{
		std::stringstream out;
		out << "Event (W): " << sstr;
		events.emplace(sstr);
		DebugLog(out.str());
	}
	return AK_SoundEngine_PostEvent_orig2(in_pszEventName, in_gameObjectID, in_uFlags, in_pfnCallback, in_pCookie, in_cExternals, in_pExternalSources, in_PlayingID);
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

class CAkFilePackageLowLevelIOBlocking
{
public:
	virtual ~CAkFilePackageLowLevelIOBlocking();
	// vtable 
	uint64_t vmt2;
	// vtable
	uint64_t vmt3;
	wchar_t basePath[260];
};

CAkFilePackageLowLevelIOBlocking* (*AK_StreamMgr_GetFileLocationResolver)();

// MAKE SURE YOU HAVE LOCKED THE SOUND LOCK BEFORE CALLING
static void LoadQueuedBanks()
{
	// if init soundbank is loaded, mod soundbanks cannot be loaded
	if (queuedBanks.size() == 0)
		return;

	// if mod overrides aren't loaded, we don't know which banks need injecting
	if (!Settings::GetModOverridesLoaded())
		return;

	// load init bank
	std::stringstream out;
	unsigned long ID;
	out << "Loading init bank " << queuedBanks[0].c_str() << " " << std::dec << AK_SoundEngine_LoadBank_orig(queuedBanks[0].c_str(), -1, ID);
	DebugLog(out.str());

	std::vector<std::string>* modSoundBanks = Settings::GetModSoundBanks();
	// clear base path so we can load from absolute directories
	std::wstring baseLocation = AK_StreamMgr_GetFileLocationResolver()->basePath;
	AK_StreamMgr_GetFileLocationResolver()->basePath[0] = L'\0';
	for (int i = 0; i < modSoundBanks->size(); ++i)
	{
		unsigned long ID;
		std::stringstream out;
		std::string bankPath = modSoundBanks->at(i);
		if (boost::filesystem::exists(std::string(baseLocation.begin(), baseLocation.end()) + bankPath))
			// use relative path if it exists
			bankPath = std::string(baseLocation.begin(), baseLocation.end()) + bankPath;
		else if (!boost::filesystem::exists(bankPath))
			// else if the absolute path doesn't exist either, error
			ErrorLog("Unable to find soundbank: " + bankPath);


		AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankPath.c_str(), -1, ID);
		out << "Mod bank load: " << bankPath << " " << -1 << " " << ID << " ";
		out << std::hex << ret;
		DebugLog(out.str());
	}
	// switch back to original base path
	AK_StreamMgr_GetFileLocationResolver()->basePath[0] = baseLocation[0];
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

	// retrigger Boot_Up_Game event after banks are loaded if needed
	if (queuedBootUpGame)
	{
		DebugLog("Force triggering Boot_Up_Game");
		AK_SoundEngine_PostEvent_hook("Boot_Up_Game", 100, 0, 0, 0, 0, 0, 0);
		queuedBootUpGame = false;
	}

	soundInitialized = true;
}

// typical args: ("bank.bnk", -1, ID (out))
// Threading notes:
// The "Init.bnk" is guaranteed to happen before mod init
// Mod init happens in parallel with other soundbank loads
// Thus, locking is needed after 1st soundbank load finishes
// as soundInitialized may be read/written by different threads
AKRESULT __cdecl AK_SoundEngine_LoadBankHook(char const* __ptr64 bankName, long int unk1, unsigned long int& unk2)
{
	std::stringstream out;

	// technically only need to do this after 1st soundbank loade but w/e
	soundLock.lock();

	if (!soundInitialized && Settings::GetModOverridesLoaded())
		LoadQueuedBanks();

	if (!soundInitialized)
	{
		// as far as I can tell, the game doesn't actually use these IDs, so hopefully this is safe
		unk2 = fakeBankID;
		++fakeBankID;
		out << "Deferred load: " << bankName << " " << unk1 << " " << unk2;
		DebugLog(out.str());
		queuedBanks.push_back(bankName);

		soundLock.unlock();
		return AKRESULT::SUCCESS;
	}
	else
	{
		soundLock.unlock();

		AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankName, unk1, unk2);
		out << "Bank load: " << bankName << " " << unk1 << " " << unk2 << " ";
		out << std::hex << ret;
		DebugLog(out.str());
		return ret;
	}
}

void Sound::TryLoadQueuedBanks()
{
	// technically only need to do this after 1st soundbank loade but w/e
	soundLock.lock();
	
	LoadQueuedBanks();

	soundLock.unlock();
}

void Sound::Init()
{
	// find function addresses
	AK_StreamMgr_GetFileLocationResolver = (CAkFilePackageLowLevelIOBlocking * (*)())Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?GetFileLocationResolver@StreamMgr@AK@@YAPEAVIAkFileLocationResolver@12@XZ");
	void* AK_SoundEngine_GetIDFromString_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?GetIDFromString@SoundEngine@AK@@YAKPEBD@Z");
	void* AK_SoundEngine_GetIDFromString2_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?GetIDFromString@SoundEngine@AK@@YAKPEB_W@Z");
	void* AK_SoundEngine_LoadBank_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?LoadBank@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDJAEAK@Z");
	void* AK_SoundEngine_SetState_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0@Z");
	void* AK_SoundEngine_SetSwitch_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0_K@Z");
	void* AK_SoundEngine_PostEvent_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?PostEvent@SoundEngine@AK@@YAKPEBD_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z");
	void* AK_SoundEngine_PostEvent2_ptr = Escort::GetFuncAddress(Kenshi::GetKenshiVersion().GetBinaryName(), "?PostEvent@SoundEngine@AK@@YAKPEB_W_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z");
	
	// add hooks
	AK_SoundEngine_GetIDFromString_orig = Escort::JmpReplaceHook<unsigned long int(char const* __ptr64)>(AK_SoundEngine_GetIDFromString_ptr, AK_SoundEngine_GetIDFromStringHook);
	AK_SoundEngine_GetIDFromString2_orig = Escort::JmpReplaceHook<unsigned long int(wchar_t const* __ptr64)>(AK_SoundEngine_GetIDFromString2_ptr, AK_SoundEngine_GetIDFromStringHook2);
	AK_SoundEngine_LoadBank_orig = Escort::JmpReplaceHook<AKRESULT(char const* __ptr64, long int, unsigned long int&)>(AK_SoundEngine_LoadBank_ptr, AK_SoundEngine_LoadBankHook);
	AK_SoundEngine_SetState_orig = Escort::JmpReplaceHook<enum AKRESULT(char const* __ptr64, char const* __ptr64)>(AK_SoundEngine_SetState_ptr, AK_SoundEngine_SetState_hook);
	AK_SoundEngine_SetSwitch_orig = Escort::JmpReplaceHook<enum AKRESULT(char const* __ptr64, char const* __ptr64, unsigned long long (unsigned __int64))>(AK_SoundEngine_SetSwitch_ptr, AK_SoundEngine_SetSwitch_hook);
	AK_SoundEngine_PostEvent_orig = Escort::JmpReplaceHook<long int (char const* __ptr64, unsigned long long, unsigned long int, void(__cdecl*)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64, unsigned long int, struct AkExternalSourceInfo* __ptr64, unsigned long int)>(AK_SoundEngine_PostEvent_ptr, AK_SoundEngine_PostEvent_hook);
	AK_SoundEngine_PostEvent_orig2 = Escort::JmpReplaceHook<long int(wchar_t const* __ptr64, unsigned long long, unsigned long int, void(__cdecl*)(enum AkCallbackType, struct AkCallbackInfo* __ptr64), void* __ptr64, unsigned long int, struct AkExternalSourceInfo* __ptr64, unsigned long int)>(AK_SoundEngine_PostEvent2_ptr, AK_SoundEngine_PostEvent_hook2);
	//AK_SoundEngine_RegisterGameObj_orig = Escort::JmpReplaceHook< enum AKRESULT(unsigned long long, char const* __ptr64, unsigned long int)>(Kenshi::GetSoundEngineRegisterGameObj(), AK_SoundEngine_RegisterGameObj_hook);
}