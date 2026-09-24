#include "SendMoveOverride.h"
#include "../../Misc/Binds/Binds.h"
#include "../../../../Utils/DebugLog.h"
#include <Windows.h>
#include <algorithm>

namespace Features::Exploits::SendMoveOverride
{
	static bool s_bKeyPrev = false;

	void Update()
	{
		if (!nKey)
			return;

		bool bDown = Features::Binds::Down(nKey);
		if (bDown && !s_bKeyPrev)
			bEnabled = !bEnabled;
		s_bKeyPrev = bDown;
	}

	bool IsActive() { return bEnabled; }

	// Layout del CLC_Move, sacado de como lo llena el builder de referencia y confirmado contra el
	// tamaño que reserva el original (mov eax, 1064h = 4196 bytes de stack: 160 de mensaje + 4000
	// de buffer + locales). Los offsets +84/+88/+132/+136/+140 son los mismos que ya usa el
	// Redirected_Send_Move de src/Storm/Hooks/HookStormOthers.hpp, o sea que estan validados
	// contra NUESTROS binarios, no contra los de otro.
	struct CLC_Move_Message
	{
		unsigned char Raw[160];

		void Init(void *pVTable, void *pData, unsigned int unSize)
		{
			std::memset(Raw, 0, sizeof(Raw));
			*reinterpret_cast<void **>(Raw)             = pVTable;      // +0   vtable
			*reinterpret_cast<void **>(Raw + 132)       = pData;        // +132 puntero al buffer
			*reinterpret_cast<int *>(Raw + 136)         = (int)unSize;  // +136 bytes
			*reinterpret_cast<int *>(Raw + 140)         = (int)unSize * 8; // +140 bits
		}

		void SetNumBackup(int n) { *reinterpret_cast<int *>(Raw + 84) = n; }
		void SetNumNew(int n)    { *reinterpret_cast<int *>(Raw + 88) = n; }

		// El escritor de comandos recibe la sub-estructura bf_write, que arranca en +132.
		void *WriteTarget() { return Raw + 132; }
	};

