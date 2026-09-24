#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../Features/Visuals/Glow/Glow.h"
#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Camera/Thirdperson.h"
#include "../Features/Lua/LuaEngine.h"

MAKE_HOOK(
	IBaseClientDLL_LevelShutdown, Memory::GetVFunc(I::BaseClientDLL, 6),
	void, __fastcall, void *ecx, void *edx)
{
	Debug::Log("[Byx] LevelShutdown called\n");
	CALL_ORIGINAL(ecx, edx);

	H::Entities->ClearCache();

	// Every entity we saved glow state for is gone with the level - drop the records instead of
	// carrying dangling pointers into the next map (see Glow::ClearState).
	Features::Visuals::Glow::ClearState();

	// Same reason, and newly possible now that the aimbot's cross-frame state lives in one struct:
	// it holds a raw C_BaseEntity* (the locked target) plus a framecount-keyed cache, and both are
	// meaningless - the pointer outright dangling - once the level is gone. Before the state was
	// consolidated these were a dozen function-local statics with no way to reach them at all.
	Features::Aimbot::state.Reset();

	// El flag de camara en tercera persona vive DENTRO de CInput, no en nuestro DLL: si queda en
	// true durante la transicion de mapa, el post-procesado de la camara de muerte se aplica en el
	// contexto equivocado y se ve el "agujero negro" (pantalla b/n con vignette). Ver Thirdperson.cpp.
	Features::Thirdperson::Reset();

	// Scripts get told too: anything they cached about entities on the old map is now dangling for
	// exactly the same reason the aimbot state above is.
	Features::Lua::OnLevelShutdown();
}