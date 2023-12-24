// dllmain.cpp : Defines the entry point for the DLL application.
#define WIN32_LEAN_AND_MEAN
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
#include <mygui/MyGUI_ComboBox.h>
#include <mygui/MyGUI_ResourceTrueTypeFont.h>
#include <mygui/MyGUI_FactoryManager.h>
#include <mygui/MyGUI_FontManager.h>

#include <iomanip>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"
#include "Kenshi/KeyBind.h"

#include "FSHook.h"
#include "HeightmapHook.h"
#include "MiscHooks.h"
#include "MyGUIHooks.h"
#include "Sound.h"
#include "io.h"
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
#include <boost/thread/condition_variable.hpp>

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
MyGUI::ButtonPtr sendUUIDToggle = nullptr;

const uint32_t DEBUG_WINDOW_WIDTH = 500;
const uint32_t DEBUG_WINDOW_HEIGHT = 450;

void TickButtonBehaviourClick(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    if(button)
        button->setStateSelected(!button->getStateSelected());
}

void ChangeHeightmapMode(MyGUI::ComboBox* sender, size_t index)
{
    HeightmapHook::HeightmapMode newMode = *sender->getItemDataAt<HeightmapHook::HeightmapMode>(index);

    // compressed heightmap selected but doesn't exist - ignore change
    if (newMode == HeightmapHook::COMPRESSED && !HeightmapHook::CompressedHeightmapFileExists())
    {
        DebugLog("Invalid heightmap mode selected");
        sender->setIndexSelected(Settings::GetHeightmapMode());
        return;
    }

    Settings::SetHeightmapMode(newMode);
    HeightmapHook::UpdateHeightmapSettings();
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

enum SliderButton
{
    CLOSE,
    CHECK,
    NONE
};

void EnableSliderToggle(MyGUI::WidgetPtr sender)
{
    std::string prefix = sender->getName().substr(0, sender->getName().size() - strlen("EnableButton"));
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    MyGUI::WidgetPtr sliderPanel = sender->getParent()->findWidget(prefix + "ScalableSliderPanel");
    if (sliderPanel && button)
    {
        if (button->getStateSelected())
        {
            sliderPanel->setAlpha(1.0f);
            sliderPanel->setEnabled(true);
        }
        else
        {
            sliderPanel->setAlpha(0.5f);
            sliderPanel->setEnabled(false);
        }
    }
    else
    {
        if (!button)
            ErrorLog("Not button");
        if (!sliderPanel)
            ErrorLog("Can't find panel");
    }
}
// Root widget name will be "[namePrefix]SliderRoot"
MyGUI::WidgetPtr CreateSlider(MyGUI::WidgetPtr parent, int x, int y, int w, int h, std::string namePrefix, SliderButton buttonType)
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

    MyGUI::WidgetPtr sliderRoot = parent->createWidget<MyGUI::Widget>("PanelEmpty", x, y, w, h, MyGUI::Align::Top | MyGUI::Align::HStretch, namePrefix + "SliderRoot");
    int endButtonSize = h - 5;
    // move widgets along if there's no delete button
    int shift = 0;
    if (buttonType == CLOSE)
    {
        MyGUI::ButtonPtr closeButton = sliderRoot->createWidget<MyGUI::Button>("Kenshi_CloseButtonSkin", w - endButtonSize, (h - endButtonSize) / 2, endButtonSize, endButtonSize, MyGUI::Align::Right | MyGUI::Align::Center, namePrefix + "DeleteButton");
    }
    else if (buttonType == CHECK)
    {
        MyGUI::ButtonPtr checkButton = sliderRoot->createWidget<MyGUI::Button>("Kenshi_TickBoxSkin", w - endButtonSize, (h - endButtonSize) / 2, endButtonSize, endButtonSize, MyGUI::Align::Right | MyGUI::Align::Center, namePrefix + "EnableButton");
        checkButton->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
        checkButton->eventMouseButtonClick += MyGUI::newDelegate(EnableSliderToggle);
    }
    else
    {
        endButtonSize = 0;
    }
    shift += endButtonSize;
    MyGUI::WidgetPtr staticScaledRoot = sliderRoot->createWidget<MyGUI::Widget>("PanelEmpty", 0, 0, w - endButtonSize, h, MyGUI::Align::Stretch, namePrefix + "ScalableSliderPanel");
    MyGUI::TextBox* sliderLabel = staticScaledRoot->createWidgetReal<MyGUI::TextBox>("Kenshi_GenericTextBoxFlat", 0, 0, 0.37f, 1, MyGUI::Align::Stretch, namePrefix + "ElementText");
    sliderLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    MyGUI::ScrollBar* scrollBar = staticScaledRoot->createWidgetReal<MyGUI::ScrollBar>("Kenshi_ScrollBar", 0.37f, 0, 0.46f, 1, MyGUI::Align::Right, namePrefix + "Slider");
    MyGUI::EditBox* valueText = staticScaledRoot->createWidgetReal<MyGUI::EditBox>("Kenshi_EditBox", 0.86f, 0,0.12f, 1, MyGUI::Align::Right | MyGUI::Align::VStretch, namePrefix + "NumberText");
    valueText->getClientWidget()->setAlign(MyGUI::Align::Stretch);
    valueText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);

    return sliderRoot;
}

