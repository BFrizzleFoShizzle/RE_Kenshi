
#include "Debug.h"
#include <sstream>
#include <fstream>
#include <ctime>

std::stringstream debugLog;
std::ofstream debugFile("RE_Kenshi_log.txt");
clock_t startTime = -1;

void DebugLog(std::string message)
{
	if (startTime == -1)
		startTime = clock();

	clock_t logTime = clock();
	clock_t timeInMSecs = (logTime-startTime) / (CLOCKS_PER_SEC / 1000);

	char timeStr[10];
	sprintf_s(timeStr, "%i.%03i", timeInMSecs/1000, timeInMSecs%1000);

	// TODO disable?
	debugFile << timeStr << ": " << message << std::endl;
	debugLog << timeStr << ": " << message << std::endl;
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

