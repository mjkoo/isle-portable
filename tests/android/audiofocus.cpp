#include "audiofocus.h"

#include <cassert>

int main()
{
	float gain = -1.0f;

	// What AudioManager reports, read the way Java hands it over. The numbers are literal
	// because they are Android's, not this project's, and the test is the place that says so.
	AudioFocus::Change change = AudioFocus::e_gain;

	// Every positive value means the sound is ours again: 1 is AUDIOFOCUS_GAIN, and 2, 3 and 4
	// are the GAIN_TRANSIENT variants an external focus policy can send. Treating one of them as
	// unknown would leave the game silent with nothing left to put it right.
	for (int androidChange = 1; androidChange <= 4; ++androidChange) {
		assert(AudioFocus::ChangeFor(androidChange, &change));
		assert(change == AudioFocus::e_gain);
	}

	assert(AudioFocus::ChangeFor(-1, &change) && change == AudioFocus::e_loss);
	assert(AudioFocus::ChangeFor(-2, &change) && change == AudioFocus::e_lossTransient);
	assert(AudioFocus::ChangeFor(-3, &change) && change == AudioFocus::e_lossTransientCanDuck);

	// AUDIOFOCUS_NONE, and anything further out than Android defines: neither says the sound
	// moved, so neither is translated.
	assert(!AudioFocus::ChangeFor(0, &change));
	assert(!AudioFocus::ChangeFor(-4, &change));

	// Each change's gain. Written literally so that moving one of them fails here.
	assert(AudioFocus::GainFor(AudioFocus::e_gain) == 1.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_loss) == 0.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_lossTransient) == 0.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_lossTransientCanDuck) == 0.2f);

	// A game nobody has taken the sound from plays at full volume, and has nothing to apply.
	{
		AudioFocus focus;
		assert(!focus.Take(&gain));
		assert(focus.Report(1));
		assert(!focus.Take(&gain));
	}

	// A duck is applied once and stays applied.
	{
		AudioFocus focus;
		assert(focus.Report(-3));
		assert(focus.Take(&gain));
		assert(gain == 0.2f);
		assert(!focus.Take(&gain));
		assert(focus.Report(-3));
		assert(!focus.Take(&gain));
	}

	// Getting the sound back restores exactly the volume the game started at, not an approximation
	// of it, so a duck cannot leave the game permanently quiet.
	{
		AudioFocus focus;
		assert(focus.Report(-3));
		assert(focus.Take(&gain));
		assert(focus.Report(1));
		assert(focus.Take(&gain));
		assert(gain == 1.0f);
		assert(!focus.Take(&gain));
	}

	// Only the latest report survives. Reports and takes run on different threads, so a loss and
	// the gain undoing it can both land between one look and the next; taking the loss first would
	// leave the game silent until something else moved the gain.
	{
		AudioFocus focus;
		assert(focus.Report(-2));
		assert(focus.Report(-3));
		assert(focus.Report(1));
		assert(!focus.Take(&gain));
	}
	{
		AudioFocus focus;
		assert(focus.Report(1));
		assert(focus.Report(-1));
		assert(focus.Take(&gain));
		assert(gain == 0.0f);
		assert(!focus.Take(&gain));
	}

	// The two silent cases differ to the system but not to the mixer, so moving between them
	// applies nothing.
	{
		AudioFocus focus;
		assert(focus.Report(-2));
		assert(focus.Take(&gain));
		assert(gain == 0.0f);
		assert(focus.Report(-1));
		assert(!focus.Take(&gain));
	}

	// A value that says nothing about the sound is refused and changes nothing, rather than
	// resolving to a gain nobody chose.
	{
		AudioFocus focus;
		assert(focus.Report(-3));
		assert(focus.Take(&gain));
		assert(!focus.Report(0));
		assert(!focus.Report(-4));
		assert(!focus.Take(&gain));
		assert(gain == 0.2f);
	}

	// A duck undone by a GAIN_TRANSIENT rather than a plain GAIN. This is the sequence that left
	// the game quiet for good when only AUDIOFOCUS_GAIN counted as getting the sound back.
	{
		AudioFocus focus;
		assert(focus.Report(-3));
		assert(focus.Take(&gain));
		assert(gain == 0.2f);
		assert(focus.Report(2));
		assert(focus.Take(&gain));
		assert(gain == 1.0f);
	}
}