class Slider
{
public:
    Slider(MyGUI::WidgetPtr parent, int x, int y, int w, int h, std::string namePrefix, SliderButton buttonType, std::string label, bool readOnlyText, std::string defaultValue, int defaultPosition, int scrollRange,
    MyGUI::delegates::IDelegate1<MyGUI::Widget*>* onClick = nullptr)
    {
        slider = CreateSlider(parent, x, y, w, h, namePrefix, buttonType);
        MyGUI::TextBox* elementText = slider->findWidget(namePrefix + "ElementText")->castType<MyGUI::TextBox>();
        elementText->setCaption(label);
        MyGUI::EditBox* numberText = slider->findWidget(namePrefix + "NumberText")->castType<MyGUI::EditBox>();
        numberText->setEditStatic(readOnlyText);
        numberText->setCaption(defaultValue);
        // grow input box size if needed
        int sizeDelta = numberText->getTextSize().height - numberText->getTextRegion().height;
        sizeDelta = std::max(sizeDelta, elementText->getTextSize().height - elementText->getTextRegion().height);

        if (sizeDelta > 0)
            slider->setSize(slider->getWidth(), slider->getHeight() + sizeDelta);

        scrollBar = slider->findWidget(namePrefix + "Slider")->castType<MyGUI::ScrollBar>();
        scrollBar->setScrollRange(scrollRange);
        scrollBar->setScrollPosition(defaultPosition);

        // TODO move to sub-class?
        if (buttonType == CHECK)
        {
            MyGUI::ButtonPtr tickBtn = slider->findWidget(namePrefix + "EnableButton")->castType<MyGUI::Button>();
            if (onClick)
                tickBtn->eventMouseButtonClick += onClick;
            tickBtn->eventMouseButtonClick += MyGUI::newDelegate(this, &Slider::OnCheckToggle);
        }
    }
    void SetEnabled(bool enabled)
    {
        std::string prefix = slider->getName().substr(0, slider->getName().size() - strlen("SliderRoot"));
        MyGUI::WidgetPtr sliderPanel = slider->findWidget(prefix + "ScalableSliderPanel");
        MyGUI::ButtonPtr button = slider->findWidget(prefix + "EnableButton")->castType<MyGUI::Button>(false);
        button->setStateSelected(enabled);
        if (enabled)
        {
            sliderPanel->setAlpha(1.0f);
            sliderPanel->setEnabled(true);
        }
        else
        {
            sliderPanel->setAlpha(0.5f);
            sliderPanel->setEnabled(false);
        }
    }
    MyGUI::WidgetPtr GetWidget()
    {
        return slider;
    }
private:
    void OnCheckToggle(MyGUI::WidgetPtr widget)
    {
        MyGUI::ButtonPtr tickBtn = widget->castType<MyGUI::Button>();
        // force update
        scrollBar->eventScrollChangePosition(scrollBar, scrollBar->getScrollPosition());
    }
    MyGUI::WidgetPtr slider;
    MyGUI::ScrollBar* scrollBar;
};

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

MyGUI::ScrollViewPtr gameplayScroll;
int gameSpeedScrollBarsStart = 0;

void RedrawGameSpeedSettings()
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    float scale = float(modMenuWindow->getClientCoord().height) / DEBUG_WINDOW_HEIGHT;
    int canvasWidth = gameplayScroll->getCanvasSize().width;

    // Clear old scroll bars
    int scrollBarIndex = 0;
    std::stringstream nameStr;
    nameStr << "SpeedSlider" << scrollBarIndex << "_";
    while (gameplayScroll->findWidget(nameStr.str() + "SliderRoot"))
    {
        gui->destroyWidget(gameplayScroll->findWidget(nameStr.str() + "SliderRoot"));
        ++scrollBarIndex;
        nameStr = std::stringstream();
        nameStr << "SpeedSlider" << scrollBarIndex << "_";
    }

    // Create new scroll bars
    std::vector<float> gameSpeeds = Settings::GetGameSpeeds();
    // get label end size
    int pad = 2 * scale;
    int positionY = gameSpeedPanel->findWidget("GameSpeedsLabel")->getHeight() + pad;
    for (int i = 0; i < gameSpeeds.size(); ++i)
    {
        std::stringstream nameStr;
        nameStr << "SpeedSlider" << i << "_";
        std::stringstream label;
        label << boost::locale::gettext("Speed ") << (i + 1) << ":";
        std::stringstream value;
        value << gameSpeeds[i];
        // 0...range-1 = 0...1000
        Slider* slider = new Slider(gameSpeedPanel, 2, positionY, canvasWidth - 4, 35 * scale, nameStr.str(), SliderButton::CLOSE, label.str(),
            false, value.str(), UnscaleGameSpeed(gameSpeeds[i]), 1001);
        MyGUI::ScrollBar* scrollBar = slider->GetWidget()->findWidget(nameStr.str() + "Slider")->castType<MyGUI::ScrollBar>();
        if (!scrollBar)
            ErrorLog("ScrollBar not found!");
        // add event listeners
        scrollBar->eventScrollChangePosition += MyGUI::newDelegate(GameSpeedScroll);
        MyGUI::EditBox* valueText = slider->GetWidget()->findWidget(nameStr.str() + "NumberText")->castType<MyGUI::EditBox>();
        valueText->eventEditTextChange += MyGUI::newDelegate(SpeedSliderTextChange);
        MyGUI::ButtonPtr deleteButton = slider->GetWidget()->findWidget(nameStr.str() + "DeleteButton")->castType<MyGUI::Button>();
        deleteButton->eventMouseButtonClick += MyGUI::newDelegate(DeleteGameSpeedScroll);
        positionY += slider->GetWidget()->getHeight() + pad;
    }

    // resize settings
    MyGUI::IntSize canvasSize = gameplayScroll->getCanvasSize();
    gameSpeedPanel->setSize(gameSpeedPanel->getWidth(), positionY);
    int newHeight = gameSpeedPanel->getBottom();
    gameplayScroll->setCanvasSize(canvasSize.width, std::max(newHeight, canvasSize.height));
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

    // update UI + var
    if (Settings::GetOverrideAttackSlots() && newPos > 0)
    {
        std::stringstream str;
        str << newPos;
        numberText->setCaption(str.str());
        // TODO this sometimes causes a crash if Kenshi is unpaused
        Kenshi::GetNumAttackSlots() = newPos;
    }
    else
    {
        // override disabled or set to default
        std::stringstream str;
        str << "(" << defaultAttackSlots << ")";
        numberText->setCaption(str.str());
        Kenshi::GetNumAttackSlots() = defaultAttackSlots;
    }

    // update stored setting
    if (newPos > 0)
        Kenshi::GetNumAttackSlots() = newPos;
    else
        Settings::SetAttackSlots(-1);
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

template<void F(bool)>
void ButtonToggleSetting(MyGUI::WidgetPtr sender)
{
    MyGUI::ButtonPtr button = sender->castType<MyGUI::Button>();
    bool newState = button->getStateSelected();

    // Update settings + hooks
    F(newState);
}

void NoDelegate(MyGUI::WidgetPtr) {};
// NOTE: this re-sizes the height so the text isn't clipped
template<void F(MyGUI::WidgetPtr)>
MyGUI::ButtonPtr CreateStandardTickButton(MyGUI::WidgetPtr parent, std::string caption, int left, int top, int width, int minHeight, std::string namePrefix, bool initialState)
{
    MyGUI::WidgetPtr root = parent->createWidget<MyGUI::Widget>("PanelEmpty", left, top, width, minHeight, MyGUI::Align::Top | MyGUI::Align::HStretch, namePrefix + "TickRoot");
    int buttonStart = (width - minHeight);
    MyGUI::TextBox* sliderLabel = root->createWidget<MyGUI::TextBox>("Kenshi_GenericTextBoxFlat", 0, 0, buttonStart - 4, minHeight, MyGUI::Align::Stretch, namePrefix + "Label");
    sliderLabel->setTextAlign(MyGUI::Align::Left | MyGUI::Align::VCenter);
    sliderLabel->setCaption(caption);
    int sizeDelta = sliderLabel->getTextSize().height - sliderLabel->getTextRegion().height;
    if (sizeDelta > 0)
    {
        root->setSize(root->getWidth(), root->getHeight() + sizeDelta);
        // update after scaling
        minHeight = root->getHeight();
        buttonStart = (width - minHeight);
        sliderLabel->setSize(buttonStart - 4, sliderLabel->getHeight());
    }

    MyGUI::ButtonPtr checkButton = root->createWidget<MyGUI::Button>("Kenshi_TickBoxSkin", buttonStart, 0, minHeight, minHeight, MyGUI::Align::Top | MyGUI::Align::Right, namePrefix + "EnableButton");
    checkButton->setStateSelected(initialState);
    checkButton->eventMouseButtonClick += MyGUI::newDelegate(TickButtonBehaviourClick);
    checkButton->eventMouseButtonClick += MyGUI::newDelegate(F);

    return checkButton;
}

