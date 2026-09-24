#include "../../SDK/SDK.h"
#include "../Features/Visuals/Chams/Chams.h"

// IVModelRender::DrawModelExecute - vtable index 19. See the long note at the top of Chams.h for
// how that index was verified against this build instead of trusted.
//
// The address expression is guarded: I::ModelRender is only non-null if the interface manager
// actually resolved VEngineModel016, and CHook::Create already skips a null target (it used to
// silently no-op on one, which is exactly why that guard exists). So a failed interface lookup
// leaves the hook uninstalled and Chams simply inert, instead of dereferencing null at inject.
MAKE_HOOK(
	IVModelRender_DrawModelExecute,
	I::ModelRender ? Memory::GetVFunc(I::ModelRender, 19) : nullptr,
	void, __fastcall, void *ecx, void *edx,
	const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pCustomBoneToWorld)
{
	Features::Visuals::Chams::LogSeen(pInfo, "DrawModelExecute");

	// bInWrapper: the DrawModelEx hook is already wrapping this same model from one level up (if
	// DrawModelEx happens to dispatch virtually on this build). Wrapping it again here would draw
	// it two or four times.
	const auto *pCfg = Features::Visuals::Chams::bInWrapper
	                       ? nullptr
	                       : Features::Visuals::Chams::Resolve(pInfo);
	if (!pCfg)
	{
		CALL_ORIGINAL(ecx, edx, state, pInfo, pCustomBoneToWorld);
		return;
	}

	// Through-walls pass FIRST, with depth testing off, so the normal pass draws over it. Done the
	// other way round the occluded silhouette would paint on top of the visible one.
	if (pCfg->bThroughWalls)
	{
		Features::Visuals::Chams::BindPass(*pCfg, true);
		CALL_ORIGINAL(ecx, edx, state, pInfo, pCustomBoneToWorld);
	}

	Features::Visuals::Chams::BindPass(*pCfg, false);
	CALL_ORIGINAL(ecx, edx, state, pInfo, pCustomBoneToWorld);

	Features::Visuals::Chams::EndPasses();
}
