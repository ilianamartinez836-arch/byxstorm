#include "Fakelag.h"
#include "../../Misc/Binds/Binds.h"
#include <algorithm>

namespace Features::Exploits::Fakelag
{
	static int  s_nCounter = 0;
	static bool s_bToggleState = false;
	static bool s_bToggleLatch = false;

	bool ShouldChoke()
	{
		if (!bEnabled)
		{
			s_nCounter = 0;
			s_bToggleState = false;
			s_bToggleLatch = false;
			nCurrentCounter = 0;
			bCurrentlyActive = false;
			return false;
		}

		// Toggle con latch de flanco propio, igual que el original.
		if (nKeyToggle)
		{
			const bool bDown = Features::Binds::Down(nKeyToggle);
			if (bDown && !s_bToggleLatch)
				s_bToggleState = !s_bToggleState;
			s_bToggleLatch = bDown;
		}

		const bool bHold = nKeyHold && Features::Binds::Down(nKeyHold);
		const bool bActive = bAlways || s_bToggleState || bHold;

		bCurrentlyActive = bActive;

		if (!bActive)
		{
			s_nCounter = 0;
			nCurrentCounter = 0;
			return false;
		}

		// clamp(nAmount, 1, 14) - el 14 es el techo del campo numnewcommands (4 bits). Ver el
		// writeup del header.
		const int nMax = std::clamp(nAmount, 1, 14);

		if (s_nCounter < nMax)
		{
			s_nCounter++;
			nCurrentCounter = s_nCounter;
			nTotalChoked++;
			return true;   // chokear
		}

		s_nCounter = 0;
		nCurrentCounter = 0;
		return false;      // mandar la rafaga
	}
}
