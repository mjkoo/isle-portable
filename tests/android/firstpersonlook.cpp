#include "extensions/firstpersoncamera/look.h"

#include <cassert>
#include <cmath>

using namespace Extensions::FirstPersonCamera;

static bool Near(float p_a, float p_b)
{
	return std::fabs(p_a - p_b) < 1e-4f;
}

static bool Near(const LookVector& p_a, const LookVector& p_b)
{
	return Near(p_a.x, p_b.x) && Near(p_a.y, p_b.y) && Near(p_a.z, p_b.z);
}

// World vector back from local, through the rows of a local-to-world rotation.
static LookVector ToWorld(const LookVector& p_local, const LookVector p_rows[3])
{
	return {
		p_local.x * p_rows[0].x + p_local.y * p_rows[1].x + p_local.z * p_rows[2].x,
		p_local.x * p_rows[0].y + p_local.y * p_rows[1].y + p_local.z * p_rows[2].y,
		p_local.x * p_rows[0].z + p_local.y * p_rows[1].z + p_local.z * p_rows[2].z
	};
}

int main()
{
	// The stick is dead near the middle, rises from zero at the edge of the dead zone, and saturates.
	{
		assert(StickRate(0) == 0.0f);
		assert(StickRate(6000) == 0.0f);
		assert(StickRate(-6000) == 0.0f);
		assert(StickRate(6554) < 0.01f);
		assert(Near(StickRate(32767), 1.0f));
		assert(Near(StickRate(-32768), -1.0f));
		float half = StickRate(19660);
		assert(half > 0.4f && half < 0.6f);
		assert(Near(StickRate(-19660), -half));
	}

	// Turns collect until taken, and are taken once.
	{
		Look look;
		look.Turn(10.0f);
		look.Turn(-3.0f);
		assert(Near(look.TakeYaw(), 7.0f));
		assert(look.TakeYaw() == 0.0f);
	}

	// Pitch clamps at the limit either way, and recentering levels it without dropping a turn.
	{
		Look look;
		look.Tilt(0.5f);
		assert(Near(look.Pitch(), 0.5f));
		look.Tilt(10.0f);
		assert(Near(look.Pitch(), c_maxPitch));
		look.Tilt(-100.0f);
		assert(Near(look.Pitch(), -c_maxPitch));
		look.Turn(5.0f);
		look.Recenter();
		assert(look.Pitch() == 0.0f);
		assert(Near(look.TakeYaw(), 5.0f));
		look.Turn(5.0f);
		look.Tilt(0.3f);
		look.Reset();
		assert(look.Pitch() == 0.0f && look.TakeYaw() == 0.0f);
	}

	// Inverted look flips pitch only.
	{
		Look look;
		look.SetInverted(true);
		look.Tilt(0.25f);
		assert(Near(look.Pitch(), -0.25f));
		look.Turn(4.0f);
		assert(Near(look.TakeYaw(), 4.0f));
	}

	// On level ground at zero pitch the level eye is the vanilla one.
	{
		LookVector rows[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
		LookVector dir, up;
		assert(LevelEye(rows[0], rows[1], rows[2], 0.0f, dir, up));
		assert(Near(dir, {0, 0, 1}));
		assert(Near(up, {0, 1, 0}));
	}

	// Standing on a slope, facing up it, the actor is pitched and so is the vanilla eye. The level
	// eye still looks along the flat heading with world up.
	{
		float angle = 0.3f;
		float c = std::cos(angle), s = std::sin(angle);
		// Rotation about the actor's right axis: forward rises, up leans back.
		LookVector rows[3] = {{1, 0, 0}, {0, c, -s}, {0, s, c}};
		LookVector dir, up;
		assert(LevelEye(rows[0], rows[1], rows[2], 0.0f, dir, up));
		assert(Near(ToWorld(dir, rows), {0, 0, 1}));
		assert(Near(ToWorld(up, rows), {0, 1, 0}));
	}

	// Banked through a turn and facing along a diagonal, the level eye keeps the heading, drops the
	// roll, and pitches up by the look pitch.
	{
		float yaw = 0.7f, roll = 0.2f;
		float cy = std::cos(yaw), sy = std::sin(yaw), cr = std::cos(roll), sr = std::sin(roll);
		LookVector forward = {sy, 0, cy};
		LookVector flatRight = {cy, 0, -sy};
		// Roll about forward: right and up swing around it.
		LookVector right = {flatRight.x * cr, sr, flatRight.z * cr};
		LookVector upRow = {-flatRight.x * sr, cr, -flatRight.z * sr};
		LookVector rows[3] = {right, upRow, forward};

		float pitch = 0.4f;
		LookVector dir, up;
		assert(LevelEye(rows[0], rows[1], rows[2], pitch, dir, up));
		LookVector worldDir = ToWorld(dir, rows);
		LookVector worldUp = ToWorld(up, rows);
		assert(Near(worldDir, {sy * std::cos(pitch), std::sin(pitch), cy * std::cos(pitch)}));
		assert(Near(Dot(worldDir, worldUp), 0.0f));
		assert(Near(Dot(worldUp, flatRight), 0.0f));
		assert(Near(worldUp.y, std::cos(pitch)));
	}

	// An actor facing straight up has no heading, and the caller keeps the vanilla eye.
	{
		LookVector rows[3] = {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}};
		LookVector dir, up;
		assert(!LevelEye(rows[0], rows[1], rows[2], 0.0f, dir, up));
	}

	// Look runs only while the user actor carries the camera and nothing else claims it.
	{
		LookSituation walking = {true, false, false, false, true};
		assert(CanLook(walking));

		LookSituation situation = walking;
		situation.m_cameraOnUser = false;
		assert(!CanLook(situation));
		situation = walking;
		situation.m_userDisabled = true;
		assert(!CanLook(situation));
		situation = walking;
		situation.m_restrictedArea = true;
		assert(!CanLook(situation));
		situation = walking;
		situation.m_helicopter = true;
		assert(!CanLook(situation));
		situation = walking;
		situation.m_freeRoamWorld = false;
		assert(!CanLook(situation));
	}

	return 0;
}
