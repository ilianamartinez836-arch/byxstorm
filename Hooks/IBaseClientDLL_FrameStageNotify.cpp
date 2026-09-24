#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../Features/Visuals/Glow/Glow.h"
#include "../Features/Effects/Effects.h"
#include "../Features/Lua/LuaEngine.h"

MAKE_HOOK(
	IBaseClientDLL_FrameStageNotify, Memory::GetVFunc(I::BaseClientDLL, 34),
	void, __fastcall, void *ecx, void *edx, ClientFrameStage_t curStage)
{
	static bool bLogged = false;
	if (!bLogged) {
		Debug::Log("[Byx] FrameStageNotify first call, stage=%d\n", (int)curStage);
		bLogged = true;
	}

	CALL_ORIGINAL(ecx, edx, curStage);

	if (curStage == ClientFrameStage_t::FRAME_RENDER_START)
		H::Input->Update();

	if (!I::EngineClient->IsInGame())
		return;

	// Scripts get every stage, unfiltered - a script that only cares about one can check the
	// argument, but one that needs a stage we did not anticipate cannot get it back.
	Features::Lua::OnFrameStage((int)curStage);

	switch (curStage)
	{
		case ClientFrameStage_t::FRAME_NET_UPDATE_START:
			H::Entities->ClearCache();
			break;

		case ClientFrameStage_t::FRAME_NET_UPDATE_END:
			H::Entities->UpdateCache();
			break;

		// Before the 3D scene is built, so CGlowManager::Update() (a per-frame game system) sees
		// this frame's glow state rather than last frame's. Also the reason this is not driven
		// from the VGUI paint hook the ESP uses: that runs AFTER the world is already drawn.
		case ClientFrameStage_t::FRAME_RENDER_START:
			Features::Visuals::Glow::Run();
			// Mismo motivo que el Glow: hay que marcar la roca con EF_NODRAW ANTES de que se arme
			// la escena 3D, o el frame actual la dibuja igual.
			Features::Effects::Run();
			break;

		default: break;
	}
}
