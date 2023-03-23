#pragma once

#include <string>

namespace Bugs
{
	// Hooks Kenshi's crash report function
	void Init();
	// manual bug reporting
	void ReportUserBug(std::string description);
	// report /w crashdump
	void ReportCrash(std::string description, std::string crashDumpName);
}