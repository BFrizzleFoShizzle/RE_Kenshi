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
//unsigned long int (*AK_SoundEngine_PostEvent_orig)(char const* __ptr64);

std::unordered_set<std::string> events;

// The game loads banks before mods are loaded - we defer these till after our mod config is run
// so we can load our banks before the games, allowing us to override sounds
std::vector<std::string> queuedBanks;
bool soundInitialized = false;
unsigned long int fakeBankID = 1;

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook(char const* __ptr64 str)
{
	std::stringstream out;
	out << "Event: " << str << " ";
	unsigned long int ret = AK_SoundEngine_GetIDFromString_orig(str);
	//out << std::hex << ret;
	if (events.find(str) == events.end())
	{
		events.emplace(str);
		DebugLog(out.str());
	}
	return ret;
}

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook2(wchar_t const* __ptr64 str)
{
	std::wstring wstr = str;
	std::string sstr = std::string(wstr.begin(), wstr.end());
	std::stringstream out;
	if (events.find(sstr) == events.end())
	{
		events.emplace(sstr);
		out << "Event (w): " << str << " ";
		DebugLog(out.str());
	}
	unsigned long int ret = AK_SoundEngine_GetIDFromString2_orig(str);
	//out << std::hex << ret;
	//DebugLog(out.str());
	return ret;
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
}