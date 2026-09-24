#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../../Utils/Memory/Memory.h"
#include "../../Utils/CrashHandler/CrashHandler.h"
#include "../Features/Movement/Bunnyhop.h"
#include "../Features/Movement/AutoStrafe.h"
#include "../Features/Movement/AutoDuck.h"
#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Aimbot/AimInfected.h"
#include "../Features/Aimbot/AutoShove/AutoShove.h"
#include "../Features/MiscTools/ChatSpam/ChatSpam.h"
#include "../Features/MiscTools/NameStealer/NameStealer.h"
#include "../Features/MiscTools/DamageIndicator/DamageIndicator.h"
#include "../Features/Misc/NoSpread.h"
#include "../Features/Misc/DisableInterp.h"
#include "../Features/Misc/AutoPistol.h"
#include "../Features/Misc/AutoClicker/AutoClicker.h"
#include "../Features/Exploits/SpeedHack/SpeedHack.h"
#include "../Features/Exploits/RapidFire/RapidFire.h"
#include "../Features/Exploits/RapidFireCustom/RapidFireCustom.h"
#include "../Features/Exploits/Reference/RefDriver.h"
#include "../Features/Exploits/RollExploit/RollExploit.h"
#include "../Features/Exploits/NoFallDamage/NoFallDamage.h"
#include "../Features/Exploits/NameBug/NameBug.h"
#include "../Features/Exploits/AirStuck/AirStuck.h"
#include "../Features/Camera/FreeLook/FreeLook.h"
#include "../Features/MiscTools/MovementRecorder/MovementRecorder.h"
#include "../Features/Lua/LuaEngine.h"

