// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>

#include "mygui/MyGUI_RenderManager.h"
#include "mygui/MyGUI_Gui.h"
#include "mygui/MyGUI_Button.h"
#include "mygui/MyGUI_Window.h"
#include "mygui/MyGUI_TextBox.h"
#include "mygui/MyGUI_EditBox.h"
#include "mygui/MyGUI_ScrollView.h"
#include "mygui/MyGUI_TabControl.h"
#include "mygui/MyGUI_TabItem.h"
#include "mygui/MyGUI_ScrollBar.h"

#include <iomanip>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"

#include "HeightmapHook.h"
#include "Debug.h"
#include "Settings.h"

#include <ogre/OgrePrerequisites.h>

float gameSpeed = 1.0f;
MyGUI::TextBox* gameSpeedText = nullptr;

int gameSpeedIdx = 0;

const std::string MOD_VERSION = "0.1.2";

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
    std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
    gameSpeedIdx += 1;
    // Clamp
    gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
    gameSpeedIdx = std::max(gameSpeedIdx, 0);
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
    gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed));
}

void decreaseSpeed(MyGUI::WidgetPtr _sender)
{
    std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
    gameSpeedIdx -= 1;
    // Clamp
    gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
    gameSpeedIdx = std::max(gameSpeedIdx, 0);
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
    gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed));
}

