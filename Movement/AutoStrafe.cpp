#include "AutoStrafe.h"

namespace Features
{
	static float AngleNormalize(float a)
	{
		while (a > 180.f) a -= 360.f;
		while (a < -180.f) a += 360.f;
		return a;
	}

	void AutoStrafe_Run(CUserCmd* pCmd, C_TerrorPlayer* pLocal)
	{
		if (!bAutoStrafe || !pCmd || !pLocal)
			return;

		if (pLocal->m_hGroundEntity().IsValid())
			return;

		int moveType = pLocal->GetMoveType();
		if (moveType == MOVETYPE_NOCLIP || moveType == MOVETYPE_LADDER || moveType == MOVETYPE_OBSERVER)
			return;

		Vec3 vel = pLocal->m_vecVelocity();
		float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
		if (speed < 2.f)
			return;

		const float maxSpeed = 1000.f;
		const float airAccel = 1000.f;
		const float wishSpeed = 1000.f;

		float term = (wishSpeed / airAccel) / maxSpeed * 400.f / speed;
		if (term <= -1.f || term >= 1.f)
			return;

		float perfectDelta = acosf(term);
		if (perfectDelta == 0.f)
			return;

		float yawRad = pCmd->viewangles.y * 0.0174533f;
		float velDir = atan2f(vel.y, vel.x) - yawRad;
		float wishAng = atan2f(-pCmd->sidemove, pCmd->forwardmove);

		float delta = AngleNormalize((velDir - wishAng) * 57.29578f);
		float moveDir = (delta < 0.f) ? (velDir + perfectDelta) : (velDir - perfectDelta);

		pCmd->forwardmove = cosf(moveDir) * 450.f;
		pCmd->sidemove = -sinf(moveDir) * 450.f;
	}
}
