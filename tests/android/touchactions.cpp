#include "touchactions.h"

#include <cassert>

int main()
{
	TouchActions actions;
	assert(actions.Generation() == 0);
	assert(!actions.Submit(TouchActions::e_space, 0));
	assert(actions.Take() == TouchActions::e_none);
	actions.SetAvailable(true);
	auto generation = actions.Generation();
	assert(generation != 0);
	assert(!actions.Submit(0, generation));
	assert(!actions.Submit(3, generation));
	assert(!actions.Submit(TouchActions::e_space, generation - 1));
	for (int i = 0; i < 16; ++i) {
		assert(actions.Submit(i % 2 + 1, generation));
	}
	assert(!actions.Submit(TouchActions::e_space, generation));
	assert(actions.Take() == TouchActions::e_space);
	assert(actions.Submit(TouchActions::e_escape, generation));
	for (int i = 1; i < 16; ++i) {
		assert(actions.Take() == i % 2 + 1);
	}
	assert(actions.Take() == TouchActions::e_escape);
	assert(actions.Take() == TouchActions::e_none);
	assert(actions.Submit(TouchActions::e_space, generation));
	actions.SetAvailable(false);
	assert(actions.Generation() == 0);
	assert(actions.Take() == TouchActions::e_none);
	assert(!actions.Submit(TouchActions::e_escape, generation));
	actions.SetAvailable(true);
	assert(actions.Generation() != generation);
	assert(!actions.Submit(TouchActions::e_escape, generation));
	generation = actions.Generation();
	assert(actions.Submit(TouchActions::e_escape, generation));
	actions.Invalidate();
	actions.SetAvailable(true);
	assert(actions.Take() == TouchActions::e_none);
	assert(!actions.Submit(TouchActions::e_space, generation));
	assert(actions.Submit(TouchActions::e_space, actions.Generation()));
	actions.SetAvailable(true);
	assert(actions.Take() == TouchActions::e_space);
}
