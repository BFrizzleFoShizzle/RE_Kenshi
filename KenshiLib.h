#pragma once
#include "MyGUI_Gui.h"
namespace KenshiLib {
	void Init();
	MyGUI::WidgetPtr FindWidget(MyGUI::EnumeratorWidgetPtr enumerator, std::string name);
	void SetGameSpeed(float speed);
}