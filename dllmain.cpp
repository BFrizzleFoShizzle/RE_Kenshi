// dllmain.cpp : Defines the entry point for the DLL application.
#include <windows.h>

#include "mygui/MyGUI_Gui.h"
#include "mygui/MyGUI_Button.h"
#include "mygui/MyGUI_Window.h"
#include "mygui/MyGUI_TextBox.h"
#include "mygui/MyGUI_EditBox.h"
#include "mygui/MyGUI_ImageBox.h"
#include "mygui/MyGUI_ScrollView.h"
#include "mygui/MyGUI_TabControl.h"
#include "mygui/MyGUI_TabItem.h"
#include "mygui/MyGUI_ScrollBar.h"

#include <iomanip>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"
#include "Kenshi/KeyBind.h"

#include "FSHook.h"
#include "HeightmapHook.h"
#include "MiscHooks.h"
#include "MyGUIHooks.h"
#include "Sound.h"
#include "Debug.h"
#include "Settings.h"
#include "Version.h"
#include "Bugs.h"
#include "Escort.h"
#include "ShaderCache.h"

#include <ogre/OgrePrerequisites.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreConfigFile.h>
#include "OISKeyboard.h"
#include "win32/Win32KeyBoard.h"

#include <boost/locale.hpp>

float gameSpeed = 1.0f;
MyGUI::TextBox* gameSpeedText = nullptr;

int gameSpeedIdx = 0;

// used to set highlighted game speed button
// IDX 1-4
void HighlightSpeedButton(int buttonIdx)
{
    // BUTTON HIGHLIGHT
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::ButtonPtr speedButton = nullptr;
    // button 1 (pause)
    MyGUI::WidgetPtr widget = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton1");
    if (widget)
        speedButton = widget->castType<MyGUI::Button>(false);
    if (speedButton)
        speedButton->setStateSelected(buttonIdx == 1);

    // button 2 (play)
    speedButton = nullptr;
    widget = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton2");
    if (widget)
        speedButton = widget->castType<MyGUI::Button>(false);
    if (speedButton)
        speedButton->setStateSelected(buttonIdx == 2);

    // button 3 (2x)
    speedButton = nullptr;
    widget = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton3");
    if (widget)
        speedButton = widget->castType<MyGUI::Button>(false);
    if (speedButton)
        speedButton->setStateSelected(buttonIdx == 3);

    // button 4 (5x)
    speedButton = nullptr;
    widget = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton4");
    if (widget)
        speedButton = widget->castType<MyGUI::Button>(false);
    if (speedButton)
        speedButton->setStateSelected(buttonIdx == 4);
}

// Game speed functions
// "play"
void SetSpeed1()
{
    if (!Settings::GetUseCustomGameSpeeds())
    {
        // vanilla - we use old functionality

        // TODO test keyboard bindings

        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        if(gameSpeedText)
            gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
    }
    else
    {
        std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
        // Clamp
        gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
        gameSpeedIdx = std::max(gameSpeedIdx, 0);

        // Kenshi will probably set game speed to 1, next time speed3 or speed4 buttons are clicked, will revert to modified game speed...
        // TODO how to handle this better?
        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        std::string gameSpeedMessage = std::to_string((long double)gameWorld.gameSpeed) + "x";
        if (gameWorld.gameSpeed != gameSpeedValues[gameSpeedIdx])
            gameSpeedMessage += " (" + std::to_string((long double)gameSpeedValues[gameSpeedIdx]) + "x)";
        //gameSpeedMessage += " (" + std::to_string((unsigned long long)fileCount) + ")";

        if (gameSpeedText)
            gameSpeedText->setCaption(gameSpeedMessage);
    }
}
// "2x speed" - decreases speed
void SetSpeed2()
{
    if (!Settings::GetUseCustomGameSpeeds())
    {
        // vanilla - reimplement
        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        gameWorld.gameSpeed = 2;

        HighlightSpeedButton(3);

        if (gameSpeedText)
            gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
    }
    else
    {
        std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
        gameSpeedIdx -= 1;
        // Clamp
        gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
        gameSpeedIdx = std::max(gameSpeedIdx, 0);
        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
        if (gameSpeedText)
            gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
    }
    
}
// "5x speed" - increases speed
void SetSpeed3()
{
    if (!Settings::GetUseCustomGameSpeeds())
    {
        // vanilla - reimplement
        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        gameWorld.gameSpeed = 5;

        HighlightSpeedButton(4);

        if (gameSpeedText)
            gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
    }
    else
    {
        std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
        gameSpeedIdx += 1;
        // Clamp
        gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
        gameSpeedIdx = std::max(gameSpeedIdx, 0);
        Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
        gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];

        if (gameSpeedText)
            gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
    }
}
// Used to re-overwrite Kenshi's value when required
void ForceWriteSpeed()
{
    Kenshi::GameWorld& gameWorld = Kenshi::GetGameWorld();
    if (Settings::GetUseCustomGameSpeeds())
    {
        std::vector<float> gameSpeedValues = Settings::GetGameSpeeds();
        // Clamp
        gameSpeedIdx = std::min(gameSpeedIdx, (int)gameSpeedValues.size() - 1);
        gameSpeedIdx = std::max(gameSpeedIdx, 0);
        gameWorld.gameSpeed = gameSpeedValues[gameSpeedIdx];
    }

    if (gameSpeedText)
        gameSpeedText->setCaption(std::to_string((long double)gameWorld.gameSpeed) + "x");
}

// reversed from game
Kenshi::KeyBind* FindBindingForKey(uint32_t keyCode, Kenshi::KeyBind* rootBind)
{
    Kenshi::KeyBind* high = rootBind;
    Kenshi::KeyBind* low = rootBind->ptr2;
    int i = 0;
    while (low->isLeaf == false)
    {
        if (low->keyCode < keyCode)
        {
            low = low->ptr3;
        }
        else
        {
            high = low;
            low = low->ptr1;
        }
        ++i;
        if (i > 100)
        {
            ErrorLog("Could not find key binding!");
            break;
        }
    }
    if (high->keyCode == keyCode)
    {
        return high;
    }
    else
    {
        return nullptr;
    }
}

