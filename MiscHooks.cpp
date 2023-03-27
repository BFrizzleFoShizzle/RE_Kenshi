
#include "MiscHooks.h"

#include "kenshi/Kenshi.h"

#include "Escort.h"
#include "Settings.h"

#include <stdlib.h>
#include <ctime>

// Dummy function to stop Kenshi calling srand()
void srand_hook(int val) {
	return;
}

// bytes before modification
uint8_t srand_old[15];

bool rngHooksInstalled = false;

void EnableFixRNG()
{
	void* srandPtr = Escort::GetFuncAddress("MSVCR100.dll", "srand");
	// backup bytes
	memcpy(srand_old, srandPtr, 15);
	Escort::PushRetHookASM(srandPtr, srand_hook, 15);
	rngHooksInstalled = true;
}

void DisableFixRNG()
{
	void* srandPtr = Escort::GetFuncAddress("MSVCR100.dll", "srand");
	// Restore backup
	Escort::WriteProtected(srand_old, srandPtr, 15);
	rngHooksInstalled = false;
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
	Escort::WriteProtected(&Kenshi::GetMaxCameraDistance(), &value, sizeof(float));
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
}