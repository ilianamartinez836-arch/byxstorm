#include "RapidFire.h"
#include "../../Misc/Binds/Binds.h"
#include <algorithm>
using std::max;
using std::min;

// ─── Núcleo del sistema: DisableClockCorrection ───────────────────────────────
// Lógica extraída de la lambda [&] que vivía en Copy_Command.hpp.
// m_AccumulativeCorrection reemplaza el static local Accumulative_Correction.

void CRapidFire::DisableClockCorrection(__int32 queue, const RFContext& ctx)
{
	if (queue <= 0)
		return;

	m_bDisabledThisInvoke = true;

	bool is_initial = (ctx.Command->Command_Number == ctx.FirstTick);

	if (is_initial)
	{
		ctx.ExtCmd->Extra_Commands = 0;
		ctx.ExtraCommands = max((__int32)(0.06f / ctx.GlobalVars->Interval_Per_Tick + 0.5f), queue);
		m_AccumulativeCorrection = 0;
	}
	else
	{
		m_AccumulativeCorrection += 1;
	}

	if (ctx.InitExtCmd->Extra_Commands == 0)
	{
		if (!m_bNetRestoreNeeded)
		{
			m_nSavedNC16 = *(__int32*)((unsigned __int32)ctx.NetworkChannel + 16);
			m_nSavedNC28 = *(__int32*)((unsigned __int32)ctx.NetworkChannel + 28);
			m_bNetRestoreNeeded = true;
		}
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 16) = -1;
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 28) = 255;
	}
}

// ─── CorrectExtendedCommand ───────────────────────────────────────────────────

void CRapidFire::CorrectExtendedCommand(const RFContext& ctx)
{
	if (*(__int32*)((unsigned __int32)ctx.NetworkChannel + 16) != -1)
	{
		ctx.ExtCmd->Extra_Commands += m_AccumulativeCorrection;
		m_AccumulativeCorrection = 0;
	}
	else if (!m_bDisabledThisInvoke && m_bNetRestoreNeeded)
	{
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 16) = m_nSavedNC16;
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 28) = m_nSavedNC28;
		m_bNetRestoreNeeded      = false;
		m_AccumulativeCorrection = 0;
	}
	m_bDisabledThisInvoke = false;
	ctx.ExtCmd->Sequence_Shift = ctx.InitExtCmd->Sequence_Shift;
}

// ─── ApplyExtraScaling ────────────────────────────────────────────────────────
// Solo RF activo (sin SH). Se llama en el bloque Extra_Commands==-1 antes de
// que InitExtCmd y NetworkChannel estén disponibles, por eso toma parámetros
// individuales en lugar del RFContext completo.

void CRapidFire::ApplyExtraScaling(void* localPlayer, Global_Variables_Structure* gv,
                                    __int32& extraCommands, __int32 interpolateFactor)
{
	extraCommands = std::clamp(g_var.iFactorRapidFire,
		(__int32)(0.06f / gv->Interval_Per_Tick + 0.5f), 500);
	*(float*)((unsigned __int32)localPlayer + 16) *= 1.f + extraCommands * interpolateFactor;
}

// ─── RunSurvivor ─────────────────────────────────────────────────────────────
// Survivors con RF activo: DC con factor combinado (SH+RF o solo RF)
// y fuerza send-rate alta mientras la tecla RF esté presionada.

void CRapidFire::RunSurvivor(const RFContext& ctx)
{
	__int32 factor = g_var.bSHActive
		? g_var.iFactorSpeedHack + g_var.iFactorRapidFire
		: g_var.iFactorRapidFire;

	DisableClockCorrection(factor, ctx);

	if (g_var.bRageActive)
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 28) = 250;
}

// ─── OnInfectedNoVictim ───────────────────────────────────────────────────────
// Infectado materializado sin víctima agarrada:
//   · RF key activa → DC(iFactorRapidFire) para abilities de ataque
//   · Condición de pinning (jockey/charger/smoker) → IN_JUMP + DC(ActionFactor)

