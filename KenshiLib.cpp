
#include "KenshiLib.h"
#include <Windows.h>

float* pKenshiTime;

void KenshiLib::Init()
{
    // pointer to first byte in EXE, as char* so we can do byte offsets
	char* exeBaseAddr = (char*)GetModuleHandle(L"kenshi_x64.exe");
	// kenshi_x64.exe+1AAE760
	pKenshiTime = (float*)(exeBaseAddr + 0x1AAE760);
}

void KenshiLib::SetGameSpeed(float speed)
{
	*pKenshiTime = speed;
}


// TODO templateize
// Kenshi prefixes it's widgets with a bunch of non-usefull stuff
MyGUI::WidgetPtr KenshiLib::FindWidget(MyGUI::EnumeratorWidgetPtr enumerator, std::string name)
{
    while (enumerator.next())
    {
        std::string widgetName = enumerator.current()->getName();
        size_t splitPos = widgetName.find('_');
        /*
        debug_out << widgetName;
        if(splitPos != std::string::npos)
            debug_out << " " << widgetName.substr(splitPos+1);
        debug_out << "\n";
        */
        if (splitPos != std::string::npos && widgetName.substr(splitPos + 1) == name)
            return enumerator.current();
        if (enumerator.current()->getChildCount() > 0)
        {
            MyGUI::WidgetPtr childFoundWidget = KenshiLib::FindWidget(enumerator.current()->getEnumerator(), name);
            if (childFoundWidget != nullptr)
                return childFoundWidget;
        }
    }
    return nullptr;
}