/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "main"
#include <memory>
#include <imagine/logger/logger.h>
#include <imagine/gfx/GfxSprite.hh>
#include <imagine/gfx/GfxText.hh>
#include <imagine/gfx/Renderer.hh>
#include <imagine/gfx/RendererTask.hh>
#include <imagine/gfx/RendererCommands.hh>
#include <imagine/gfx/DrawableHolder.hh>
#include <imagine/base/Base.hh>
#include <imagine/base/Screen.hh>
#include <imagine/base/Window.hh>
#include <imagine/base/platformExtras.hh>
#include <imagine/util/string.h>
#include "tests.hh"
#include "TestPicker.hh"
#include "cpuUtils.hh"
#ifdef __ANDROID__
#include <imagine/base/android/RootCpufreqParamSetter.hh>
#endif

static constexpr uint framesToRun = 60*60;

struct WindowData
{
	WindowData(ViewAttachParams attachParams):picker{attachParams} {}

	Gfx::DrawableHolder drawableHolder{};
	Gfx::Projection proj{};
	IG::WindowRect testRectWin{};
	Gfx::GCRect testRect{};
	TestPicker picker;
};

static std::unique_ptr<Gfx::Renderer> rendererPtr{};
static std::unique_ptr<TestFramework> activeTest{};
#ifdef __ANDROID__
static std::unique_ptr<Base::RootCpufreqParamSetter> cpuFreq{};
#endif

static void finishTest(Base::Window &win, Gfx::Renderer &r, IG::FrameTime frameTime);

static WindowData &windowData(const Base::Window &win)
{
	return *win.customData<WindowData>();
}

static void setPickerHandlers(Base::Window &win, Gfx::Renderer &r)
{
	win.setOnDraw(
		[&task = r.task()](Base::Window &win, Base::Window::DrawParams params)
		{
			auto &winData = windowData(win);
			task.draw(winData.drawableHolder, win, params, {}, winData.proj.plane().viewport(), winData.proj.matrix(),
				[&picker = winData.picker](Gfx::DrawableHolder &drawableHolder, Base::Window &win, Gfx::RendererCommands &cmds)
				{
					cmds.clear();
					picker.draw(cmds);
					cmds.setClipTest(false);
					cmds.present();
				});
			return false;
		});
}

static void setActiveTestHandlers(Base::Window &win, Gfx::Renderer &r)
{
	win.addOnFrame([&win, &r](Base::FrameParams params)
		{
			auto atOnFrame = IG::steadyClockTimestamp();
			auto &winData = windowData(win);
			r.setPresentationTime(winData.drawableHolder, params.presentTime());
			if(activeTest->started)
			{
				activeTest->frameUpdate(r.task(), win, params);
			}
			else
			{
				activeTest->started = true;
			}
			activeTest->lastFramePresentTime.timestamp = params.timestamp();
			activeTest->lastFramePresentTime.atOnFrame = atOnFrame;
			if(activeTest->frames == framesToRun || activeTest->shouldEndTest)
			{
				finishTest(win, r, params.timestamp());
				return false;
			}
			else
			{
				win.postDraw();
				return true;
			}
		});
	win.setOnDraw(
		[&task = r.task()](Base::Window &win, Base::Window::DrawParams params)
		{
			auto &winData = windowData(win);
			task.draw(winData.drawableHolder, win, params, {}, winData.proj.plane().viewport(), winData.proj.matrix(),
				[rect = winData.testRectWin](Gfx::DrawableHolder &drawableHolder, Base::Window &win, Gfx::RendererCommands &cmds)
				{
					cmds.setClipTest(false);
					activeTest->draw(cmds, cmds.renderer().makeClipRect(win, rect));
					activeTest->lastFramePresentTime.atWinPresent = IG::steadyClockTimestamp();
					activeTest->presentFence = cmds.clientWaitSyncReset(activeTest->presentFence);
					cmds.present();
				});
			return false;
		});
}

static void placeElements(const Base::Window &win, Gfx::Renderer &r)
{
	auto &winData = windowData(win);
	auto &picker = winData.picker;
	auto projP = winData.proj.plane();
	TableView::setDefaultXIndent(win, projP);
	if(!activeTest)
	{
		picker.setViewRect(projP);
		picker.place();
	}
	else
	{
		activeTest->place(r, projP, winData.testRect);
	}
}

static void finishTest(Base::Window &win, Gfx::Renderer &r, IG::FrameTime frameTime)
{
	if(activeTest)
	{
		activeTest->finish(r.task(), frameTime);
	}
	r.task().awaitPending();
	activeTest.reset();
	deinitCPUFreqStatus();
	deinitCPULoadStatus();
	Base::setIdleDisplayPowerSave(true);
	#ifdef __ANDROID__
	if(cpuFreq)
		cpuFreq->setDefaults();
	Base::setSustainedPerformanceMode(false);
	#endif
	placeElements(win, r);
	setPickerHandlers(win, r);
	win.postDraw();
}

