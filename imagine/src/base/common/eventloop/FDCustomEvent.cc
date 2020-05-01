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

#define LOGTAG "CustomEvent"
#include <imagine/base/CustomEvent.hh>
#include <imagine/logger/logger.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/errno.h>
#define USE_EVENTFD
#else
// kqueue
#include <sys/event.h>
static constexpr uintptr_t CUSTOM_IDENT = 1;
#endif

static int makeEventFD()
{
#ifdef USE_EVENTFD
	return eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#else
	int fd = kqueue();
	if(fd == -1)
		return -1;
	struct kevent kev;
	EV_SET(&kev, CUSTOM_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
	kevent(fd, &kev, 1, nullptr, 0, nullptr);
	return fd;
#endif
}

static void notifyEventFD(int fd, const char *debugLabel)
{
#ifdef USE_EVENTFD
	eventfd_t counter = 1;
	auto ret = write(fd, &counter, sizeof(counter));
	if(ret == -1)
	{
		logErr("error writing eventfd (%s)", debugLabel);
	}
#else
	struct kevent kev;
	EV_SET(&kev, CUSTOM_IDENT, EVFILT_USER, EV_ENABLE, NOTE_TRIGGER, 0, nullptr);
	kevent(fd, &kev, 1, nullptr, 0, nullptr);
#endif
}

static void cancelEventFD(int fd, const char *debugLabel)
{
#ifdef USE_EVENTFD
	eventfd_t counter;
	auto ret = read(fd, &counter, sizeof(counter));
	if(ret == -1)
	{
		if(Config::DEBUG_BUILD && errno != EAGAIN)
			logErr("error reading eventfd (%s)", debugLabel);
	}
#else
	struct kevent kev;
	EV_SET(&kev, CUSTOM_IDENT, EVFILT_USER, EV_DISABLE, 0, 0, nullptr);
	kevent(fd, &kev, 1, nullptr, 0, nullptr);
#endif
}

namespace Base
{

#ifdef NDEBUG
FDCustomEvent::FDCustomEvent():
#else
FDCustomEvent::FDCustomEvent(const char *debugLabel):
	debugLabel{debugLabel ? debugLabel : "unnamed"},
#endif
	fdSrc{label(), makeEventFD()},
	callback_{std::make_unique<CustomEventDelegate>()}
{
	if(fdSrc.fd() == -1)
	{
		logErr("error creating fd (%s)", label());
	}
}

FDCustomEvent::FDCustomEvent(FDCustomEvent &&o)
{
	*this = std::move(o);
}

FDCustomEvent &FDCustomEvent::operator=(FDCustomEvent &&o)
{
	deinit();
	fdSrc = std::move(o.fdSrc);
	callback_ = std::move(o.callback_);
	#ifndef NDEBUG
	debugLabel = o.debugLabel;
	#endif
	return *this;
}

FDCustomEvent::~FDCustomEvent()
{
	deinit();
}

void CustomEvent::attach(EventLoop loop)
{
	fdSrc.attach(loop,
		[callback = callback_.get()](int fd, int)
		{
			#ifdef USE_EVENTFD
			eventfd_t counter;
			auto ret = read(fd, &counter, sizeof(counter));
			if(ret == -1)
			{
				if(Config::DEBUG_BUILD && errno != EAGAIN)
					logErr("error reading eventfd in callback");
				return true;
			}
			#else
			struct timespec timeout{};
			struct kevent kev;
			int ret = kevent(fd, nullptr, 0, &kev, 1, &timeout);
			if(ret < 1)
			{
				if(ret == -1)
					logErr("error in kevent() in callback");
				return true;
			}
			#endif
			callback->callSafe();
			return true;
		});
}

void CustomEvent::detach()
{
	fdSrc.detach();
}

void CustomEvent::setCallback(CustomEventDelegate callback)
{
	*callback_ = callback;
}

void CustomEvent::notify()
{
	notifyEventFD(fdSrc.fd(), label());
}

void CustomEvent::cancel()
{
	cancelEventFD(fdSrc.fd(), label());
}

CustomEvent::operator bool() const
{
	return fdSrc.fd() != -1;
}

const char *FDCustomEvent::label()
{
	#ifdef NDEBUG
	return nullptr;
	#else
	return debugLabel;
	#endif
}

void FDCustomEvent::deinit()
{
	fdSrc.closeFD();
}

}
