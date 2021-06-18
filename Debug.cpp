
#include "Debug.h"
#include <sstream>
#include <fstream>


std::stringstream debugLog;
std::ofstream debugFile("RE_Kenshi_log.txt");

void DebugLog(std::string message)
{
	// TODO disable?
	debugFile << message << std::endl;
	debugLog << message << std::endl;
}

void ErrorLog(std::string message)
{
	debugFile << "Error: " << message << std::endl;
	debugLog << "Error: " << message << std::endl;
}

std::string GetDebugLog()
{
	return debugLog.str();
}

