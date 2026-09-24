#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../../Utils/Memory/Memory.h"
#include "../Features/Effects/Effects.h"

typedef char *(__thiscall *GetEffectNameFn)(void *);

// Isolated in its own function (not inlined into the hook) because __try/__except can't safely share
// a function with C++ objects that have destructors, and because we genuinely don't trust this read:
// confirmed in-game (2026-07-17) that Draw_Effect fires with IsInGame()==true during the map LOADING
// transition too, before the local player/world exists - GetEffectName(this-8) crashed the process
// hard on an "Effect" object from that window, even after gating on IsInGame() alone. Rather than
// guess at some other, more precise "actually ready" check, catch the access violation with SEH and
// just treat it as "no name available" - a single skipped frame's effect-name check is harmless, a
// hard crash on inject is not.
static char *SafeGetEffectName(GetEffectNameFn fn, void *pEffect)
{
	__try
	{
		return fn(pEffect);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

// L4D2 client effect dispatcher. Suppressing an effect = NOT calling the original (it never renders).
// Technique from the Storm reference (Downloads/.../Draw_Effect.hpp): the effect's name is read via a
// GetEffectName(this - 8) accessor, then strstr'd against the user's blocked list. Returns __int8
// like the real function ("did it draw"): 0 when we suppress, the original's own value on pass-through.
//
// REAL SIGNATURE HAS TWO stack args beyond `this`, confirmed against the reference's
// `Redirected_Draw_Effect(void* Effect, void* Unknown_Parameter_1, void* Unknown_Parameter_2)`
// (same function: Client_Module+1425904 == our 0x15C1F0). The original stub here only declared ONE
// stack param (`pUnknown`) - a callee-cleans-stack (thiscall/fastcall) function that under-declares
// its own parameter count pops fewer bytes than the real caller pushed on every return, leaving the
// stack 4 bytes off from then on. That's a silent, delayed corruption - not an access violation
// inside this function, which is exactly why wrapping the GetEffectName read in SEH didn't stop the
// crash (confirmed in-game 2026-07-17): the damage happens on RETURN, after our try/except already
// exited cleanly, and manifests later in whatever client.dll code runs next on the corrupted stack.
MAKE_HOOK(
	Draw_Effect, Memory::ResolveFn("Draw_Effect", "client.dll", "55 8B EC 83 EC ?? A1 ?? ?? ?? ?? 53 56 8B F1 33 C9 89 4D ?? 89 4D ?? 57 39 48 ?? 74 ?? 8B 40 ?? 68 ?? ?? ?? ?? 8B 50 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 51 51 51 8D 4D ?? 51 50 FF D2 A1 ?? ?? ?? ?? 83 C4 ?? 8B 78 ?? 8B 5D", 0x15C1F0),
	__int8, __fastcall, void *ecx, void *edx, void *pUnknown1, void *pUnknown2)
{
	static bool bOnce = false; if (!bOnce) { Debug::Log("[Byx] Draw_Effect hooked!\n"); bOnce = true; }

	// Draw_Effect also fires outside real gameplay (main menu background AND the map-loading
	// transition, where IsInGame() already reads true - confirmed in-game 2026-07-17) with "Effect"
	// objects that don't share in-match effects' layout. Still gate on IsInGame() as the correct
	// domain (every effect we actually filter - Spitter acid, vomit, smoke, Tank rocks - only exists
	// in a real match), but the loading-transition window means that alone isn't sufficient, so the
	// actual read below is SEH-protected too (see SafeGetEffectName).
	if (I::EngineClient->IsInGame() && ecx)
	{
		// GetEffectName: char* __thiscall(void* effect). Resolved by signature (doc 4 / xd.md) with a
		// fallback RVA baked from a confirmed-good inject log.
		static auto GetEffectName = reinterpret_cast<GetEffectNameFn>(
			Memory::ResolveFn("GetEffectName", "client.dll", "83 C1 ?? E9 ?? ?? ?? ?? CC CC CC CC CC CC CC CC 56", 0x15B720));

		if (GetEffectName)
		{
			if (const char *name = SafeGetEffectName(GetEffectName, reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(ecx) - 8)))
			{
				if (Features::Effects::bLogNames)
					Debug::Log("[Byx][FX] %s\n", name);

				if (Features::Effects::ShouldBlock(name))
					return 0; // suppress: original never runs, effect never renders
			}
		}
	}

	return CALL_ORIGINAL(ecx, edx, pUnknown1, pUnknown2);
}
