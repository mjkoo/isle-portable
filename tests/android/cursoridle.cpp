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
		idle.Reveal(10 * second, true);
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
		idle.Reveal(0, true);
		assert(!idle.Tick(5 * second, true));
		assert(!idle.Tick(5 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(5 * second + CursorIdle::c_hideDelay, false));
	}

	// Revealing reports whether the cursor had been hidden, and restarts the delay.
	{
		CursorIdle idle;
		assert(!idle.Reveal(0, true));
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Reveal(3 * second, true));
		assert(!idle.IsHidden());
		assert(!idle.Tick(3 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(3 * second + CursorIdle::c_hideDelay, false));
	}

	// A mouse taking over brings a hidden cursor back and keeps it up, until the stick moves it
	// again.
	{
		CursorIdle idle;
		idle.Reveal(0, true);
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Reveal(5 * second, false));
		assert(!idle.Tick(60 * second, false));
		idle.Reveal(70 * second, true);
		assert(idle.Tick(70 * second + CursorIdle::c_hideDelay, false));
	}

	return 0;
}
