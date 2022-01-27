#pragma once

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

#include <imagine/config/defs.hh>
#include <imagine/base/timerDefs.hh>
#include <imagine/base/EventLoop.hh>
#include <imagine/util/concepts.hh>

#if defined __linux__
#include <imagine/base/timer/TimerFD.hh>
#elif defined __APPLE__
#include <imagine/base/timer/CFTimer.hh>
#endif

namespace IG
{

struct Timer : public TimerImpl
{
public:
	struct NullInit{};

	using TimerImpl::TimerImpl;
	explicit constexpr Timer(NullInit) {}
	Timer() : Timer{CallbackDelegate{}} {}
	Timer(const char *debugLabel): Timer{debugLabel, CallbackDelegate{}} {}
	void run(Time time, Time repeatTime, bool isAbsoluteTime = false, EventLoop loop = {}, CallbackDelegate c = {});
	void cancel();
	void setCallback(CallbackDelegate c);
	void dispatchEarly();
	bool isArmed();
	explicit operator bool() const;

	void runIn(IG::ChronoDuration auto time,
		IG::ChronoDuration auto repeatTime,
		EventLoop loop = {}, CallbackDelegate f = {})
	{
		run(std::chrono::duration_cast<Time>(time),
			std::chrono::duration_cast<Time>(repeatTime), false, loop, f);
	}

	void runAt(IG::ChronoDuration auto time,
		IG::ChronoDuration auto repeatTime,
		EventLoop loop = {}, CallbackDelegate f = {})
	{
		run(std::chrono::duration_cast<Time>(time),
			std::chrono::duration_cast<Time>(repeatTime), true, loop, f);
	}

	// non-repeating timer
	void runIn(IG::ChronoDuration auto time, EventLoop loop = {}, CallbackDelegate f = {})
	{
		run(std::chrono::duration_cast<Time>(time), Time{}, false, loop, f);
	}

	void runAt(IG::ChronoDuration auto time, EventLoop loop = {}, CallbackDelegate f = {})
	{
		run(std::chrono::duration_cast<Time>(time), Time{}, true, loop, f);
	}
};

}
