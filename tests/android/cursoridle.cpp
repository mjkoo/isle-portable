#include "cursoridle.h"

#include <cassert>

int main()
{
	const uint64_t second = 1000000000ULL;

	// A cursor that was just moved stays up until the delay has passed, then hides exactly once.
	{
		CursorIdle idle;
		idle.Reveal(10 * second);
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
		idle.Reveal(0);
		assert(!idle.Tick(5 * second, true));
		assert(!idle.Tick(5 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(5 * second + CursorIdle::c_hideDelay, false));
	}

	// Revealing reports whether the cursor had been hidden, and restarts the delay.
	{
		CursorIdle idle;
		assert(!idle.Reveal(0));
		assert(idle.Tick(CursorIdle::c_hideDelay, false));
		assert(idle.Reveal(3 * second));
		assert(!idle.IsHidden());
		assert(!idle.Tick(3 * second + CursorIdle::c_hideDelay - 1, false));
		assert(idle.Tick(3 * second + CursorIdle::c_hideDelay, false));
	}

	return 0;
}
