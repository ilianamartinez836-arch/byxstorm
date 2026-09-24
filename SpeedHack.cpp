#include "SpeedHack.h"
#include "../../Misc/Binds/Binds.h"

namespace Features::Exploits::SpeedHack
{
	static bool s_bKeyPrev = false;

	void Update()
	{
		if (!nKey)
			return;

		bool bDown = Features::Binds::Down(nKey);
		if (bDown && !s_bKeyPrev)
			bEnabled = !bEnabled;
		s_bKeyPrev = bDown;
	}

	int GetExtraCalls()
	{
		int n = bEnabled ? nValue : 0;
		return n < 0 ? 0 : n;
	}
}
