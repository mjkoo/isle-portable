#pragma once

#include "extensions/firstpersoncamera/look.h"

#include <SDL3/SDL_events.h>
#include <map>
#include <set>
#include <utility>

namespace Extensions
{
namespace FirstPersonCamera
{

enum class TouchDisposition {
	Pass,
	Consume,
	Tap
};

struct TouchResult {
	TouchDisposition disposition = TouchDisposition::Pass;
	float x = 0.0f;
	float y = 0.0f;
	float dx = 0.0f;
	float dy = 0.0f;
};

// A claimed contact stays ours through cancellation, until its matching release. This prevents
// the tail of a look drag from becoming a click in the next scene.
class TouchLook {
public:
	TouchResult Handle(const SDL_TouchFingerEvent& p_event, bool p_eligible)
	{
		auto contact = std::make_pair(p_event.touchID, p_event.fingerID);
		if (p_event.type == SDL_EVENT_FINGER_DOWN) {
			m_canceledContacts.erase(contact);
			if (m_held || !p_eligible || p_event.x < 0.5f) {
				return {};
			}
			m_held = true;
			m_canceled = m_dragging = false;
			m_touch = p_event.touchID;
			m_finger = p_event.fingerID;
			m_startX = m_lastX = p_event.x;
			m_startY = m_lastY = p_event.y;
			return {TouchDisposition::Consume};
		}
		if (m_canceledContacts.count(contact)) {
			if (p_event.type == SDL_EVENT_FINGER_UP || p_event.type == SDL_EVENT_FINGER_CANCELED) {
				m_canceledContacts.erase(contact);
			}
			return {TouchDisposition::Consume};
		}
		if (!m_held || p_event.touchID != m_touch || p_event.fingerID != m_finger) {
			return {};
		}
		if (!p_eligible) {
			m_canceled = true;
		}
		TouchResult result{TouchDisposition::Consume};
		float dx = (p_event.x - m_startX) * 640.0f;
		float dy = (p_event.y - m_startY) * 480.0f;
		if (dx * dx + dy * dy > 25.0f) {
			m_dragging = true;
		}
		if (p_event.type == SDL_EVENT_FINGER_MOTION && m_dragging && !m_canceled) {
			result.dx = p_event.x - m_lastX;
			result.dy = p_event.y - m_lastY;
			m_lastX = p_event.x;
			m_lastY = p_event.y;
		}
		if (p_event.type == SDL_EVENT_FINGER_UP || p_event.type == SDL_EVENT_FINGER_CANCELED) {
			m_held = false;
			if (p_event.type == SDL_EVENT_FINGER_UP && !m_dragging && !m_canceled) {
				result = {TouchDisposition::Tap, m_startX, m_startY};
			}
		}
		return result;
	}

	void Cancel()
	{
		if (m_held) {
			m_canceledContacts.emplace(m_touch, m_finger);
			m_held = false;
		}
	}

private:
	std::set<std::pair<SDL_TouchID, SDL_FingerID>> m_canceledContacts;
	bool m_held = false;
	bool m_dragging = false;
	bool m_canceled = false;
	SDL_TouchID m_touch = 0;
	SDL_FingerID m_finger = 0;
	float m_startX = 0.0f, m_startY = 0.0f;
	float m_lastX = 0.0f, m_lastY = 0.0f;
};

// A failed or canceled capture still owns its release, but never applies motion.
class MouseLookGesture {
public:
	void Begin(bool p_captured)
	{
		m_owned = true;
		m_captured = p_captured;
	}
	void Cancel() { m_captured = false; }
	bool End()
	{
		bool owned = m_owned;
		m_owned = m_captured = false;
		return owned;
	}
	bool OwnsMotion() const { return m_owned; }
	bool CanLook() const { return m_captured; }

private:
	bool m_owned = false;
	bool m_captured = false;
};

class LookSticks {
public:
	void Axis(SDL_JoystickID p_pad, SDL_GamepadAxis p_axis, Sint16 p_value)
	{
		if (p_axis != SDL_GAMEPAD_AXIS_RIGHTX && p_axis != SDL_GAMEPAD_AXIS_RIGHTY) {
			return;
		}
		auto& axes = m_pads[p_pad];
		(p_axis == SDL_GAMEPAD_AXIS_RIGHTX ? axes.x : axes.y) = p_value;
		if (StickRate(p_value) != 0.0f) {
			m_owner = p_pad;
		}
	}

	void Remove(SDL_JoystickID p_pad)
	{
		m_pads.erase(p_pad);
		if (m_owner == p_pad) {
			m_owner = 0;
		}
	}

	void Cancel()
	{
		m_pads.clear();
		m_owner = 0;
	}
	float X() const { return StickRate(Current().x); }
	float Y() const { return StickRate(Current().y); }

private:
	struct Axes {
		Sint16 x = 0, y = 0;
	};
	Axes Current() const
	{
		auto it = m_pads.find(m_owner);
		return it == m_pads.end() ? Axes{} : it->second;
	}
	std::map<SDL_JoystickID, Axes> m_pads;
	SDL_JoystickID m_owner = 0;
};

} // namespace FirstPersonCamera
} // namespace Extensions
