#pragma once
#include "../../../../SDK/SDK.h"

// ── Send Move Override ────────────────────────────────────────────────────────
//
// Reemplazo completo del packet-builder del engine (engine.dll CL_SendMove, RVA 0x7CEC0 en esta
// version del juego). Es la pieza que nos faltaba: hasta ahora peleabamos con los gates INTERNOS
// de CL_Move para que cada llamada extra llegara a transmitir; esto ataca el otro lado, el que
// arma y manda el paquete.
//
// ── Como se llego aca (2026-08-03, todo verificado, nada deducido) ────────────
//
// 1) El .lua de referencia, desofuscado. Toda la maraña de `_0xA1B2...` calcula UN numero:
//    floor(2897.342)+ceil(3126.891)+floor(5243.129)+ceil(7020.567)+floor(736.923) = 19024,
//    y el array {0x77,0x72,0x69,0x74,0x65,0x5f,0x69,0x6e,0x74} deletrea "write_int". O sea que
//    todo el script se reduce a tres escrituras, dentro de un callback que corre por frame
//    mientras se mantiene una tecla:
//        write_int(client_state + 19024, 45)      <- "exploitvalue"
//        write_int(netchannel   + 16, -1)         <- m_nOutSequenceNrAck
//        write_int(netchannel   + 28, 255)        <- m_nChokedPackets
//    client_state = *(engine_base + 4352236), netchannel = *(client_state + 24).
//    Es EXACTAMENTE lo que ya hacen nuestros Bleed/Choke Exploit - mismo campo, mismo valor.
//
// 2) El binario de referencia, en IDA. Su detour de CL_Move mete N llamadas extra igual que
//    nosotros, y - clave - el dispatcher del callback del script corre DENTRO de ese loop, antes
//    de CADA llamada extra. Por eso el script reescribe el 45 N veces por frame y no una.
//
// 3) El mismo binario reemplaza CL_SendMove por completo. Se instala sobre `engine_base + 0x7CEC0`
//    pasando 0 como trampolin: NUNCA llama al original. Su reemplazo es recto, sin una sola rama:
//        choked   = *(client_state + 19024)
//        newCmds  = min(choked + 1, 15)        -> mensaje + 88
//        leftover = choked + 1 - newCmds
//        backup   = min(leftover, 7)           -> mensaje + 84
//        nextCmd  = *(client_state + 19020) + choked + 2
//        for (to = nextCmd - newCmds - backup; to != nextCmd; ++to) WriteUsercmd(from, to)
//        if (netchannel + 16 != -1) netchannel + 28 -= leftover
//        SendNetMsg(netchannel, mensaje)
//
// 4) El CL_SendMove REAL de nuestro engine.dll, leido byte a byte del binario del juego (no de
//    ninguna referencia ajena) en RVA 0x7CEC0:
//        55 8B EC B8 64 10 00 00      push ebp / mov ebp,esp / mov eax,1064h
//        E8 ..                        call GetClientState  (engine+0x82CA0, el mismo accessor
//                                     que ya resuelve CL_Move.cpp: devuelve raw_client_state + 8)
//        8D B0 48 4A 00 00            lea esi,[eax+4A48h]   -> 19016 + 8 = raw + 19024  ✓
//        E8 .. / 8B 80 44 4A 00 00    -> [eax+4A44h] = 19012 + 8 = raw + 19020  ✓
//        8B 0E / 8D 54 08 01          lea edx,[eax+ecx+1]   -> nextCmd = last + choked + 1
//        ...
//        8B 42 20 / FF D0             call obj->vtbl[8]()
//        83 FE FF / 0F 84 E8 01 00 00 cmp eax,-1 / jz  -> SALE de la funcion sin mandar nada
//    Esto CONFIRMA de primera mano dos cosas y descubre una tercera:
//      · el offset 19024 de Bleed/Choke es el campo que el engine lee para armar el paquete;
//      · nuestro `pGetClientState() + 19016` es algebraicamente el mismo campo (el accessor ya
//        trae +8), o sea que lo que ya teniamos apuntaba bien;
//      · el original tiene un EARLY-OUT que el reemplazo no tiene, y usa `+ 1` donde el
//        reemplazo usa `+ 2` (un command number mas de adelanto por paquete).
//
// ── Que hace este feature ─────────────────────────────────────────────────────
//
// Cuando esta apagado (default) el hook llama al original y no cambia absolutamente nada. Cuando
// esta prendido, corre el builder de arriba en vez del original: sin early-out, sin gates, un
// CLC_Move armado y mandado en cada invocacion.
//
// ── CL_SendMove REAL, decompilado entero (2026-08-03, engine.dll cargado en IDA) ──
//
// Lo de arriba (leido byte a byte) quedo confirmado, y la funcion completa corrige DOS cosas que
// se habian asumido mirando solo el reemplazo de referencia:
//
//   · `numbackupcommands` NO es `min(leftover, 7)` en el engine: es **2, hardcodeado**
//     (`v29 = 2` sobre mensaje+84, el default de cl_cmdbackup). El min(...,7) es invento de la
//     implementacion de referencia. Ahi estan los +5 comandos por paquete reales:
//     engine = numnew + 2 (techo 17), referencia = numnew + backup (techo 22).
//
//   · El `+2` de la referencia no manda un tick mas adelante. El engine escribe el rango
//     [nextcmd-numnew-1 .. nextcmd] INCLUSIVE con nextcmd = last+choked+1; la referencia escribe
//     [nextcmd-numnew-backup .. nextcmd-1] con nextcmd = last+choked+2. Los dos terminan en el
//     MISMO numero de comando (last+choked+1). El bias solo corre el arranque de la tanda.
//
// Lo que el engine tiene y el reemplazo no:
//   · un early-out: `if (off_104268E4->vtbl[8]() == -1) return;` (no hay slot local activo);
//   · un loop sobre slots de splitscreen (vtbl[8] = primero, vtbl[9] = siguiente, vtbl[13] =
//     saltear este), con un CLC_Move por slot. El reemplazo manda uno solo con slot 0;
//   · marca `bIsNewCommand` por comando (`to >= nextcmd - numnew + 1`); el reemplazo pasa false
//     a todos. Este port replica el criterio del ENGINE, que es el que el servidor espera.
// Lo que el reemplazo tiene y el engine no: `netchannel+28 -= leftover`.
//
// ── El limite REAL, que ningun slider puede correr ────────────────────────────
//
// `min(choked+1, 15)` y el tope 7 del backup NO son un cap nuestro: son el ancho de los campos del
// mensaje CLC_Move del protocolo Source - numnewcommands ocupa 4 bits (0..15) y numbackupcommands
// 3 bits (0..7). 22 comandos por paquete es el techo duro, y subirlo del lado del cliente solo
// trunca el numero al serializarlo. Poner 45, 100 o 500 en el valor de Bleed/Choke da EXACTAMENTE
// el mismo paquete que poner 14.
//
// Lo que el valor SI cambia, y por eso el 45 del script no es arbitrario, es `nextCmd`:
// `lastoutgoing + choked + 2` crece sin techo. Con 45, el numero de comando del cliente salta 47
// por paquete, o sea que el servidor adelanta su contador 47 ticks por paquete recibido en vez de
// 1. Eso es el "lag propio / guarda mas paquetes" que se ve, y es lo que hace que el cargador se
// vacie de un click. Multiplicar de verdad se logra por dos vias, no por el slider:
//   · mas PAQUETES por frame  -> las llamadas extra de CL_Move (SpeedHack), y
//   · mas SALTO por paquete   -> el valor de Bleed/Choke, que si escala linealmente.
namespace Features::Exploits::SendMoveOverride
{
	inline bool bEnabled = false;
	inline int  nKey = 0;   // 0 = sin asignar; se toca para prender/apagar