void CRapidFire::OnInfectedNoVictim(const RFContext& ctx)
{
	if (g_var.bRageActive)
		DisableClockCorrection(g_var.iFactorRapidFire, ctx);

	bool hasPinState =
		(*(void**)((unsigned __int32)ctx.LocalPlayer + 10012) != INVALID_HANDLE_VALUE) ||
		(*(void**)((unsigned __int32)ctx.LocalPlayer + 10024) != INVALID_HANDLE_VALUE) ||
		(*(void**)((unsigned __int32)ctx.LocalPlayer + 10056) != INVALID_HANDLE_VALUE);

	if (hasPinState)
	{
		ctx.Command->Buttons |=
			(*(void**)((unsigned __int32)ctx.LocalPlayer + 10056) != INVALID_HANDLE_VALUE) * 2;
		DisableClockCorrection(ctx.ActionFactor, ctx);
	}
}

// ─── ToggleAttackOnExtraTicks ────────────────────────────────────────────────-
// En ticks extra del loop (SH o RF activos) alterna IN_ATTACK par/impar
// para que el servidor detecte cada press como un click nuevo.

void CRapidFire::ToggleAttackOnExtraTicks(const RFContext& ctx)
{
	bool bNotFirstTick = (ctx.Command->Command_Number != ctx.FirstTick);

	if ((g_var.bRageActive || g_var.bSHActive) && bNotFirstTick)
	{
		using Get_Weapon_Type_t = void* (__thiscall*)(void*);
		void* Weapon = *(__int8*)((unsigned __int32)ctx.LocalPlayer + 7867) == 0
			? Get_Weapon_Type_t((unsigned __int32)Client_Module + 74304)(ctx.LocalPlayer)
			: nullptr;

		float fireRate = 0.1f;
		if (Weapon)
		{
			float cycle = *(float*)((unsigned __int32)Weapon + 2400) - ctx.GlobalVars->Time;
			if (cycle > ctx.GlobalVars->Interval_Per_Tick && cycle < 2.f)
				fireRate = cycle;
		}

		int tickSpacing = max(2, (int)(fireRate / ctx.GlobalVars->Interval_Per_Tick + 0.5f));
		int tickOffset  = ctx.Command->Command_Number - ctx.FirstTick;

		if (tickOffset % tickSpacing != 0)
			ctx.Command->Buttons &= ~IN_ATTACK;
	}

	if ((g_var.bRageActive || g_var.bSHActive) &&
	    bNotFirstTick && (ctx.Command->Buttons & IN_ATTACK2))
	{
		if (ctx.Command->Command_Number % 2 == 0)
			ctx.Command->Buttons &= ~IN_ATTACK2;
	}
}

void CRapidFire::Reset()
{
	m_AccumulativeCorrection = 0;
	m_bNetRestoreNeeded      = false;
	m_bDisabledThisInvoke    = false;
	m_nSavedNC16             = 0;
	m_nSavedNC28             = 0;
}

// ── Features::Exploits::RapidFire glue ─────────────────────────────────────
// CRapidFire's per-command algorithm wants to run once per generated command (RunSurvivor decides
// "is this the first command of the burst" via Command_Number == FirstTick, then
// CorrectExtendedCommand runs immediately after in the SAME tick - that pairing matters, see
// below). Our CL_Move.cpp instead decides up front how many extra commands to send, then loops.
// The glue below makes that work without changing either side's core logic: BeginFrame() resets
// the per-frame output slot, OnCommand() runs CRapidFire for every command (real + every extra)
// and keeps the netchannel/accumulator state correct tick-by-tick exactly like the original design
// intends, and GetExtraCalls() hands the freshly computed count back to CL_Move.cpp's loop.
namespace Features::Exploits::RapidFire
{
	static Extended_Command_Structure s_ExtCmd{};
	static __int32 s_nFirstTick = 0;
	static bool    s_bFirstCommandThisFrame = true;
	static int     s_nExtraResult = 0;
	static int     s_nCarriedDebt = 0;
	static bool    s_bWasActive = false;