void SetIncreaseMaxCameraDistance(bool increaseMaxCameraDistance)
{
    if (increaseMaxCameraDistance)
        MiscHooks::SetMaxCameraDistance(4000.0f);
    else
        MiscHooks::SetMaxCameraDistance(2000.0f);

    Settings::SetIncreaseMaxCameraDistance(increaseMaxCameraDistance);
}

void SetUseCustomGameSpeeds(bool useCustomGameSpeeds)
{
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
    std::string uuid = "";
    if (sendUUIDToggle->getStateSelected())
        uuid = Bugs::GetUUIDHash();
    Bugs::ReportUserBug(description->getCaption(), uuid);
    description->setCaption("Report sent.");
    bugReportWindow->setVisible(false);
}

void CreateGameSpeedTutorialWindow(MyGUI::Gui* gui, float scale)
{
    gameSpeedTutorialWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, 630 * scale, 470 * scale, MyGUI::Align::Center, "Window", "GameSpeedTutorialWindow");
    gameSpeedTutorialWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    gameSpeedTutorialWindow->setCaption(boost::locale::gettext("Game speed tutorial"));
    // don't know why this isn't centering properly...
    MyGUI::ImageBox* gameSpeedTutImage = gameSpeedTutorialWindow->getClientWidget()->createWidget<MyGUI::ImageBox>("ImageBox", 10 * scale, 10 * scale, 600 * scale, 400 * scale, MyGUI::Align::Center, "GameSpeedTutorialImage");
    gameSpeedTutImage->setImageTexture("game_speed_tutorial.png");
    MyGUI::TextBox* tutorialPauseLabel = gameSpeedTutImage->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 65 * scale, 190 * scale, 100 * scale, 30 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "TutorialPauseLabel");
    tutorialPauseLabel->setCaption(boost::locale::gettext("Pause"));
    tutorialPauseLabel->setTextAlign(MyGUI::Align::Center);
    MyGUI::EditBox* tutorial1xSpeedLabel = gameSpeedTutImage->createWidget<MyGUI::EditBox>("Kenshi_TextboxStandardText", 105 * scale, 130 * scale, 140 * scale, 60 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "Tutorial1xSpeedLabel");
    tutorial1xSpeedLabel->setTextAlign(MyGUI::Align::Center);
    // German translation is long
    tutorial1xSpeedLabel->setEditWordWrap(true);
    tutorial1xSpeedLabel->setEditStatic(true);
    tutorial1xSpeedLabel->setCaption(boost::locale::gettext("1x speed"));
    MyGUI::TextBox* tutorialDecreaseSpeedLabel = gameSpeedTutImage->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 180 * scale, 170 * scale, 140 * scale, 60 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "TutorialDecreaseSpeedLabel");
    tutorialDecreaseSpeedLabel->setCaption(boost::locale::gettext("Decrease\nspeed"));
    tutorialDecreaseSpeedLabel->setTextAlign(MyGUI::Align::Center);
    MyGUI::TextBox* tutorialIncreaseSpeedLabel = gameSpeedTutImage->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 250 * scale, 130 * scale, 140 * scale, 60 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "TutorialIncreaseSpeedLabel");
    tutorialIncreaseSpeedLabel->setCaption(boost::locale::gettext("Increase\nspeed"));
    tutorialIncreaseSpeedLabel->setTextAlign(MyGUI::Align::Center);
    MyGUI::TextBox* currentSpeedLabel = gameSpeedTutImage->createWidget<MyGUI::TextBox>("Kenshi_TextboxStandardText", 330 * scale, 185 * scale, 200 * scale, 60 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "currentSpeedLabel");
    currentSpeedLabel->setCaption(boost::locale::gettext("Current speed"));
    currentSpeedLabel->setTextAlign(MyGUI::Align::Center);
    gameSpeedTutorialWindow->setVisible(false);
}

void CreateBugReportWindow(MyGUI::Gui* gui, float scale)
{
    // Create bug report window
    int width = 450 * scale;
    int height = 450 * scale;
    bugReportWindow = gui->createWidget<MyGUI::Window>("Kenshi_WindowCX", 100, 100, width, height, MyGUI::Align::Center, "Window", "BugReportWindow");
    bugReportWindow->setCaption(boost::locale::gettext("RE_Kenshi Bug Report"));
    bugReportWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
    int panelWidth = bugReportWindow->getClientCoord().width;
    // Only edit boxes support word wrap
    MyGUI::EditBox* infoText = bugReportWindow->getClientWidget()->createWidget<MyGUI::EditBox>("Kenshi_GenericTextBoxFlat", 2, 2, panelWidth - 4, height * 0.4f, MyGUI::Align::Top | MyGUI::Align::Left, "BugReportInfo");
    infoText->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
    infoText->setEditMultiLine(true);
    infoText->setEditWordWrap(true);
    infoText->setEditStatic(true);
    infoText->setCaption(MyGUI::UString(boost::locale::gettext("Your report will be sent to RE_Kenshi's developer (BFrizzleFoShizzle) with the following information:"))
        + "\n" + boost::locale::gettext("\nRE_Kenshi version: ") + Version::GetDisplayVersion()
        + boost::locale::gettext("\nKenshi version: ") + Kenshi::GetKenshiVersion().ToString()
        + boost::locale::gettext("\nUUID hash: ") + Bugs::GetUUIDHash() + boost::locale::gettext(" (optional - allows the developer to know all your reports come from the same machine)")
        + boost::locale::gettext("\nYour bug description")
        + boost::locale::gettext("\n\nPlease describe the bug:"));
    int sizeDelta = infoText->getTextSize().height - infoText->getTextRegion().height;
    if(sizeDelta > 0)
        infoText->setSize(infoText->getWidth(), infoText->getHeight() + sizeDelta);
    int positionY = infoText->getHeight() + 4;

    MyGUI::EditBox* bugDescription = bugReportWindow->getClientWidget()->createWidget<MyGUI::EditBox>("Kenshi_WordWrap", 2, positionY, panelWidth - 4, height * 0.4f, MyGUI::Align::Top | MyGUI::Align::Left, "BugDescription");
    bugDescription->setEditStatic(false);
    positionY += bugDescription->getHeight() + 4;

    sendUUIDToggle = CreateStandardTickButton<NoDelegate>(bugReportWindow->getClientWidget(), boost::locale::gettext("Include UUID hash"), 2, positionY, panelWidth - 4, 26 * scale, "SendUUIDToggle", true);
    positionY += sendUUIDToggle->getHeight() + 4;

    MyGUI::ButtonPtr sendBugButton = bugReportWindow->getClientWidget()->createWidget<MyGUI::Button>("Kenshi_Button1", 2, positionY, panelWidth - 4, 26 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "SendReportButton");
    sendBugButton->setCaption(boost::locale::gettext("Send report"));
    if (sendBugButton->getHeight() < sendBugButton->getTextSize().height)
        sendBugButton->setSize(sendBugButton->getWidth(), sendBugButton->getTextSize().height);
    sendBugButton->eventMouseButtonClick += MyGUI::newDelegate(SendBugPress);
    positionY += sendBugButton->getHeight() + 4;

    sizeDelta = positionY - bugReportWindow->getClientWidget()->getHeight();
    if (sizeDelta > 0)
        bugReportWindow->setSize(bugReportWindow->getWidth(), bugReportWindow->getHeight() + sizeDelta);

    bugReportWindow->setVisible(false);
}

