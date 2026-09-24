#pragma once
#include "RefShared.h"

// ── SpeedHack (Referencia) ────────────────────────────────────────────────────
//
// Port del `CSpeedHack::OnFirstTick` del codigo de referencia externo. Se agrega APARTE del
// SpeedHack que ya teniamos (Exploits/SpeedHack) para poder prender uno u otro y comparar en vivo,
// no para reemplazarlo. Los dos pueden convivir prendidos - cada uno tiene su propio bucket de
// llamadas extra en CL_Move.cpp y su propio estado.
//
// El .cpp es el codigo de referencia tal cual, con exactamente TRES cambios, todos obligatorios:
//   1. `CSpeedHack` -> `CSpeedHackRef` y `g_var` -> `g_varRef`, para no pisar la feature que ya
//      existe (todo el punto del agregado es poder correr las dos y compararlas).
//   2. `(float)((uintptr_t)localPlayer + 16) = 0.f;` -> `*(float*)((uintptr_t)localPlayer + 16) = 0.f;`
//      El original tal cual llego no compila: castear a `float` produce un rvalue, no se le puede
//      asignar. La version con el deref es la que hace lo que el comentario de al lado describe
//      (poner sim_time en 0) y es la unica lectura posible del codigo.
//   3. El parametro `gv` queda sin usar - en el original solo lo usaba la rama que quedo
//      comentada. Se marca `(void)gv;` para que no tire warning.
//
// DIFERENCIAS DE FONDO contra nuestro SpeedHack (esto es lo que hay que ir a mirar en el juego):
//
//   · Ticks extra: los dos terminan pidiendo `max(0, factor)` llamadas extra por frame, o sea que
//     el numero crudo es el mismo. Nuestro SpeedHack ya portaba el zeroing de sim_time (ver
//     CL_Move.cpp) cuando se confirmo este fragmento, asi que ahi tampoco hay diferencia de idea.
//
//   · sim_time: el original escribe el offset CRUDO `localPlayer + 16`. Nuestro SpeedHack escribe
//     `pLocal->m_flSimulationTime()`, que resuelve el offset por netvar en runtime. Se dejo el
//     offset crudo a proposito - si los dos no apuntan al mismo campo, esta es justamente la
//     diferencia que el A/B tiene que sacar a la luz. RefDriver.cpp loguea UNA vez el offset que
//     resuelve el netvar para poder compararlo con 16 sin adivinar.
//
//   · Disparo: nuestro SpeedHack borra IN_ATTACK/IN_ATTACK2 en sus ticks extra (decision explicita
//     del usuario: movimiento puro, cero efecto sobre el disparo). El de referencia NO borra nada;
//     en vez de eso `bSHActive` prende el pacing de `CRapidFireRef::ToggleAttackOnExtraTicks`, que
//     deja pasar IN_ATTACK cada N ticks segun el cooldown real del arma. O sea: el de referencia
//     SI dispara mientras corre, pero al ritmo del arma, no descontrolado.
//
//   · `G::shouldForceSendPackets`: existe en el original y no tenia consumidor conocido. Se porta
//     la escritura tal cual y CL_Move.cpp lo limpia por frame; hoy es redundante porque nuestro
//     loop de llamadas extra ya fuerza los mismos gates (m_fClearTime / clientstate+112) en cada
//     iteracion, incondicionalmente. Se deja escrito para que el port este completo y para poder
//     verlo en el log de diagnostico.
class CSpeedHackRef
{
public:
	void OnFirstTick(Extended_Command_Structure *extCmd,
		Global_Variables_Structure *gv,
		__int32 &extraCommands,
		void *localPlayer);
};

inline CSpeedHackRef g_SpeedHackRefEngine;

// Config propia de la feature. El toggle por tecla es el mismo patron de rising-edge que usa el
// resto de los Exploits (el codigo de referencia no dice si su SpeedHack era toggle o hold, y
// dejarlo igual que el nuestro es lo que hace que la comparacion sea sobre el MECANISMO y no sobre
// como se prende).
namespace Features::Exploits::SpeedHackRef
{
	inline bool bEnabled = false;
	inline int  nKey = 0;   // 0 = sin asignar; se toca para prender/apagar
	inline int  nValue = 0; // comandos extra por frame mientras este prendido

	// Rising-edge del toggle. Se llama una vez por frame real desde el hook de CL_Move.
	void Update();

	bool IsActive();
}
