
#include <kenshi/GameLauncher.h>
#include "MiscHooks.h"

#include <kenshi/Kenshi.h>
#include <core/Functions.h>

#include "Escort.h"
#include "Settings.h"
#include "TLS.h"

#include <stdlib.h>
#include <ctime>
#include <crtdefs.h>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/random_device.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#include <kenshi/util/UtilityT.h>
#include <kenshi/Building.h>

// bootleg hack so we can switch between seeded rand() and true random
// we use thread-local storage so we can switch rand() behaviour based on what function is executing
// without causing race conditions with other threads
typedef TLS::TLSSlot<TLS::GCTLSObj> TLSRandFlag;
DWORD TLSRandFlag::TLSSlotIndex = TLS_OUT_OF_INDEXES;

boost::random_device rd;
boost::mt19937 trueRandom(rd);
boost::mutex randMutex;

// launcher mod tab scroll bugfix
void (*TabMods_updateModsList_orig)(GameLauncher::TabMods* thisptr, bool validate);
void TabMods_updateModsList_hook(GameLauncher::TabMods* thisptr, bool validate)
{
	if (Settings::GetFixModListScroll())
	{
		// stop Kenshi from scrolling to the top of the mod list
		int topIndex = thisptr->clListMods.GetTopIndex();
		int caretIndex = thisptr->clListMods.GetCaretIndex();
		TabMods_updateModsList_orig(thisptr, validate);
		thisptr->clListMods.SetCaretIndex(caretIndex);
		thisptr->clListMods.SetTopIndex(topIndex);
	}
	else
	{
		TabMods_updateModsList_orig(thisptr, validate);
	}
}

// taken from https://stackoverflow.com/questions/6793065/understanding-the-algorithm-of-visual-cs-rand-function
// seems to be correct based on disassembly
unsigned long srand_state = 0;

// We recreate srand()/rand() because the hooks break the function
// it's easier to just replace the whole thing rather than fix the hooks
void srand_hook(int val)
{
	srand_state = (unsigned long)val;
}

int rand_hook()
{
	if (TLSRandFlag::GetPtr() == 0)
	{
		// true random
		boost::lock_guard<boost::mutex> lock(randMutex);
		return trueRandom() % RAND_MAX;
	}
	else
		// return seeded random (this should be identical to rand())
		return (((srand_state = srand_state * 214013L + 2531011L) >> 16) & 0x7fff);
}

bool rngHooksInstalled = false;
int (*randomInt_orig)(int lo, int hi);
int randomInt_hook(int lo, int hi)
{
	if (Settings::GetFixRNG())
	{
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)1);
		int val = randomInt_orig(lo, hi);
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)0);
		return val;
	}
	else
	{
		return randomInt_orig(lo, hi);
	}
}

float (*random_orig)(float lo, float hi);
float random_hook(float lo, float hi)
{
	if (Settings::GetFixRNG())
	{
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)1);
		int val = random_orig(lo, hi);
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)0);
		return val;
	}
	else
	{
		return random_orig(lo, hi);
	}
}

namespace FoliageSystem
{
	class EntData;
}
// TODO move to header
void getFoliageRotation(FoliageSystem::EntData* data, float x, float z, Ogre::Quaternion& rotation);// RVA = 0x6CB8A0
void (*getFoliageRotation_orig)(FoliageSystem::EntData* data, float x, float z, Ogre::Quaternion& rotation) = nullptr;
void getFoliageRotation_hook(FoliageSystem::EntData* data, float x, float z, Ogre::Quaternion& rotation)
{
	if (Settings::GetFixRNG())
	{
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)1);
		getFoliageRotation_orig(data, x, z, rotation);
		TLSRandFlag::SetPtr((TLS::GCTLSObj*)0);
	}
	else
	{
		getFoliageRotation_orig(data, x, z, rotation);
	}
}

// TODO move to Escort?
void CallOverwrite(void* writeAddress, void* target)
{
	// call (32) -> jmp (64) in near page -> function
	std::vector<uint8_t> bytes;
	Escort::JmpAbsPtr(bytes, 6);
	uint8_t* rwx = (uint8_t*)Escort::AllocateRWXNear(writeAddress, bytes.size() + 8);
	// write absolute jmp
	memcpy(rwx, &bytes[0], bytes.size());
	uint64_t offset = ((uint64_t)target) - ((uint64_t)(rwx + bytes.size()));
	memcpy(rwx + bytes.size(), &target, 8);
	// hook up call
	bytes.clear();
	offset = ((uint64_t)rwx) - ((uint64_t)writeAddress);
	Escort::CallRel(bytes, offset);
	Escort::WriteProtected(writeAddress, &bytes[0], bytes.size());
}