void InitGUI()
{
    DebugLog("Main menu loaded.");
    Kenshi::BinaryVersion gameVersion = Kenshi::GetKenshiVersion();

    DebugLog("Kenshi version: " + gameVersion.GetVersion());
    DebugLog("Kenshi platform: " + gameVersion.GetPlatformStr());

    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        DebugLog("Version supported.");

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

        MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>();
        MyGUI::UString version = versionText->getCaption();
        DebugLog(version);

        Ogre::ResourceGroupManager* resMan = Ogre::ResourceGroupManager::getSingletonPtr();
        if (resMan)
            resMan->addResourceLocation("./RE_Kenshi", "FileSystem", "GUI");

        MyGUI::FactoryManager& factory = MyGUI::FactoryManager::getInstance();
        MyGUI::IObject* object = factory.createObject("Resource", "ResourceTrueTypeFont");

        // Fixes fonts
        // TODO remove after dropping support for old versions
        MyGUIHooks::InitMainMenu();

        // small font has to be created BEFORE the font fix
        // TODO move back after dropping support for old versions
        MyGUI::ResourceTrueTypeFont* smallFont = object->castType<MyGUI::ResourceTrueTypeFont>(false);
        if (smallFont != nullptr)
        {
            smallFont->setResourceName("Kenshi_StandardFont_Medium18");
            smallFont->setSource("Exo2-Bold.ttf");
            // hack to get the actual font size after kenshi's scaling
            smallFont->setSize(versionText->getFontHeight());
            smallFont->setHinting("use_native");
            smallFont->setResolution(50);
            smallFont->setAntialias(false);
            smallFont->setTabWidth(4);
            smallFont->setOffsetHeight(0);
            smallFont->setSubstituteCode(0);
            smallFont->setDistance(2);
            smallFont->addCodePointRange(32, 126);
            smallFont->addCodePointRange(8127, 8217);
            smallFont->initialise();

            MyGUI::ResourceManager::getInstance().addResource(smallFont);
        }

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
        versionText->setCaption("RE_Kenshi " + Version::GetDisplayVersion() + " - " + version);

        // Create mod menu
        float initScale = versionText->getTextSize().height / 18.0f;
        float windowWidth = DEBUG_WINDOW_WIDTH * initScale;
        float windowHeight = DEBUG_WINDOW_HEIGHT * initScale;
        // hack to get resolution height
        modMenuWindow = gui->createWidgetReal<MyGUI::Window>("Kenshi_WindowCX", 0, 0, 1, 1, MyGUI::Align::Center, "Window", "DebugWindow");
        int resolutionHeight = modMenuWindow->getHeight();
        // fix aspect ratio
        modMenuWindow->setCoord(100, 100, windowWidth, windowHeight);
        float scale = float(modMenuWindow->getClientCoord().height) / DEBUG_WINDOW_HEIGHT;
        scale = versionText->getTextSize().height / 18.0f;
        modMenuWindow->setCaption(boost::locale::gettext("RE_Kenshi Menu"));
        modMenuWindow->eventKeyButtonReleased += MyGUI::newDelegate(debugMenuKeyRelease);
        modMenuWindow->eventWindowButtonPressed += MyGUI::newDelegate(debugMenuButtonPress);
        if (!Settings::GetOpenSettingsOnStart())
            modMenuWindow->setVisible(false);
        MyGUI::Widget* client = modMenuWindow->findWidget("Client");
        client->setAlign(MyGUI::Align::Stretch);
        MyGUI::TabControl* tabControl = client->createWidget<MyGUI::TabControl>("Kenshi_TabControl", MyGUI::IntCoord(2, 2, modMenuWindow->getClientCoord().width - 4, std::min(resolutionHeight - 100, modMenuWindow->getClientCoord().height - 4)), MyGUI::Align::Top | MyGUI::Align::Left | MyGUI::Align::Stretch, "SettingsTabs");

        // Create game speed tutorial window
        // Hacky way of picking up font size in pixels - version text is one line in height
        CreateGameSpeedTutorialWindow(gui, versionText->getTextSize().height / 18.0f);

        // Create bug report window
        CreateBugReportWindow(gui, scale);

        // create all tabs so we can figure out if the window width is large enough to fit all tabs
        MyGUI::TabItemPtr generalTab = tabControl->addItem(boost::locale::gettext("General"));
        MyGUI::ScrollViewPtr generalScroll = generalTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollViewEmpty", MyGUI::IntCoord(2, 2, generalTab->getClientCoord().width - 4, generalTab->getClientCoord().height - 4), MyGUI::Align::Stretch, "GeneralTab");

        MyGUI::TabItemPtr gameplayTab = tabControl->addItem(boost::locale::gettext("Gameplay"));
        gameplayScroll = gameplayTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollViewEmpty", MyGUI::IntCoord(2, 2, gameplayTab->getClientCoord().width - 4, gameplayTab->getClientCoord().height - 4), MyGUI::Align::Top | MyGUI::Align::Left| MyGUI::Align::Stretch);
        gameplayScroll->setCanvasAlign(MyGUI::Align::Stretch);

        MyGUI::TabItemPtr performanceTab = tabControl->addItem(boost::locale::gettext("Performance"));
        MyGUI::ScrollViewPtr performanceScroll = performanceTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollViewEmpty", MyGUI::IntCoord(2, 2, performanceTab->getClientCoord().width - 4, performanceTab->getClientCoord().height - 4), MyGUI::Align::Stretch);

        MyGUI::TabItemPtr debugLogTab = tabControl->addItem(boost::locale::gettext("Debug log"));
        debugLogScrollView = debugLogTab->createWidget<MyGUI::ScrollView>("Kenshi_ScrollViewEmpty", MyGUI::IntCoord(2, 2, debugLogTab->getClientCoord().width - 4, debugLogTab->getClientCoord().height - 4), MyGUI::Align::Stretch);

        // resize to fit tabs if needed
        size_t currentTabBarWidth = 0;
        for (int i = 0; i < tabControl->getItemCount(); ++i)
            currentTabBarWidth += tabControl->getButtonWidthAt(i);

        // Kenshi_ScrollView has 20px of padding
        int extraPixelsNeeded = (currentTabBarWidth - tabControl->getWidth()) + 20;
        if (extraPixelsNeeded > 0)
        {
            // need to scale up window to fit the tabs
            DebugLog("Adding " + std::to_string((int64_t)extraPixelsNeeded) + " pixels for tabs");
            MyGUI::IntCoord modMenuCoord = modMenuWindow->getCoord();
            // factor we're stretching the UI by
            float stretchFactor = float(modMenuCoord.width + extraPixelsNeeded) / modMenuCoord.width;
            // this update is done with the integer number becasue it has to be PIXEL-PERFECT WITH NO ROUNDING
            modMenuCoord.width += extraPixelsNeeded;
            modMenuWindow->setCoord(modMenuCoord);
        }

        // This has to be done after fixing the scale
        // setting canvas align to Stretch doesn't work btw
        generalScroll->setCanvasSize(generalScroll->getWidth() - 20, generalScroll->getHeight() - 20);
        gameplayScroll->setCanvasSize(gameplayScroll->getWidth() - 20, gameplayScroll->getHeight() - 20);
        performanceScroll->setCanvasSize(performanceScroll->getWidth() - 20, performanceScroll->getHeight() - 20);
        debugLogScrollView->setCanvasSize(debugLogScrollView->getWidth() - 20, debugLogScrollView->getHeight() - 20);

        int canvasWidth = generalScroll->getWidth() - 20;

        // TODO scale?
        int pad = 2 * scale;

        /******            GENERAL SETTINGS            ******/
        int positionY = 2;
        MyGUI::ButtonPtr reportBugButton = generalScroll->createWidget<MyGUI::Button>("Kenshi_Button1", 0, positionY, canvasWidth, 26 * scale, MyGUI::Align::Top | MyGUI::Align::HStretch, "ReportBugButton");
        reportBugButton->setCaption(boost::locale::gettext("Report bug"));
        if (reportBugButton->getHeight() < reportBugButton->getTextSize().height)
            reportBugButton->setSize(reportBugButton->getWidth(), reportBugButton->getTextSize().height);
        reportBugButton->eventMouseButtonClick += MyGUI::newDelegate(ReportBugPress);
        positionY += reportBugButton->getHeight() + pad + 10;

        MyGUI::ButtonPtr checkUpdatesButton = CreateStandardTickButton<ButtonToggleSetting<Settings::SetCheckUpdates>>(generalScroll,
            boost::locale::gettext("Automatically check for updates"), 2, positionY, canvasWidth - 4, 26 * scale, "CheckUpdates", Settings::GetCheckUpdates());
        positionY += checkUpdatesButton->getHeight() + pad;
        
        MyGUI::ButtonPtr openSettingsOnStartButton = CreateStandardTickButton<ButtonToggleSetting<Settings::SetOpenSettingsOnStart>>(generalScroll,
            boost::locale::gettext("Open RE_Kenshi settings on startup"), 2, positionY, canvasWidth - 4, 26 * scale, "OpenSettingsOnStart", Settings::GetOpenSettingsOnStart());
        positionY += openSettingsOnStartButton->getHeight() + pad;

        /******            GAMEPLAY SETTINGS            ******/
        positionY = 2;
        MyGUI::ButtonPtr fixRNGButton = CreateStandardTickButton<ButtonToggleSetting<MiscHooks::SetFixRNG>>(gameplayScroll,
            boost::locale::gettext("Fix Kenshi's RNG bug"), 2, positionY, canvasWidth - 4, 26 * scale, "FixRNGToggle", Settings::GetFixRNG());
        positionY += fixRNGButton->getHeight() + pad;

        MyGUI::ButtonPtr increaseMaxCameraDistanceButton = CreateStandardTickButton<ButtonToggleSetting<SetIncreaseMaxCameraDistance>>(gameplayScroll,
            boost::locale::gettext("Increase max camera distance"), 2, positionY, canvasWidth - 4, 26 * scale, "IncreaseMaxCameraDistance", Settings::GetIncreaseMaxCameraDistance());
        positionY += increaseMaxCameraDistanceButton->getHeight() + pad;

        // Attack slots
        defaultAttackSlots = Kenshi::GetNumAttackSlots();
        int numAttackSlots = Settings::GetAttackSlots();
        bool attackSlotsOverrideEnabled = Settings::GetOverrideAttackSlots();
        // Apply settings
        if (attackSlotsOverrideEnabled && numAttackSlots > 0)
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
        std::string attackSlotsLabelTxt = boost::locale::gettext("Attack slots (") + std::to_string((long long)defaultAttackSlots) + "):";
        Slider* attackSlotsSlider = new Slider(gameplayScroll, 2, positionY, canvasWidth - 4, 35 * scale, "AttackSlotsSlider_", SliderButton::CHECK,
            attackSlotsLabelTxt, true, attackSlotsValue.str(), numAttackSlots, 6, MyGUI::newDelegate(ButtonToggleSetting<Settings::SetOverrideAttackSlots>));
        attackSlotsSlider->SetEnabled(attackSlotsOverrideEnabled);
        MyGUI::ScrollBar* attackSlotsScrollBar = attackSlotsSlider->GetWidget()->findWidget("AttackSlotsSlider_Slider")->castType<MyGUI::ScrollBar>();
        attackSlotsScrollBar->eventScrollChangePosition += MyGUI::newDelegate(AttackSlotScroll);
        MyGUI::TextBox* attackSlotsLabel = attackSlotsSlider->GetWidget()->findWidget("AttackSlotsSlider_ElementText")->castType<MyGUI::TextBox>();
        int widthDelta = attackSlotsLabel->getTextSize().width - attackSlotsLabel->getWidth();
        positionY += attackSlotsSlider->GetWidget()->getHeight() + pad;

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
        std::string maxFactionSizeLabelTxt = boost::locale::gettext("Max. faction size (") + std::to_string((long long)defaultMaxFactionSize) + "):";
        Slider* maxFactionSizeSlider = new Slider(gameplayScroll, 2, positionY, canvasWidth - 4, 35 * scale, "MaxFactionSizeSlider_", SliderButton::CHECK,
            maxFactionSizeLabelTxt, false, maxFactionSizeValue.str(), maxFactionSize, 1001, MyGUI::newDelegate(ButtonToggleSetting<Settings::SetOverrideMaxFactionSize>));
        maxFactionSizeSlider->SetEnabled(Settings::GetOverrideMaxFactionSize());
        MyGUI::ScrollBar* maxFactionSizeScrollBar = maxFactionSizeSlider->GetWidget()->findWidget("MaxFactionSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
        maxFactionSizeScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxFactionSizeScroll);
        MyGUI::EditBox* valueText = maxFactionSizeSlider->GetWidget()->findWidget("MaxFactionSizeSlider_NumberText")->castType<MyGUI::EditBox>();
        valueText->eventEditTextChange += MyGUI::newDelegate(MaxFactionSizeSliderTextChange);
        MyGUI::TextBox* maxFactionSizeLabel = maxFactionSizeSlider->GetWidget()->findWidget("MaxFactionSizeSlider_ElementText")->castType<MyGUI::TextBox>();
        widthDelta = std::max(widthDelta, maxFactionSizeLabel->getTextSize().width - maxFactionSizeLabel->getTextRegion().width);
        positionY += maxFactionSizeSlider->GetWidget()->getHeight() + pad;

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
        std::string maxSquadSizeLabelTxt = boost::locale::gettext("Max. squad size (") + std::to_string((long long)defaultMaxSquadSize) + "):";
        Slider* maxSquadSizeSlider = new Slider(gameplayScroll, 2, positionY, canvasWidth - 4, 35 * scale, "MaxSquadSizeSlider_", SliderButton::CHECK,
            maxSquadSizeLabelTxt, false, maxSquadSizeValue.str(), maxSquadSize, 1001, MyGUI::newDelegate(ButtonToggleSetting<Settings::SetOverrideMaxSquadSize>));
        maxSquadSizeSlider->SetEnabled(Settings::GetOverrideMaxSquadSize());
        MyGUI::ScrollBar* maxSquadSizeScrollBar = maxSquadSizeSlider->GetWidget()->findWidget("MaxSquadSizeSlider_Slider")->castType<MyGUI::ScrollBar>();
        maxSquadSizeScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxSquadSizeScroll);
        valueText = maxSquadSizeSlider->GetWidget()->findWidget("MaxSquadSizeSlider_NumberText")->castType<MyGUI::EditBox>();
        valueText->eventEditTextChange += MyGUI::newDelegate(MaxSquadSizeSliderTextChange);
        MyGUI::TextBox* maxSquadSizeLabel = maxSquadSizeSlider->GetWidget()->findWidget("MaxSquadSizeSlider_ElementText")->castType<MyGUI::TextBox>();
        widthDelta = std::max(widthDelta, maxSquadSizeLabel->getTextSize().width - maxSquadSizeLabel->getTextRegion().width);
        positionY += maxSquadSizeSlider->GetWidget()->getHeight() + pad;

        // max squads
        defaultMaxSquads = Kenshi::GetMaxSquads();
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
        std::string maxSquadsLabeltxt = boost::locale::gettext("Max. squads (") + std::to_string((long long)defaultMaxSquads) + "):";
        Slider* maxSquadsSlider = new Slider(gameplayScroll, 2, positionY, canvasWidth - 4, 35 * scale, "MaxSquadsSlider_", SliderButton::CHECK,
            maxSquadsLabeltxt, false, maxSquadsValue.str(), maxSquads, 1001, MyGUI::newDelegate(ButtonToggleSetting<Settings::SetOverrideMaxSquads>));
        maxSquadsSlider->SetEnabled(Settings::GetOverrideMaxSquads());
        MyGUI::ScrollBar* maxSquadsScrollBar = maxSquadsSlider->GetWidget()->findWidget("MaxSquadsSlider_Slider")->castType<MyGUI::ScrollBar>();
        maxSquadsScrollBar->eventScrollChangePosition += MyGUI::newDelegate(MaxSquadsScroll);
        valueText = maxSquadsSlider->GetWidget()->findWidget("MaxSquadsSlider_NumberText")->castType<MyGUI::EditBox>();
        valueText->eventEditTextChange += MyGUI::newDelegate(MaxSquadsSliderTextChange);
        MyGUI::TextBox* maxSquadsLabel = maxSquadsSlider->GetWidget()->findWidget("MaxSquadsSlider_ElementText")->castType<MyGUI::TextBox>();
        widthDelta = std::max(widthDelta, maxSquadsLabel->getTextSize().width - maxSquadsLabel->getTextRegion().width);
        positionY += maxSquadsSlider->GetWidget()->getHeight() + pad;

        MyGUI::ButtonPtr setCustomGameSpeedsButton = CreateStandardTickButton<ButtonToggleSetting<SetUseCustomGameSpeeds>>(gameplayScroll,
            boost::locale::gettext("Use custom game speed controls"), 2, positionY, canvasWidth - 4, 26 * scale, "UseCustomGameSpeeds", Settings::GetUseCustomGameSpeeds());
        positionY += setCustomGameSpeedsButton->getHeight() + pad;

        if (widthDelta > 0)
        {
            // scale up the window + tabs (tab canvas doesn't appear to stretch with MyGUI::Align::Stretch)
            modMenuWindow->setSize(modMenuWindow->getWidth() + widthDelta, modMenuWindow->getHeight());
            gameplayScroll->setCanvasSize(gameplayScroll->getCanvasSize().width + widthDelta, gameplayScroll->getCanvasSize().height);
            generalScroll->setCanvasSize(generalScroll->getCanvasSize().width + widthDelta, generalScroll->getCanvasSize().height);
            performanceScroll->setCanvasSize(performanceScroll->getCanvasSize().width + widthDelta, performanceScroll->getCanvasSize().height);
            debugLogScrollView->setCanvasSize(debugLogScrollView->getCanvasSize().width + widthDelta, debugLogScrollView->getCanvasSize().height);
            canvasWidth += widthDelta;
        }

        gameSpeedPanel = gameplayScroll->createWidget<MyGUI::Widget>("", 0, positionY, canvasWidth - 4, 100, MyGUI::Align::Top | MyGUI::Align::Left, "GameSpeedPanel");
        gameSpeedPanel->setVisible(Settings::GetUseCustomGameSpeeds());

        MyGUI::TextBox* gameSpeedsLabel = gameSpeedPanel->createWidget<MyGUI::TextBox>("Kenshi_GenericTextBoxFlat", 2, 0, canvasWidth * 0.35, 30 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "GameSpeedsLabel");
        gameSpeedsLabel->setCaption(boost::locale::gettext("Game speeds"));
        int heightDelta = gameSpeedsLabel->getTextSize().height - gameSpeedsLabel->getTextRegion().height;
        MyGUI::ButtonPtr addGameSpeed = gameSpeedPanel->createWidget<MyGUI::Button>("Kenshi_Button1", ((canvasWidth * 0.35) + 10) - 2, 0, canvasWidth - ((canvasWidth * 0.35) + 10), 30 * scale, MyGUI::Align::Top | MyGUI::Align::Right, "AddGameSpeedBtn");
        addGameSpeed->setCaption(boost::locale::gettext("Add game speed"));
        addGameSpeed->eventMouseButtonClick += MyGUI::newDelegate(AddGameSpeed);
        if (heightDelta > 0)
        {
            gameSpeedsLabel->setSize(gameSpeedsLabel->getWidth(), gameSpeedsLabel->getHeight() + heightDelta);
            addGameSpeed->setSize(addGameSpeed->getWidth(), addGameSpeed->getHeight() + heightDelta);
        }
        gameSpeedScrollBarsStart = positionY;

        RedrawGameSpeedSettings();

        /******            PERFORMANCE SETTINGS            ******/
        positionY = 2;

        MyGUI::ButtonPtr cacheShadersButton = CreateStandardTickButton<ButtonToggleSetting<Settings::SetCacheShaders>>(performanceScroll,
            boost::locale::gettext("Cache shaders"), 2, positionY, canvasWidth - 4, 26 * scale, "CacheShadersToggle", Settings::GetCacheShaders());
        positionY += cacheShadersButton->getHeight() + pad;

        int performanceTextStart = positionY;
        positionY += 10;
        std::string heightmapRecommendationText = boost::locale::gettext("Fast uncompressed heightmap should be faster on SSDs\nCompressed heightmap should be faster on HDDs");
        MyGUI::EditBox* heighmapRecommendLabel = performanceScroll->createWidget<MyGUI::EditBox>("Kenshi_TextboxStandardText", 15, positionY, canvasWidth - 30, 40 * scale, MyGUI::Align::VStretch | MyGUI::Align::Left, "HeightmapRecommendLabel");
        heighmapRecommendLabel->setEditWordWrap(true);
        heighmapRecommendLabel->setEditMultiLine(true);
        heighmapRecommendLabel->setCaption(heightmapRecommendationText);
        heighmapRecommendLabel->setSize(canvasWidth - 30, heighmapRecommendLabel->getTextSize().height);
        positionY += heighmapRecommendLabel->getTextSize().height + 4;

        if (!HeightmapHook::CompressedHeightmapFileExists())
        {
            MyGUI::EditBox* noCompressedHeightmapLabel = performanceScroll->createWidget<MyGUI::EditBox>("Kenshi_TextboxStandardText", 15, positionY, canvasWidth - 30 * scale, 30 * scale, MyGUI::Align::VStretch | MyGUI::Align::Left, "NoCompressedHeightmapLabel");
            noCompressedHeightmapLabel->setFontName("Kenshi_StandardFont_Medium18");
            noCompressedHeightmapLabel->setCaption(boost::locale::gettext("To enable compressed heightmap, reinstall RE_Kenshi and check \"Compress Heightmap\""));
            noCompressedHeightmapLabel->setEditWordWrap(true);
            noCompressedHeightmapLabel->setSize(noCompressedHeightmapLabel->getWidth(), noCompressedHeightmapLabel->getTextSize().height);
            positionY += noCompressedHeightmapLabel->getTextSize().height + 4;
        }
        positionY += 10;
        MyGUI::WidgetPtr performanceTextPanel = performanceScroll->createWidget<MyGUI::Widget>("Kenshi_GenericTextBox", 0, performanceTextStart, canvasWidth, (positionY - performanceTextStart) - pad, MyGUI::Align::VStretch | MyGUI::Align::Left, "PerformanceTextPanel");

        std::string compressedHeighmapTip = "";
        std::string fastUncompressedHeighmapTip = "";
        if (HeightmapHook::GetRecommendedHeightmapMode() == HeightmapHook::COMPRESSED)
            compressedHeighmapTip = boost::locale::gettext("[RECOMMENDED] ");
        if (HeightmapHook::GetRecommendedHeightmapMode() == HeightmapHook::FAST_UNCOMPRESSED)
            fastUncompressedHeighmapTip = boost::locale::gettext("[RECOMMENDED] ");
        if (!HeightmapHook::CompressedHeightmapFileExists())
            compressedHeighmapTip += "#440000" + boost::locale::gettext("[UNAVAILABLE] ");
        MyGUI::ComboBoxPtr heightmapOptions = performanceScroll->createWidget<MyGUI::ComboBox>("Kenshi_ComboBox", 2, positionY, canvasWidth - 4, 36 * scale, MyGUI::Align::Top | MyGUI::Align::Left, "HeightmapComboBox");
        heightmapOptions->addItem(boost::locale::gettext("Use vanilla heightmap implementation"), HeightmapHook::VANILLA);
        heightmapOptions->addItem(compressedHeighmapTip + boost::locale::gettext("Use compressed heightmap"), HeightmapHook::COMPRESSED);
        heightmapOptions->addItem(fastUncompressedHeighmapTip + boost::locale::gettext("Use fast uncompressed heightmap"), HeightmapHook::FAST_UNCOMPRESSED);
        heightmapOptions->setIndexSelected(Settings::GetHeightmapMode());
        heightmapOptions->setComboModeDrop(true);
        heightmapOptions->setSmoothShow(false);
        heightmapOptions->eventComboAccept += MyGUI::newDelegate(ChangeHeightmapMode);
        MyGUI::TextBox* itemLabelText = heightmapOptions->getClientWidget()->castType<MyGUI::TextBox>();
        int sizeNeededForText = itemLabelText->getTextSize().height - itemLabelText->getHeight();
        if (sizeNeededForText > 0)
        {
            heightmapOptions->setSize(heightmapOptions->getWidth(), heightmapOptions->getHeight() + sizeNeededForText);
            itemLabelText->setSize(itemLabelText->getWidth(), itemLabelText->getHeight() + sizeNeededForText);
        }
        positionY += heightmapOptions->getHeight() + 4;;

        /******            DEBUG LOG            ******/
        positionY = 2;
        MyGUI::ButtonPtr logFileIOButton = CreateStandardTickButton<ButtonToggleSetting<Settings::SetLogFileIO>>(debugLogScrollView,
            boost::locale::gettext("Log file IO"), 2, positionY, canvasWidth - 4, 26 * scale, "LogFileIO", Settings::GetLogFileIO());
        positionY += logFileIOButton->getHeight() + pad;

        MyGUI::ButtonPtr lodAudioButton = CreateStandardTickButton<ButtonToggleSetting<Settings::SetLogAudio>>(debugLogScrollView,
            boost::locale::gettext("Log audio IDs/events/switches/states"), 2, positionY, canvasWidth - 4, 26 * scale, "LogAudio", Settings::GetLogAudio());
        positionY += lodAudioButton->getHeight() + pad;

        MyGUI::TextBox* debugOut = debugLogScrollView->createWidget<MyGUI::TextBox>("Kenshi_GenericTextBox", 0, positionY, canvasWidth, 100, MyGUI::Align::Stretch, "DebugPrint");
    }
}

