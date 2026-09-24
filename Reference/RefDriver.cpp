#include "RefDriver.h"
#include "SpeedHackRef.h"
#include "RapidFireRef.h"
#include "../ExploitsDiag.h"
#include "../../../../Utils/DebugLog.h"
#include "../../../../SDK/L4D2/NetVars/NetVars.h"
#include <algorithm>
using std::max;
using std::min;

namespace Features::Exploits::Reference
{
	// Ring de slots de comando, uno por Command_Number % 150, igual que el `Extended_Commands[150]`
	// del original. Copia propia (no la global de UtilsStorm.hpp) para que este modulo no comparta
	// estado con nada mas del proyecto.
	static Extended_Command_Structure s_ExtCmds[150] = {};

	static Extended_Command_Structure *s_pInitExt = nullptr;
	static __int32 s_nFirstTick = 0;
	static bool    s_bFirstCommandThisFrame = true;
	static int     s_nExtraResult = 0;
	static bool    s_bWasRfActive = false;
	static bool    s_bDrivenByRapidFire = false;

	// Una sola vez, la primera vez que el par se activa: deja escrito en el log el offset que
	// resuelve nuestro netvar de m_flSimulationTime, para poder compararlo contra el 16 crudo que
	// escribe el codigo de referencia (ver SpeedHackRef.h). Es la unica forma de saber si los dos
	// caminos tocan el mismo campo sin adivinarlo.
	static void LogSimTimeOffsetOnce()
	{
		static bool s_bLogged = false;
		if (s_bLogged)
			return;
		s_bLogged = true;

		const int nNetVarOffset = NetVars::GetNetVar("CBaseEntity", "m_flSimulationTime");
		Debug::Log(
			"[Byx][Ref] m_flSimulationTime netvar offset = %d (0x%X). El SpeedHack de referencia "
			"escribe el offset crudo 16 (0x10). %s\n",
			nNetVarOffset, nNetVarOffset,
			nNetVarOffset == 16 ? "COINCIDEN." : "NO COINCIDEN - son campos distintos.");
	}

	bool IsAnyActive()
	{
		return Features::Exploits::SpeedHackRef::IsActive()
			|| Features::Exploits::RapidFireRef::IsActive();
	}

	bool IsRapidFireDriving() { return s_bDrivenByRapidFire; }

	void BeginFrame()
	{
		s_bFirstCommandThisFrame = true;
		s_nExtraResult = 0;
		s_bDrivenByRapidFire = false;
	}

	void OnCommand(CUserCmd *pCmd)
	{
		if (!pCmd)
			return;

		const bool bSH = Features::Exploits::SpeedHackRef::IsActive();
		const bool bRF = Features::Exploits::RapidFireRef::IsActive();

		auto pLocal = H::Entities->GetLocal();
		auto pNetChan = reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo());

		// Guard de GLUE, no de las funciones portadas (esas quedan tal cual). El codigo original
		// desreferencia LocalPlayer y NetworkChannel sin chequear nada; Copy_Command puede correr
		// con el jugador local todavia nulo (cambio de mapa, spawn) y ahi seria un crash directo.
		if (!pLocal || !pNetChan)
		{
			if (s_bWasRfActive)
			{
				// No hay netchannel que restaurar - solo limpiar el estado interno para no
				// arrastrar un m_bNetRestoreNeeded con offsets viejos al proximo mapa.
				g_RapidFireRefEngine.Reset();
				s_bWasRfActive = false;
			}
			s_bFirstCommandThisFrame = false;
			return;
		}

		if (bSH || bRF)
			LogSimTimeOffsetOnce();

		// Igual que el `Redirected_Copy_Command` del original: el slot es el del comando ACTUAL y
		// se limpia al entrar. `InitExtCmd` en cambio es el slot del PRIMER comando del frame y se
		// queda fijo hasta el frame siguiente - esa distincion es la que nuestro RapidFire no
		// tiene (usa un unico struct para los dos, ver RapidFireRef.h).
		Extended_Command_Structure *pExt =
			&s_ExtCmds[static_cast<unsigned int>(pCmd->command_number) % 150];

		pExt->Extra_Commands = 0;
		pExt->Sequence_Shift = 0;

		if (s_bFirstCommandThisFrame)
		{
			s_nFirstTick = pCmd->command_number;
			s_pInitExt = pExt;
		}
		if (!s_pInitExt)
			s_pInitExt = pExt;

