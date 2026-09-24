#include "../../SDK/SDK.h"


MAKE_HOOK(
	CViewRender_RenderView, Signatures::CViewRender_RenderView.Get(),
	void, __fastcall, void *ecx, void *edx, const CViewSetup &view, const CViewSetup &hudViewSetup, int nClearFlags, int whatToDraw)
{
	I::ViewRender = reinterpret_cast<IViewRender *>(ecx);

	CALL_ORIGINAL(ecx, edx, view, hudViewSetup, nClearFlags, whatToDraw);

	G::View = view;
}