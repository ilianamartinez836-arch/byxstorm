#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Weapon/NoRecoil.h"
#include "../Features/Misc/NoSpread.h"

MAKE_HOOK(
	CBasePlayer_CalcPlayerView,
	Memory::ResolveFn("CalcPlayerView", "client.dll", "55 8B EC 83 EC ?? 53 56 8B F1 8B 0D ?? ?? ?? ?? 8B 01 8B 50 ?? 57 FF D2 84 C0", 0x020750),
	void, __fastcall, void *ecx, void *edx, Vector &eyeOrigin, QAngle &eyeAngles, float &fov)
{
	static bool bOnce=false;if(!bOnce){Debug::Log("[Byx] CalcPlayerView hooked!\n");bOnce=true;}
	CALL_ORIGINAL(ecx, edx, eyeOrigin, eyeAngles, fov);

	// La camara en tercera persona ya NO se maneja aca: ahora la mueve el hook de
	// IClientMode::OverrideView (indice 19), que corre DESPUES de este punto y escribe pSetup.
	// Ver Thirdperson.h.

	// Silent aim writes its angle into pCmd->viewangles; the engine's prediction then copies
	// that onto the local player, so by the time this runs the player-side angles ARE the aim
	// angle and restoring from them is restoring the corruption. IVEngineClient::GetViewAngles
	// is the one source silent never touches (it's the raw mouse-driven value), so it is the
	// authoritative "where the player is really looking" - use it instead of the player-side
	// +0x1384 copy whenever silent is what we're undoing. NoRecoil keeps +0x1384: that path is
	// separately verified and is about punch, not about our own writes. (2026-07-27)
	if(Features::Aimbot::bSilentActive){
		Vec3 vReal=I::EngineClient->GetViewAngles();
		eyeAngles.x=vReal.x; eyeAngles.y=vReal.y;
	}else if(Features::NoRecoil::bEnabled){
		float *pRealAngles=(float*)((DWORD_PTR)ecx+0x1384);
		eyeAngles.x=pRealAngles[0]; eyeAngles.y=pRealAngles[1];
	}
	if(Features::NoRecoil::bEnabled){
		float f=1.f-Features::NoRecoil::flAmount/100.f;
		float *pPunch=(float*)((DWORD_PTR)ecx+0x1204);
		eyeAngles.x-=pPunch[0]*f; eyeAngles.y-=pPunch[1]*f;

		// ONE owner for the punch field (2026-07-27). client.dll's CTerrorGun::FireBullet adds
		// GetPunchAngle() straight into the shot direction, so this field is not merely visual -
		// writing to it changes where bullets go. NoSpread's punch compensation subtracts the
		// exact same value from the command angle, which is the correct inverse; if BOTH ran,
		// each would be correcting a value the other had already altered. That double-handling
		// is what made the 2026-07-22 attempt overcorrect (shots high) and get reverted. So the
		// writes below are skipped whenever NoSpread owns the field - the eyeAngles adjustment
		// above still runs either way, since that part is purely the rendered view.
		if(!(Features::NoSpread::IsActive()&&Features::NoSpread::bCompensatePunch)){
			pPunch[0]*=f; pPunch[1]*=f; pPunch[2]*=f;
			// m_vecPunchAngleVel - leftover velocity re-kicks punch on later frames if the
			// angle alone is decayed.
			float *pPunchVel=(float*)((DWORD_PTR)ecx+0x1210);
			pPunchVel[0]*=f; pPunchVel[1]*=f; pPunchVel[2]*=f;
		}
	}
}
