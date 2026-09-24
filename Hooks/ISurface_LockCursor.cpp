#include "../../SDK/SDK.h"

#include "../Features/Menu/ImguiMenu.h"

MAKE_HOOK(
	ISurface_LockCursor, Memory::GetVFunc(I::MatSystemSurface, 59),
	void, __fastcall, void *ecx, void *edx)
{
	F::MenuImgui->IsOpen() ? I::MatSystemSurface->UnlockCursor() : CALL_ORIGINAL(ecx, edx);
}