bool hookedLoad = false;

void LoadStart(MyGUI::WidgetPtr widget)
{
    DebugLog("Game loading...");
}

// used for profiling loads
bool oldLoadingPanelVisible = false;
static clock_t loadTime = 0;

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
        gameSpeedText->setTextAlign(MyGUI::Align::Center);

        speedButton3->eventMouseButtonClick.clear();
        speedButton4->eventMouseButtonClick.clear();
        speedButton2->eventMouseButtonClick += MyGUI::newDelegate(playButtonHook);
        speedButton3->eventMouseButtonClick += MyGUI::newDelegate(decreaseSpeed);
        speedButton4->eventMouseButtonClick += MyGUI::newDelegate(increaseSpeed);
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
    if (loadingPanel && loadingPanel->getInheritedVisible() != oldLoadingPanelVisible && Settings::GetProfileLoads())
    {
        clock_t newTime = clock();
        oldLoadingPanelVisible = loadingPanel->getInheritedVisible();
        std::string msg = oldLoadingPanelVisible ? "true" : "false";
        if (!oldLoadingPanelVisible)
        {
            clock_t timeInMSecs = (newTime - loadTime) / (CLOCKS_PER_SEC / 1000);
            char timeStr[10];
            sprintf_s(timeStr, "%i.%03i", timeInMSecs / 1000, timeInMSecs % 1000);
            msg = msg + ", load time: " + timeStr;
        }
        DebugLog("Loading panel visibility changed to " + msg);
        loadTime = newTime;
    }

    if (debugLogScrollView && debugLogScrollView->getInheritedVisible() && debugOut)
    {
        std::stringstream debugStr;
        debugStr << GetDebugLog();
        debugOut->setCaption(debugStr.str());

        // update scroll view size if needed
        int logSize = debugOut->getTextSize().height;
        int padding = debugOut->getTextRegion().top;
        int finalHeight = logSize + padding;
        int finalBottom = debugOut->getTop() + finalHeight;
        debugOut->setSize(debugOut->getWidth(), finalHeight);
        if (debugLogScrollView->getCanvasSize().height < finalBottom)
        {
            debugLogScrollView->setCanvasSize(debugLogScrollView->getCanvasSize().width, finalBottom);
            debugLogScrollView->setVisibleHScroll(false);
        }
    }
}

