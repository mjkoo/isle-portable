#ifndef ANDROID_TOUCHINPUT_H
#define ANDROID_TOUCHINPUT_H

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#include <set>
#include <utility>

// SDL-thread state: a resumed game accepts only contacts that began after cancellation.
class Android_TouchInput {
public:
	void Cancel()
	{
		m_contacts.clear();
		m_lastTap = 0;
	}
	bool Accept(const SDL_TouchFingerEvent& p_event)
	{
		auto contact = std::make_pair(p_event.touchID, p_event.fingerID);
		if (p_event.type == SDL_EVENT_FINGER_DOWN) {
			return m_contacts.insert(contact).second;
		}
		if (p_event.type == SDL_EVENT_FINGER_UP || p_event.type == SDL_EVENT_FINGER_CANCELED) {
			return m_contacts.erase(contact) != 0;
		}
		return m_contacts.count(contact) != 0;
	}
	bool DoubleTap(const SDL_TouchFingerEvent& p_event)
	{
		if (p_event.type == SDL_EVENT_FINGER_CANCELED) {
			m_lastTap = 0;
			return false;
		}
		float dx = p_event.x - m_lastX, dy = p_event.y - m_lastY;
		if (m_lastTap && p_event.timestamp >= m_lastTap && SDL_NS_TO_MS(p_event.timestamp - m_lastTap) < 500 &&
			dx * dx + dy * dy < 0.001f) {
			m_lastTap = 0;
			return true;
		}
		m_lastTap = p_event.timestamp;
		m_lastX = p_event.x;
		m_lastY = p_event.y;
		return false;
	}

private:
	std::set<std::pair<SDL_TouchID, SDL_FingerID>> m_contacts;
	Uint64 m_lastTap = 0;
	float m_lastX = 0, m_lastY = 0;
};

#endif
