#pragma once
#include "MyGUI_Gui.h"


namespace KenshiLib {
	void Init();
	MyGUI::WidgetPtr FindWidget(MyGUI::EnumeratorWidgetPtr enumerator, std::string name);
	void SetGameSpeed(float speed);
	void* GetKenshiPtr(size_t offset);

	template<typename T>
	T* GetKenshiPtr(size_t offset)
	{
		return (T*)GetKenshiPtr(offset);
	}
}