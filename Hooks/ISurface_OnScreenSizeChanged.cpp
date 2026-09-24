#include "../../SDK/SDK.h"


MAKE_HOOK(
	ISurface_OnScreenSizeChanged, Memory::GetVFunc(I::MatSystemSurface, 108),
	void, __fastcall, void *ecx, void *edx, int nOldWidth, int OldHeight)
{
	CALL_ORIGINAL(ecx, edx, nOldWidth, OldHeight);

	H::Fonts->Reload();
	H::Draw->UpdateScreenSize();
}