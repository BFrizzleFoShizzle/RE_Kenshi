#pragma once

#include <string>

namespace Version
{
	void Init();
	// May block for a while
	bool IsCurrentVersion();
	// "x.x.x"
	std::string GetCurrentVersion();
	// version text to display in main menu
	std::string GetDisplayVersion();
}