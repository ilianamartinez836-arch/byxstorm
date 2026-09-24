#pragma once

// CRapidFire - port verbatim de un RapidFire de referencia externa.
// El .cpp (RapidFire.cpp) es una copia byte-a-byte de ese código, sin modificar - este header
// solo provee las piezas que ese .cpp asume que ya existen (RFContext, g_var, Client_Module,
// Global_Variables_Structure/Command_Structure/Extended_Command_Structure).
//
// Diferencia de arquitectura importante: la version original de referencia asume que el motor
// mismo lee Extended_Command_Structure::Extra_Commands para empaquetar comandos extra dentro de
// un solo packet (via su propio hook de Run_Command, ver el bloque comentado en
// src/Storm/Hooks/HooksStorm.cpp). Ese hook nunca se verifico contra nuestro engine.dll (firmas
// de ejemplo, nunca resueltas). En vez de activar eso sin probar, este header conecta la MISMA
// logica de decision (DisableClockCorrection/CorrectExtendedCommand/ToggleAttackOnExtraTicks) a
// nuestro mecanismo de RapidFire ya confirmado funcionando en servidor real (llamar CL_Move N
// veces extra, ver src/App/Hooks/CL_Move.cpp) - RFContext::ExtraCommands es la bisagra: una
// referencia al contador de llamadas extra que ese hook ya usa.
//
// Global_Variables_Structure/Command_Structure/Extended_Command_Structure vienen de
// src/Storm/Utils/UtilsStorm.hpp, y Client_Module + 74304 (Get_Weapon_Type) / LocalPlayer + 7867
// (byte gate) ya se usan tal cual en src/Storm/Hooks/HookStormOthers.hpp:1548 - offsets reales
// contra nuestro propio client.dll, no ajenos.
//
// OnInfectedNoVictim queda definido tal cual pero NO se invoca desde este modulo: es logica de
// Versus/Infected y este proyecto es Survivor-only por ahora.
// ApplyExtraScaling tambien queda definida pero sin punto de llamada:
// es la ruta alternativa "solo matematica, sin llamada extra real" que la arquitectura original
// activa desde su propio loop de CL_Move - no aplica a nuestro mecanismo de N-llamadas-reales.

#include <Windows.h>
#include "../../../../SDK/SDK.h"
#include "../../../../SDK/L4D2/inetchannel.h"
#include "../../../../Utils/BytePatchManager/BytePatchManager.h"
#include "../../../../Storm/Utils/UtilsStorm.hpp"

// UtilsStorm.hpp define macros crudos `max`/`min` (y Windows.h puede definir los suyos). El
// RapidFire.cpp original hace `#include <algorithm>` + `using std::max;`/`using std::min;`
// despues de incluir este header - sin este undef, esas macros corrompen las declaraciones
// internas de <algorithm> (std::max/std::min/std::clamp) y no compila.
#undef max
#undef min

extern void *Client_Module;

struct RFContext
{
	Command_Structure *Command;
	Extended_Command_Structure *ExtCmd;
	Extended_Command_Structure *InitExtCmd;
	CNetChannel *NetworkChannel;
	Global_Variables_Structure *GlobalVars;
	void *LocalPlayer;
	__int32 FirstTick;
	__int32 ActionFactor;
	__int32 &ExtraCommands;
};

// Config minimo que CRapidFire conoce, con los mismos nombres que el .cpp original espera -
// alimentado por Features::Exploits::RapidFire::bEnabled/nKey/nValue mas abajo.
struct RapidFireVars
{
	__int32 iFactorRapidFire = 0;
	__int32 iFactorSpeedHack = 0;
	bool bSHActive = false;
	bool bRageActive = false;
};

inline RapidFireVars g_var;

class CRapidFire
{
public:
	void DisableClockCorrection(__int32 queue, const RFContext &ctx);
	void CorrectExtendedCommand(const RFContext &ctx);
	void ApplyExtraScaling(void *localPlayer, Global_Variables_Structure *gv,
		__int32 &extraCommands, __int32 interpolateFactor);
	void RunSurvivor(const RFContext &ctx);
	void OnInfectedNoVictim(const RFContext &ctx);
	void ToggleAttackOnExtraTicks(const RFContext &ctx);
	void Reset();

private:
	__int32 m_AccumulativeCorrection = 0;
	bool m_bNetRestoreNeeded = false;
	bool m_bDisabledThisInvoke = false;
	__int32 m_nSavedNC16 = 0;
	__int32 m_nSavedNC28 = 0;
};

inline CRapidFire g_RapidFireEngine;

// RapidFire: master enable checkbox + a fire key that must be HELD (not a toggle - you hold your
// attack key to dump extra shots). Same underlying mechanism as SpeedHack - extra CL_Move calls =
// extra commands sent = extra shots fired per real frame. Fully self-contained: this namespace
// owns its own config, its own per-frame glue into CRapidFire above, and its own extra-call count -
// nothing outside this folder needs to know CRapidFire exists.
namespace Features::Exploits::RapidFire
{
	inline bool bEnabled = false;
	inline int  nKey = 0;   // fire key to HOLD; 0 = unbound
	inline int  nValue = 0; // extra CL_Move calls per frame while held

	// [2026-08-13] ApplyExtraScaling (el multiplicador "explosivo" del codigo de referencia).
	// Estira m_flSimulationTime por (1 + extraCommands * nInterpolate). En 0 es inerte
	// (multiplicador 1.0) - arranca en 0 para no cambiar nada hasta que se suba a mano.
	inline int  nInterpolate = 0;

	// True this frame if RapidFire's extra calls are contributing. Confirmed in-game: without
	// forcing the net channel's choke bookkeeping, RapidFire's extra CL_Move calls advance
	// movement but the extra shots don't register - the CL_Move hook uses this to gate the
	// netchan override to just the RapidFire case, since SpeedHack/CustomSpeedHack already work
	// fine without it.
	bool IsActive();

	// Call once from CL_Move.cpp, before the frame's one real CALL_ORIGINAL. Resets the per-frame
	// output slot.
	void BeginFrame();

	// Call from Copy_Command.cpp on every generated command this frame (the real one AND every
	// synthetic extra one) - this is CRapidFire's real per-tick call site. Runs CRapidFire's
	// algorithm (DisableClockCorrection/CorrectExtendedCommand/ToggleAttackOnExtraTicks - real
	// accumulator, real ack-progress detection, real weapon-cycle-paced attack toggling),
	// confirmed working on a real dedicated server.
	void OnCommand(CUserCmd *pCmd);

	// Call once from CL_Move.cpp right after the real CALL_ORIGINAL returns, to size the extra-
	// call loop. 0 when RapidFire is off.
	int GetExtraCalls();
}
