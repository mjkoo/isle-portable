#ifndef ANDROID_TOUCHACTIONS_H
#define ANDROID_TOUCHACTIONS_H

#include <array>
#include <cstddef>
#include <cstdint>

// Callers serialize access together with the touch visibility snapshot.
class TouchActions {
public:
	enum Action {
		e_none = 0,
		e_space = 1,
		e_escape = 2
	};
	void Invalidate()
	{
		m_available = false;
		m_head = m_size = 0;
		++m_generation;
	}
	void SetAvailable(bool p_available)
	{
		if (!p_available && m_available) {
			Invalidate();
		}
		m_available = p_available;
	}
	std::int64_t Generation() const { return m_available ? m_generation : 0; }
	bool Submit(int p_action, std::int64_t p_generation)
	{
		if (!m_available || p_generation != m_generation || (p_action != e_space && p_action != e_escape) ||
			m_size == m_queue.size()) {
			return false;
		}
		m_queue[(m_head + m_size++) % m_queue.size()] = static_cast<Action>(p_action);
		return true;
	}
	Action Take()
	{
		if (!m_available || !m_size) {
			return e_none;
		}
		Action action = m_queue[m_head];
		m_head = (m_head + 1) % m_queue.size();
		--m_size;
		return action;
	}

private:
	std::array<Action, 16> m_queue{};
	std::size_t m_head = 0, m_size = 0;
	std::int64_t m_generation = 1;
	bool m_available = false;
};

#endif
