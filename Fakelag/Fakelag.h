#pragma once
#include "../../../../SDK/SDK.h"

// Fakelag - chokea N paquetes seguidos y manda uno, para que tus updates de posicion lleguen a los
// demas en rafagas (te ven a los tirones) mientras vos ves todo normal.
//
// ── Logica, reverseada del binario de referencia (sub_100E38E0) ──────────────
//
//     active = FakelagAlways || toggleState || holdKey;
//     if (!active)          { counter = 0; enviar }
//     else if (counter < N) { counter++;   CHOKEAR }
//     else                  { counter = 0; enviar la rafaga }
//
// con N = clamp(amount, 1, 14). Ese 14 es el techo de `numnewcommands` del CLC_Move (4 bits, max
// 15 comandos por paquete): 14 chokeados + 1 real es exactamente lo que entra. Mas que eso
// perderia comandos en vez de acumularlos.
//
// ── DONDE se inyecta - reescrito 2026-08-04 tras decompilar el CL_Move real ──
//
// PRIMER INTENTO (fallido, reportado en juego): incrementar `client_state + 19016` desde nuestro
// hook de CL_SendMove y no mandar. Se decompilo el CL_Move real (engine+0x7D120) y quedo claro por
// que no podia andar:
//
//     v55 = 1;                                  // bSendPacket
//     if (v4+112 > net_time || !CanPacket() || !bFinalTick) v55 = 0;
//     ...
//     ... CreateMove() ...                      // muestrea el usercmd
//     if (!v55) { netchan->SetChoked(); ++client_state[19016]; return; }   // choke REAL
//     CL_SendMove();
//     ...
//     if (v55) { client_state[19012] = SendDatagram(0); client_state[19016] = 0; }
//
// O sea que el engine pone chokedcommands en CERO despues de cada envio real, unas instrucciones
// despues de llamar a CL_SendMove - nuestro incremento se borraba en el mismo frame. Y peor: como
// CL_Move seguia hasta el `if (v55)`, el SendDatagram salia igual, asi que en vez de chokear
// estabamos DESCARTANDO comandos.
//
// (Este mismo hallazgo confirma, de primera mano, el comentario viejo de CL_Move.cpp sobre que el
// engine resetea ese campo tras cada envio - y por eso Bleed/Choke tienen que reescribirlo cada
// frame.)
//
// AHORA: se ataca `v55` directamente, que es el mismo local que el binario de referencia alcanza
// caminando el stack del caller. No hace falta: v55 se pone en 0 cuando `!CanPacket()`, y
// CanPacket() es `net_time > m_fClearTime` - un campo con nombre en nuestro inetchannel.h
// (CNetChannel+0xB8) que este proyecto ya manipula desde hace tiempo en el sentido contrario (lo
// pone en 0 para FORZAR el envio en las llamadas extra del SpeedHack). Para chokear se hace lo
// inverso: se sube m_fClearTime a un valor enorme justo antes del CALL_ORIGINAL real y se
// restaura apenas vuelve. El engine ve "no puedo mandar", toma SOLO su propia rama de choke
// (SetChoked + ++chokedcommands) y retorna antes del SendDatagram.
//
// Ventaja sobre el stack-walk del original: no dependemos del layout del marco de una funcion
// ajena, y los comandos chokeados los acumula y empaqueta el engine con SU propia logica, asi que
// salen juntos en la proxima rafaga sin que toquemos nada mas.
namespace Features::Exploits::Fakelag
{
	inline bool bEnabled = false;
	inline bool bAlways = false;   // activo siempre, sin teclas
	inline int  nKeyToggle = 0;    // toca para prender/apagar
	inline int  nKeyHold = 0;      // mantene para activar
	inline int  nAmount = 8;       // paquetes chokeados por rafaga; se clampea a [1,14]

	// Diagnostico para el menu.
	//
	// OJO con nCurrentCounter y bCurrentlyActive: con el MENU ABIERTO los binds estan bloqueados a
	// proposito (Binds::bMenuOpen, para que clickear el menu no dispare features), asi que la tecla
	// de hold/toggle no cuenta y los dos leen 0/inactivo aunque el feature ande perfecto. Ademas el
	// contador cicla 1..14..0 cada pocos frames, o sea que seria ilegible igual.
	//
	// Por eso existe nTotalChoked: acumulado que NO se resetea al soltar la tecla, asi se puede
	// jugar un rato, abrir el menu despues y verificar que efectivamente estuvo chokeando.
	inline int  nCurrentCounter = 0;
	inline bool bCurrentlyActive = false;
	inline int  nTotalChoked = 0;

	// Se llama UNA vez por frame real desde el hook de CL_Move, ANTES del CALL_ORIGINAL real.
	// Avanza el contador y devuelve true si este frame tiene que chokearse.
	bool ShouldChoke();
}
