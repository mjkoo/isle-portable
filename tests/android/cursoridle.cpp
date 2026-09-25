#include "cursoridle.h"

#include <cassert>

int main()
{
	const uint64_t second = 1000000000ULL;

	// A cursor nothing has moved yet belongs to no stick, so it never hides. This is DOS at
	// startup, where the mouse drives the game-drawn cursor.
	{
		CursorIdle idle;
		assert(!idle.Tick(60 * second, false));
		assert(!idle.IsHidden());
	}

	// A cursor the stick just moved stays up until the delay has passed, then hides exactly once.
	{
		CursorIdle idle;
		idle.Moved(10 * second, true);
		assert(!idle.Tick(10 * second + CursorIdle::c_hideDelay - 1, false));
		assert(!idle.IsHidden());
		assert(idle.Tick(10 * second + CursorIdle::c_hideDelay, false));
		assert(idle.IsHidden());
		assert(!idle.Tick(20 * second, false));
	}

	// A held button is a drag in progress: the cursor stays, and the delay starts again from the
	// release.
	{
		CursorIdle idle;
		idle.Moved(0, true);
		assert(!idle.Tick(5 * second, true));
		assert(!idle.Tick(5 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(5 * second + CursorIdle::c_hideDelay, false));
	}

	// Moving reports whether the cursor had been hidden, and restarts the delay.
	{
		CursorIdle idle;
		assert(!idle.Moved(0, true));
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Moved(3 * second, true));
		assert(!idle.IsHidden());
		assert(!idle.Tick(3 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(3 * second + CursorIdle::c_hideDelay, false));
	}

	// A mouse taking over brings a hidden cursor back and keeps it up, until the stick moves it
	// again.
	{
		CursorIdle idle;
		idle.Moved(0, true);
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Moved(5 * second, false));
		assert(!idle.Tick(60 * second, false));
		idle.Moved(70 * second, true);
		assert(idle.Tick(70 * second + CursorIdle::c_hideDelay, false));
	}

	// A press brings a hidden cursor back and restarts the delay, but leaves it with whatever last
	// moved it: a stick-moved cursor hides again, a mouse-moved one never starts.
	{
		CursorIdle idle;
		idle.Moved(0, true);
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Pressed(3 * second));
		assert(!idle.Tick(3 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(3 * second + CursorIdle::c_hideDelay, false));

		CursorIdle mouse;
		mouse.Moved(0, false);
		assert(!mouse.Pressed(second));
		assert(!mouse.Tick(60 * second, false));
	}

	return 0;
}
