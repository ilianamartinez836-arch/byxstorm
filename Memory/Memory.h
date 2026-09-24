#pragma once

#include <Windows.h>

#include <cstdint>

namespace Memory
{
	DWORD FindSignature(const char *szModule, const char *szPattern);

	// Hardened resolver for game functions that a game update can MOVE (doc rule of gold #1).
	// 1) tries the signature; 2) if it fails, falls back to <module base> + rvaFallback (the
	// last-known-good offset) when one is provided; 3) ALWAYS logs the outcome (OK + resolved
	// RVA / FALLBACK / FAIL) to ByxStorm_debug.txt so a broken signature is VISIBLE instead of
	// silently killing the feature. Returns 0 only when both the signature and the fallback
	// fail - callers/CHook::Create then skip installation instead of hooking a null address.
	// rvaFallback == 0 means "no known-good offset yet" (fill it in from a good inject log).
	DWORD ResolveFn(const char *szName, const char *szModule, const char *szPattern, unsigned int rvaFallback = 0);

	PVOID FindInterface(const char *szModule, const char *szObject);

	inline void *GetVFunc(void *pBaseClass, unsigned int unIndex)
	{
		return reinterpret_cast<void *>((*static_cast<int **>(pBaseClass))[unIndex]);
	}

	inline std::uintptr_t RelToAbs(const std::uintptr_t address)
	{
		return *reinterpret_cast<std::uintptr_t *>(address + 1) + address + 5;
	}

	inline void Redirect_Function(__int8 Modify_Access_Rights, void* Original_Function_Location, void* Redirected_Function_Location)
	{
		unsigned long __int32 Previous_Access_Rights;

		if (Modify_Access_Rights == 1)
		{
			VirtualProtect(Original_Function_Location, 6, PAGE_EXECUTE_READWRITE, &Previous_Access_Rights);
		}

		* (__int8*)Original_Function_Location = 104;

		*(void**)((unsigned __int32)Original_Function_Location + 1) = Redirected_Function_Location;

		*(unsigned __int8*)((unsigned __int32)Original_Function_Location + 5) = 195;

		if (Modify_Access_Rights == 1)
		{
			VirtualProtect(Original_Function_Location, 6, Previous_Access_Rights, &Previous_Access_Rights);
		}
	}

	inline void Redirect_FunctionP(void*& Original_Function_Caller_Location, unsigned __int32 Original_Function_Caller_Offset, void* Original_Function_Location, __int8 Modify_Access_Rights, void* Redirected_Function_Location)
	{
		unsigned long __int32 Previous_Access_Rights;

		Original_Function_Caller_Location = malloc(12 + Original_Function_Caller_Offset);

		*(void**)Original_Function_Caller_Location = *(void**)Original_Function_Location;

		*(unsigned __int16*)((unsigned __int32)Original_Function_Caller_Location + 4) = *(unsigned __int16*)((unsigned __int32)Original_Function_Location + 4);

		__builtin_memcpy((void*)((unsigned __int32)Original_Function_Caller_Location + 6), (void*)((unsigned __int32)Original_Function_Location + 6), Original_Function_Caller_Offset);

		*(__int8*)((unsigned __int32)Original_Function_Caller_Location + 6 + Original_Function_Caller_Offset) = 104;

		*(void**)((unsigned __int32)Original_Function_Caller_Location + 7 + Original_Function_Caller_Offset) = (void*)((unsigned __int32)Original_Function_Location + 6 + Original_Function_Caller_Offset);

		*(unsigned __int8*)((unsigned __int32)Original_Function_Caller_Location + 11 + Original_Function_Caller_Offset) = 195;

		VirtualProtect(Original_Function_Caller_Location, 12 + Original_Function_Caller_Offset, PAGE_EXECUTE_READWRITE, &Previous_Access_Rights);

		if (Modify_Access_Rights == 1)
		{
			VirtualProtect(Original_Function_Location, 6, PAGE_EXECUTE_READWRITE, &Previous_Access_Rights);
		}

		* (__int8*)Original_Function_Location = 104;

		*(void**)((unsigned __int32)Original_Function_Location + 1) = Redirected_Function_Location;

		*(unsigned __int8*)((unsigned __int32)Original_Function_Location + 5) = 195;

		if (Modify_Access_Rights == 1)
		{
			VirtualProtect(Original_Function_Location, 6, Previous_Access_Rights, &Previous_Access_Rights);
		}
	}
}