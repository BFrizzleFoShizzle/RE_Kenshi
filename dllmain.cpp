// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>

#include "mygui/MyGUI_RenderManager.h"
#include "mygui/MyGUI_Gui.h"
#include "mygui/MyGUI_Button.h"
#include "mygui/MyGUI_Window.h"
#include "mygui/MyGUI_TextBox.h"

#include <fstream>
#include <iostream>
#include <algorithm>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"


#include <ogre/OgrePrerequisites.h>

float gameSpeed = 1.0f;
MyGUI::TextBox* gameSpeedText = nullptr;

int gameSpeedIdx = 0;
std::vector<float> gameSpeedValues;

/*
const std::string MOD_VERSION = "0.1.2";

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
    gameSpeedIdx = std::min(gameSpeedIdx + 1, (int)gameSpeedValues.size() - 1);
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
    gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed));
}

void decreaseSpeed(MyGUI::WidgetPtr _sender)
{
    gameSpeedIdx = std::max(gameSpeedIdx - 1, 0);
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
    gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed));
}

void playButtonHook(MyGUI::WidgetPtr _sender)
{
    // Kenshi will probably set game speed to 1, next time speed3 or speed4 buttons are clicked, will revert to modified game speed...
    // TODO how to handle this better?
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    std::string gameSpeedMessage = std::to_string((long double)gameWorld.gameSpeed);
    if(gameWorld.gameSpeed != gameSpeedValues[gameSpeedIdx])
        gameSpeedMessage += " (" + std::to_string((long double)gameSpeedValues[gameSpeedIdx]) + ")";
    gameSpeedText->setCaption(gameSpeedMessage);
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
        speedButtonsPanel = Kenshi::FindWidget(gui->getEnumerator(), "SpeedButtonsPanel");
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
        versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText");
        Sleep(100);
    }
}

void LoadGameSpeedValues(std::string path)
{
    float val = -1;
    std::ifstream gameSpeedValueFile = std::ifstream(path);
    // if file is empty or doesn't contain values, reset
    if (!gameSpeedValueFile || !(gameSpeedValueFile >> val))
    {
        std::ofstream gameSpeedOutFile = std::ofstream(path);
        gameSpeedOutFile << "1 2 3 4 5";
        gameSpeedOutFile.close();
    }

    // reopen because above logic is bad
    gameSpeedValueFile = std::ifstream(path);
    if (!gameSpeedValueFile)
        MessageBoxA(0, "Could not load/create game speeds settings file!", "Debug", MB_OK);

    while (gameSpeedValueFile >> val)
    {
        gameSpeedValues.push_back(val);
    }
}

// TODO make nicer
MyGUI::Window* modMenuWindow;

void debugMenuButtonPress(MyGUI::Window* _sender, const std::string &name)
{
    if (name == "close")
        _sender->setVisible(false);
}

void dllmain()
{
    LoadGameSpeedValues("game_speeds.ini");

    WaitForMainMenu();

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    
    modMenuWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, 400, 400, MyGUI::Align::Center, "Window", "DebugWindow");
    modMenuWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    MyGUI::Widget* client = modMenuWindow->findWidget("Client");
    MyGUI::Widget* widg = client->createWidgetReal<MyGUI::Widget>("Kenshi_GenericTextBoxSkin", 0,0,1,1, MyGUI::Align::Center);
    MyGUI::FloatCoord debugOutCoord = MyGUI::FloatCoord(0.03, 0.03, 0.94, 0.94);
    MyGUI::TextBox* debugOut = widg->createWidgetReal<MyGUI::TextBox>("Kenshi_TextboxStandardText", debugOutCoord, MyGUI::Align::Center, "DebugPrint");
    debugOut->setCaption("Main menu loaded.\n");

    MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
    MyGUI::UString version = versionText->getCaption();

    Kenshi::BinaryVersion gameVersion = Kenshi::GetKenshiVersion();

    debugOut->setCaption(debugOut->getCaption() + "Kenshi version: " + gameVersion.GetVersion() + "\n");
    debugOut->setCaption(debugOut->getCaption() + "Kenshi platform: " + gameVersion.GetPlatformStr() + "\n");
    // TODO make this better
    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        debugOut->setCaption(debugOut->getCaption() + "Version supported.\n");
        versionText->setCaption("RE_Kenshi " + MOD_VERSION + " - " + version);
    }
    else
    {
        debugOut->setCaption(debugOut->getCaption() + "ERROR: Game version not recognized.\n");
        debugOut->setCaption(debugOut->getCaption() + "Supported versions: .\n");
        // TODO auto-generate?
        debugOut->setCaption(debugOut->getCaption() + "GOG 1.0.51\n");
        debugOut->setCaption(debugOut->getCaption() + "Steam 1.0.51\n");
        debugOut->setCaption(debugOut->getCaption() + "RE_Kenshi initialization aborted!\n");
        versionText->setCaption("RE_Kenshi " + MOD_VERSION + " (ERROR) - " + version);
        return;
    }

    WaitForInGame();

    debugOut->setCaption(debugOut->getCaption() + "In-game.\n");

    try
    {

        MyGUI::WidgetPtr timeMoneyPanel = Kenshi::FindWidget(gui->getEnumerator(), "TimeMoneyPanel");
        if (timeMoneyPanel == nullptr)
            MessageBoxA(0, "TimeMoneyPanel not found.", "Debug", MB_OK);
        MyGUI::WidgetPtr speedButton2 = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton2");// ->castType<MyGUI::Button>();
        if (speedButton2 == nullptr) {
            MessageBoxA(0, "TimeSpeedButton2 not found.", "Debug", MB_OK);
            return;
        }
        MyGUI::WidgetPtr speedButton3 = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton3");// ->castType<MyGUI::Button>();
        if (speedButton3 == nullptr) {
            MessageBoxA(0, "TimeSpeedButton3 not found.", "Debug", MB_OK);
            return;
        }
        MyGUI::WidgetPtr speedButton4 = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton4");// ->castType<MyGUI::Button>();
        if (speedButton4 == nullptr) {
            MessageBoxA(0, "TimeSpeedButton4 not found.", "Debug", MB_OK);
            return;
        }

        MyGUI::FloatCoord gameSpeedCoord = MyGUI::FloatCoord(0.0f, 0.522388f, 1.0f, 0.298507f);
        gameSpeedText = timeMoneyPanel->createWidgetReal<MyGUI::TextBox>("Kenshi_TextboxStandardText", gameSpeedCoord, MyGUI::Align::Center, "GameSpeedText");
        // HACK use same code as play button hook to display current in-game speed + current modified speed
        playButtonHook(nullptr);
        //gameSpeedText->setCaption("1");
        gameSpeedText->setTextAlign(MyGUI::Align::Center);

        //MessageBoxA(0, "Clearing...", "Debug", MB_OK);
        speedButton3->eventMouseButtonClick.clear();
        speedButton4->eventMouseButtonClick.clear();
        //MessageBoxA(0, "Cleared.", "Debug", MB_OK);
        speedButton2->eventMouseButtonClick += MyGUI::newDelegate(playButtonHook);
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

DWORD WINAPI threadWrapper(LPVOID param)
{
    dllmain();
    return 0;
}

// Ogre plugin export
extern "C" void __declspec(dllexport) dllStartPlugin(void)
{
    CreateThread(NULL, 0, threadWrapper, 0, 0, 0);
}
