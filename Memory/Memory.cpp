#include "Memory.h"
#include "../DebugLog.h"

#define INRANGE(x, a, b) (x >= a && x <= b)
#define GetBits(x) (INRANGE((x & (~0x20)),'A','F') ? ((x & (~0x20)) - 'A' + 0xA) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define GetBytes(x) (GetBits(x[0]) << 4 | GetBits(x[1]))

typedef void *(*InstantiateInterfaceFn)();

struct InterfaceInit_t
{
	InstantiateInterfaceFn m_pInterface = nullptr;
	const char *m_pszInterfaceName = nullptr;
	InterfaceInit_t *m_pNextInterface = nullptr;
};

#include <vector>
#include <Psapi.h>

std::vector<int> pattern_to_byte(const char *pattern)
{
	/// Prerequisites
	auto              bytes = std::vector<int>{};
	const auto        start = const_cast<char *>(pattern);
	const char *const end = const_cast<char *>(pattern) + strlen(pattern);

	/// Convert signature into corresponding bytes
	for (char *current = start; current < end; ++current)
	{
		/// Is current byte a wildcard? Simply ignore that that byte later
		if (*current == '?')
		{
			++current;

			/// Check if following byte is also a wildcard
			if (*current == '?')
				++current;

			/// Dummy byte
			bytes.push_back(-1);
		}
		else
		{
			/// Convert character to byte on hexadecimal base
			bytes.push_back(std::strtoul(current, &current, 16));
		}
	}

	return bytes;
}

DWORD Memory::FindSignature(const char *szModule, const char *szPattern)
{
	if (const auto hMod = GetModuleHandleA(szModule))
	{
#ifdef _DEBUG
#define DEBUG_SIG
#endif

		/// Get module information to search in the given module
		MODULEINFO module_info;
		if (!GetModuleInformation(GetCurrentProcess(), hMod, &module_info, sizeof(MODULEINFO)))
		{
#ifdef DEBUG_SIG
			MessageBox(nullptr, fmt::format("GetModuleInformation {} failed\n", sig).c_str(), "", 0);
#endif
			return {};
		}

		/// The region where we will search for the byte sequence
		const auto image_size = module_info.SizeOfImage;

		/// Check if the image is faulty
		if (!image_size)
			return {};

		/// Convert IDA-Style signature to a byte sequence
		const auto pattern_bytes = pattern_to_byte(szPattern);

		const auto image_bytes = reinterpret_cast<byte *>(hMod);

		const auto signature_size = pattern_bytes.size();
		const int *signature_bytes = pattern_bytes.data();

		/// Now loop through all bytes and check if the byte sequence matches
		for (auto i = 0ul; i < image_size - signature_size; ++i)
		{
			auto byte_sequence_found = true;

			/// Go through all bytes from the signature and check if it matches
			for (auto j = 0ul; j < signature_size; ++j)
			{
				if (image_bytes[i + j] != signature_bytes[j] /// Bytes don't match
					&& signature_bytes[j] != -1)             /// Byte isn't a wildcard either, WHAT THE HECK
				{
					byte_sequence_found = false;
					break;
				}
			}

			/// All good, now return the right address
			if (byte_sequence_found)
				return { reinterpret_cast<uintptr_t>(&image_bytes[i]) };
		}

#if defined DEBUG_SIG
		MessageBox(nullptr, fmt::format("find_ida_sig {} failed\n", sig).c_str(), "", 0);
#endif

		/// Byte sequence wasn't found
		return {};
	}

	return 0x0;
}

DWORD Memory::ResolveFn(const char *szName, const char *szModule, const char *szPattern, unsigned int rvaFallback)
{
	const auto dwBase = reinterpret_cast<DWORD>(GetModuleHandleA(szModule));

	if (const auto dwAddr = FindSignature(szModule, szPattern))
	{
		// Print the resolved RVA too - that's exactly the value to bake into rvaFallback next time.
		Debug::Log("[Byx][SIG] %-26s OK        %s+0x%06X  (0x%08X)\n",
			szName, szModule, dwBase ? (dwAddr - dwBase) : 0u, dwAddr);
		return dwAddr;
	}

	if (rvaFallback && dwBase)
	{
		const auto dwFb = dwBase + rvaFallback;

		// Only trust the last-known RVA if it STILL starts with this function's first opcode (parsed
		// from the signature's leading byte). If an update MOVED the function, the old RVA now points
		// at unrelated bytes - reject rather than hook garbage, so the fallback is never *worse* than
		// a clean miss. (Skip this check when the signature itself starts with a wildcard.)
		const int nFirst = (szPattern && szPattern[0] && szPattern[0] != '?')
			? static_cast<int>(std::strtoul(szPattern, nullptr, 16)) : -1;

		if (nFirst < 0 || *reinterpret_cast<unsigned char *>(dwFb) == static_cast<unsigned char>(nFirst))
		{
			Debug::Log("[Byx][SIG] %-26s FALLBACK  %s+0x%06X  (0x%08X)  <-- signature broke, using last-known RVA\n",
				szName, szModule, rvaFallback, dwFb);
			return dwFb;
		}

		Debug::Log("[Byx][SIG] %-26s FB-REJECT %s+0x%06X  <-- prologue changed (function moved?); hook DISABLED\n",
			szName, szModule, rvaFallback);
	}

	Debug::Log("[Byx][SIG] %-26s FAIL      %s  <-- signature broke and no fallback RVA; feature/hook DISABLED\n",
		szName, szModule);
	return 0x0;
}

PVOID Memory::FindInterface(const char *szModule, const char *szObject)
{
	auto hmModule = GetModuleHandleA(szModule);

	if (!hmModule)
		return nullptr;

	auto dwCreateInterface = reinterpret_cast<DWORD>(GetProcAddress(hmModule, "CreateInterface"));
	auto dwShortJmp = dwCreateInterface + 0x5;
	auto dwJmp = (dwShortJmp + *reinterpret_cast<DWORD *>(dwShortJmp)) + 0x4;

	auto pList = **reinterpret_cast<InterfaceInit_t ***>(dwJmp + 0x6);

	while (pList)
	{
		if ((strstr(pList->m_pszInterfaceName, szObject) && (strlen(pList->m_pszInterfaceName) - strlen(szObject)) < 5))
			return pList->m_pInterface();

		pList = pList->m_pNextInterface;
	}

	return nullptr;
}