	// Techos del protocolo. Se exponen porque el pedido fue explicito ("liberar restricciones"),
	// pero 15/7 ES el maximo real: por encima, el serializador trunca al ancho del campo y el
	// paquete sale mal formado. Se dejan configurables para poder medirlo, no porque suba nada.
	inline int nMaxNewCommands = 15;
	inline int nMaxBackupCommands = 7;

	// `+2` es lo que usa el reemplazo de referencia; el CL_SendMove original de nuestro engine.dll
	// usa `+1`. Un tick de adelanto por paquete de diferencia - configurable para poder comparar
	// las dos variantes en vivo en vez de elegir a ciegas.
	inline int nNextCommandBias = 2;

	// Contadores del ultimo paquete armado, para el HUD/log: cuantos comandos nuevos, cuantos de
	// backup, y cuanto salto el numero de comando. Es la unica forma de ver si el valor de
	// Bleed/Choke esta haciendo algo o ya saturo.
	inline int nLastNewCommands = 0;
	inline int nLastBackupCommands = 0;
	inline int nLastCommandJump = 0;
	inline int nLastChoked = 0;

	// Rising-edge del toggle. Se llama una vez por frame real desde el hook de CL_Move.
	void Update();

	bool IsActive();

	// El reemplazo. Devuelve false si algun puntero no esta listo, en cuyo caso el hook llama al
	// original en vez de tragarse el envio del frame.
	bool BuildAndSend();
}
