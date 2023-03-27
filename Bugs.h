#pragma once

#include <string>

namespace Bugs
{
	// Hooks Kenshi's crash report function
	void Init();
	std::string GetUUIDHash();
	// manual bug reporting
	bool ReportUserBug(std::string description, std::string uuidHash = "");
	// report /w crashdump
	bool ReportCrash(std::string description, std::string crashDumpName, std::string uuidHash = "");
}