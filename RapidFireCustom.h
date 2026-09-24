#pragma once
#include "../../../../SDK/SDK.h"

// RapidFire Custom - the reference implementation's menu label is "Rapid Fire+", confirmed
// 2026-07-18 via IDA. NOT the extra-ticks mechanism every other Exploit uses - its config'd Factor
// slider (iFactorRapidFireCustom, dword_101C01B0) is CONFIRMED DEAD CODE (exhaustive xref search:
// 3 references total - registration, menu draw, and the Toggle Mode dispatcher's enable-check -
// none of them ever read the factor's VALUE). The feature's REAL, actually-consumed effect (found
// in sub_101530A0's tail, gated on `byte_101C01A9==1 && key-active`): every tick while active,
// `*(GetActiveWeapon() + 0x960) = 0` - directly zeroes the weapon's own fire-delay/cooldown field
// (m_flNextPrimaryAttack-equivalent, offset 2400 - same field RapidFire's ToggleAttackOnExtraTicks
// already reads elsewhere in this project). No extra CL_Move calls, no netchannel writes - pure
// "remove the weapon's cooldown" toggle. No factor/slider needed - it's a binary on/off effect.
// Applied directly from Copy_Command.cpp (see that file's call site) since it's a per-command
// weapon-field write, not an extra-CL_Move-calls feature.
namespace Features::Exploits::RapidFireCustom
{
	inline bool bEnabled = false;
	inline int  nKey = 0;

	// Rising-edge toggle handling. Called once per real frame from the CL_Move hook.
	void Update();
	bool IsActive();
}
