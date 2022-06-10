
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