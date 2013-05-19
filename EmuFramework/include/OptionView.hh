#pragma once

/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <gui/View.hh>
#include <gui/AlertView.hh>
#include <gui/MenuItem/MenuItem.hh>
#include <util/gui/BaseMenuView.hh>
#include <audio/Audio.hh>
#include <EmuInput.hh>
#include <EmuOptions.hh>
#include <MultiChoiceView.hh>
#include <EmuView.hh>
#include <FilePicker.hh>
#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
#include <TouchConfigView.hh>
extern TouchConfigView tcMenu;
#endif
extern ViewStack viewStack;
extern EmuView emuView;
void setupStatusBarInMenu();
void setupFont();
void applyOSNavStyle();
Gfx::BufferImage *getArrowAsset();
extern WorkDirStack<1> workDirStack;
void onCloseModalPopWorkDir(const Input::Event &e);
void chdirFromFilePath(const char *path);

class OptionView : public BaseMenuView
{
protected:
	// Video
	#ifdef CONFIG_BASE_ANDROID
		#ifdef SUPPORT_ANDROID_DIRECT_TEXTURE
		BoolMenuItem directTexture;
		#endif
		#if CONFIG_ENV_ANDROID_MINSDK >= 9
		BoolMenuItem surfaceTexture;
		#endif
	BoolMenuItem glSyncHack;
	#endif
	MultiChoiceSelectMenuItem frameSkip;
	void frameSkipInit();
	MultiChoiceSelectMenuItem aspectRatio;
	void aspectRatioInit();
	MultiChoiceSelectMenuItem zoom;
	void zoomInit();
	MultiChoiceSelectMenuItem imgFilter;
	void imgFilterInit();
	MultiChoiceSelectMenuItem overlayEffect;
	void overlayEffectInit();
	MultiChoiceSelectMenuItem overlayEffectLevel;
	void overlayEffectLevelInit();
	#if defined (CONFIG_BASE_X11) || defined (CONFIG_BASE_ANDROID) || defined (CONFIG_BASE_IOS)
	BoolMenuItem bestColorModeHint;
	void bestColorModeHintHandler(BoolMenuItem &item, const Input::Event &e);
	#endif
	BoolMenuItem dither;
	MultiChoiceSelectMenuItem gameOrientation;
	void gameOrientationInit();

	// Audio
	BoolMenuItem snd;
	#ifdef CONFIG_AUDIO_CAN_USE_MAX_BUFFERS_HINT
	MultiChoiceSelectMenuItem soundBuffers;
	void soundBuffersInit();
	#endif
	MultiChoiceSelectMenuItem audioRate;
	void audioRateInit();
	#ifdef CONFIG_AUDIO_OPENSL_ES
	BoolMenuItem sndUnderrunCheck;
	#endif
	#ifdef CONFIG_AUDIO_SOLO_MIX
	BoolMenuItem audioSoloMix;
	#endif

	// Input
	#if defined(CONFIG_INPUT_ANDROID) && CONFIG_ENV_ANDROID_MINSDK >= 9
	BoolMenuItem useOSInputMethod;
	#endif
	#ifdef CONFIG_EMUFRAMEWORK_VCONTROLS
		#ifdef CONFIG_ENV_WEBOS
		BoolMenuItem touchCtrl;
		#else
		MultiChoiceSelectMenuItem touchCtrl;
		#endif
	void touchCtrlInit();
	TextMenuItem touchCtrlConfig;
	#endif
	BoolMenuItem altGamepadConfirm;
	#ifdef CONFIG_BLUETOOTH_SCAN_SECS
	MultiChoiceSelectMenuItem btScanSecs;
	void btScanSecsInit();
	#endif
	#ifdef CONFIG_BLUETOOTH
	BoolMenuItem keepBtActive;
	#endif
	#ifdef CONFIG_BLUETOOTH_SCAN_CACHE_USAGE
	BoolMenuItem btScanCache;
	#endif
	MultiChoiceSelectMenuItem relativePointerDecel;
	void relativePointerDecelInit();

