#pragma once

namespace MiscHooks
{
	void Init();

	// bugfix for Kenshi's RNG initialization
	void SetFixRNG(bool value);
	// moved here because it's in RWX memory and I don't wanna put those calls in the GUI code
	void SetMaxCameraDistance(float value);
}