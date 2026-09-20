#ifndef ANDROID_AUDIOFOCUS_H
#define ANDROID_AUDIOFOCUS_H

#include <atomic>

// What the system does to an application's sound when something else needs it. Java translates
// the AudioManager.AUDIOFOCUS_* constants into these, so this header stays free of them and the
// policy below can be tested on a host.
//
// Reported from the thread Android delivers the change on and taken on the SDL thread. Only the
// latest report survives: SDL_AppIterate does not run while the activity is paused, so a loss and
// the gain that undoes it can both arrive before the game looks again.
class AudioFocus {
public:
	enum Change {
		e_gain = 0,
		e_loss = 1,
		e_lossTransient = 2,
		e_lossTransientCanDuck = 3
	};

	// Loud enough to keep dialogue intelligible under a navigation prompt, quiet enough that the
	// prompt wins. Not measured; see docs/android-install.md.
	static constexpr float c_duckedGain = 0.2f;

	static float GainFor(Change p_change)
	{
		switch (p_change) {
		case e_lossTransientCanDuck:
			return c_duckedGain;
		case e_loss:
		case e_lossTransient:
			return 0.0f;
		case e_gain:
			break;
		}
		return 1.0f;
	}

	// False for anything outside the enum, which leaves the gain alone rather than guessing. Java
	// only sends translated values, so this catches a constant the two sides stopped agreeing on.
	bool Report(int p_change)
	{
		if (p_change < e_gain || p_change > e_lossTransientCanDuck) {
			return false;
		}
		m_change.store(p_change, std::memory_order_relaxed);
		return true;
	}

	// True when the gain moved, and only then, so the caller never re-applies what is already set.
	bool Take(float* p_gain)
	{
		float gain = GainFor(static_cast<Change>(m_change.load(std::memory_order_relaxed)));
		if (gain == m_applied) {
			return false;
		}
		m_applied = gain;
		*p_gain = gain;
		return true;
	}

private:
	std::atomic<int> m_change{e_gain};
	float m_applied = 1.0f;
};

// Reports a focus change from the thread Android delivered it on.
void Android_ReportAudioFocus(int p_change);

// Reads the gain the game should now play at, on the SDL thread. True when it changed.
bool Android_TakeAudioGain(float* p_gain);

#endif