std::string FindEventNameForKey(uint32_t keyCode, Kenshi::KeyBind* rootBind)
{
    Kenshi::KeyBind* keyBind = FindBindingForKey(keyCode, rootBind);
    if (keyBind != nullptr && keyBind->keyEvent != nullptr)
        return keyBind->keyEvent->eventName;
    return "ERROR";
}

uint32_t FindEventCodeForKey(uint32_t keyCode, Kenshi::KeyBind* rootBind)
{
    Kenshi::KeyBind* keyBind = FindBindingForKey(keyCode, rootBind);
    if (keyBind != nullptr && keyBind->keyEvent != nullptr)
        return keyBind->keyEvent->eventCode;
    return -1;
}

class KeyboardHook : public OIS::KeyListener
{
public:
    KeyboardHook(OIS::KeyListener* kenshiKeyListener)
        : kenshiKeyListener(kenshiKeyListener)
    {

    }
    ~KeyboardHook()
    {
        // TODO delete?
        kenshiKeyListener->~KeyListener();
    }
    bool keyPressed(const OIS::KeyEvent& arg) override
    {
        Kenshi::InputHandler& inputHandler = Kenshi::GetInputHandler();
        // Game speed hooks - runs after game listener so we can overwrite values
        std::string eventName = FindEventNameForKey(arg.key, inputHandler.keyBindingsStart);
        bool output = kenshiKeyListener->keyPressed(arg);
        // TODO pause?
        if (eventName == "speed_1")
        {
            SetSpeed1();
        }
        else if (eventName == "speed_2")
        {
            // decrement on press
            SetSpeed2();
        }
        else if (eventName == "speed_3")
        {
            // increment on press
            SetSpeed3();
        }
        return output;
    };
    bool keyReleased(const OIS::KeyEvent& arg) override
    {
        Kenshi::InputHandler& inputHandler = Kenshi::GetInputHandler();
        // Game speed hooks - runs after game listener so we can overwrite values
        std::string eventName = FindEventNameForKey(arg.key, inputHandler.keyBindingsStart);
        bool output = kenshiKeyListener->keyReleased(arg);
        // TODO pause?
        if (eventName == "speed_1")
        {
            SetSpeed1();
        }
        else if (eventName == "speed_2")
        {
            // update on release
            ForceWriteSpeed();
        }
        else if (eventName == "speed_3")
        {
            // update on release
            ForceWriteSpeed();
        }
        return output;
    };
private:
    // Games key listener
    OIS::KeyListener* kenshiKeyListener;
};

std::shared_ptr<KeyboardHook> keyboardHook;

void increaseSpeed(MyGUI::WidgetPtr _sender)
{
    SetSpeed3();
}

void decreaseSpeed(MyGUI::WidgetPtr _sender)
{
    SetSpeed2();
}

void playButtonHook(MyGUI::WidgetPtr _sender)
{
    SetSpeed1();
}

void WaitForInGame()
{
    MyGUI::Gui* gui = nullptr;
    while (gui == nullptr)
    {
        gui = MyGUI::Gui::getInstancePtr();
        Sleep(10);
    }
    MyGUI::WidgetPtr speedButtonsPanel = nullptr;
    while (speedButtonsPanel == nullptr)
    {
        speedButtonsPanel = Kenshi::FindWidget(gui->getEnumerator(), "SpeedButtonsPanel");
        Sleep(10);
    }
}

void WaitForMainMenu()
{
    MyGUI::Gui* gui = nullptr;
    while (gui == nullptr)
    {
        gui = MyGUI::Gui::getInstancePtr();
        Sleep(10);
    }
    MyGUI::WidgetPtr versionText = nullptr;
    while (versionText == nullptr)
    {
        versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText");
        Sleep(10);
    }
}

// TODO make nicer
MyGUI::Window* modMenuWindow = nullptr;
MyGUI::Window* bugReportWindow = nullptr;
MyGUI::Window* gameSpeedTutorialWindow = nullptr;

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
MyGUI::WidgetPtr gameSpeedPanel = nullptr;

const uint32_t DEBUG_WINDOW_WIDTH = 700;
const uint32_t DEBUG_WINDOW_HEIGHT = 600;
const uint32_t DEBUG_WINDOW_RIGHT = DEBUG_WINDOW_WIDTH - 30; // technically correct value would have VScrollBar width subtracted

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

void SpeedSliderTextChange(MyGUI::EditBox* editBox)
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
MyGUI::WidgetPtr CreateSlider(MyGUI::WidgetPtr parent, int x, int y, int w, int h, std::string namePrefix, bool deleteButton)
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
    int deleteSize = h - 10;
    // move widgets along if there's no delete button
    int shift = 0;
    if (deleteButton)
        MyGUI::ButtonPtr deleteButton = sliderRoot->createWidget<MyGUI::Button>("Kenshi_CloseButtonSkin", w - deleteSize, (h - deleteSize) / 2, deleteSize, deleteSize, MyGUI::Align::Right | MyGUI::Align::Top, namePrefix + "DeleteButton");
    else
        shift += deleteSize;
    MyGUI::TextBox* sliderLabel = sliderRoot->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 0, 0, shift + w * 0.3f, h, MyGUI::Align::Left | MyGUI::Align::VStretch, namePrefix + "ElementText");
    sliderLabel->setTextAlign(MyGUI::Align::Left);
    MyGUI::ScrollBar* scrollBar = sliderRoot->createWidget<MyGUI::ScrollBar>("Kenshi_ScrollBar", shift + w * 0.32f, 0, w * 0.48f, h, MyGUI::Align::Stretch, namePrefix + "Slider");
    MyGUI::EditBox* valueText = sliderRoot->createWidget<MyGUI::EditBox>("Kenshi_EditBox", shift + w * 0.82f, 0, w * 0.10f, h, MyGUI::Align::Right | MyGUI::Align::VStretch, namePrefix + "NumberText");

    return sliderRoot;
}

