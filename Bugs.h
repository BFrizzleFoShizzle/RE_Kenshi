#pragma once

#include <string>

namespace Bugs
{
	// Hooks globally uncaught exceptions that occur before Kenshi's crash handler is set up
	// 1st init stage - run before KenshiLib init
	void PreInit();
	// run after 1st stage if the version isn't supported
	void UndoPreInit();
	// 2nd init stage - run after KenshiLib init
	void Init();
	// Hooks Kenshi's crash report function
	void InitMenu();
	void InitInGame();
	std::string GetUUIDHash();
	// manual bug reporting
	bool ReportUserBug(std::string description, std::string uuidHash = "");
	// report /w crashdump
	bool ReportCrash(std::string description, std::string crashDumpName, std::string uuidHash = "");
}