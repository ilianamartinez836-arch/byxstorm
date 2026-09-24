#pragma once

// ── Piezas compartidas por el par de features de REFERENCIA (SpeedHackRef + RapidFireRef) ──
//
// Por que existe este header y no una copia por feature: en el codigo de referencia, SpeedHack y
// RapidFire NO son independientes - comparten UN solo struct de estado (`g_var`) y UN solo
// contador de comandos extra. `CRapidFire::RunSurvivor` lee `bSHActive`/`iFactorSpeedHack` para
// combinar los dos factores en una sola cifra, y `ToggleAttackOnExtraTicks` se activa con
// `(bRageActive || bSHActive)`, o sea que el SpeedHack de referencia tambien pasa por el pacing de
// disparo del RapidFire. Ese acoplamiento ES una de las diferencias que se quieren medir contra
// nuestras versiones (las nuestras corren en buckets separados y `bSHActive` nunca se prende),
// asi que reproducirlo requiere estado compartido.
//
// Es un header de DATOS compartidos, no un dispatcher: nadie llama features desde aca. Cada
// feature sigue teniendo su propio .h/.cpp con su propia config y su propio toggle.
//
// Nada de esto toca el estado de las features existentes: `g_varRef` es un objeto distinto de
// `g_var` (RapidFire/RapidFire.h), asi que se pueden prender las cuatro a la vez sin pisarse.

#include <Windows.h>
#include "../../../../SDK/SDK.h"
#include "../../../../SDK/L4D2/inetchannel.h"
#include "../../../../Storm/Utils/UtilsStorm.hpp"

// UtilsStorm.hpp define macros crudos `max`/`min` (y Windows.h puede definir los suyos). Los .cpp
// portados hacen `#include <algorithm>` + `using std::max;`/`using std::min;` despues de este
// header - sin este undef, esas macros corrompen las declaraciones internas de <algorithm>
// (std::max/std::min/std::clamp) y no compila. Mismo motivo que en RapidFire/RapidFire.h.
#undef max
#undef min

extern void *Client_Module;

// Copia 1:1 del RFContext que usan las funciones portadas, con nombre propio para no chocar con
// el RFContext de la feature RapidFire que ya existe.
struct RFRefContext
{
	Command_Structure *Command;
	Extended_Command_Structure *ExtCmd;
	Extended_Command_Structure *InitExtCmd;
	CNetChannel *NetworkChannel;
	Global_Variables_Structure *GlobalVars;
	void *LocalPlayer;
	__int32 FirstTick;
	__int32 ActionFactor;
	__int32 &ExtraCommands;
};

// El `g_var` del codigo de referencia, con los mismos nombres de campo que esperan los .cpp
// portados. A diferencia del `g_var` de RapidFire/RapidFire.h (donde `bSHActive`/`iFactorSpeedHack`
// quedan muertos para siempre), aca los CUATRO campos se alimentan de verdad - ver RefDriver.cpp.
struct RefVars
{
	__int32 iFactorRapidFire = 0;
	__int32 iFactorSpeedHack = 0;
	bool bSHActive = false;
	bool bRageActive = false;
};

inline RefVars g_varRef;
