#ifndef CURSORIDLE_H
#define CURSORIDLE_H

#include <cstdint>

// When to take the game-drawn cursor out of sight.
//
// A stick-driven cursor has no hand on a mouse to explain it, so left alone it sits over the scene
// for good. This hides it once nothing has moved or pressed it for a while, and brings it back at
// the same spot on the next nudge. A held button keeps it up, since a drag is still in progress.
//
// Only a cursor the stick last moved ever hides. Where a real mouse drives the game-drawn cursor,
// as on DOS, it stays put like any other mouse pointer.
//
// Times are nanoseconds from any monotonic clock, such as SDL_GetTicksNS. Owned by the SDL thread.
class CursorIdle {
public:
	static constexpr uint64_t c_hideDelay = 2000000000ULL;

	// The player moved the cursor, with a stick or gamepad when p_stickDriven, otherwise with a mouse.
	// Returns whether it had been hidden; bringing it back is the caller's to do.
	bool Moved(uint64_t p_now, bool p_stickDriven)
	{
		m_stickDriven = p_stickDriven;
		return Pressed(p_now);
	}

	// The player pressed a button without moving the cursor, which leaves it with whatever last moved
	// it. Returns whether it had been hidden, like Moved.
	bool Pressed(uint64_t p_now)
	{
		bool wasHidden = m_hidden;
		m_hidden = false;
		m_lastActive = p_now;
		return wasHidden;
	}

	// Called while the cursor is on screen. Returns true once per idle stretch, at the moment the
	// caller should hide it.
	bool Tick(uint64_t p_now, bool p_buttonHeld)
	{
		if (p_buttonHeld) {
			m_lastActive = p_now;
		}
		if (!m_stickDriven || m_hidden || p_now - m_lastActive < c_hideDelay) {
			return false;
		}
		m_hidden = true;
		return true;
	}

	bool IsHidden() const { return m_hidden; }

private:
	uint64_t m_lastActive = 0;
	bool m_stickDriven = false;
	bool m_hidden = false;
};

#endif // CURSORIDLE_H
