#pragma once
#include "../../../../SDK/SDK.h"

// Charger Turn - permite girar libremente mientras hacés la carga del Charger, en vez de quedar
// clavado en linea recta.
//
// ── REESCRITO 2026-08-04. La primera version estaba MAL ──────────────────────
//
// El primer intento manipulaba el pitch del comando hacia 12 grados. Eso era un bloque REAL del
// binario de referencia, pero NO era esta feature: se asumio que `byte_1052F4C7` era Charger Turn
// porque era el bloque que quedaba escribiendo angulos en su CreateMove, sin verificar el global.
// Verificado despues con el metodo correcto (leer los bytes del sitio del checkbox en el menu),
// `Enable Charger turn` es `byte_10530DA7`, y su consumidor real es otra funcion por completo.
//
// ── Lo que hace de verdad: UN BYTE PATCH ─────────────────────────────────────
//
// El consumidor (sub_100C69A0) no toca comandos ni angulos: parchea client.dll.
//
//     v1 = client_base + N;
//     if (!guardado) { byteOriginal = *v1; guardado = 1; }      // snapshot una sola vez
//     if (enabled != ultimoEstado) {
//         if (enabled) *v1 = 0xEB;          // -21 = JMP short
//         else         *v1 = byteOriginal;  // restaura
//         ultimoEstado = enabled;
//     }
//
// El offset que muestra el decompilado es `HMODULE + 104301`, y ESTA ESCALADO x4: IDA tipa la base
// como HMODULE, que es puntero a una struct de 4 bytes, asi que la aritmetica de punteros lo
// multiplica. El offset real en bytes es 104301 * 4 = 417204 = 0x65DB4. Confirmado leyendo el
// client.dll del juego (parseando el PE, no adivinando):
//
//     0x65DAD:  80 BE C8 14 00 00 00   cmp  byte ptr [esi+14C8h], 0
//     0x65DB4:  74 2D                  jz   short +0x2D          <-- EL BYTE
//     0x65DB6:  D9 86 BC 14 00 00      fld  dword ptr [esi+14BCh]   ; angulo de carga guardado
//     0x65DBC:  8D 47 0C               lea  eax, [edi+0Ch]          ; edi+0xC = viewangles.x
//     0x65DBF:  D9 18                  fstp dword ptr [eax]         ; TE LO SOBRESCRIBE
//
// O sea: mientras el bool de `esi+0x14C8` este activo (estas cargando), el juego te pisa los
// viewangles con la direccion de carga bloqueada. Cambiar el `74` (JZ, salta solo si el bool esta
// en 0) por `EB` (JMP incondicional) hace que ese bloque se saltee SIEMPRE, asi que tus angulos
// sobreviven y podes girar. Exactamente el comportamiento que se buscaba.
//
// Es un toggle de estado (no una tecla de hold): al prender se parchea, al apagar se restaura el
// byte original guardado. Tambien se restaura al descargar el cheat, para no dejar el client.dll
// del juego modificado.
namespace Features::Exploits::ChargerTurn
{
	inline bool bEnabled = false;

	// Aplica o restaura el patch segun bEnabled. Idempotente: solo escribe cuando el estado cambia.
	// Se llama una vez por frame desde el hook de CL_Move.
	void Update();

	// Restaura el byte original si estaba parcheado. Se llama al descargar.
	void Restore();

	// Para el menu: si el patch esta aplicado ahora mismo, y que byte se encontro originalmente
	// (deberia ser 0x74; si no lo es, el offset no corresponde a esta version del juego).
	bool IsPatched();
	int  OriginalByte();
}