		g_varRef.bSHActive        = bSH;
		g_varRef.iFactorSpeedHack = bSH ? Features::Exploits::SpeedHackRef::nValue : 0;
		g_varRef.bRageActive      = bRF;
		g_varRef.iFactorRapidFire = bRF ? Features::Exploits::RapidFireRef::nValue : 0;

		RFRefContext ctx{
			reinterpret_cast<Command_Structure *>(pCmd),
			pExt,
			s_pInitExt,
			pNetChan,
			reinterpret_cast<Global_Variables_Structure *>(I::GlobalVars),
			reinterpret_cast<void *>(pLocal),
			s_nFirstTick,
			Features::Exploits::RapidFireRef::nValue, // ActionFactor (solo lo usa OnInfectedNoVictim)
			s_nExtraResult
		};

		if (s_bFirstCommandThisFrame)
		{
			// Condicion propia de ApplyExtraScaling segun el original: "Solo RF activo (sin SH)".
			// Con nInterpolate en 0 el `*= 1.f + extra*0` es un no-op y lo unico que hace es fijar
			// extraCommands - por eso arranca en 0 y no cambia nada hasta que se suba a mano.
			if (bRF && !bSH)
			{
				g_RapidFireRefEngine.ApplyExtraScaling(ctx.LocalPlayer, ctx.GlobalVars,
					s_nExtraResult, Features::Exploits::RapidFireRef::nInterpolate);
			}

			// Se llama SIEMPRE: la funcion tiene su propio `if (g_varRef.bSHActive)` adentro, igual
			// que en el original. Si SH esta activo pisa lo que haya dejado ApplyExtraScaling, y
			// despues RunSurvivor puede volver a pisarlo con el factor combinado - ese orden
			// (SH -> RF gana) es el del codigo original, no una eleccion nuestra.
			g_SpeedHackRefEngine.OnFirstTick(pExt, ctx.GlobalVars, s_nExtraResult, ctx.LocalPlayer);
		}

		if (bRF)
			g_RapidFireRefEngine.RunSurvivor(ctx);

		// Gateada adentro con `(bRageActive || bSHActive)`, o sea que tambien corre con SOLO el
		// SpeedHack de referencia prendido - esa es la diferencia grande contra nuestro SpeedHack,
		// que en vez de pacing borra IN_ATTACK/IN_ATTACK2 por completo en sus ticks extra.
		if (bRF || bSH)
			g_RapidFireRefEngine.ToggleAttackOnExtraTicks(ctx);

		if (bRF)
		{
			g_RapidFireRefEngine.CorrectExtendedCommand(ctx);
		}
		else if (s_bWasRfActive)
		{
			// Tecla recien soltada: en este tick no hubo DisableClockCorrection, asi que
			// m_bDisabledThisInvoke sigue en false - que es exactamente la condicion de la rama de
			// restore de CorrectExtendedCommand, la que devuelve ack/choked reales al netchannel en
			// vez de dejarlo clavado en -1/255.
			g_RapidFireRefEngine.CorrectExtendedCommand(ctx);
			g_RapidFireRefEngine.Reset();
		}

		// Quien termino fijando el contador. RunSurvivor solo lo toca en el tick inicial y solo si
		// el factor combinado es > 0; si RF esta activo pero DisableClockCorrection salio temprano
		// (queue <= 0), el numero que quedo es el que dejo SpeedHack o ApplyExtraScaling.
		if (s_bFirstCommandThisFrame)
			s_bDrivenByRapidFire = bRF && (g_varRef.iFactorSpeedHack + g_varRef.iFactorRapidFire) > 0;

		if (Features::Exploits::Diag::bLogReference && s_bFirstCommandThisFrame && (bSH || bRF))
		{
			Debug::Log(
				"[Byx][Ref] sh=%d(f=%d) rf=%d(f=%d) | extra=%d rfDrive=%d | firstTick=%d cmd=%d | "
				"ack=%d choked=%d | forceSend=%d\n",
				bSH, g_varRef.iFactorSpeedHack, bRF, g_varRef.iFactorRapidFire,
				s_nExtraResult, s_bDrivenByRapidFire,
				s_nFirstTick, pCmd->command_number,
				pNetChan->m_nOutSequenceNrAck, pNetChan->m_nChokedPackets,
				G::shouldForceSendPackets ? 1 : 0);
		}

		s_bWasRfActive = bRF;
		s_bFirstCommandThisFrame = false;
	}

	int GetExtraCalls()
	{
		return s_nExtraResult < 0 ? 0 : s_nExtraResult;
	}
}
