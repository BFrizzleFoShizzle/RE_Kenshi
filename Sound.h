#pragma once

namespace Sound
{
	void Init();
	// Will load queued banks if init soundbank + modbank is loaded
	void TryLoadQueuedBanks();
	void SetAlwaysLog(bool alwaysLog);
}