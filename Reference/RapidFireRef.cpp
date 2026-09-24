#include "RapidFireRef.h"
#include "../../Misc/Binds/Binds.h"
#include <algorithm>
using std::max;
using std::min;

// ─── Núcleo del sistema: DisableClockCorrection ───────────────────────────────
// Lógica extraída de la lambda [&] que vivía en Copy_Command.hpp.
// m_AccumulativeCorrection reemplaza el static local Accumulative_Correction.

void CRapidFireRef::DisableClockCorrection(__int32 queue, const RFRefContext& ctx)
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

void CRapidFireRef::CorrectExtendedCommand(const RFRefContext& ctx)
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

void CRapidFireRef::ApplyExtraScaling(void* localPlayer, Global_Variables_Structure* gv,
                                       __int32& extraCommands, __int32 interpolateFactor)
{
	extraCommands = std::clamp(g_varRef.iFactorRapidFire,
		(__int32)(0.06f / gv->Interval_Per_Tick + 0.5f), 500);
	*(float*)((unsigned __int32)localPlayer + 16) *= 1.f + extraCommands * interpolateFactor;
}

// ─── RunSurvivor ─────────────────────────────────────────────────────────────
// Survivors con RF activo: DC con factor combinado (SH+RF o solo RF)
// y fuerza send-rate alta mientras la tecla RF esté presionada.

void CRapidFireRef::RunSurvivor(const RFRefContext& ctx)
{
	__int32 factor = g_varRef.bSHActive
		? g_varRef.iFactorSpeedHack + g_varRef.iFactorRapidFire
		: g_varRef.iFactorRapidFire;

	DisableClockCorrection(factor, ctx);

	if (g_varRef.bRageActive)
		*(__int32*)((unsigned __int32)ctx.NetworkChannel + 28) = 250;
}

// ─── OnInfectedNoVictim ───────────────────────────────────────────────────────
// Infectado materializado sin víctima agarrada:
//   · RF key activa → DC(iFactorRapidFire) para abilities de ataque
//   · Condición de pinning (jockey/charger/smoker) → IN_JUMP + DC(ActionFactor)

void CRapidFireRef::OnInfectedNoVictim(const RFRefContext& ctx)
{
	if (g_varRef.bRageActive)
		DisableClockCorrection(g_varRef.iFactorRapidFire, ctx);

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

void CRapidFireRef::ToggleAttackOnExtraTicks(const RFRefContext& ctx)
{
	bool bNotFirstTick = (ctx.Command->Command_Number != ctx.FirstTick);

	if ((g_varRef.bRageActive || g_varRef.bSHActive) && bNotFirstTick)
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

	if ((g_varRef.bRageActive || g_varRef.bSHActive) &&
	    bNotFirstTick && (ctx.Command->Buttons & IN_ATTACK2))
	{
		if (ctx.Command->Command_Number % 2 == 0)
			ctx.Command->Buttons &= ~IN_ATTACK2;
	}
}

void CRapidFireRef::Reset()
{
	m_AccumulativeCorrection = 0;
	m_bNetRestoreNeeded      = false;
	m_bDisabledThisInvoke    = false;
	m_nSavedNC16             = 0;
	m_nSavedNC28             = 0;
}

// ── Features::Exploits::RapidFireRef ──────────────────────────────────────────
// Solo config. Quien maneja el ciclo por frame/comando es RefDriver.cpp.
namespace Features::Exploits::RapidFireRef
{
	bool IsActive()
	{
		return bEnabled && nKey && Features::Binds::Down(nKey);
	}
}