MyGUI::WidgetPtr CreateSlider(MyGUI::WidgetPtr parent, int x, int y, int w, int h, std::string namePrefix, bool deleteButton, std::string label, bool readOnlyText, std::string defaultValue, int defaultPosition, int scrollRange)
{
    MyGUI::WidgetPtr slider = CreateSlider(parent, x, y, w, h, namePrefix, deleteButton);
    MyGUI::TextBox* elementText = slider->findWidget(namePrefix + "ElementText")->castType<MyGUI::TextBox>();

    elementText->setCaption(label);
    MyGUI::EditBox* numberText = slider->findWidget(namePrefix + "NumberText")->castType<MyGUI::EditBox>();
    numberText->setEditStatic(readOnlyText);

    numberText->setCaption(defaultValue);
    MyGUI::ScrollBar* scrollBar = slider->findWidget(namePrefix + "Slider")->castType<MyGUI::ScrollBar>();
    scrollBar->setScrollRange(scrollRange);
    scrollBar->setScrollPosition(defaultPosition);
    
    return slider;
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
    if (scaled < 1000.0)
        str << std::setprecision(3);
    else
        str << std::setprecision(4);
    str << scaled;
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

    float scale = float(modMenuWindow->getClientCoord().height) / DEBUG_WINDOW_HEIGHT;

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
    int positionY = 35;
    for (int i = 0; i < gameSpeeds.size(); ++i)
    {
        std::stringstream nameStr;
        nameStr << "SpeedSlider" << i << "_";
        std::stringstream label;
        label << boost::locale::gettext("Speed ") << (i + 1) << ":";
        std::stringstream value;
        value << gameSpeeds[i];
        // 0...range-1 = 0...1000
        MyGUI::WidgetPtr slider = CreateSlider(gameSpeedPanel, 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 40 * scale, nameStr.str(), true, label.str(),
            false, value.str(), UnscaleGameSpeed(gameSpeeds[i]), 1001);
        MyGUI::ScrollBar* scrollBar = slider->findWidget(nameStr.str() + "Slider")->castType<MyGUI::ScrollBar>();
        if (!scrollBar)
            ErrorLog("ScrollBar not found!");
        // add event listeners
        scrollBar->eventScrollChangePosition += MyGUI::newDelegate(GameSpeedScroll);
        MyGUI::EditBox* valueText = slider->findWidget(nameStr.str() + "NumberText")->castType<MyGUI::EditBox>();
        valueText->eventEditTextChange += MyGUI::newDelegate(SpeedSliderTextChange);
        MyGUI::ButtonPtr deleteButton = slider->findWidget(nameStr.str() + "DeleteButton")->castType<MyGUI::Button>();
        deleteButton->eventMouseButtonClick += MyGUI::newDelegate(DeleteGameSpeedScroll);
        positionY += 45;
    }

    // resize settings
    MyGUI::IntSize canvasSize = settingsView->getCanvasSize();
    gameSpeedPanel->setSize(gameSpeedPanel->getWidth(), positionY * scale);
    settingsView->setCanvasSize(canvasSize.width, std::max(int((gameSpeedScrollBarsStart + positionY) * scale), canvasSize.height));
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

// Takes into account mods
int defaultMaxFactionSize = -1;

void MaxFactionSizeScroll(MyGUI::ScrollBar* scrollBar, size_t newPos)
{
    // Update number text
    MyGUI::EditBox* numberText = scrollBar->getParent()->findWidget("MaxFactionSizeSlider_NumberText")->castType<MyGUI::EditBox>();

    if (newPos > 0)
    {
        std::stringstream str;
        str << newPos;
        numberText->setCaption(str.str());
        Settings::SetMaxFactionSize(newPos);
        // TODO this sometimes causes a crash if Kenshi is unpaused
        Kenshi::GetMaxFactionSize() = newPos;
    }
    else
    {
        std::stringstream str;
        str << "(" << defaultMaxFactionSize << ")";
        numberText->setCaption(str.str());
        Settings::SetMaxFactionSize(-1);
        Kenshi::GetMaxFactionSize() = defaultMaxFactionSize;
    }
}

void MaxFactionSizeSliderTextChange(MyGUI::EditBox* editBox)
{
    MyGUI::ScrollBar* scrollBar = editBox->getParent()->findWidget("MaxFactionSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
    std::string valueStr = editBox->getCaption();
    int value = -1;
    // parse float
    std::stringstream str(valueStr);
    str >> value;
    // bounds check - ignore if invalid
    if (value < 0)
        return;
    scrollBar->setScrollPosition(value + 1);
    MaxFactionSizeScroll(scrollBar, value + 1);
}

// Takes into account mods
int defaultMaxSquadSize = -1;

void MaxSquadSizeScroll(MyGUI::ScrollBar* scrollBar, size_t newPos)
{
    // Update number text
    MyGUI::EditBox* numberText = scrollBar->getParent()->findWidget("MaxSquadSizeSlider_NumberText")->castType<MyGUI::EditBox>();

    if (newPos > 0)
    {
        std::stringstream str;
        str << newPos;
        numberText->setCaption(str.str());
        Settings::SetMaxSquadSize(newPos);
        // TODO this sometimes causes a crash if Kenshi is unpaused
        Kenshi::GetMaxSquadSize() = newPos;
    }
    else
    {
        std::stringstream str;
        str << "(" << defaultMaxSquadSize << ")";
        numberText->setCaption(str.str());
        Settings::SetMaxSquadSize(-1);
        Kenshi::GetMaxSquadSize() = defaultMaxSquadSize;
    }
}

void MaxSquadSizeSliderTextChange(MyGUI::EditBox* editBox)
{
    MyGUI::ScrollBar* scrollBar = editBox->getParent()->findWidget("MaxSquadSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
    std::string valueStr = editBox->getCaption();
    int value = -1;
    // parse float
    std::stringstream str(valueStr);
    str >> value;
    // bounds check - ignore if invalid
    if (value < 0)
        return;
    scrollBar->setScrollPosition(value + 1);
    MaxSquadSizeScroll(scrollBar, value + 1);
}

// Takes into account mods
int defaultMaxSquads = -1;

void MaxSquadsScroll(MyGUI::ScrollBar* scrollBar, size_t newPos)
{
    // Update number text
    MyGUI::EditBox* numberText = scrollBar->getParent()->findWidget("MaxSquadsSlider_NumberText")->castType<MyGUI::EditBox>();

    if (newPos > 0)
    {
        std::stringstream str;
        str << newPos;
        numberText->setCaption(str.str());
        Settings::SetMaxSquads(newPos);
        // TODO this sometimes causes a crash if Kenshi is unpaused
        Kenshi::GetMaxSquads() = newPos;
    }
    else
    {
        std::stringstream str;
        str << "(" << defaultMaxSquads << ")";
        numberText->setCaption(str.str());
        Settings::SetMaxSquads(-1);
        Kenshi::GetMaxSquads() = defaultMaxSquads;
    }
}

void MaxSquadsSliderTextChange(MyGUI::EditBox* editBox)
{
    MyGUI::ScrollBar* scrollBar = editBox->getParent()->findWidget("MaxSquadsSlider_Slider")->castType<MyGUI::ScrollBar>();
    std::string valueStr = editBox->getCaption();
    int value = -1;
    // parse float
    std::stringstream str(valueStr);
    str >> value;
    // bounds check - ignore if invalid
    if (value < 0)
        return;
    scrollBar->setScrollPosition(value + 1);
    MaxSquadsScroll(scrollBar, value + 1);
}

void ToggleFixRNG(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool fixRNG = button->getStateSelected();

    // Update settings + hooks
    MiscHooks::SetFixRNG(fixRNG);
}

void ToggleIncreaseMaxCameraDistance(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool increaseMaxCameraDistance = button->getStateSelected();

    if (increaseMaxCameraDistance)
        MiscHooks::SetMaxCameraDistance(4000.0f);
    else
        MiscHooks::SetMaxCameraDistance(2000.0f);

    Settings::SetIncreaseMaxCameraDistance(increaseMaxCameraDistance);
}

void ToggleCacheShaders(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool cacheShaders = button->getStateSelected();

    // Update settings + hooks
    Settings::SetCacheShaders(cacheShaders);
}

void ToggleLogFileIO(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool logFileIO = button->getStateSelected();

    // Update settings + hooks
    Settings::SetLogFileIO(logFileIO);
}

void ToggleLogAudio(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool logAudio = button->getStateSelected();

    // Update settings + hooks
    Settings::SetLogAudio(logAudio);
}

void ToggleCheckUpdates(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool checkUpdates = button->getStateSelected();

    // Update settings + hooks
    Settings::SetCheckUpdates(checkUpdates);
}

void ToggleOpenSettingsOnStart(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool openSettingsOnStart = button->getStateSelected();

    // Update settings + hooks
    Settings::SetOpenSettingsOnStart(openSettingsOnStart);
}

void ToggleUseCustomGameSpeeds(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool useCustomGameSpeeds = button->getStateSelected();

    gameSpeedPanel->setVisible(useCustomGameSpeeds);
    gameSpeedTutorialWindow->setVisible(useCustomGameSpeeds);

    // Update settings + hooks
    Settings::SetUseCustomGameSpeeds(useCustomGameSpeeds);

    // hit play to switch to 1x speed
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    MyGUI::WidgetPtr speedButton2 = Kenshi::FindWidget(gui->getEnumerator(), "TimeSpeedButton2");// ->castType<MyGUI::Button>();
    if (speedButton2) 
    {
        speedButton2->eventMouseButtonClick(speedButton2);
    }
}

void ReportBugPress(MyGUI::WidgetPtr sender)
{
    MyGUI::EditBox* description = bugReportWindow->findWidget("BugDescription")->castType<MyGUI::EditBox>(false);
    MyGUI::EditBox* infoText = bugReportWindow->findWidget("BugReportInfo")->castType<MyGUI::EditBox>(false);
    // bootleg way of expanding bugDescription upwards to end of infoText - this solves font size issues
    int descriptionBottom = description->getBottom();
    description->setPosition(infoText->getLeft(), infoText->getTop() + infoText->getTextSize().height + 8);
    description->setSize(infoText->getSize().width, descriptionBottom - description->getTop());

    bugReportWindow->setVisible(true);
}

void SendBugPress(MyGUI::WidgetPtr sender)
{
    MyGUI::EditBox* description = bugReportWindow->findWidget("BugDescription")->castType<MyGUI::EditBox>(false);
    Bugs::ReportUserBug(description->getCaption());
    description->setCaption("Report sent.");
    bugReportWindow->setVisible(false);
}

void InitGUI()
{
    DebugLog("Main menu loaded.");

    // can't seem to find where the language is kept in memory...
    // I've found the std::locale - but it doesn't work for some reason?
    // Also found the boost::locale::generator but that isn't really useful
    // unless you know the locale string... and std::locale::name/c_str 
    // returns "*". Also, generator::generate seems to be called before 
    // RE_Kenshi is loaded, so we can't hook that either...
    // So we pick up langauge settings from the config file after
    // the game is started
    Ogre::ConfigFile config;
    config.load("settings.cfg"); 
    std::string language = config.getSetting("language");

    boost::locale::generator gen;
    gen.add_messages_path("RE_Kenshi/locale");
    gen.add_messages_domain("re_kenshi");
    // extend Kenshi's existing locale with our own
    std::locale::global(gen.generate(std::locale(), language + ".UTF-8"));
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    Ogre::ResourceGroupManager* resMan = Ogre::ResourceGroupManager::getSingletonPtr();
    if (resMan)
        resMan->addResourceLocation("./RE_Kenshi", "FileSystem", "GUI");

    // Create mod menu
    float windowWidth = DEBUG_WINDOW_WIDTH / 1920.0f;
    float windowHeight = DEBUG_WINDOW_HEIGHT / 1080.0f;
    modMenuWindow = gui->createWidgetReal<MyGUI::Window>("Kenshi_WindowCX", 0.1f, 0.1f, windowWidth, windowHeight, MyGUI::Align::Center, "Window", "DebugWindow");
    // fix aspect ratio
    float initScale = float(modMenuWindow->getCoord().height) / DEBUG_WINDOW_HEIGHT;
    float horizontalScale = float(modMenuWindow->getCoord().width) / DEBUG_WINDOW_WIDTH;
    modMenuWindow->setCoord(((1920 * horizontalScale) - (DEBUG_WINDOW_WIDTH * initScale)) - 100, 100, DEBUG_WINDOW_WIDTH * initScale, modMenuWindow->getCoord().height);
    float scale = float(modMenuWindow->getClientCoord().height) / DEBUG_WINDOW_HEIGHT;
    modMenuWindow->setCaption(boost::locale::gettext("RE_Kenshi Menu"));
    modMenuWindow->eventKeyButtonReleased += MyGUI::newDelegate(debugMenuKeyRelease);
    modMenuWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    if (!Settings::GetOpenSettingsOnStart())
        modMenuWindow->setVisible(false);
    MyGUI::Widget* client = modMenuWindow->findWidget("Client");
    MyGUI::TabControl *tabControl = client->createWidget<MyGUI::TabControl>("Kenshi_TabControl", MyGUI::IntCoord(2, 2, modMenuWindow->getClientCoord().width - 4, modMenuWindow->getClientCoord().height - 4), MyGUI::Align::Stretch);
    
    // Create game speed tutorial window
    gameSpeedTutorialWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, 620 * scale, 460 * scale, MyGUI::Align::Center, "Window", "GameSpeedTutorialWindow");
    gameSpeedTutorialWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    // don't know why this isn't centering properly...
    MyGUI::ImageBox* gameSpeedTutImage = gameSpeedTutorialWindow->getClientWidget()->createWidget<MyGUI::ImageBox>("ImageBox", 10 * scale, 10 * scale, 600 * scale, 400 * scale, MyGUI::Align::Center, "GameSpeedTutorialImage");
    gameSpeedTutImage->setImageTexture("game_speed_tutorial.png");
    gameSpeedTutorialWindow->setVisible(false);

    // Create bug report window
    bugReportWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, 500 * scale, 500 * scale, MyGUI::Align::Center, "Window", "BugReportWindow");
    bugReportWindow->setCaption(boost::locale::gettext("RE_Kenshi Bug Report"));
    bugReportWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress); 
    MyGUI::WidgetPtr bugReportPanel = bugReportWindow->getClientWidget()->createWidgetReal<MyGUI::Widget>("Kenshi_FloatingPanelLight", 0.0f, 0.0f, 1.0f, 1.0f, MyGUI::Align::Stretch, "BugReportPanel");
    // Only edit boxes support word wrap?
    MyGUI::EditBox* infoText = bugReportPanel->createWidgetReal<MyGUI::EditBox>("Kenshi_TextboxStandardText", 0.05f, 0.05f, 0.90f, 0.48f, MyGUI::Align::Top | MyGUI::Align::Left, "BugReportInfo");
    infoText->setEditMultiLine(true);
    infoText->setEditWordWrap(true);
    infoText->setEditStatic(true);
    infoText->setCaption(MyGUI::UString(boost::locale::gettext("Your report will be sent to RE_Kenshi's developer (BFrizzleFoShizzle) with the following information:"))
        + boost::locale::gettext("\nRE_Kenshi version: ") + Version::GetDisplayVersion()
        + boost::locale::gettext("\nKenshi version: ") + Kenshi::GetKenshiVersion().ToString()
        + boost::locale::gettext("\nYour bug description")
        + boost::locale::gettext("\n\nPlease describe the bug:"));
    
    MyGUI::EditBox* bugDescription = bugReportPanel->createWidgetReal<MyGUI::EditBox>("Kenshi_WordWrap", 0.05f, 0.50f, 0.90f, 0.33f, MyGUI::Align::Top | MyGUI::Align::Left, "BugDescription");
    bugDescription->setEditStatic(false);
    MyGUI::ButtonPtr sendBugButton = bugReportPanel->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.05f, 0.85f, 0.90f, 0.10f, MyGUI::Align::Top | MyGUI::Align::Left, "SendReportButton");
    sendBugButton->setCaption(boost::locale::gettext("Send report"));
    sendBugButton->eventMouseButtonClick += MyGUI::newDelegate(SendBugPress);
    bugReportWindow->setVisible(false);

    // Mod settings
    MyGUI::TabItemPtr settingsTab = tabControl->addItem(boost::locale::gettext("Settings"));
    settingsView = settingsTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollView", MyGUI::IntCoord(2, 2, settingsTab->getClientCoord().width - 4, settingsTab->getClientCoord().height - 4), MyGUI::Align::Stretch);
    settingsView->setVisibleHScroll(false);
    int positionY = 2;
    settingsView->setCanvasSize(settingsView->getWidth(), settingsView->getHeight());

    MyGUI::ButtonPtr reportBugButton = settingsView->createWidget<MyGUI::Button>("Kenshi_Button1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "ReportBugButton");
    reportBugButton->setCaption(boost::locale::gettext("Report bug"));
    reportBugButton->eventMouseButtonClick += MyGUI::newDelegate(ReportBugPress);
    positionY += 40;

    MyGUI::ButtonPtr checkUpdatesToggle = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "CheckUpdates");
    checkUpdatesToggle->setStateSelected(Settings::GetCheckUpdates());
    checkUpdatesToggle->setCaption(boost::locale::gettext("Automatically check for updates"));
    checkUpdatesToggle->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    checkUpdatesToggle->eventMouseButtonClick += MyGUI::newDelegate(ToggleCheckUpdates);
    positionY += 30;

    MyGUI::ButtonPtr openSettingsOnStartToggle = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "OpenSettingsOnStart");
    openSettingsOnStartToggle->setStateSelected(Settings::GetOpenSettingsOnStart());
    openSettingsOnStartToggle->setCaption(boost::locale::gettext("Open RE_Kenshi settings on startup"));
    openSettingsOnStartToggle->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    openSettingsOnStartToggle->eventMouseButtonClick += MyGUI::newDelegate(ToggleOpenSettingsOnStart);
    positionY += 30;

    MyGUI::ButtonPtr fixRNGToggle = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "FixRNGToggle");
    fixRNGToggle->setStateSelected(Settings::GetFixRNG());
    fixRNGToggle->setCaption(boost::locale::gettext("Fix Kenshi's RNG bug"));
    fixRNGToggle->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    fixRNGToggle->eventMouseButtonClick += MyGUI::newDelegate(ToggleFixRNG);
    positionY += 30;

    MyGUI::ButtonPtr increaseMaxCameraDistance = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "IncreaseMaxCameraDistance");
    increaseMaxCameraDistance->setStateSelected(Settings::GetIncreaseMaxCameraDistance());
    increaseMaxCameraDistance->setCaption(boost::locale::gettext("Increase max camera distance"));
    increaseMaxCameraDistance->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    increaseMaxCameraDistance->eventMouseButtonClick += MyGUI::newDelegate(ToggleIncreaseMaxCameraDistance);
    positionY += 30;

    MyGUI::ButtonPtr cacheShaders = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "CacheShadersToggle");
    cacheShaders->setStateSelected(Settings::GetCacheShaders());
    cacheShaders->setCaption(boost::locale::gettext("Cache shaders"));
    cacheShaders->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    cacheShaders->eventMouseButtonClick += MyGUI::newDelegate(ToggleCacheShaders);
    positionY += 30;

    MyGUI::ButtonPtr logFileIO = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "LogFileIO");
    logFileIO->setStateSelected(Settings::GetLogFileIO());
    logFileIO->setCaption(boost::locale::gettext("Log file IO"));
    logFileIO->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    logFileIO->eventMouseButtonClick += MyGUI::newDelegate(ToggleLogFileIO);
    positionY += 30;

    MyGUI::ButtonPtr logAudio = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "LogAudio");
    logAudio->setStateSelected(Settings::GetLogAudio());
    logAudio->setCaption(boost::locale::gettext("Log audio IDs/events/switches/states"));
    logAudio->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    logAudio->eventMouseButtonClick += MyGUI::newDelegate(ToggleLogAudio);
    positionY += 30;

    if (!HeightmapHook::CompressedHeightmapFileExists())
    {
        MyGUI::TextBox* noCompressedHeightmapLabel = settingsView->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 30 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "NoCompressedHeightmapLabel");
        noCompressedHeightmapLabel->setCaption(boost::locale::gettext("To enable, reinstall RE_Kenshi and check \"Compress Heightmap\""));
        positionY += 30;
    }
    MyGUI::ButtonPtr useCompressedHeightmap = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "UseCompressedHeightmapToggle");
    useCompressedHeightmap->setStateSelected(Settings::UseHeightmapCompression());
    useCompressedHeightmap->setCaption(boost::locale::gettext("[TOGGLE MAY CRASH] Use compressed heightmap"));
    useCompressedHeightmap->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    useCompressedHeightmap->eventMouseButtonClick += MyGUI::newDelegate(ToggleUseCompressedHeightmap);
    // Disable if the file doesn't exist
    if (!HeightmapHook::CompressedHeightmapFileExists())
    {
        useCompressedHeightmap->setEnabled(false);
    }
    positionY += 30;
    // Attack slots
    defaultAttackSlots = Kenshi::GetNumAttackSlots();
    int numAttackSlots = Settings::GetAttackSlots();
    // Apply settings
    if (numAttackSlots > 0)
        Kenshi::GetNumAttackSlots() = numAttackSlots;
    std::stringstream attackSlotsValue;
    if (numAttackSlots > 0)
    {
        attackSlotsValue << numAttackSlots;
    }
    else
    {
        attackSlotsValue << "(" << defaultAttackSlots << ")";
        numAttackSlots = 0;
    }
    std::string attackSlotsLabel = boost::locale::gettext("Attack slots (") + std::to_string((long long)defaultAttackSlots) + "):";
    MyGUI::WidgetPtr attackSlotsSlider = CreateSlider(settingsView, 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 40 * scale, "AttackSlotsSlider_", false,
        attackSlotsLabel, true, attackSlotsValue.str(), numAttackSlots, 6);
    MyGUI::ScrollBar* attackSlotsScrollBar = attackSlotsSlider->findWidget("AttackSlotsSlider_Slider")->castType<MyGUI::ScrollBar>();
    attackSlotsScrollBar->eventScrollChangePosition += MyGUI::newDelegate(AttackSlotScroll);
    positionY += 45;

    // max faction size
    defaultMaxFactionSize = Kenshi::GetMaxFactionSize();
    int maxFactionSize = Settings::GetMaxFactionSize();
    // Apply settings
    if (maxFactionSize > 0)
        Kenshi::GetMaxFactionSize() = maxFactionSize;
    std::stringstream maxFactionSizeValue;
    if (maxFactionSize > 0)
    {
        maxFactionSizeValue << maxFactionSize;
    }
    else
    {
        maxFactionSizeValue << "(" << defaultMaxFactionSize << ")";
        maxFactionSize = 0;
    }
    std::string maxFactionSizeLabel = boost::locale::gettext("Max. faction size (") + std::to_string((long long)defaultMaxFactionSize) + "):";
    MyGUI::WidgetPtr maxFactionSizeSlider = CreateSlider(settingsView, 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 40 * scale, "MaxFactionSizeSlider_", false,
        maxFactionSizeLabel, false, maxFactionSizeValue.str(), maxFactionSize, 1001);
    MyGUI::ScrollBar* maxFactionSizeScrollBar = maxFactionSizeSlider->findWidget("MaxFactionSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
    maxFactionSizeScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxFactionSizeScroll);
    MyGUI::EditBox* valueText = maxFactionSizeSlider->findWidget("MaxFactionSizeSlider_NumberText")->castType<MyGUI::EditBox>();
    valueText->eventEditTextChange += MyGUI::newDelegate(MaxFactionSizeSliderTextChange);
    positionY += 45;

    // max squad size
    defaultMaxSquadSize = Kenshi::GetMaxSquadSize();
    int maxSquadSize = Settings::GetMaxSquadSize();
    // Apply settings
    if (maxSquadSize > 0)
        Kenshi::GetMaxSquadSize() = maxSquadSize;
    std::stringstream maxSquadSizeValue;
    if (maxSquadSize > 0)
    {
        maxSquadSizeValue << maxSquadSize;
    }
    else
    {
        maxSquadSizeValue << "(" << defaultMaxSquadSize << ")";
        maxSquadSize = 0;
    }
    std::string maxSquadSizeLabel = boost::locale::gettext("Max. squad size (") + std::to_string((long long)defaultMaxSquadSize) + "):";
    MyGUI::WidgetPtr maxSquadSizeSlider = CreateSlider(settingsView, 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 40 * scale, "MaxSquadSizeSlider_", false,
        maxSquadSizeLabel, false, maxSquadSizeValue.str(), maxSquadSize, 1001);
    MyGUI::ScrollBar* maxSquadSizeScrollBar = maxSquadSizeSlider->findWidget("MaxSquadSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
    maxSquadSizeScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxSquadSizeScroll);
    valueText = maxSquadSizeSlider->findWidget("MaxSquadSizeSlider_NumberText")->castType<MyGUI::EditBox>();
    valueText->eventEditTextChange += MyGUI::newDelegate(MaxSquadSizeSliderTextChange);
    positionY += 45;

    // max squads
    defaultMaxSquads= Kenshi::GetMaxSquads();
    int maxSquads = Settings::GetMaxSquads();
    // Apply settings
    if (maxSquads > 0)
        Kenshi::GetMaxSquads() = maxSquads;
    std::stringstream maxSquadsValue;
    if (maxSquads > 0)
    {
        maxSquadsValue << maxSquads;
    }
    else
    {
        maxSquadsValue << "(" << defaultMaxSquads << ")";
        maxSquads = 0;
    }
    std::string maxSquadsLabel = boost::locale::gettext("Max. squads (") + std::to_string((long long)defaultMaxSquads) + "):";
    MyGUI::WidgetPtr maxSquadsSlider = CreateSlider(settingsView, 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 40 * scale, "MaxSquadsSlider_", false,
        maxSquadsLabel, false, maxSquadsValue.str(), maxSquads, 1001);
    MyGUI::ScrollBar* maxSquadsScrollBar = maxSquadsSlider->findWidget("MaxSquadsSlider_Slider")->castType<MyGUI::ScrollBar>();
    maxSquadsScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxSquadsScroll);
    valueText = maxSquadsSlider->findWidget("MaxSquadsSlider_NumberText")->castType<MyGUI::EditBox>();
    valueText->eventEditTextChange += MyGUI::newDelegate(MaxSquadsSliderTextChange);
    positionY += 45;

    MyGUI::ButtonPtr useCustomGameSpeeds = settingsView->createWidget<MyGUI::Button>("Kenshi_TickButton1", 2, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "UseCustomGameSpeeds");
    useCustomGameSpeeds->setStateSelected(Settings::GetUseCustomGameSpeeds());
    useCustomGameSpeeds->setCaption(boost::locale::gettext("Use custom game speed controls"));
    useCustomGameSpeeds->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    useCustomGameSpeeds->eventMouseButtonClick += MyGUI::newDelegate(ToggleUseCustomGameSpeeds);
    positionY += 30;
    
    gameSpeedPanel = settingsView->createWidget<MyGUI::Widget>("", 0, positionY * scale, DEBUG_WINDOW_RIGHT * scale, 100, MyGUI::Align::Top | MyGUI::Align::Left, "GameSpeedPanel");
    gameSpeedPanel->setVisible(Settings::GetUseCustomGameSpeeds());
    
    MyGUI::TextBox* gameSpeedsLabel = gameSpeedPanel->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 2, 0, DEBUG_WINDOW_RIGHT * scale, 30 * scale, MyGUI::Align::Top | MyGUI::Align::Center, "GameSpeedsLabel");
    gameSpeedsLabel->setCaption(boost::locale::gettext("Game speeds"));
    MyGUI::ButtonPtr addGameSpeed = gameSpeedPanel->createWidget<MyGUI::Button>("Kenshi_Button1", 250 * scale, 0, 300 * scale, 30 * scale, MyGUI::Align::Top | MyGUI::Align::Right, "AddGameSpeedBtn");
    addGameSpeed->setCaption(boost::locale::gettext("Add game speed"));
    addGameSpeed->eventMouseButtonClick += MyGUI::newDelegate(AddGameSpeed);
    gameSpeedScrollBarsStart = positionY;
    RedrawGameSpeedSettings();
    
    // Debug log
    MyGUI::TabItemPtr debugLogTab = tabControl->addItem(boost::locale::gettext("Debug log"));
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
        versionText->setCaption("RE_Kenshi " + Version::GetDisplayVersion() + " - " + version);

        // Fixes fonts
        // TODO remove after dropping support for old versions
        MyGUIHooks::InitMainMenu();
    }
    else
    {
        DebugLog("ERROR: Game version not recognized.");
        DebugLog("");
        DebugLog("Supported versions:");
        DebugLog("GOG 1.0.59");
        DebugLog("Steam 1.0.55, 1.0.62, 1.0.64");
        DebugLog("RE_Kenshi initialization aborted!");

        versionText->setCaption("RE_Kenshi " + Version::GetDisplayVersion() + " (ERROR) - " + version);
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
    // this call can return nullptr so need an explicit check before the cast
    MyGUI::Widget* optionsTabWidget = Kenshi::FindWidget(gui->getEnumerator(), "OptionsTab");
    if (optionsTabWidget == nullptr)
        return;

    MyGUI::TabControl* optionsTabCtrl = optionsTabWidget->castType<MyGUI::TabControl>(false);
    if (optionsTabCtrl == nullptr)
        return;

    if (optionsTabCtrl->getItemCount() <= 0)
        return;

    MyGUI::TabItemPtr modsTab = optionsTabCtrl->getItemAt(optionsTabCtrl->getItemCount() - 1);
    if (modsTab == nullptr)
        return;

    // This causes crashes when the options menu is closed
    //optionsTabCtrl->addItem("RE_KENSHI");

    MyGUI::ButtonPtr settingsButton = modsTab->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.7f, 0.02f, 0.25f, 0.05f, MyGUI::Align::Top | MyGUI::Align::Left, "ModSettingsOpen");
    settingsButton->setCaption(boost::locale::gettext("RE_Kenshi settings"));
    settingsButton->eventMouseButtonClick += MyGUI::newDelegate(openSettingsMenu);
    DebugLog("Injected settings button");
}

