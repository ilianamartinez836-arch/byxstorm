#pragma once
#include "../../../../SDK/SDK.h"

// Same mechanism as SpeedHack (extra CL_Move calls per real frame), but left unrestricted -
// extra shots included while held, unlike plain SpeedHack which suppresses attack buttons
// entirely (see SpeedHack.h). User's explicit design choice (2026-07-16): an earlier attempt
// paced this feature's fire to the weapon's real cooldown instead, matching a reference
// implementation - the user tried that and preferred full suppression removed / unrestricted
// behavior instead, so that pacing logic was removed again.
namespace Features::Exploits::CustomSpeedHack
{
	inline bool bEnabled = false;
	inline int  nKey = 0;   // 0 = unbound; press to toggle bEnabled
	inline int  nValue = 0; // extra CL_Move calls per frame while enabled

	// Rising-edge toggle handling. Called once per real frame from the CL_Move hook.
	void Update();

	// Extra CL_Move calls to issue this frame (0 = normal speed, uncapped by design).
	int GetExtraCalls();
}
