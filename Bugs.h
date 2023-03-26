#pragma once

#include <string>

namespace Bugs
{
	// Hooks Kenshi's crash report function
	void Init();
	std::string GetUUIDHash();
	// manual bug reporting
	void ReportUserBug(std::string description, std::string uuidHash = "");
	// report /w crashdump
	void ReportCrash(std::string description, std::string crashDumpName, std::string uuidHash = "");
}