void ReHookTimeButtons()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    try
    {
        MyGUI::WidgetPtr timeMoneyPanel = Kenshi::FindWidget(gui->getEnumerator(), "TimeMoneyPanel");

        // When the game recreates it's GUI, gameSpeedText gets totally messed up
        // various calls on gameSpeedText succeed with sane values
        // re-adding gameSpeedText to new nodes throws an exception
        // gui->destroyWidget(gameSpeedText) throws an error
        // as best I can tell, the game deallocates this without telling us, but not 100% sure
        // so, I'm not going to deallocate it here as that may or may not lead to a double-free
        // and an occasional memory leak is less bad than a double-free
        
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

void GUIUpdate(float timeDelta)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    if (modMenuWindow == nullptr)
        InitGUI();

    // HACK sometimes Kenshi re-generates it's UI and we have to re-inject our elements
    // a nicer method would be to call "gameSpeedText->getParent() == nullptr", but
    // I have a hunch Kenshi may have actually freed our gameSpeedText object,
    // which would make that dangerous.
    // C++03 is awful. Shared pointers would completely solve this problem.
    MyGUI::WidgetPtr timeMoneyPanel = Kenshi::FindWidget(gui->getEnumerator(), "TimeMoneyPanel");
    if (timeMoneyPanel != nullptr)
    {
        if (timeMoneyPanel->findWidget("GameSpeedText") == nullptr)
        {
            DebugLog("UI borked, re-generating...");
            ReHookTimeButtons();
        }
    }

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

// I haven't reverse-engineered this function, it probably does more than just load mods
// but we hook it for sync'ing with the mod loader
void (*LoadMods_orig)(Kenshi::GameWorld* gameWorld);
bool setupMods = false;
void LoadMods_hook(Kenshi::GameWorld* gameWorld)
{
    LoadMods_orig(gameWorld);

    if (!setupMods)
    {
        DebugLog("Load mod hook");
        // Must be called AFTER mod load
        Settings::LoadModOverrides();
        Sound::TryLoadQueuedBanks();
        setupMods = true;
    }

    return;
}


void dllmain()
{
    DebugLog("RE_Kenshi " + Version::GetDisplayVersion());

    Kenshi::BinaryVersion gameVersion = Kenshi::GetKenshiVersion();

    // TODO refactor these branches
    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        // hook for loading mod config - has to be done early so we can override certain early I/O operations
        LoadMods_orig = Escort::JmpReplaceHook<void(Kenshi::GameWorld*)>(Kenshi::GetModLoadFunction(), LoadMods_hook, 6);

        FSHook::Init();
    }
    else
    {
        DebugLog("Error: Unsupported Kenshi version. Hooks disabled.");
    }
    
    Settings::Init();
    Version::Init();

    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        Bugs::Init();
        ShaderCache::Init();
        HeightmapHook::Preload();
        MiscHooks::Init();
        Sound::Init();
    }

    DebugLog("Waiting for main menu.");
    WaitForMainMenu();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    
    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        // GUI will be created next frame
        gui->eventFrameStart += MyGUI::newDelegate(GUIUpdate);

        HeightmapHook::Init();

        WaitForInGame();

        DebugLog("In-game.");

        ReHookTimeButtons();

        // Keyboard hooks
        if (!keyboardHook)
        {
            Kenshi::InputHandler& inputHandler = Kenshi::GetInputHandler();
            keyboardHook = std::make_shared<KeyboardHook>(inputHandler.keyboardInput->getEventCallback());
            inputHandler.keyboardInput->setEventCallback(keyboardHook.get());
        }
    }
    else
    {
        // TODO refactor this so version text only gets set in one place
        // display error in version text
        // this *probably* won't crash new versions
        MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
        if (versionText != nullptr)
        {
            MyGUI::UString version = versionText->getCaption();
            versionText->setCaption("RE_Kenshi " + Version::GetDisplayVersion() + " (ERROR) - " + version + boost::locale::gettext(" - [NOT SUPPORTED, MOD DISABLED]"));
        }       
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
