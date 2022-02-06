#pragma once

namespace MiscHooks
{
	void Init();

	// bugfix for Kenshi's RNG initialization
	void SetFixRNG(bool value);
}