MAKE_HOOK(
	Copy_Command, Memory::ResolveFn("Copy_Command", "client.dll", "55 8B EC 8B 45 ?? 85 C0 74 ?? 50 81 C1", 0x10B410),
	void, __fastcall, void *ecx, void *edx, CUserCmd *pCmd)
{
	static bool bLogged = false;
	if (!bLogged) { Debug::Log("[Byx] Copy_Command hooked. pCmd=0x%p\n", pCmd); bLogged = true; }
	if (!pCmd) { CALL_ORIGINAL(ecx, edx, pCmd); return; }

	// Plain SpeedHack's extra CL_Move calls resample the whole real input state, attack button
	// included - without this, holding fire while SpeedHack is active also multiplies shots
	// exactly like RapidFire (confirmed in-game). User's explicit choice: SpeedHack is movement
	// only, zero shooting effect while held (Custom SpeedHack and RapidFire are left unrestricted -
	// see Exploits.h). Stripped before any feature below gets to see IN_ATTACK, so AutoPistol/
	// NoSpread naturally treat a masked tick as "not firing" too.
	if (Features::Exploits::SpeedHack::bExtraTick)
		pCmd->buttons &= ~(IN_ATTACK | IN_ATTACK2);

	// CRapidFire's real per-tick call site (see RapidFire.h) - runs on the real command AND every
	// synthetic extra one this frame. Drives the extra-call count CL_Move.cpp reads after its real
	// CALL_ORIGINAL, and paces IN_ATTACK/IN_ATTACK2 on the extra ticks to the weapon's own
	// fire-rate cycle instead of leaving RapidFire unrestricted.
	Features::Exploits::RapidFire::OnCommand(pCmd);

	// Par de referencia (SpeedHackRef + RapidFireRef), aparte del de arriba y con su propio estado -
	// se pueden prender los dos a la vez para comparar. Corre despues a proposito: si los dos pacean
	// IN_ATTACK sobre el mismo comando, el ultimo en escribir es el que se ve, y aca el que interesa
	// medir es el de referencia. Ver Features/Exploits/Reference/RefDriver.h.
	Features::Exploits::Reference::OnCommand(pCmd);

	auto pLocal = H::Entities->GetLocal();
	G::bAimbotActive = false;

	// RapidFire Custom ("Rapid Fire+" in a reference implementation's own menu) - confirmed 2026-07-18 via IDA
	// (see RapidFireCustom.h for the full writeup): NOT extra-ticks, just zeroes the active weapon's
	// own cooldown field (weapon+2400, m_flNextPrimaryAttack-equivalent) every command. Runs on
	// the real command and every synthetic extra one - idempotent, safe to call redundantly.
	if (Features::Exploits::RapidFireCustom::IsActive())
	{
		if (auto pWeaponRFC = pLocal ? pLocal->GetActiveWeaponForSelection() : nullptr)
			*reinterpret_cast<float *>(reinterpret_cast<uintptr_t>(pWeaponRFC) + 2400) = 0.f;
	}

	Features::AutoDuck_Run(pCmd);
	Features::Bunnyhop_Run(pCmd, pLocal);
	Features::AutoStrafe_Run(pCmd, pLocal);
	// Free Look corre ANTES del aimbot a proposito: si el aimbot engancha, sus angulos ganan y
	// el personaje gira. Al reves, el freelook pisaria al aimbot. Ver FreeLook.h.
	Features::FreeLook::Run(pCmd);
	// AutoShove runs BEFORE the aimbot on purpose. A shove and a shot in the same command fight
	// each other (the game services the shove and eats the shot), so the shove has to be decided
	// first and the aimbot has to be able to see that decision - it reads AutoShove::
	// bShovedThisTick and stands down for that command. The reverse order would have the aimbot
	// commit a shot that silently never happens.
	Features::Aimbot::AutoShove::Run(pCmd);
	// SEH guard: si el aimbot revienta, se escribe el reporte (modulo+offset + barrido de pila) y el
	// juego sigue en vez de cerrarse. Es la unica forma de saber DONDE crashea sin debugger.
	__try
	{
		Features::Aimbot::Run(pCmd);
	}
	__except (CrashHandler::WriteReport(GetExceptionInformation(), "Aimbot::Run"))
	{
		Debug::Log("[Byx] Aimbot::Run threw - frame skipped (report in ByxStorm/crashes/)\n");
	}
	// Runs AFTER the main Aimbot so it wins pCmd->viewangles if both somehow end up active at
	// once (separate feature, own hold-key gate - see AimInfected.h for why it's not a mode
	// on the main Aimbot).
	Features::AimInfected::Run(pCmd);
	Features::MiscTools::ChatSpam::Run(pLocal);
	Features::MiscTools::NameStealer::Run(pLocal);
	Features::MiscTools::DamageIndicator::Run(pLocal, pCmd);
	Features::MiscTools::MovementRecorder::Run(pCmd, pLocal);
	Features::Exploits::RollExploit::Run(pCmd);
	Features::Exploits::NoFallDamage::Run(pLocal);
	Features::Exploits::AirStuck::Run(pCmd);
	Features::Exploits::NameBug::Run();
	Features::DisableInterp::Run();
	Features::AutoPistol::Run(pCmd);
	Features::AutoClicker::Run(pCmd);

	// Scripts run LAST of the command-building features, so whatever they write to the command
	// wins over the built-in ones. That is the point of a scripting layer: if a script and a
	// feature disagree, the script is the more specific intent. It also means a script sees the
	// command as the cheat left it, not as the player sent it.
	Features::Lua::OnCreateMove(pCmd);

	// NoSpread internally gates on the weapon's own fire-rate cooldown, so this
	// compensates every real shot while held (not just the first of a burst). Runs after
	// AutoPistol so it sees buttons post-reinforcement (skips ticks AutoPistol masked off).
	if(pCmd->buttons & IN_ATTACK)
	{
		__try { Features::NoSpread::Apply(pCmd, pLocal); }
		__except (CrashHandler::WriteReport(GetExceptionInformation(), "NoSpread::Apply"))
		{
			Debug::Log("[Byx] NoSpread::Apply threw - frame skipped\n");
		}
	}

	CALL_ORIGINAL(ecx, edx, pCmd);
}
