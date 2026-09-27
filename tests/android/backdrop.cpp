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

	BackdropFrame frame;
	assert(!frame.Finish());
	frame.Note();
	assert(frame.Finish());
	assert(!frame.Finish());
	frame.Note();
	frame.Note();
	assert(frame.Finish());

	auto wide = BackdropBars(1920, 1080, 640, 480, 2.25f, 240.0f, 0.0f);
	assert(wide[0].x == -107 && wide[0].w == 107 && wide[0].h == 480);
	assert(wide[1].x == 640 && wide[1].w == 107);
	assert(wide[2].h == 0 && wide[3].h == 0);
	auto tall = BackdropBars(640, 640, 640, 480, 1.0f, 0.0f, 80.0f);
	assert(tall[0].w == 0 && tall[1].w == 0);
	assert(tall[2].y == -80 && tall[2].h == 80);
	assert(tall[3].y == 480 && tall[3].h == 80);
	auto square = BackdropBars(640, 480, 640, 480, 1.0f, 0.0f, 0.0f);
	for (const auto& bar : square) {
		assert(bar.w == 0 || bar.h == 0);
	}
	return 0;
}