	bool BuildAndSend()
	{
		static auto hClient = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
		static auto hEngine = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));

		if (!hClient || !hEngine)
			return false;

		// client_state CRUDO. Ojo con la diferencia que ya documenta CL_Move.cpp: nuestro
		// pGetClientState() del engine devuelve raw + 8, asi que los offsets relativos a EL son
		// 19016/19012. Aca leemos el puntero crudo directo (engine + 4352236), igual que el .lua y
		// que el builder de referencia, asi que los offsets son los crudos: 19024 / 19020.
		auto pClientState = *reinterpret_cast<uintptr_t *>(hEngine + 4352236);
		if (!pClientState)
			return false;

		auto pNetChannel = *reinterpret_cast<void **>(pClientState + 24);
		if (!pNetChannel)
			return false;

		auto pWriteUsercmdGlobal = *reinterpret_cast<void **>(hEngine + 5171072);
		if (!pWriteUsercmdGlobal)
			return false;

		const int nChoked = *reinterpret_cast<int *>(pClientState + 19024);
		const int nLastOutgoing = *reinterpret_cast<int *>(pClientState + 19020);

		// min(choked + 1, 15) y min(sobrante, 7). Los topes vienen del ancho de los campos del
		// mensaje (4 y 3 bits) - ver el writeup del header. Se leen de la config para poder
		// medir que pasa al pasarse, no porque pasarse sirva.
		const int nMaxNew = std::clamp(nMaxNewCommands, 0, 15);
		const int nMaxBackup = std::clamp(nMaxBackupCommands, 0, 7);

		int nNewCommands = nChoked + 1;
		if (nNewCommands > nMaxNew)
			nNewCommands = nMaxNew;
		if (nNewCommands < 0)
			nNewCommands = 0;

		const int nLeftover = nChoked + 1 - nNewCommands;

		int nBackupCommands = nLeftover;
		if (nBackupCommands > nMaxBackup)
			nBackupCommands = nMaxBackup;
		if (nBackupCommands < 0)
			nBackupCommands = 0;

		// Sin comandos que mandar no hay paquete que armar - el original tiene su propio early-out
		// para esto y saltarselo aca solo mandaria un CLC_Move vacio por frame.
		if (nNewCommands + nBackupCommands <= 0)
			return false;

		CLC_Move_Message Message;
		unsigned char Data[4000];
		Message.Init(reinterpret_cast<void *>(hEngine + 3501364), Data, sizeof(Data));
		Message.SetNumNew(nNewCommands);
		Message.SetNumBackup(nBackupCommands);

		const int nNextCommand = nLastOutgoing + nChoked + nNextCommandBias;
		int nTo = nNextCommand - nNewCommands - nBackupCommands;
		int nFrom = -1;

		// WriteUsercmdDeltaToBuffer(nSlot, bf_write*, from, to, bIsNewCommand).
		//
		// Se resuelve por VTABLE, slot 22, igual que hace el CL_SendMove real
		// (`(*(_DWORD *)obj + 88)` = 88/4 = 22). La implementacion de referencia en cambio llama
		// derecho a `client.dll + 691088`, que es la direccion concreta de ese mismo slot en SU
		// build - hardcodear eso se rompe con cualquier update que mueva la funcion, y por el
		// vtable no. `obj` es el mismo en los dos: *(engine + 5171072) == *(engine + 0x4EE780).
		using WriteUsercmd_t = bool(__thiscall *)(void *, int, void *, int, int, bool);
		auto pWriteUsercmd = (*reinterpret_cast<WriteUsercmd_t **>(pWriteUsercmdGlobal))[22];
		if (!pWriteUsercmd)
			return false;

		// Delta-encodea cada comando contra el anterior: el primero contra "ninguno" (-1) y cada
		// siguiente contra su predecesor.
		//
		// El flag bIsNewCommand marca cuales cuentan como comandos NUEVOS y cuales como backup. El
		// engine lo calcula por comando (`v9 >= nextcmd - numnew + 1`); la implementacion de
		// referencia pasa false a todos y deja que el conteo lo lleve el campo msg+88. Se replica
		// el criterio del ENGINE, que es el que el servidor espera - los ultimos `numnew` de la
		// tanda son los nuevos, el resto backup.
		const int nFirstNewCommand = nNextCommand - nNewCommands;

		while (nTo != nNextCommand)
		{
			pWriteUsercmd(pWriteUsercmdGlobal, 0, Message.WriteTarget(), nFrom, nTo,
				nTo >= nFirstNewCommand);
			nFrom = nTo;
			++nTo;
		}

		// Devolver el sobrante al contador de choked. El builder de referencia lo hace solo si el
		// ack NO esta forzado a -1 - y como Bleed/Choke justamente lo fuerzan a -1, con esos
		// activos esta resta no corre. Se replica la condicion tal cual en vez de "arreglarla":
		// es parte del mecanismo, no un bug.
		// + guard choked_packets > 0 (el de la referencia): nunca restar por debajo de cero, que
		// dejaria el contador en un valor basura negativo.
		int *pnChokedPackets = reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(pNetChannel) + 28);
		if (*reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(pNetChannel) + 16) != -1
			&& *pnChokedPackets > 0)
			*pnChokedPackets -= nLeftover;

		// SendNetMsg: slot 41 del vtable de CNetChannel (41 * 4 = 164), el mismo indice que usa el
		// builder de referencia.
		using SendNetMsg_t = bool(__thiscall *)(void *, void *, void *, void *);
		auto pSendNetMsg = (*reinterpret_cast<SendNetMsg_t **>(pNetChannel))[41];
		if (!pSendNetMsg)
			return false;

		pSendNetMsg(pNetChannel, &Message, nullptr, nullptr);

		nLastChoked = nChoked;
		nLastNewCommands = nNewCommands;
		nLastBackupCommands = nBackupCommands;
		nLastCommandJump = nChoked + nNextCommandBias;

		static bool s_bLoggedOnce = false;
		if (!s_bLoggedOnce)
		{
			s_bLoggedOnce = true;
			Debug::Log(
				"[Byx][SendMove] override activo. choked=%d new=%d backup=%d salto=%d "
				"(cs=0x%p nc=0x%p)\n",
				nChoked, nNewCommands, nBackupCommands, nLastCommandJump,
				(void *)pClientState, pNetChannel);
		}

		return true;
	}
}