TestFramework *startTest(Base::Window &win, Gfx::Renderer &r, const TestParams &t)
{
	#ifdef __ANDROID__
	if(cpuFreq)
		cpuFreq->setLowLatency();
	Base::setSustainedPerformanceMode(true);
	#endif
	switch(t.test)
	{
		bcase TEST_CLEAR:
			activeTest = std::make_unique<ClearTest>();
		bcase TEST_DRAW:
			activeTest = std::make_unique<DrawTest>();
		bcase TEST_WRITE:
			activeTest = std::make_unique<WriteTest>();
	}
	activeTest->init(r, t.pixmapSize, t.bufferMode);
	Base::setIdleDisplayPowerSave(false);
	initCPUFreqStatus();
	initCPULoadStatus();
	placeElements(win, r);
	auto &winData = windowData(win);
	setActiveTestHandlers(win, r);
	return activeTest.get();
}

namespace Base
{

void onInit(int argc, char** argv)
{
	{
		auto [r, err] = Gfx::Renderer::makeConfiguredRenderer();
		if(err)
		{
			Base::exitWithErrorMessagePrintf(-1, "Error creating renderer: %s", err->what());
			return;
		}
		rendererPtr = std::make_unique<Gfx::Renderer>(std::move(r));
	}
	auto &renderer = *rendererPtr;
	View::compileGfxPrograms(renderer);
	View::defaultFace = Gfx::GlyphTextureSet::makeSystem(renderer, IG::FontSettings{});

	WindowConfig winConf;

	winConf.setOnSurfaceChange(
		[&renderer](Base::Window &win, Base::Window::SurfaceChange change)
		{
			auto &winData = windowData(win);
			renderer.task().updateDrawableForSurfaceChange(winData.drawableHolder, win, change);
			if(change.resized())
			{
				auto viewport = Gfx::Viewport::makeFromWindow(win);
				winData.proj = {viewport, Gfx::Mat4::makePerspectiveFovRH(M_PI/4.0, viewport.realAspectRatio(), 1.0, 100.)};
				winData.testRectWin = viewport.rectWithRatioBestFitFromViewport(0, 0, 4./3., C2DO, C2DO);
				winData.testRect = winData.proj.plane().unProjectRect(winData.testRectWin);
				placeElements(win, renderer);
			}
		});

	winConf.setOnInputEvent(
		[](Base::Window &win, Input::Event e)
		{
			if(!activeTest)
			{
				if(e.pushed() && !e.repeated() && e.isDefaultCancelButton())
				{
					Base::exit();
					return true;
				}
				return windowData(win).picker.inputEvent(e);
			}
			else if(e.pushed() && (e.isDefaultCancelButton() || Config::envIsIOS))
			{
				logMsg("canceled activeTest from input");
				activeTest->shouldEndTest = true;
				return true;
			}
			else if(e.pushed(Input::Keycode::D))
			{
				logMsg("posting extra draw");
				win.postDraw();
			}
			return false;
		});

	auto &mainWin = *renderer.makeWindow(winConf);
	mainWin.setTitle("Frame Rate Test");

	uint faceSize = mainWin.heightSMMInPixels(3.5);
	View::defaultFace.setFontSettings(renderer, faceSize);
	View::defaultFace.precacheAlphaNum(renderer);
	View::defaultFace.precache(renderer, ":.%()");
	std::vector<TestDesc> testDesc;
	testDesc.emplace_back(TEST_CLEAR, "Clear");
	IG::WP pixmapSize{256, 256};
	for(auto desc: renderer.textureBufferModes())
	{
		testDesc.emplace_back(TEST_DRAW, string_makePrintf<64>("Draw RGB565 %ux%u (%s)", pixmapSize.x, pixmapSize.y, desc.name).data(),
			pixmapSize, desc.mode);
		testDesc.emplace_back(TEST_WRITE, string_makePrintf<64>("Write RGB565 %ux%u (%s)", pixmapSize.x, pixmapSize.y, desc.name).data(),
			pixmapSize, desc.mode);
	}
	mainWin.makeCustomData<WindowData>(ViewAttachParams{mainWin, renderer.task()});
	auto &picker = windowData(mainWin).picker;
	picker.setTests(testDesc.data(), testDesc.size());
	setPickerHandlers(mainWin, renderer);
	mainWin.show();

	Base::addOnResume(
		[&renderer, &mainWin](bool focused)
		{
			windowData(mainWin).picker.prepareDraw();
			if(activeTest)
			{
				activeTest->prepareDraw(renderer);
			}
			return true;
		});

	Base::addOnExit(
		[&renderer, &mainWin](bool backgrounded)
		{
			if(backgrounded)
			{
				if(activeTest)
				{
					finishTest(mainWin, renderer, IG::steadyClockTimestamp());
				}
				View::defaultFace.freeCaches();
			}
			return true;
		});

	#ifdef __ANDROID__
	bool manageCPUFreq = false;
	if(manageCPUFreq)
	{
		cpuFreq = std::make_unique<Base::RootCpufreqParamSetter>();
		if(!(*cpuFreq))
		{
			cpuFreq.reset();
		}
	}
	#endif
}

}