// I haven't reverse-engineered this function, it probably does more than just load mods
// but we hook it for sync'ing with the mod loader
void (*LoadMods_orig)(Kenshi::GameWorld* gameWorld);

// used so we can wait for mods to load on the main thread, so we don't have to do post-mod init in LoadMods_hook() directly
bool setupMods = false;
boost::mutex modLoadLock;
boost::condition_variable modLoadCV;

void LoadMods_hook(Kenshi::GameWorld* gameWorld)
{
    LoadMods_orig(gameWorld);
    if (!setupMods)
    {
        DebugLog("Load mod hook");
        boost::lock_guard<boost::mutex> lock(modLoadLock);
        setupMods = true;
        modLoadCV.notify_all();
    }

    return;
}

void WaitForModSetup()
{
    boost::unique_lock<boost::mutex> lock(modLoadLock);
    while (!setupMods)
    {
        modLoadCV.wait(lock);
    }
}

// has to be done on the GUI thread
void DisplayVersionError(float timeDelta)
{
    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

    MyGUI::TextBox* versionText = Kenshi::FindWidget(gui->getEnumerator(), "VersionText")->castType<MyGUI::TextBox>(false);
    if (versionText != nullptr)
    {
        MyGUI::UString version = versionText->getCaption();
        if (version == "")
            ErrorLog("Version text has unexpected value");
        versionText->setCaption("RE_Kenshi " + Version::GetDisplayVersion() + " (ERROR) - " + version + boost::locale::gettext(" - [NOT SUPPORTED, MOD DISABLED]"));
        gui->eventFrameStart -= MyGUI::newDelegate(DisplayVersionError);
    }
}