void EnableFixRNG()
{
	// default to trueRandom()
	TLSRandFlag::SetPtr((TLS::GCTLSObj*)0);
	// Note: rand() can't be  wrapper hooked because it has problematic instructions
	// we bork the function and just reimplement it entirely
	void* randPtr = Escort::GetFuncAddress("MSVCR100.dll", "rand");
	Escort::JmpReplaceHook<int()>(randPtr, rand_hook, 9);
	if (getFoliageRotation_orig == nullptr)
		getFoliageRotation_orig = Escort::JmpReplaceHook<void(FoliageSystem::EntData*, float, float, Ogre::Quaternion&)>((void*)GetRealAddress(&getFoliageRotation) , getFoliageRotation_hook, 9);
	randomInt_orig = (int(*)(int, int))GetRealAddress(&UtilityT::randomInt);// Kenshi::GetUtilityTRandomIntFunction();
	random_orig = (float(*)(float, float))GetRealAddress((float(*)(float, float)) &UtilityT::random);//GetUtilityTRandomFunction();
	uint8_t* buildingSelectPartsPtr = (uint8_t*)GetRealAddress(&Building::selectParts);//Kenshi::GetBuildingSelectPartsFunction();
	// call to UtilityT::randomInt
	CallOverwrite(buildingSelectPartsPtr + 0x113, randomInt_hook);
	CallOverwrite(buildingSelectPartsPtr + 0x26D, random_hook);
	// call to UtilityT::random
	void* srandPtr = Escort::GetFuncAddress("MSVCR100.dll", "srand");
	Escort::PushRetHookASM(srandPtr, srand_hook, 15);
	rngHooksInstalled = true;
}

void DisableFixRNG()
{
	void* srandPtr = Escort::GetFuncAddress("MSVCR100.dll", "srand");
	rngHooksInstalled = false;
	// default to rand()
	TLSRandFlag::SetPtr((TLS::GCTLSObj*)1);
}

// bugfix for Kenshi's RNG initialization
void MiscHooks::SetFixRNG(bool value)
{
	if (value == rngHooksInstalled)
		return;

	if (value != Settings::GetFixRNG())
		Settings::SetFixRNG(value);

	if (value)
	{
		EnableFixRNG();
	}
	else
	{
		DisableFixRNG();
	}
}

void MiscHooks::SetMaxCameraDistance(float value)
{
	Escort::WriteProtected(&KenshiLib::GetMaxCameraDistance(), &value, sizeof(float));
}

// Kenshi calls ShowCursor a million times (probably every frame) - this causes problems 
// because every time you call the function it INCREMENTS or DECREMENTS an internal counter.
// If the game runs for a million frames, the "show cursor" counter will  be at one million.
// We need to show the cursor when the crash report window opens, so users can interact with
// the UI. So, we patch this function to work as if it set a boolean flag, so we don't have
// to call it A MILLION FUCKING TIMES when the bug reporter opens just to SHOW THE DAMN CURSOR.
// I don't know WHY Microsoft thought implementing it this way was a good idea.
// I also don't know why they don't have a method for SETTING THE FUCKING COUNTER.
// The ONLY "official" way of forcing the cursor to show after it has been hidden is literally 
// calling ShowCursor(true) in a loop until the counter is >= 0. WHAT THE FUCK.
int (*ShowCursor_orig)(bool value);
static bool cursorShowing = true;
int ShowCursor_hook(bool value)
{
	// only run if we're actually changing the value
	if (cursorShowing != value)
	{
		ShowCursor_orig(value);
		cursorShowing = value;
	}
	// < 0 = not showing
	return cursorShowing ? 0 : -1;
}

void MiscHooks::Init()
{
	// good enough
	srand(clock() ^ (time(NULL) << 3));

	if (Settings::GetFixRNG())
	{
		EnableFixRNG();
	}
	else
	{
		DisableFixRNG();
	}

	if (Settings::GetIncreaseMaxCameraDistance())
	{
		MiscHooks::SetMaxCameraDistance(4000.0f);
	}

	// trying to hook USER32.dll/ShowCursor is a massive pain, so we go straight to win32u/NtUserShowCursor
	// (ShowCursor is a wrapper for NtUserShowCursor)
	void* NtUserShowCursor = Escort::GetFuncAddress("win32u.dll", "NtUserShowCursor");
	ShowCursor_orig = Escort::JmpReplaceHook<int(bool)>(NtUserShowCursor, ShowCursor_hook, 8);
	TabMods_updateModsList_orig = Escort::JmpReplaceHook<void(GameLauncher::TabMods*, bool)>((void*)GetRealAddress(&GameLauncher::TabMods::updateModsList), TabMods_updateModsList_hook, 6);
}