#include "../../SDK/SDK.h"
#include "../Features/Visuals/Chams/Chams.h"

// IVModelRender::DrawModelEx - vtable index 16.
//
// The second half of Chams. See the note on Chams::bInWrapper for why one hook is not enough:
// client.dll draws players through DrawModelExecute (vtable, our other hook sees it) and
// everything else - commons included - through DrawModelEx, which resolves DrawModelExecute
// internally inside engine.dll where no vtable hook can reach.
//
// Index 16 rests on the same verified layout as index 19: a scan of every modelrender vtable call
// in client.dll lines up with our header at indices 3, 4, 5, 6, 7, 9, 10, 12, 13, 16, 18, 19, 20,
// 22 and 25, each in a function whose job matches the method's name (CreateInstance/DestroyInstance
// in entity setup/teardown, DrawModelShadowSetup+DrawModelShadow in the shadow pass,
// GetBrightestShadowingLightSource in UpdateShadowDirectionFromLocalLightSource, and this one in a
// routine that builds a ModelRenderInfo_t on the stack and passes it as the only argument).
MAKE_HOOK(
	IVModelRender_DrawModelEx,
	I::ModelRender ? Memory::GetVFunc(I::ModelRender, 16) : nullptr,
	int, __fastcall, void *ecx, void *edx, ModelRenderInfo_t &pInfo)
{
	Features::Visuals::Chams::LogSeen(pInfo, "DrawModelEx");

	const auto *pCfg = Features::Visuals::Chams::Resolve(pInfo);
	if (!pCfg || Features::Visuals::Chams::bInWrapper)
		return CALL_ORIGINAL(ecx, edx, pInfo);

	Features::Visuals::Chams::bInWrapper = true;

	// Through-walls pass first, same ordering reason as the other hook: depth-test-off underneath,
	// the normal pass painted over it.
	if (pCfg->bThroughWalls)
	{
		Features::Visuals::Chams::BindPass(*pCfg, true);
		CALL_ORIGINAL(ecx, edx, pInfo);
	}

	Features::Visuals::Chams::BindPass(*pCfg, false);
	const int nRet = CALL_ORIGINAL(ecx, edx, pInfo);

	Features::Visuals::Chams::EndPasses();
	Features::Visuals::Chams::bInWrapper = false;
	return nRet;
}
