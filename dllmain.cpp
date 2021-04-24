// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>

#include "mygui/MyGUI_RenderManager.h"
#include "mygui/MyGUI_Gui.h"
#include "mygui/MyGUI_Button.h"
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

void dllmain()
{
    LoadGameSpeedValues("game_speeds.ini");

    WaitForMainMenu();

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
    MyGUI::UString version = versionText->getCaption();
    versionText->setCaption("RE_Kenshi 0.1.1 - " + version);

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
