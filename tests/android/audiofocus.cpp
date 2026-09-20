#include "audiofocus.h"

#include <cassert>

int main()
{
	float gain = -1.0f;

	// Each change's gain. Written literally so that moving one of them fails here.
	assert(AudioFocus::GainFor(AudioFocus::e_gain) == 1.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_loss) == 0.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_lossTransient) == 0.0f);
	assert(AudioFocus::GainFor(AudioFocus::e_lossTransientCanDuck) == 0.2f);

	// A game nobody has taken the sound from plays at full volume, and has nothing to apply.
	{
		AudioFocus focus;
		assert(!focus.Take(&gain));
		assert(focus.Report(AudioFocus::e_gain));
		assert(!focus.Take(&gain));
	}

	// A duck is applied once and stays applied.
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_lossTransientCanDuck));
		assert(focus.Take(&gain));
		assert(gain == 0.2f);
		assert(!focus.Take(&gain));
		assert(focus.Report(AudioFocus::e_lossTransientCanDuck));
		assert(!focus.Take(&gain));
	}

	// Getting the sound back restores exactly the volume the game started at, not an approximation
	// of it, so a duck cannot leave the game permanently quiet.
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_lossTransientCanDuck));
		assert(focus.Take(&gain));
		assert(focus.Report(AudioFocus::e_gain));
		assert(focus.Take(&gain));
		assert(gain == 1.0f);
		assert(!focus.Take(&gain));
	}

	// Only the latest report survives. Reports and takes run on different threads, so a loss and
	// the gain undoing it can both land between one look and the next; taking the loss first would
	// leave the game silent until something else moved the gain.
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_lossTransient));
		assert(focus.Report(AudioFocus::e_lossTransientCanDuck));
		assert(focus.Report(AudioFocus::e_gain));
		assert(!focus.Take(&gain));
	}
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_gain));
		assert(focus.Report(AudioFocus::e_loss));
		assert(focus.Take(&gain));
		assert(gain == 0.0f);
		assert(!focus.Take(&gain));
	}

	// The two silent cases differ to the system but not to the mixer, so moving between them
	// applies nothing.
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_lossTransient));
		assert(focus.Take(&gain));
		assert(gain == 0.0f);
		assert(focus.Report(AudioFocus::e_loss));
		assert(!focus.Take(&gain));
	}

	// A value the two sides stopped agreeing on is refused and changes nothing, rather than
	// resolving to a gain nobody chose.
	{
		AudioFocus focus;
		assert(focus.Report(AudioFocus::e_lossTransientCanDuck));
		assert(focus.Take(&gain));
		assert(!focus.Report(-1));
		assert(!focus.Report(4));
		assert(!focus.Take(&gain));
		assert(gain == 0.2f);
	}
}
