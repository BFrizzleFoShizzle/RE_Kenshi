
#include "MyGUIHooks.h"

#include "debug.h"
#include "Settings.h"
#include <kenshi/Kenshi.h>

// TODO remove after dropping support for old versions
void MyGUIHooks::InitMainMenu()
{
	
	// this has to be done *after* the UI is created
	// bug is patched in 1.0.60
	if ((Kenshi::GetKenshiVersion() == Kenshi::BinaryVersion(Kenshi::BinaryVersion::STEAM, "1.0.55") 
		|| Kenshi::GetKenshiVersion() == Kenshi::BinaryVersion(Kenshi::BinaryVersion::GOG, "1.0.59"))
		&& Settings::GetFixFontSize())
	{
		DebugLog("Fixing fonts...");
		Kenshi::GetCurrentFontSize() = 16;
		void (*updateFonts)() = (void(*)())Kenshi::GetUpdateFonts();
		updateFonts();
	}
	
}
