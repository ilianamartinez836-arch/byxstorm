#include "Bunnyhop.h"

namespace Features
{
	void Bunnyhop_Run(CUserCmd* pCmd, C_TerrorPlayer* pLocal)
	{
		if (!bBunnyhop || !pCmd || !pLocal)
			return;

		if (!(pCmd->buttons & IN_JUMP))
			return;

		if (!pLocal->m_hGroundEntity().IsValid())
			pCmd->buttons &= ~IN_JUMP;
	}
}