void dllmain()
{
    DebugLog("RE_Kenshi " + Version::GetDisplayVersion());

    Kenshi::BinaryVersion gameVersion = Kenshi::GetKenshiVersion();

    // this has to be done *before* the file I/O hook since that causes settings reads
    Settings::Init();

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
    
    Version::Init();

    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        Bugs::Init();
        ShaderCache::Init();
        HeightmapHook::Preload();
        MiscHooks::Init();
        Sound::Init();

        DebugLog("Waiting for mod setup.");
        WaitForModSetup();

        Settings::LoadModOverrides();
        Sound::TryLoadQueuedBanks();
        HeightmapHook::Init();
    }

    DebugLog("Waiting for main menu.");
    WaitForMainMenu();

    MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
    
    if (gameVersion.GetPlatform() != Kenshi::BinaryVersion::UNKNOWN)
    {
        // GUI will be created next frame
        gui->eventFrameStart += MyGUI::newDelegate(GUIUpdate);

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
        DebugLog("ERROR: Game version not recognized.");
        DebugLog("");
        DebugLog("Supported versions:");
        DebugLog("GOG 1.0.59");
        DebugLog("Steam 1.0.55, 1.0.62, 1.0.64");
        DebugLog("RE_Kenshi initialization aborted!");

        // doing this on our thread is unsafe, need to do it on the GUI thread so we don't access UI elements as the game creates them
        // if we try to read + modify the version text on our current thread, the code fails about 50% of the time
        gui->eventFrameStart += MyGUI::newDelegate(DisplayVersionError);
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

    // HACK this has to be done BEFORE switching to (non-borderless) fullscreen or bad things happen
    // it should really be run in a hook
    IO::Init();
    CreateThread(NULL, 0, threadWrapper, 0, 0, 0);
}
