#ifndef PLAYERNAME_H
#define PLAYERNAME_H

#include <cstdint>

// Spells out a player's name as the registration book stores it: one letter index per slot, 0 for
// A through 25 for Z, ended by -1 when shorter than the slots. Anything else, which the book cannot
// produce, reads as '?' rather than being dropped, so a damaged save still shows a name.
//
// p_out holds at least p_count + 1 characters and is always terminated.
inline void FormatPlayerName(const int16_t* p_letters, int p_count, char* p_out)
{
	int i = 0;
	for (; i < p_count && p_letters[i] != -1; i++) {
		int16_t letter = p_letters[i];
		p_out[i] = letter >= 0 && letter <= 25 ? static_cast<char>('A' + letter) : '?';
	}
	p_out[i] = '\0';
}

#endif // PLAYERNAME_H