void playButtonHook(MyGUI::WidgetPtr _sender)
{
    std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
    // Clamp
    gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
    gameSpeedIdx = std::max(gameSpeedIdx, 0);

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

// TODO make nicer
MyGUI::Window* modMenuWindow = nullptr;

void openSettingsMenu(MyGUI::WidgetPtr button)
{
    modMenuWindow->setVisible(true);
}

void debugMenuButtonPress(MyGUI::Window* _sender, const std::string &name)
{
    if (name == "close")
        _sender->setVisible(false);
}

void debugMenuKeyRelease(MyGUI::Widget* _sender, MyGUI::KeyCode _key)
{
    // tilde/backtick
    if (_key.getValue() == 41)
        modMenuWindow->setVisible(!modMenuWindow->getVisible());
}

std::string kenshiVersionStr = "UNKNOWN";
std::string kenshiPlatformStr = "UNKNOWN";
MyGUI::CanvasPtr debugImgCanvas = nullptr;
MyGUI::ScrollViewPtr debugLogScrollView = nullptr;

const uint32_t DEBUG_WINDOW_WIDTH = 600;
const uint32_t DEBUG_WINDOW_HEIGHT = 600;

void TickButtonBehaviourClick(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    if(button)
        button->setStateSelected(!button->getStateSelected());
}

void ToggleUseCompressedHeightmap(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool useHeightmapCompression = button->getStateSelected();

    // Update settings (done first in case of crash)
    Settings::SetUseHeightmapCompression(useHeightmapCompression);
    
    // Update hooks
    if (useHeightmapCompression)
        HeightmapHook::EnableHeightmapHooks();
    else
        HeightmapHook::DisableHeightmapHooks();
}

int SliderIndexFromName(std::string name)
{
    size_t splitIdx = name.find("_");

    // Extract scroll bar index
    // length of "SpeedSlider" - 1
    size_t numberStart = 11;
    std::string numberStr = name.substr(numberStart, splitIdx - numberStart);

    // gross str -> int conversion
    std::stringstream str = std::stringstream(numberStr);
    int number = -1;
    str >> number;
    return number;
}

float ScaleGameSpeed(size_t speed)
{
    float scaled = std::max(1ull, speed) / 1000.0f;
    float base = 50.0f;
    scaled = (powf(base, scaled) - 1.0f) / (base - 1.0f);
    scaled = scaled * 1000.0f;
    return scaled;
}

size_t UnscaleGameSpeed(float speed)
{
    float scaled = std::min(1000.0f, speed) / 1000.0f;
    float base = 50.0f;
    // undo post-pow scaling
    scaled = scaled * (base - 1.0f);
    scaled = scaled + 1.0f;
    // undo pow
    scaled = logf(scaled) / logf(base);
    // scale from 0-1000
    scaled = scaled * 1000.0f;
    return std::min(1000, (int)scaled);
}

void SliderTextChange(MyGUI::EditBox* editBox)
{
    std::string valueStr = editBox->getCaption();
    float value = -1;
    // parse float
    std::stringstream str(valueStr);
    str >> value;
    // bounds check - ignore if invalid
    if (value < 0.0f)
        return;

    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();

    int sliderIdx = SliderIndexFromName(editBox->getName());

    assert(sliderIdx < gameSpeeds.size());
    gameSpeeds[sliderIdx] = value;

    std::stringstream prefix;
    prefix << "SpeedSlider" << sliderIdx << "_";
    MyGUI::ScrollBar *scrollBar = editBox->getParent()->findWidget(prefix.str() + "Slider")->castType<MyGUI::ScrollBar>();
    scrollBar->setScrollPosition(UnscaleGameSpeed(value));

    Settings::SetGameSpeeds(gameSpeeds);
}

// Root widget name will be "[namePrefix]SliderRoot"
MyGUI::WidgetPtr CreateSlider(MyGUI::WidgetPtr parent, int x, int y, int w, int h, std::string namePrefix)
{
    /* THIS TEMPLATE USES A READ-ONLY TEXTBOX SO ISN'T USEFUL
    MyGUI::IResourcePtr sliderRes = MyGUI::ResourceManager::getInstancePtr()->findByName("Kenshi_Slider");
    if (!sliderRes)
        DebugLog("Slider resource not found!");

    MyGUI::ResourceLayout* sliderResLayout = sliderRes->castType<MyGUI::ResourceLayout>();

    MyGUI::VectorWidgetPtr sliderVec = sliderResLayout->createLayout(namePrefix, parent);
    MyGUI::WidgetPtr slider = sliderVec[0];

    assert(slider->getName() == namePrefix + "Root");

    slider->setCoord(MyGUI::IntCoord(x, y, w, h));

    */
    // Kenshi doesn't use "Kenshi_Slider" for float sliders, they seem to use custom C++ interface, so I roll my own to match
    // Naming convention matches "Kenshi_Slider"
    MyGUI::WidgetPtr sliderRoot = parent->createWidget<MyGUI::Widget>("PanelEmpty", x, y, w, h, MyGUI::Align::Top | MyGUI::Align::Left, namePrefix + "SliderRoot");
    MyGUI::ButtonPtr deleteButton = sliderRoot->createWidget<MyGUI::Button>("Kenshi_CloseButtonSkin", w - 30, (h - 30) / 2, 30, 30, MyGUI::Align::Right | MyGUI::Align::Top, namePrefix + "DeleteButton");
    MyGUI::TextBox *sliderLabel = sliderRoot->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 0, 0, 80, h, MyGUI::Align::Left | MyGUI::Align::VStretch, namePrefix + "ElementText");
    sliderLabel->setTextAlign(MyGUI::Align::Left);
    MyGUI::ScrollBar *scrollBar = sliderRoot->createWidget<MyGUI::ScrollBar>("Kenshi_ScrollBar", 80, 0, w - 175, h, MyGUI::Align::Stretch, namePrefix + "Slider");
    MyGUI::EditBox* valueText = sliderRoot->createWidget<MyGUI::EditBox>("Kenshi_EditBox", w - 90, 0, 60, h, MyGUI::Align::Right | MyGUI::Align::VStretch, namePrefix + "NumberText");
    valueText->eventEditTextChange += MyGUI::newDelegate(SliderTextChange);

    return sliderRoot;
}

