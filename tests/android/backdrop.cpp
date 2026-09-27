#include "backdrop.h"

#include <cassert>

int main()
{
	// An opaque blit over the whole game area is a backdrop, as is one that overhangs it.
	assert(IsBackdrop({0, 0, 640, 480}, 640, 480, false));
	assert(IsBackdrop({-1, -1, 642, 482}, 640, 480, false));

	// A color-keyed one lets the 3D scene through, as a movie frame or the cursor does.
	assert(!IsBackdrop({0, 0, 640, 480}, 640, 480, true));

	// Anything short of the whole area leaves 3D showing inside it.
	assert(!IsBackdrop({0, 0, 639, 480}, 640, 480, false));
	assert(!IsBackdrop({0, 0, 640, 479}, 640, 480, false));
	assert(!IsBackdrop({1, 0, 640, 480}, 640, 480, false));
	assert(!IsBackdrop({0, 1, 640, 480}, 640, 480, false));
	assert(!IsBackdrop({204, 81, 40, 46}, 640, 480, false));

	return 0;
}