	static RFContext BuildContext(CUserCmd *pCmd)
	{
		auto pLocal = H::Entities->GetLocal();
		return RFContext{
			reinterpret_cast<Command_Structure *>(pCmd),
			&s_ExtCmd,
			&s_ExtCmd,
			reinterpret_cast<CNetChannel *>(I::EngineClient->GetNetChannelInfo()),
			reinterpret_cast<Global_Variables_Structure *>(I::GlobalVars),
			reinterpret_cast<void *>(pLocal),
			s_nFirstTick,
			nValue,
			s_nExtraResult
		};
	}

	bool IsActive()
	{
		return bEnabled && nKey && Features::Binds::Down(nKey);
	}

	void BeginFrame()
	{
		s_bFirstCommandThisFrame = true;
		s_nExtraResult = 0;
	}

	void OnCommand(CUserCmd *pCmd)
	{
		if (!pCmd)
			return;

		// Must update s_nFirstTick BEFORE building the context - RFContext::FirstTick is copied by
		// value at construction (unlike ExtraCommands, which is a live reference), so building the
		// context first would hand DisableClockCorrection last frame's stale FirstTick on this
		// frame's first command. That silently breaks the is_initial check (Command_Number ==
		// FirstTick) forever, which is the ONLY place that ever sets ExtCmd->Extra_Commands to a
		// real value - everything downstream (the extra-call loop, the actual extra shots) depends
		// on this comparison succeeding exactly once per frame. Bug found 2026-07-17: user reported
		// RapidFire only changed the choke stat (the unconditional netchannel force-write, which
		// doesn't depend on is_initial) with zero actual extra ticks - exactly the symptom of
		// is_initial never being true.
		bool bActive = IsActive();

		if (s_bFirstCommandThisFrame)
			s_nFirstTick = pCmd->command_number;

		RFContext ctx = BuildContext(pCmd);

		g_var.bRageActive = bActive;

		if (bActive)
		{
			// Carry-over debt from a previous frame's cash-in (CorrectExtendedCommand's
			// ExtCmd->Extra_Commands += m_AccumulativeCorrection branch) only gets folded in once,
			// on the first command of a fresh burst - matches "never reduce the request, only add
			// pending debt on top".
			g_var.iFactorRapidFire = nValue +
				(s_bFirstCommandThisFrame ? s_nCarriedDebt : 0);
			if (s_bFirstCommandThisFrame)
				s_nCarriedDebt = 0;

			// [2026-08-13] ApplyExtraScaling: el multiplicador del codigo de referencia. Con
			// nInterpolate en 0 es un no-op (multiplica m_flSimulationTime por 1.0). Subirlo estira
			// sim_time por (1 + extraCommands * nInterpolate), acelerando el avance de ticks del
			// arma = mas disparos/accion por rafaga. Condicion del original: solo RF activo, en el
			// primer comando del frame, antes de que RunSurvivor pueda volver a pisar el contador.
			if (s_bFirstCommandThisFrame && nInterpolate != 0)
				g_RapidFireEngine.ApplyExtraScaling(ctx.LocalPlayer, ctx.GlobalVars,
					s_nExtraResult, nInterpolate);

			g_RapidFireEngine.RunSurvivor(ctx);
			g_RapidFireEngine.ToggleAttackOnExtraTicks(ctx);
			g_RapidFireEngine.CorrectExtendedCommand(ctx);

			s_nCarriedDebt = s_ExtCmd.Extra_Commands;
		}
		else if (s_bWasActive)
		{
			// Key just released: no DisableClockCorrection call happened this tick, so
			// m_bDisabledThisInvoke is still false - this is exactly the condition
			// CorrectExtendedCommand's restore branch checks, so it un-forces the netchannel back
			// to the last genuine ack/choked values instead of leaving it stuck at -1/255.
			g_RapidFireEngine.CorrectExtendedCommand(ctx);
			g_RapidFireEngine.Reset();
			s_nCarriedDebt = 0;
		}

		s_bWasActive = bActive;
		s_bFirstCommandThisFrame = false;
	}

	int GetExtraCalls()
	{
		return s_nExtraResult < 0 ? 0 : s_nExtraResult;
	}
}
