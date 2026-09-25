#include "playername.h"

#include <cassert>
#include <cstring>

int main()
{
	char name[8];

	// Every slot used: no terminator follows the last letter.
	{
		const int16_t letters[7] = {0, 1, 2, 3, 4, 5, 25};
		FormatPlayerName(letters, 7, name);
		assert(strcmp(name, "ABCDEFZ") == 0);
	}

	// A shorter name ends at the first -1, whatever follows it.
	{
		const int16_t letters[7] = {15, 4, 15, 15, 4, 17, -1};
		FormatPlayerName(letters, 7, name);
		assert(strcmp(name, "PEPPER") == 0);
		const int16_t trailing[7] = {12, 0, -1, 12, 12, 12, 12};
		FormatPlayerName(trailing, 7, name);
		assert(strcmp(name, "MA") == 0);
	}

	// No letters at all.
	{
		const int16_t letters[7] = {-1, -1, -1, -1, -1, -1, -1};
		FormatPlayerName(letters, 7, name);
		assert(name[0] == '\0');
	}

	// A letter the book cannot produce still takes its place.
	{
		const int16_t letters[7] = {1, 26, -2, 1, -1, -1, -1};
		FormatPlayerName(letters, 7, name);
		assert(strcmp(name, "B??B") == 0);
	}

	return 0;
}
