#pragma once

#include <cmath>

namespace Extensions
{
namespace FirstPersonCamera
{

// The pure part of free look: no game types, so it builds and tests on its own.

struct LookVector {
	float x, y, z;
};

constexpr float c_maxPitch = 1.0471976f; // 60 degrees either way
constexpr float c_stickDeadZone = 0.2f;
constexpr float c_stickYawDegreesPerSecond = 150.0f;
constexpr float c_stickPitchRadiansPerSecond = 2.0f;
constexpr float c_mouseYawDegreesPerPixel = 0.25f;
constexpr float c_mousePitchRadiansPerPixel = 0.005f;
constexpr float c_touchYawDegreesPerUnit = 180.0f; // a full screen width turns half way round
constexpr float c_touchPitchRadiansPerUnit = 2.0f;

// A stick axis, in SDL's -32768..32767, as a rate in -1..1. Zero inside the dead zone, and rising
// from zero at its edge rather than jumping, so a small push gives a slow turn.
inline float StickRate(int p_axis)
{
	float value = p_axis / 32767.0f;
	float magnitude = std::fabs(value);
	if (magnitude <= c_stickDeadZone) {
		return 0.0f;
	}
	float rate = (magnitude - c_stickDeadZone) / (1.0f - c_stickDeadZone);
	if (rate > 1.0f) {
		rate = 1.0f;
	}
	return value < 0.0f ? -rate : rate;
}

// Where the player is looking, relative to where the actor faces.
//
// Yaw is not kept here: turning the view turns the actor, as in any first-person game, so movement
// stays forward-relative. Turns collect until the navigation step takes them. Pitch belongs to the
// camera alone, clamped so the view cannot flip over.
class Look {
public:
	void SetInverted(bool p_inverted) { m_inverted = p_inverted; }

	// Positive degrees turn the way the game's own turn-right input does.
	void Turn(float p_degrees) { m_pendingYaw += p_degrees; }

	// Positive radians look up, or down when inverted.
	void Tilt(float p_radians)
	{
		m_pitch += m_inverted ? -p_radians : p_radians;
		if (m_pitch > c_maxPitch) {
			m_pitch = c_maxPitch;
		}
		else if (m_pitch < -c_maxPitch) {
			m_pitch = -c_maxPitch;
		}
	}

	// The turn collected since the last call, in degrees.
	float TakeYaw()
	{
		float yaw = m_pendingYaw;
		m_pendingYaw = 0.0f;
		return yaw;
	}

	float Pitch() const { return m_pitch; }
	void Recenter() { m_pitch = 0.0f; }

	void Reset()
	{
		m_pitch = 0.0f;
		m_pendingYaw = 0.0f;
	}

private:
	float m_pitch = 0.0f;
	float m_pendingYaw = 0.0f;
	bool m_inverted = false;
};

inline float Dot(const LookVector& p_a, const LookVector& p_b)
{
	return p_a.x * p_b.x + p_a.y * p_b.y + p_a.z * p_b.z;
}

// The eye's direction and up, in the actor's local frame, for a view that stays level with the world
// whatever the ground under the actor does.
//
// The game stands the actor on the slope it walks on and banks it through turns, and the vanilla eye,
// fixed in the actor's frame, tilts with it. Here the view keeps only the actor's heading: forward
// is that heading flattened onto the horizon and raised by p_pitch, and up leans back from world
// up by the same angle. p_right, p_up and p_forward are the rows of the actor's local-to-world
// rotation, which is orthonormal, so its transpose takes world vectors back to local ones.
//
// Returns false when the actor faces straight up or down and has no heading to keep.
inline bool LevelEye(
	const LookVector& p_right,
	const LookVector& p_up,
	const LookVector& p_forward,
	float p_pitch,
	LookVector& p_dir,
	LookVector& p_eyeUp
)
{
	float length = std::sqrt(p_forward.x * p_forward.x + p_forward.z * p_forward.z);
	if (length < 1e-4f) {
		return false;
	}

	LookVector heading = {p_forward.x / length, 0.0f, p_forward.z / length};
	float cosPitch = std::cos(p_pitch);
	float sinPitch = std::sin(p_pitch);
	LookVector worldDir = {heading.x * cosPitch, sinPitch, heading.z * cosPitch};
	LookVector worldUp = {-heading.x * sinPitch, cosPitch, -heading.z * sinPitch};

	p_dir = {Dot(worldDir, p_right), Dot(worldDir, p_up), Dot(worldDir, p_forward)};
	p_eyeUp = {Dot(worldUp, p_right), Dot(worldUp, p_up), Dot(worldUp, p_forward)};
	return true;
}

// Free look runs only while the player walks or drives the user actor through the world with the
// eye camera. Anything else owns the camera, or needs it as the game left it.
struct LookSituation {
	bool m_cameraOnUser;   // the camera rides the user actor
	bool m_userDisabled;   // a scripted animation has the actor
	bool m_restrictedArea; // a fixed-camera spot such as the elevator or the observatory
	bool m_helicopter;     // aims its shots along the camera
	bool m_freeRoamWorld;  // the island itself, not a building, a race or another screen
};

inline bool CanLook(const LookSituation& p_situation)
{
	return p_situation.m_cameraOnUser && !p_situation.m_userDisabled && !p_situation.m_restrictedArea &&
		   !p_situation.m_helicopter && p_situation.m_freeRoamWorld;
}

} // namespace FirstPersonCamera
} // namespace Extensions
