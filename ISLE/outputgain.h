#ifndef OUTPUTGAIN_H
#define OUTPUTGAIN_H

// The one owner of the mixer's master volume.
//
// Two things turn the game down: its own pause, and whatever the system has done to the sound on
// platforms that report that. Both reach the mix through the same engine volume, so both are
// composed here rather than written to it independently, where the later writer would undo the
// earlier one - a pause would be lifted by the next focus change, and a resume would play over a
// duck the system had asked for.
//
// The game's pause is the reason this exists at all: MxSoundManager::Pause reaches the wave
// presenters and nothing else, so cached and 3D sounds play on through a menu. The master volume
// is the one knob that reaches all of them. They keep advancing while it is down; only their sound
// is gone.
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

	// True when the gain moved, and only then, so the caller never rewrites what is already set.
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
