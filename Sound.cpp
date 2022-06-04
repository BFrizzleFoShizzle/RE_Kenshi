#include "Sound.h"
#include "Escort.h"
#include "Kenshi/Kenshi.h"
#include <strstream>
#include <unordered_set>
#include <stdint.h>

enum AKRESULT
{
	UNKNOWN
};

unsigned long int (*AK_SoundEngine_GetIDFromString_orig)(char const* __ptr64);
unsigned long int (*AK_SoundEngine_GetIDFromString2_orig)(wchar_t const* __ptr64);
enum AKRESULT (*AK_SoundEngine_LoadBank_orig)(char const* __ptr64, long int, unsigned long int&);
//unsigned long int (*AK_SoundEngine_PostEvent_orig)(char const* __ptr64);

std::unordered_set<std::string> events;

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook(char const* __ptr64 str)
{
	std::stringstream out;
	out << "Event: " << str << " ";
	unsigned long int ret = AK_SoundEngine_GetIDFromString_orig(str);
	out << std::hex << ret;
	if (events.find(str) == events.end())
	{
		events.emplace(str);
		DebugLog(out.str());
	}
	return ret;
}

unsigned long int __cdecl AK_SoundEngine_GetIDFromStringHook2(wchar_t const* __ptr64 str)
{
	std::stringstream out;
	out << "Event (w): " << str << " ";
	unsigned long int ret = AK_SoundEngine_GetIDFromString2_orig(str);
	out << std::hex << ret;
	//DebugLog(out.str());
	return ret;
}

bool loadedBank = false;

AKRESULT __cdecl AK_SoundEngine_LoadBankHook(char const* __ptr64 bankName, long int unk1, unsigned long int& unk2)
{
	std::stringstream out;
	out << "Bank load: " << bankName << " " << unk1 << " ";
	AKRESULT ret = AK_SoundEngine_LoadBank_orig(bankName, unk1, unk2);
	out << std::hex << ret;

	// Load after the "Init" soundbank
	if (!loadedBank)
	{
		out << "\nLoading hack bank " << std::dec << AK_SoundEngine_LoadBank_orig("test.bnk", unk1, unk2);
		loadedBank = true;
	}
	DebugLog(out.str());
	return ret;
}

void Sound::Init()
{
	AK_SoundEngine_GetIDFromString_orig = Escort::JmpReplaceHook<unsigned long int(char const* __ptr64)>(Kenshi::GetSoundEngineGetIDFromString(), AK_SoundEngine_GetIDFromStringHook);
	AK_SoundEngine_GetIDFromString2_orig = Escort::JmpReplaceHook<unsigned long int(wchar_t const* __ptr64)>(Kenshi::GetSoundEngineGetIDFromString2(), AK_SoundEngine_GetIDFromStringHook2);
	AK_SoundEngine_LoadBank_orig = Escort::JmpReplaceHook<AKRESULT(char const* __ptr64, long int, unsigned long int&)>(Kenshi::GetSoundEngineLoadBank(), AK_SoundEngine_LoadBankHook);
}