#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../Features/Camera/Thirdperson.h"

// IClientMode::OverrideView, indice de vtable 19.
//
// Verificado contra client.dll (13/08, IDA): la vtable de FullscreenTerrorClientMode esta en
// 0x105BCDD4 y su entrada 19 (0x100C3EE0) es la UNICA que llama CAM_IsThirdPerson (IInput vtbl[30])
// + CAM_GetCameraOffset (vtbl[31]) sobre el global de IInput (client.dll+0x6E2F04) y despues hace
// VectorMA(pSetup->origin, -dist, forward) + VectorCopy(camAngles, pSetup->angles). Es exactamente
// el OverrideView del motor. El header iclientmode.h lo contaria en 16 (esta 3 slots corto hasta
// aca y 6 hasta CreateMove). Ver docs/04_OFFSETS.md.
//
// Es el ULTIMO que toca la vista antes del render (corre despues de CalcPlayerView y de
// C_BasePlayer::OverrideView). Por eso la tercera persona se resuelve aca: escribir
// pSetup->origin/angles despues del CALL_ORIGINAL no puede ser pisado por nada del juego.
MAKE_HOOK(
	IClientMode_OverrideView, I::ClientModeShared ? Memory::GetVFunc(I::ClientModeShared, 19) : nullptr,
	void, __fastcall, void *ecx, void *edx, CViewSetup *pSetup)
{
	CALL_ORIGINAL(ecx, edx, pSetup);
	Features::Thirdperson::OverrideView(pSetup);
}