void GameSpeedScroll(MyGUI::ScrollBar *scrollBar, size_t newPos)
{
    // nonlinear scaling to add more resolution to low game speeds
    float scaled = ScaleGameSpeed(newPos);

    size_t splitIdx = scrollBar->getName().find("_");
    std::string prefix = scrollBar->getName().substr(0, splitIdx);

    // Update number text
    MyGUI::EditBox* numberText = scrollBar->getParent()->findWidget(prefix + "_NumberText")->castType<MyGUI::EditBox>();
    if (!numberText)
        ErrorLog("Number text not found on scrollbar!");
    std::stringstream str;
    str << std::setprecision(3) << scaled;
    numberText->setCaption(str.str());
    float finalSpeedVal = 0.0f;
    str >> finalSpeedVal;

    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();

    int number = SliderIndexFromName(scrollBar->getName());
    assert(number < gameSpeeds.size());
    gameSpeeds[number] = finalSpeedVal;
    Settings::SetGameSpeeds(gameSpeeds);
}

void RedrawGameSpeedSettings();

void DeleteGameSpeedScroll(MyGUI::WidgetPtr deleteBtn)
{
    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();

    int number = SliderIndexFromName(deleteBtn->getName());
    assert(number < gameSpeeds.size());

    // Remove from speeds
    gameSpeeds.erase(gameSpeeds.begin() + number);

    // Save
    Settings::SetGameSpeeds(gameSpeeds);

    // Update GUI
    RedrawGameSpeedSettings();
}

MyGUI::ScrollViewPtr settingsView;
int gameSpeedScrollBarsStart = 0;

void RedrawGameSpeedSettings()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    // Clear old scroll bars
    int scrollBarIndex = 0;
    std::stringstream nameStr;
    nameStr << "SpeedSlider" << scrollBarIndex << "_";
    while (settingsView->findWidget(nameStr.str() + "SliderRoot"))
    {
        gui->destroyWidget(settingsView->findWidget(nameStr.str() + "SliderRoot"));
        ++scrollBarIndex;
        nameStr = std::stringstream();
        nameStr << "SpeedSlider" << scrollBarIndex << "_";
    }

    // Create new scroll bars
    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();
    int positionY = gameSpeedScrollBarsStart;
    for (int i = 0; i < gameSpeeds.size(); ++i)
    {
        std::stringstream nameStr;
        nameStr << "SpeedSlider" << i << "_";
        MyGUI::WidgetPtr slider = CreateSlider(settingsView, 2, positionY, 500, 40, nameStr.str());

        MyGUI::TextBox* elementText = slider->findWidget(nameStr.str() + "ElementText")->castType<MyGUI::TextBox>();
        if (!elementText)
            DebugLog("ElementText not found!");
        std::stringstream label;
        label << "Speed " << (i + 1) << ":";
        elementText->setCaption(label.str());
        MyGUI::ScrollBar* scrollBar = slider->findWidget(nameStr.str() + "Slider")->castType<MyGUI::ScrollBar>();
        // 0...range-1 = 0...1000
        scrollBar->setScrollRange(1001);
        scrollBar->setScrollPosition(UnscaleGameSpeed(gameSpeeds[i]));
        scrollBar->eventScrollChangePosition += MyGUI::newDelegate(GameSpeedScroll);
        MyGUI::TextBox* numberText = slider->findWidget(nameStr.str() + "NumberText")->castType<MyGUI::TextBox>();
        std::stringstream value;
        value << gameSpeeds[i];
        numberText->setCaption(value.str());
        MyGUI::ButtonPtr deleteButton = slider->findWidget(nameStr.str() + "DeleteButton")->castType<MyGUI::Button>();
        deleteButton->eventMouseButtonClick += MyGUI::newDelegate(DeleteGameSpeedScroll);
        positionY += 45;
    }

    // resize settings
    MyGUI::IntSize canvasSize = settingsView->getCanvasSize();
    settingsView->setCanvasSize(canvasSize.width, std::max(positionY, canvasSize.height));
}

