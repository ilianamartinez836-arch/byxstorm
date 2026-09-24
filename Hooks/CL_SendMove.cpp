#include "../../SDK/SDK.h"
#include "../../Utils/DebugLog.h"
#include "../../Utils/Memory/Memory.h"
#include "../Features/Exploits/SendMoveOverride/SendMoveOverride.h"
#include "../Features/Exploits/Fakelag/Fakelag.h"
#include <Windows.h>

// engine.dll CL_SendMove: el armador del paquete CLC_Move. Se llama desde adentro de CL_Move y es
// lo que efectivamente pone los usercmds en el cable.
//
// FIRMA: no se saco de ninguna referencia ajena ni se adivino - se leyeron los bytes reales de
// C:\...\Left 4 Dead 2\bin\engine.dll en RVA 0x7CEC0 parseando el PE, y el patron de abajo se
// verifico contra el archivo completo: 1 sola coincidencia, exactamente en 0x7CEC0. El ancla
// `8D B0 48 4A 00 00` es `lea esi,[eax+4A48h]`, o sea el acceso a client_state+19016 relativo al
// GetClientState que ya resuelve CL_Move.cpp (= raw client_state + 19024, el campo que escriben
// Bleed y Choke). Ver Features/Exploits/SendMoveOverride/SendMoveOverride.h para el writeup entero.
MAKE_HOOK(
	CL_SendMove, Memory::ResolveFn("CL_SendMove", "engine.dll",
		"55 8B EC B8 64 10 00 00 E8 ? ? ? ? A1 ? ? ? ? 33 C5 89 45 FC 56 E8 ? ? ? ? 8D B0 48 4A 00 00",
		0x7CEC0),
	void, __cdecl)
{
	static bool bLogged = false;
	if (!bLogged) { Debug::Log("[Byx] CL_SendMove hooked.\n"); bLogged = true; }

	// NOTA: el Fakelag estuvo aca y NO funcionaba. Decompilar el CL_Move real mostro que el engine
	// hace `client_state[19016] = 0` pocas instrucciones despues de llamarnos, borrando cualquier
	// incremento que hicieramos, y que ademas seguia hasta el SendDatagram igual - o sea que en vez
	// de chokear se descartaban comandos. Ahora se inyecta en el gate `bSendPacket` desde
	// CL_Move.cpp; ver Features/Exploits/Fakelag/Fakelag.h para el writeup completo.

	// Apagado = comportamiento identico al del juego sin el cheat. Solo cuando el feature esta
	// prendido se reemplaza el armador; y si el reemplazo no pudo correr (punteros no listos,
	// nada que mandar) se cae al original en vez de comerse el envio del frame - a diferencia de
	// la implementacion de referencia, que instala el hook SIN trampolin y nunca puede volver
	// atras.
	if (Features::Exploits::SendMoveOverride::IsActive())
	{
		if (Features::Exploits::SendMoveOverride::BuildAndSend())
			return;
	}

	CALL_ORIGINAL();
}
