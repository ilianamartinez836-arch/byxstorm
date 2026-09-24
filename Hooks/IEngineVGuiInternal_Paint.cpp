#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../Features/ESP/ESP.h"
#include "../Features/ESP/ItemESP.h"
#include "../Features/ESP/OffScreen.h"
#include "../Features/MiscTools/DamageIndicator/DamageIndicator.h"
#include "../Features/MiscTools/VirtualKeyboard/VirtualKeyboard.h"
#include "../Features/MiscTools/MovementRecorder/MovementRecorder.h"
#include "../Features/Camera/Thirdperson.h"
#include "../Features/Camera/ViewModel.h"
#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Lua/LuaEngine.h"

MAKE_HOOK(
	IEngineVGuiInternal_Paint, Memory::GetVFunc(I::EngineVGui, 14),
	void, __fastcall, void *ecx, void *edx, int mode)
{
	static bool bLogged = false;
	if (!bLogged) { Debug::Log("[Byx] Paint hook first call, mode=%d\n", mode); bLogged = true; }

	CALL_ORIGINAL(ecx, edx, mode);

	if ((mode & PAINT_UIPANELS) && I::EngineClient->IsInGame() && !I::EngineClient->IsDrawingLoadingImage())
	{
		H::Draw->UpdateW2SMatrix();
		I::MatSystemSurface->StartDrawing();
		{
			Features::ESP::Render();
			Features::ESP::ItemESP::Render();
			Features::ESP::OffScreen::Render();
			Features::MiscTools::DamageIndicator::Paint();
			Features::MiscTools::VirtualKeyboard::Paint();
			Features::MiscTools::MovementRecorder::Paint();

			// Scripts draw INSIDE StartDrawing/FinishDrawing - the surface draw calls are only
			// valid here, which is why byx.draw_* refuses to do anything outside on_paint.
			Features::Lua::OnPaint();
			// Network Info / Watchers moved to the ImGui pipeline (CMenuImgui::Run(), see
			// ImguiMenu.cpp) so they render as real movable/resizable ImGui windows - this VGUI
			// surface (I::MatSystemSurface->StartDrawing/FinishDrawing) is a different, unrelated
			// draw pipeline that can't host ImGui windows.
			Features::ViewModel::Apply();

			int cx=H::Draw->GetScreenW()/2,cy=H::Draw->GetScreenH()/2;

			float flGameHorizFOV=90.f;
			auto pLocal=H::Entities->GetLocal();
			if(pLocal)flGameHorizFOV=(float)pLocal->m_iDefaultFOV();
			float flScale=(cy*4.f/3.f)/tanf(DEG2RAD(flGameHorizFOV/2.f));

			if(Features::Aimbot::cfg.bEnabled&&Features::Aimbot::cfg.bDrawFOV&&Features::Aimbot::cfg.nMode==Features::Aimbot::MODE_LEGIT){
				// BUG FOUND (2026-07-31): this used to draw tan(flFOV/2), i.e. a cone HALF as wide
				// as the one the aimbot actually enforces. cfg.flFOV is already a half-angle -
				// Aimbot.cpp rejects a candidate when the angular deviation from the view exceeds
				// flFOV outright, it never halves it - so the circle has to be tan(flFOV). Clamped
				// because the slider reaches 180 and tan blows up at 90.
				float flDeg=Features::Aimbot::cfg.flFOV; if(flDeg>85.f)flDeg=85.f;
				float r=tanf(DEG2RAD(flDeg))*flScale;
				H::Draw->OutlinedCircle(cx,cy,(int)r,64,Color_t(0,255,100,60));
			}
		}
		I::MatSystemSurface->FinishDrawing();
	}
}
