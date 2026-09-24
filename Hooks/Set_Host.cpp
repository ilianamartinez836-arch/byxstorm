#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"

MAKE_HOOK(
	Set_Host, Memory::ResolveFn("Set_Host", "client.dll", "55 8B EC 8B 45 ?? 8B 11 89 41", 0x145010),
	void, __fastcall, void *ecx, void *edx, void *pPlayer)
{
	static bool bOnce=false;if(!bOnce){Debug::Log("[Byx] Set_Host hooked!\n");bOnce=true;}
	CALL_ORIGINAL(ecx, edx, pPlayer);
}
