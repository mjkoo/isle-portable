#ifndef ANDROID_AUDIOFOCUS_H
#define ANDROID_AUDIOFOCUS_H

#include <atomic>

// What the system does to an application's sound when something else needs it.
//
// Reported from the thread Android delivers the change on and taken on the SDL thread. Only the
// latest report survives, because the two run independently: several changes can land between one
// look and the next, and applying anything but the last of them would leave the game at a gain the
// system has already moved on from.
class AudioFocus {
public:
	enum Change {
		e_gain = 0,
		e_loss = 1,
		e_lossTransient = 2,
		e_lossTransientCanDuck = 3
	};

	// The AudioManager.AUDIOFOCUS_* values. Public Android API, fixed since API 8, so Java hands
	// this side the number it was given rather than translating it first: there is no second copy
	// of the mapping to keep in step, and a variant neither side thought about still lands
	// somewhere defined.
	enum AndroidChange {
		e_androidLossTransientCanDuck = -3,
		e_androidLossTransient = -2,
		e_androidLoss = -1,
		e_androidNone = 0,
		e_androidGain = 1
		// 2, 3 and 4 are the GAIN_TRANSIENT variants. Every positive value means the sound is
		// ours again, so they are read by sign rather than named: an external focus policy, as
		// Android Automotive and some TV builds install, can send any of them, and treating one
		// as unknown would leave the game silent with nothing left to put it right.
	};

	// Loud enough to keep dialogue intelligible under a navigation prompt, quiet enough that the
	// prompt wins. Not measured; see docs/android-audio.md.
	static constexpr float kDuckedGain = 0.2f;

	static float GainFor(Change p_change)
	{
		switch (p_change) {
		case e_lossTransientCanDuck:
			return kDuckedGain;
		case e_loss:
		case e_lossTransient:
			return 0.0f;
		case e_gain:
			break;
		}
		return 1.0f;
	}

	static bool ChangeFor(int p_androidChange, Change* p_change)
	{
		if (p_androidChange >= e_androidGain) {
			*p_change = e_gain;
			return true;
		}
		switch (p_androidChange) {
		case e_androidLoss:
			*p_change = e_loss;
			return true;
		case e_androidLossTransient:
			*p_change = e_lossTransient;
			return true;
		case e_androidLossTransientCanDuck:
			*p_change = e_lossTransientCanDuck;
			return true;
		default:
			// AUDIOFOCUS_NONE, and anything further out that Android has not defined. Neither
			// says the sound moved, so leave the gain where it is rather than guessing.
			return false;
		}
	}

	// Takes what AudioManager reported, untranslated. False leaves the gain alone.
	bool Report(int p_androidChange)
	{
		Change change;
		if (!ChangeFor(p_androidChange, &change)) {
			return false;
		}
		m_change.store(change, std::memory_order_relaxed);
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

// Reports what AudioManager said, from the thread Android delivered it on.
void Android_ReportAudioFocus(int p_androidChange);

// Plays the game at whatever the system last left it at. Call from the SDL thread; it does
// nothing until the gain moves.
void Android_ApplyAudioGain();

#endif
