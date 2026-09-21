#ifndef OUTPUTGAIN_H
#define OUTPUTGAIN_H

// The one owner of the mixer's master volume.
//
// Two things turn the game down - its own pause, and whatever the system has done to the sound on
// platforms that report that - and both reach the mix through the same engine volume. Written
// independently the later one undoes the earlier: a pause lifted by the next focus change, a
// resume playing over a duck the system asked for.
//
// The pause is why this exists. MxSoundManager::Pause reaches the wave presenters and nothing
// else, so cached and 3D sounds played on through a menu; the master volume is the one knob that
// reaches them. It silences them rather than stopping them, so those keep advancing while they are
// down, where a wave presenter is genuinely paused and picks up where it left off.
//
// It reaches a pause only where the caller does. The main loop is not running to poll it while an
// Emscripten tab is hidden, or between a 3DS sleep and its wakeup, and Vita and Switch raise no
// pause at all outside the game's own Pause key. See docs/android-audio.md.
//
// Owned by the SDL thread, which is where every setter and the take are called from.
class OutputGain {
public:
	// The game's own pause, whatever raised it: a menu, a quit prompt, a lost window, or LEGO1's
	// own Pause key.
	void SetPaused(bool p_paused) { m_paused = p_paused; }

	// What the system last left the game's sound at. Platforms with nothing to say about it leave
	// this at full volume for the life of the process.
	void SetFocus(float p_focus) { m_focus = p_focus; }

	// True when the gain moved, and only then, so the caller never re-applies what is already set.
	bool Take(float* p_gain)
	{
		// A pause wins outright rather than scaling what the system left, so that resuming
		// restores the gain the system asked for. Multiplying the two would reach the same values
		// today, both factors being exact; this says which one is in charge.
		float gain = m_paused ? 0.0f : m_focus;
		if (gain == m_applied) {
			return false;
		}
		m_applied = gain;
		*p_gain = gain;
		return true;
	}

private:
	bool m_paused = false;
	float m_focus = 1.0f;
	float m_applied = 1.0f;
};

#endif