void AddGameSpeed(MyGUI::WidgetPtr button)
{
    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();

    if (gameSpeeds.size() > 0)
        gameSpeeds.push_back(gameSpeeds.back());
    else
        gameSpeeds.push_back(1.0f);

    Settings::SetGameSpeeds(gameSpeeds);
    RedrawGameSpeedSettings();
}

// Takes into account mods
int defaultAttackSlots = -1;

void AttackSlotScroll(MyGUI::ScrollBar* scrollBar, size_t newPos)
{
    // Update number text
    MyGUI::EditBox* numberText = scrollBar->getParent()->findWidget("AttackSlotsSlider_NumberText")->castType<MyGUI::EditBox>();

    if (newPos > 0)
    {
        std::stringstream str;
        str << newPos;
        numberText->setCaption(str.str());
        Settings::SetAttackSlots(newPos);
        // TODO this sometimes causes a crash if Kenshi is unpaused
        Kenshi::GetNumAttackSlots() = newPos;
    }
    else
    {
        std::stringstream str;
        str << "(" << defaultAttackSlots << ")";
        numberText->setCaption(str.str());
        Settings::SetAttackSlots(-1);
        Kenshi::GetNumAttackSlots() = defaultAttackSlots;
    }
}

void InitGUI()
{
    DebugLog("Main menu loaded.");

    MyGUI::RenderManager* renderManager = MyGUI::RenderManager::getInstancePtr();
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    // Create mod menu
    modMenuWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, DEBUG_WINDOW_WIDTH, DEBUG_WINDOW_HEIGHT, MyGUI::Align::Center, "Window", "DebugWindow");
    modMenuWindow->setCaption("RE_Kenshi Menu");
    modMenuWindow->eventKeyButtonReleased += MyGUI::newDelegate(debugMenuKeyRelease);
    modMenuWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    MyGUI::Widget* client = modMenuWindow->findWidget("Client");
    MyGUI::TabControl *tabControl = client->createWidget<MyGUI::TabControl>("Kenshi_TabControl", MyGUI::IntCoord(2, 2, modMenuWindow->getClientCoord().width - 4, modMenuWindow->getClientCoord().height - 4), MyGUI::Align::Stretch);
    
    // Mod settings
    MyGUI::TabItemPtr settingsTab = tabControl->addItem("Settings");
    settingsView = settingsTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollView", MyGUI::IntCoord(2, 2, settingsTab->getClientCoord().width - 4, settingsTab->getClientCoord().height - 4), MyGUI::Align::Stretch);
    settingsView->setVisibleHScroll(false);
    int positionY = 2;
    settingsView->setCanvasSize(settingsView->getWidth(), settingsView->getHeight());
    MyGUI::ButtonPtr useCompressedHeightmap = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY, 500, 26, MyGUI::Align::Top | MyGUI::Align::Left, "UseCompressedHeightmapToggle");
    useCompressedHeightmap->setStateSelected(Settings::UseHeightmapCompression());
    useCompressedHeightmap->setCaption("[TOGGLE MAY CRASH] Use compressed heightmap");
    useCompressedHeightmap->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    useCompressedHeightmap->eventMouseButtonClick += MyGUI::newDelegate(ToggleUseCompressedHeightmap);
    positionY += 30;
    // Attack slots
    defaultAttackSlots = Kenshi::GetNumAttackSlots();
    int numAttackSlots = Settings::GetAttackSlots();
    // Apply settings
    if (numAttackSlots > 0)
        Kenshi::GetNumAttackSlots() = numAttackSlots;
    MyGUI::WidgetPtr attackSlotsSlider = CreateSlider(settingsView, 2, positionY, 500, 40, "AttackSlotsSlider_");
    MyGUI::TextBox* elementText = attackSlotsSlider->findWidget("AttackSlotsSlider_ElementText")->castType<MyGUI::TextBox>();
    std::stringstream attackSlotsLabel;
    attackSlotsLabel << "Attack slots (" << defaultAttackSlots << "):";
    elementText->setCaption(attackSlotsLabel.str());
    MyGUI::EditBox* numberText = attackSlotsSlider->findWidget("AttackSlotsSlider_NumberText")->castType<MyGUI::EditBox>();
    numberText->setEditStatic(true);
    std::stringstream attackSlotsStr;
    if (numAttackSlots > 0)
        attackSlotsStr << numAttackSlots;
    else
        attackSlotsStr << "(" << defaultAttackSlots << ")";
    numberText->setCaption(attackSlotsStr.str());
    MyGUI::ScrollBar* attackSlotsScrollBar = attackSlotsSlider->findWidget("AttackSlotsSlider_Slider")->castType<MyGUI::ScrollBar>();
    attackSlotsScrollBar->setScrollRange(6);
    if(numAttackSlots > 0)
        attackSlotsScrollBar->setScrollPosition(Settings::GetAttackSlots());
    else
        attackSlotsScrollBar->setScrollPosition(0);
    attackSlotsScrollBar->eventScrollChangePosition += MyGUI::newDelegate(AttackSlotScroll);
    positionY += 45;
    MyGUI::TextBox* gameSpeedsLabel = settingsView->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 2, positionY, 500, 30, MyGUI::Align::Top | MyGUI::Align::Center, "GameSpeedsLabel");
    gameSpeedsLabel->setCaption("Game speeds");
    MyGUI::ButtonPtr addGameSpeed = settingsView->createWidget<MyGUI::Button>("Kenshi_Button1", 300, positionY, 200, 30, MyGUI::Align::Top | MyGUI::Align::Right, "AddGameSpeedBtn");
    addGameSpeed->setCaption("Add game speed");
    addGameSpeed->eventMouseButtonClick += MyGUI::newDelegate(AddGameSpeed);
    positionY += 35;
    gameSpeedScrollBarsStart = positionY;
    RedrawGameSpeedSettings();
    
    // Debug log
    MyGUI::TabItemPtr debugLogTab = tabControl->addItem("Debug log");
    debugLogScrollView = debugLogTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollView", MyGUI::IntCoord(2, 2, debugLogTab->getClientCoord().width - 4, debugLogTab->getClientCoord().height - 4), MyGUI::Align::Stretch);
    debugLogScrollView->setVisibleHScroll(false);
    debugLogScrollView->setCanvasSize(debugLogScrollView->getWidth(), DEBUG_WINDOW_HEIGHT / 2);
    MyGUI::TextBox* debugOut = debugLogScrollView->createWidgetReal<MyGUI::TextBox>("Kenshi_TextboxStandardText", 0, 0, 1, 1, MyGUI::Align::Stretch, "DebugPrint");

    MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
    MyGUI::UString version = versionText->getCaption();
    DebugLog(version);
    std::istringstream stream(version);
    while (stream)
    {
        std::string currWord;
        stream >> currWord;
        if (std::count(currWord.begin(), currWord.end(), '.') == 2)
        {
            // probably version text
            // TODO error-checking
            kenshiVersionStr = currWord;
        }
    }

    Kenshi::BinaryVersion gameVersion = Kenshi::GetKenshiVersion();

    DebugLog("Kenshi version: " + gameVersion.GetVersion());
    DebugLog("Kenshi platform: " + gameVersion.GetPlatformStr());

    // TODO make this better
    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        DebugLog("Version supported.");
        versionText->setCaption("RE_Kenshi " + MOD_VERSION + " - " + version);
    }
    else
    {
        DebugLog("ERROR: Game version not recognized.");
        DebugLog("");
        DebugLog("Supported versions:");
        DebugLog("GOG 1.0.51");
        DebugLog("Steam 1.0.51");
        DebugLog("RE_Kenshi initialization aborted!");

        versionText->setCaption("RE_Kenshi " + MOD_VERSION + " (ERROR) - " + version);
        return;
    }
}

