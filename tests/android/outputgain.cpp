#include "outputgain.h"

#include <cassert>

int main()
{
	float gain = -1.0f;

	// A game nobody has paused and nobody has taken the sound from plays at full volume, and has
	// nothing to apply.
	{
		OutputGain output;
		assert(!output.Take(&gain));
		output.SetPaused(false);
		output.SetFocus(1.0f);
		assert(!output.Take(&gain));
	}

	// Pausing silences the whole mix, and stays silent. This is the point of the class: the game's
	// own pause reaches the cached and 3D sounds that MxSoundManager::Pause leaves running.
	{
		OutputGain output;
		output.SetPaused(true);
		assert(output.Take(&gain));
		assert(gain == 0.0f);
		assert(!output.Take(&gain));
		output.SetPaused(true);
		assert(!output.Take(&gain));
	}

	// Resuming restores exactly the volume the game was playing at, not an approximation of it.
	{
		OutputGain output;
		output.SetPaused(true);
		assert(output.Take(&gain));
		output.SetPaused(false);
		assert(output.Take(&gain));
		assert(gain == 1.0f);
		assert(!output.Take(&gain));
	}

	// A duck survives a pause. Resuming hands the sound back to the system's gain rather than to
	// full volume, which is the whole reason the two factors are arbitrated here instead of being
	// written to the engine independently.
	{
		OutputGain output;
		output.SetFocus(0.2f);
		assert(output.Take(&gain));
		assert(gain == 0.2f);
		output.SetPaused(true);
		assert(output.Take(&gain));
		assert(gain == 0.0f);
		output.SetPaused(false);
		assert(output.Take(&gain));
		assert(gain == 0.2f);
	}

	// The system moving the sound around while the game is paused writes nothing, because a paused
	// game is already silent. The resume that follows applies wherever the system left it.
	{
		OutputGain output;
		output.SetPaused(true);
		assert(output.Take(&gain));
		assert(gain == 0.0f);
		output.SetFocus(0.0f);
		assert(!output.Take(&gain));
		output.SetFocus(1.0f);
		assert(!output.Take(&gain));
		output.SetPaused(false);
		assert(output.Take(&gain));
		assert(gain == 1.0f);
	}

	// Losing the sound to another app while paused, and not getting it back before the resume.
	{
		OutputGain output;
		output.SetPaused(true);
		assert(output.Take(&gain));
		output.SetFocus(0.0f);
		output.SetPaused(false);
		assert(!output.Take(&gain));
	}

	// Several changes between two takes collapse to the last state. The pump runs once an
	// iteration, so a pause and the resume undoing it can both land between one look and the next.
	{
		OutputGain output;
		output.SetPaused(true);
		output.SetFocus(0.2f);
		output.SetPaused(false);
		output.SetFocus(1.0f);
		assert(!output.Take(&gain));
	}
	{
		OutputGain output;
		output.SetFocus(0.2f);
		output.SetFocus(0.0f);
		output.SetPaused(true);
		output.SetPaused(false);
		assert(output.Take(&gain));
		assert(gain == 0.0f);
		assert(!output.Take(&gain));
	}
}