	// System
	MultiChoiceSelectMenuItem autoSaveState;
	void autoSaveStateInit();
	BoolMenuItem confirmAutoLoadState;
	BoolMenuItem confirmOverwriteState;
	void savePathUpdated(const char *newPath);
	char savePathStr[256] {0};
	TextMenuItem savePath;
	#if defined CONFIG_BASE_ANDROID && CONFIG_ENV_ANDROID_MINSDK >= 9
	void processPriorityInit();
	MultiChoiceSelectMenuItem processPriority;
	#endif
	MultiChoiceSelectMenuItem statusBar;
	void statusBarInit();

	// GUI
	BoolMenuItem pauseUnfocused {Config::envIsPS3 ? "Pause in XMB" : "Pause if unfocused"};
	MultiChoiceSelectMenuItem fontSize {"Large Fonts"};
	void fontSizeInit();
	BoolMenuItem notificationIcon {"Suspended App Icon"};
	BoolMenuItem lowProfileOSNav {"Dim OS Navigation"};
	BoolMenuItem hideOSNav {"Hide OS Navigation"};
	BoolMenuItem idleDisplayPowerSave {"Dim Screen If Idle"};
	BoolMenuItem navView {"Title Bar"};
	BoolMenuItem backNav {"Title Back Navigation"};
	BoolMenuItem rememberLastMenu {"Remember Last Menu"};
	MultiChoiceSelectMenuItem menuOrientation {"Orientation"};
	void menuOrientationInit();

	virtual void loadVideoItems(MenuItem *item[], uint &items);
	virtual void loadAudioItems(MenuItem *item[], uint &items);
	virtual void loadInputItems(MenuItem *item[], uint &items);
	virtual void loadSystemItems(MenuItem *item[], uint &items);
	virtual void loadGUIItems(MenuItem *item[], uint &items);

	MenuItem *item[24] {nullptr};
public:
	OptionView();//: BaseMenuView("Options") { }

	void init(uint idx, bool highlightFirst);
};

class BiosSelectMenu : public BaseMultiChoiceView
{
public:
	constexpr BiosSelectMenu() { }
	constexpr BiosSelectMenu(FsSys::cPath *biosPathStr, int (*fsFilter)(const char *name, int type))
		:biosPathStr(biosPathStr), fsFilter(fsFilter) { }
	TextMenuItem choiceEntry[2];
	MenuItem *choiceEntryItem[2] {nullptr};
	typedef DelegateFunc<void ()> BiosChangeDelegate;
	BiosChangeDelegate onBiosChangeD;
	BiosChangeDelegate &onBiosChange() { return onBiosChangeD; };
	FsSys::cPath *biosPathStr = nullptr;
	int (*fsFilter)(const char *name, int type) = nullptr;

	void onSelectFile(const char* name, const Input::Event &e)
	{
		logMsg("size %d", (int)sizeof(*biosPathStr));
		snprintf(*biosPathStr, sizeof(*biosPathStr), "%s/%s", FsSys::workDir(), name);
		if(onBiosChangeD) onBiosChangeD();
		View::removeModalView();
		workDirStack.pop();
	}

	void init(FsSys::cPath *biosPathStr, int (*fsFilter)(const char *name, int type), bool highlightFirst)
	{
		var_selfs(biosPathStr);
		var_selfs(fsFilter);
		init(highlightFirst);
	}

	void init(bool highlightFirst)
	{
		assert(biosPathStr);
		choiceEntry[0].init("Select File"); choiceEntryItem[0] = &choiceEntry[0];
		choiceEntry[0].onSelect() =
			[this](TextMenuItem &, const Input::Event &e)
			{
				removeModalView();
				workDirStack.push();
				chdirFromFilePath(*biosPathStr);
				auto &fPicker = *allocModalView<EmuFilePicker>();
				fPicker.init(!e.isPointer(), fsFilter);
				fPicker.onSelectFile() =
					[this](const char* name, const Input::Event &e)
					{
						onSelectFile(name, e);
					};
				fPicker.onClose() =
					[](const Input::Event &e)
					{
						View::removeModalView();
						workDirStack.pop();
					};
				View::addModalView(fPicker);
			};
		choiceEntry[1].init("Unset"); choiceEntryItem[1] = &choiceEntry[1];
		choiceEntry[1].onSelect() =
			[this](TextMenuItem &, const Input::Event &e)
			{
				removeModalView();
				strcpy(*biosPathStr, "");
				if(onBiosChangeD) onBiosChangeD();
			};
		BaseMenuView::init(choiceEntryItem, sizeofArray(choiceEntry), highlightFirst, C2DO);
	}
};