bool hookedLoad = false;

void LoadStart(MyGUI::WidgetPtr widget)
{
    DebugLog("Game loading...");
}

bool oldLoadingPanelVisible = false;

void InjectSettings()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    //  inject mod menu into Kenshi's settings menu "MODS" tab
    MyGUI::TabControl* optionsTabCtrl = Kenshi::FindWidget(gui->getEnumerator(), "OptionsTab")->castType<MyGUI::TabControl>();
    if (optionsTabCtrl == nullptr)
        return;

    if (optionsTabCtrl->getItemCount() <= 0)
        return;

    MyGUI::TabItemPtr modsTab = optionsTabCtrl->getItemAt(optionsTabCtrl->getItemCount() - 1);
    if (modsTab == nullptr || modsTab->getCaption() != "MODS")
        return;

    // This causes crashes when the options menu is closed
    //optionsTabCtrl->addItem("RE_KENSHI");

    MyGUI::ButtonPtr settingsButton = modsTab->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.7f, 0.02f, 0.25f, 0.05f, MyGUI::Align::Top | MyGUI::Align::Left, "ModSettingsOpen");
    settingsButton->setCaption("RE_Kenshi settings");
    settingsButton->eventMouseButtonClick += MyGUI::newDelegate(openSettingsMenu);
    DebugLog("Injected settings button");
}

