#include "ChargerTurn.h"
#include "../../../../Utils/DebugLog.h"
#include <Windows.h>

namespace Features::Exploits::ChargerTurn
{
	// client.dll + 0x65DB4 = el `jz short +0x2D` que saltea la sobreescritura de viewangles durante
	// la carga. Ver el writeup del header para el desensamblado y como se llego al offset.
	static constexpr unsigned int kPatchOffset = 0x65DB4;
	static constexpr BYTE kExpectedOriginal = 0x74; // JZ short
	static constexpr BYTE kPatchedByte = 0xEB;      // JMP short

	static BYTE *s_pTarget = nullptr;
	static BYTE  s_nOriginal = 0;
	static bool  s_bHaveOriginal = false;
	static bool  s_bPatched = false;

	bool IsPatched() { return s_bPatched; }
	int  OriginalByte() { return s_bHaveOriginal ? s_nOriginal : -1; }

	static BYTE *Target()
	{
		if (s_pTarget)
			return s_pTarget;

		static auto hClient = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
		if (!hClient)
			return nullptr;

		s_pTarget = reinterpret_cast<BYTE *>(hClient + kPatchOffset);
		return s_pTarget;
	}

	static void WriteByte(BYTE *pTarget, BYTE nValue)
	{
		DWORD dwOld = 0;
		if (!VirtualProtect(pTarget, 1, PAGE_EXECUTE_READWRITE, &dwOld))
			return;
		*pTarget = nValue;
		VirtualProtect(pTarget, 1, dwOld, &dwOld);
	}

	void Update()
	{
		BYTE *pTarget = Target();
		if (!pTarget)
			return;

		// Snapshot del byte original, una sola vez. Si no es el JZ esperado, el offset no
		// corresponde a esta version del juego - se avisa y NO se parchea nada, en vez de escribir
		// a ciegas en medio de una instruccion.
		if (!s_bHaveOriginal)
		{
			s_nOriginal = *pTarget;
			s_bHaveOriginal = true;

			if (s_nOriginal != kExpectedOriginal)
			{
				Debug::Log(
					"[Byx][ChargerTurn] client.dll+0x%X tiene 0x%02X, se esperaba 0x%02X (JZ). "
					"Offset no valido para esta version - patch DESACTIVADO.\n",
					kPatchOffset, s_nOriginal, kExpectedOriginal);
			}
			else
			{
				Debug::Log("[Byx][ChargerTurn] target OK: client.dll+0x%X = 0x74 (JZ short)\n",
					kPatchOffset);
			}
		}

		if (s_nOriginal != kExpectedOriginal)
			return;

		if (bEnabled == s_bPatched)
			return;   // nada que hacer

		WriteByte(pTarget, bEnabled ? kPatchedByte : s_nOriginal);
		s_bPatched = bEnabled;

		Debug::Log("[Byx][ChargerTurn] %s\n", s_bPatched ? "patch aplicado" : "patch restaurado");
	}

	void Restore()
	{
		if (!s_bPatched || !s_bHaveOriginal)
			return;

		if (BYTE *pTarget = Target())
			WriteByte(pTarget, s_nOriginal);

		s_bPatched = false;
	}
}
