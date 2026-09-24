#include "AutoDuck.h"

namespace Features
{
	void AutoDuck_Run(CUserCmd* pCmd)
	{
		if (bAutoDuck && pCmd)
			pCmd->buttons |= IN_DUCK;
	}
}
