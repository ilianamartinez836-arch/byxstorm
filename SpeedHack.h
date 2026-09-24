#pragma once
#include "../../../../SDK/SDK.h"

// Real speedhack technique: the engine's CL_Move builds and SENDS one usercmd to the
// server per real frame. Calling it N extra times per frame makes the server advance N extra
// ticks of movement per real frame - that is the actual speed gain. This is NOT the same as
// inflating a single command's tick_count (which does nothing / stalls). The CL_Move hook
// (src/App/Hooks/CL_Move.cpp) drives this; here we only hold the config + toggle-key handling +
// the extra-call count.
//
// Movement only, by design (2026-07-16): every extra CL_Move call resamples the whole real
// input state, attack buttons included, so without stripping them plain SpeedHack would also
// multiply shots like RapidFire. Copy_Command.cpp strips IN_ATTACK/IN_ATTACK2 while bExtraTick
// is true (set by CL_Move.cpp immediately before a SpeedHack-attributed extra call).
namespace Features::Exploits::SpeedHack
{
	inline bool bEnabled = false;
	inline int  nKey = 0;   // 0 = unbound; press to toggle bEnabled
	inline int  nValue = 0; // extra CL_Move calls per frame while enabled

	// [2026-08-13] Rebobinado de return address (la tecnica de la implementacion de referencia).
	// Genera los comandos extra re-ejecutando el `call CL_Move` del motor en vez del loop de
	// CALL_ORIGINAL. OPT-IN: apagado por defecto hasta verificar en servidor dedicado. Si el call
	// site no es un `call rel32` (E8), cae solo al loop clasico.
	inline bool bRewind = false;

	// Set by CL_Move.cpp immediately before a SpeedHack-attributed extra call, cleared right
	// after. Copy_Command.cpp strips IN_ATTACK/IN_ATTACK2 from the command while this is true.
	inline bool bExtraTick = false;

	// Rising-edge toggle handling. Called once per real frame from the CL_Move hook (before the
	// extra calls) so a single press flips bEnabled exactly once.
	void Update();

	// Extra CL_Move calls to issue this frame (0 = normal speed, uncapped by design).
	int GetExtraCalls();
}