void GUIUpdate(float timeDelta)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (modMenuWindow == nullptr)
        InitGUI();

    // HACK button gets deleted when settings menu is closed
    if (gui->findWidget<MyGUI::Button>("ModSettingsOpen", false) == nullptr)
        InjectSettings();

    MyGUI::TextBox* debugOut = modMenuWindow->findWidget("DebugPrint")->castType<MyGUI::TextBox>();

    /*
    if (debugImgCanvas && debugImgCanvas->getInheritedVisible())
    {
        HeightmapHook::WriteBlockLODsToCanvas(debugImgCanvas);
    }
    */

    if (!hookedLoad)
    {
        MyGUI::WidgetPtr loadBtn = Kenshi::FindWidget(MyGUI::Gui::getInstancePtr()->getEnumerator(), "LoadButton");
        if (loadBtn)
        {
            loadBtn->eventMouseButtonClick += MyGUI::newDelegate(LoadStart);
            hookedLoad = true;
        }
    }

    MyGUI::WidgetPtr loadingPanel = Kenshi::FindWidget(MyGUI::Gui::getInstancePtr()->getEnumerator(), "LoadingPanel");
    if (loadingPanel && loadingPanel->getInheritedVisible() != oldLoadingPanelVisible)
    {
        oldLoadingPanelVisible = loadingPanel->getInheritedVisible();
        std::string msg = oldLoadingPanelVisible ? "true" : "false";
        DebugLog("Loading panel visibility changed to " + msg);
    }

    if (debugLogScrollView && debugLogScrollView->getInheritedVisible() && debugOut)
    {
        std::stringstream debugStr;
        debugStr << GetDebugLog();
        debugOut->setCaption(debugStr.str());

        // update scroll view size if needed
        int logSize = debugOut->getTextSize().height;
        if (debugLogScrollView->getCanvasSize().height < logSize)
        {
            debugLogScrollView->setCanvasSize(debugLogScrollView->getWidth(), logSize);
            debugLogScrollView->setVisibleHScroll(false);
            debugOut->setRealSize(1, 1);
        }
    }
}

void dllmain()
{
    Settings::Init();
    HeightmapHook::Preload();

    WaitForMainMenu();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    // GUI will be created next frame
    gui->eventFrameStart += MyGUI::newDelegate(GUIUpdate);

    HeightmapHook::Init();
    WaitForInGame();

    DebugLog("In-game.");

    //debugOut->setCaption(debugOut->getCaption() + "In-game.\n");

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
