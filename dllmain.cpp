// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>

#include "MyGUI_RenderManager.h"
#include "MyGUI_Gui.h"
#include "MyGUI_Button.h"
#include "MyGUI_TextBox.h"

#include <fstream>
#include <iostream>

#include "KenshiLib.h"

float gameSpeed = 1.0f;
MyGUI::TextBox* gameSpeedText = nullptr;

/*
std::string ConvertGUIToText(MyGUI::EnumeratorWidgetPtr enumerator, std::string pad)
{
    std::string output = "";
    // you might think this should be a do..while() loop.
    // You'd be wrong. The first next() call does nothing in MyGUI.
    // if you don't call next() before processing the first element, 
    // the first element gets returned twice.
    while (enumerator.next())
    {
        std::string widgetName = enumerator.current()->getName();
        std::string widgetType = enumerator.current()->getTypeName();
        std::string widgetClass = enumerator.current()->getClassTypeName();
        std::string widgetEnabled = enumerator.current()->getEnabled() ? "true" : "false";
        std::string widgetVisible = enumerator.current()->getVisible() ? "true" : "false";
        std::string widgetUserStrings = "";
        output += pad + "\"" + widgetName + "\" - \"" + widgetType + "\" - " + widgetClass + "\" - " + widgetEnabled + " " + widgetVisible +  " \"" + widgetUserStrings + "\"\n";
        if (enumerator.current()->getChildCount() > 0)
        {
            output += ConvertGUIToText(enumerator.current()->getEnumerator(), "\t" + pad);
        }
    } 
    return output;
}
*/
void increaseSpeed(MyGUI::WidgetPtr _sender)
{
    gameSpeed += 1.0f;
    KenshiLib::SetGameSpeed(gameSpeed);
    gameSpeedText->setCaption(std::to_string((long double)gameSpeed));
}

void decreaseSpeed(MyGUI::WidgetPtr _sender)
{
    gameSpeed -= 1.0f;
    gameSpeed = max(gameSpeed, 1.0f);
    KenshiLib::SetGameSpeed(gameSpeed);
    gameSpeedText->setCaption(std::to_string((long double)gameSpeed));
}

void WaitForInGame()
{
    MyGUI::Gui* gui = nullptr;
    while (gui == nullptr)
    {
        gui = MyGUI::Gui::getInstancePtr();
        Sleep(100);
    }
    MyGUI::WidgetPtr speedButtonsPanel = nullptr;
    while (speedButtonsPanel == nullptr)
    {
        speedButtonsPanel = KenshiLib::FindWidget(gui->getEnumerator(), "SpeedButtonsPanel");
        Sleep(100);
    }
}

void WaitForMainMenu()
{
    MyGUI::Gui* gui = nullptr;
    while (gui == nullptr)
    {
        gui = MyGUI::Gui::getInstancePtr();
        Sleep(100);
    }
    MyGUI::WidgetPtr versionText = nullptr;
    while (versionText == nullptr)
    {
        versionText = KenshiLib::FindWidget(gui->getEnumerator(), "VersionText");
        Sleep(100);
    }
}

void dllmain()
{
    KenshiLib::Init();

    WaitForMainMenu();

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::TextBox* versionText = KenshiLib::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
    MyGUI::UString version = versionText->getCaption();
    versionText->setCaption("RE_Kenshi 0.1 - " + version);

    WaitForInGame();

    /*
    std::ofstream outf = std::ofstream("gui_modules.txt");
    if (!outf)
    {
        // Print an error and exit
        MessageBoxA(0, "Error opening output file", "Debug", MB_OK); 
        return;
    }
    MyGUI::EnumeratorWidgetPtr enumerator = gui->getEnumerator();
    std::string guiString = ConvertGUIToText(enumerator, "");
    outf << guiString;
    outf.close();
    MessageBoxA(0, "GUI saved successfully.", "Debug", MB_OK);
    */
    try
    {

        MyGUI::WidgetPtr speedButtonsPanel = KenshiLib::FindWidget(gui->getEnumerator(), "SpeedButtonsPanel");
        if (speedButtonsPanel == nullptr)
            MessageBoxA(0, "SpeedButtonsPanel not found.", "Debug", MB_OK);
        MyGUI::WidgetPtr speedButton3 = KenshiLib::FindWidget(gui->getEnumerator(), "TimeSpeedButton3");// ->castType<MyGUI::Button>();
        if (speedButton3 == nullptr) {
            MessageBoxA(0, "TimeSpeedButton3 not found.", "Debug", MB_OK);
            return;
        }
        MyGUI::WidgetPtr speedButton4 = KenshiLib::FindWidget(gui->getEnumerator(), "TimeSpeedButton4");// ->castType<MyGUI::Button>();
        if (speedButton4 == nullptr) {
            MessageBoxA(0, "TimeSpeedButton4 not found.", "Debug", MB_OK);
            return;
        }
        
        MyGUI::FloatCoord gameSpeedCoord = MyGUI::FloatCoord(0.0f, 0.0f, 1.0f, 0.2f);
        gameSpeedText = speedButtonsPanel->createWidgetReal<MyGUI::TextBox>("Kenshi_TextboxStandardText", gameSpeedCoord, MyGUI::Align::Center, "GameSpeedText");
        gameSpeedText->setCaption("1");
        gameSpeedText->setTextAlign(MyGUI::Align::Center);

        //MessageBoxA(0, "Clearing...", "Debug", MB_OK);
        speedButton3->eventMouseButtonClick.clear();
        speedButton4->eventMouseButtonClick.clear();
        //MessageBoxA(0, "Cleared.", "Debug", MB_OK);
        speedButton3->eventMouseButtonClick += MyGUI::newDelegate(decreaseSpeed);
        speedButton4->eventMouseButtonClick += MyGUI::newDelegate(increaseSpeed);
        //MessageBoxA(0, "Delegated.", "Debug", MB_OK);
    }
    catch (MyGUI::Exception e)
    {
        MessageBoxA(0, e.what(), "Debug - Error", MB_OK);

    }
    catch (std::exception e)
    {
        MessageBoxA(0, e.what(), "Debug - Error", MB_OK);
    }
}

__declspec(dllexport) void ExportFunc()
{

}

DWORD WINAPI threadWrapper(LPVOID param)
{
    dllmain();
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, 0, threadWrapper, 0, 0, 0);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

