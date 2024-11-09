#pragma once

#include <string>

namespace Bugs
{
	// Hooks globally uncaught exceptions that occur before Kenshi's crash handler is set up
	void Init();
	// Hooks Kenshi's crash report function
	void InitMenu();
	std::string GetUUIDHash();
	// manual bug reporting
	bool ReportUserBug(std::string description, std::string uuidHash = "");
	// report /w crashdump
	bool ReportCrash(std::string description, std::string crashDumpName, std::string uuidHash = "");
}