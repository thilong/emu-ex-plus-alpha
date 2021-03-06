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

#include <imagine/gui/View.hh>
#include <imagine/gui/MenuItem.hh>
#include <imagine/gfx/Renderer.hh>
#include <imagine/gfx/RendererTask.hh>
#include <imagine/base/Window.hh>
#include <imagine/util/string.h>
#include <imagine/logger/logger.h>

Gfx::GlyphTextureSet View::defaultFace{};
Gfx::GlyphTextureSet View::defaultBoldFace{};
bool View::needsBackControl = needsBackControlDefault;

Gfx::Renderer &ViewAttachParams::renderer() const
{
	return rTask.renderer();
}

void ViewController::pushAndShow(std::unique_ptr<View> v, Input::Event e)
{
	pushAndShow(std::move(v), e, true, false);
}

void ViewController::pop()
{
	dismissView(-1, false);
}

void ViewController::popAndShow()
{
	dismissView(-1);
}

void ViewController::popTo(View &v)
{
	logErr("popTo() not implemented for this controller");
}

bool ViewController::inputEvent(Input::Event)
{
	return false;
}

bool ViewController::moveFocusToNextView(Input::Event, _2DOrigin)
{
	return false;
};

View::View() {}

View::View(ViewAttachParams attach):
	View{{}, attach}
{}

View::View(NameString name, ViewAttachParams attach):
	win(&attach.window()), rendererTask_{&attach.rendererTask()}, nameStr{std::move(name)}
{}

View::View(const char *name, ViewAttachParams attach):
	View{makeNameString(name), attach}
{}

View::~View() {}

void View::pushAndShow(std::unique_ptr<View> v, Input::Event e, bool needsNavView, bool isModal)
{
	assumeExpr(controller_);
	controller_->pushAndShow(std::move(v), e, needsNavView, isModal);
}

void View::pushAndShowModal(std::unique_ptr<View> v, Input::Event e, bool needsNavView)
{
	pushAndShow(std::move(v), e, needsNavView, true);
}

void View::popTo(View &v)
{
	assumeExpr(controller_);
	controller_->popTo(v);
}

void View::dismiss(bool refreshLayout)
{
	if(controller_)
	{
		controller_->dismissView(*this, refreshLayout);
	}
	else
	{
		logWarn("called dismiss with no controller");
	}
}

void View::dismissPrevious()
{
	if(controller_)
	{
		controller_->dismissView(-2);
	}
	else
	{
		logWarn("called dismissPrevious with no controller");
	}
}

bool View::compileGfxPrograms(Gfx::Renderer &r)
{
	r.make(imageCommonTextureSampler);
	auto compiled = r.makeCommonProgram(Gfx::CommonProgram::NO_TEX);
	// for text
	compiled |= r.makeCommonProgram(Gfx::CommonProgram::TEX_ALPHA);
	return compiled;
}

Gfx::Color View::menuTextColor(bool isSelected)
{
	return isSelected ? Gfx::color(0.f, .8f, 1.f) : Gfx::color(Gfx::ColorName::WHITE);
}

void View::clearSelection() {}

void View::onShow() {}

void View::onHide() {}

void View::onAddedToController(ViewController *, Input::Event) {}

void View::prepareDraw() {}

void View::setFocus(bool) {}

void View::setViewRect(IG::WindowRect rect, Gfx::ProjectionPlane projP)
{
	this->viewRect_ = rect;
	this->projP = projP;
}

void View::setViewRect( Gfx::ProjectionPlane projP)
{
	setViewRect(projP.viewport().bounds(), projP);
}

void View::postDraw()
{
	if(likely(win))
		win->postDraw();
}

Base::Window &View::window() const
{
	assumeExpr(win);
	return *win;
}

Gfx::Renderer &View::renderer() const
{
	assumeExpr(rendererTask_);
	return rendererTask_->renderer();
}

Gfx::RendererTask &View::rendererTask() const
{
	assumeExpr(rendererTask_);
	return *rendererTask_;
}

ViewAttachParams View::attachParams() const
{
	return {*win, *rendererTask_};
}

Base::Screen *View::screen() const
{
	return win ? win->screen() : nullptr;
}

View::NameStringView View::name() const
{
	return nameStr;
}

void View::setName(const char *str)
{
	if(!str)
	{
		nameStr.clear();
		nameStr.shrink_to_fit();
		return;
	}
	nameStr = makeNameString(str);
}

void View::setName(NameString name)
{
	nameStr = std::move(name);
}

View::NameString View::makeNameString(const char *name)
{
	if(!name)
	{
		return {};
	}
	return string_makeUTF16(name);
}

View::NameString View::makeNameString(const BaseTextMenuItem &item)
{
	return NameString{item.text().stringView()};
}

void View::setNeedsBackControl(bool on)
{
	if(!needsBackControlIsConst) // only modify on environments that make sense
	{
		needsBackControl = on;
	}
}

void View::show()
{
	prepareDraw();
	onShow();
	//logMsg("showed view");
	postDraw();
}

bool View::moveFocusToNextView(Input::Event e, _2DOrigin direction)
{
	if(!controller_)
		return false;
	return controller_->moveFocusToNextView(e, direction);
}

void View::setWindow(Base::Window *w)
{
	win = w;
}

void View::setOnDismiss(DismissDelegate del)
{
	dismissDel = del;
}

void View::onDismiss()
{
	dismissDel.callSafe(*this);
}

void View::setController(ViewController *c, Input::Event e)
{
	controller_ = c;
	if(c)
	{
		onAddedToController(c, e);
	}
}

ViewController *View::controller() const
{
	return controller_;
}

IG::WindowRect View::viewRect() const
{
	return viewRect_;
}

Gfx::ProjectionPlane View::projection() const
{
	return projP;
}

bool View::pointIsInView(IG::WP pos)
{
	return viewRect().overlaps(pos);
}

void View::waitForDrawFinished()
{
	// currently a no-op due to RendererTask only running present() async
	/*assumeExpr(rendererTask_);
	rendererTask_->waitForDrawFinished();*/
}
