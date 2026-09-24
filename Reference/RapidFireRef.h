#pragma once
#include "RefShared.h"

// ── RapidFire (Referencia) ────────────────────────────────────────────────────
//
// Segunda copia, INDEPENDIENTE, del CRapidFire de referencia. Se agrega aparte de la feature
// RapidFire que ya existe (Exploits/RapidFire) para poder correr las dos y comparar.
//
// HALLAZGO IMPORTANTE (verificado con diff, no a ojo): el RapidFire.cpp que se paso esta vez es
// IDENTICO, caracter por caracter, al que ya teniamos en Exploits/RapidFire/RapidFire.cpp - la
// unica linea distinta es un `#include` que agregamos nosotros para el glue. O sea: EL ALGORITMO
// NO CAMBIO. Todo lo que puede diferir entre "el suyo" y "el nuestro" esta en el GLUE (quien lo
// llama, con que contexto y cuando), no en estas funciones.
//
// Por eso este modulo NO repite el mismo glue que ya tenemos: reproduce el del original, que es
// donde estan las diferencias reales de comportamiento. Ver RefDriver.cpp para el detalle, pero
// en resumen son cuatro:
//
//   1. `bSHActive` / `iFactorSpeedHack` VIVOS. En nuestra version esos dos campos nunca se
//      escriben (quedan en false/0 para siempre), asi que `RunSurvivor` siempre toma la rama
//      "solo RF" y `ToggleAttackOnExtraTicks` solo se activa con la tecla de RapidFire. Aca los
//      alimenta SpeedHackRef, asi que con los dos prendidos el factor se COMBINA
//      (`iFactorSpeedHack + iFactorRapidFire`) en un unico numero de comandos extra, en vez de
//      sumar dos buckets separados como hace CL_Move.cpp con las features nuestras.
//
//   2. `ExtCmd` real por comando. Nuestro glue usa UN solo `Extended_Command_Structure` estatico
//      para `ExtCmd` y para `InitExtCmd`, o sea que son siempre el mismo objeto. El original usa
//      `Extended_Commands[Command_Number % 150]` (un slot por comando, ring de 150) y guarda
//      aparte el slot del PRIMER comando del frame como `InitExtCmd`. Eso cambia de verdad la
//      condicion `if (ctx.InitExtCmd->Extra_Commands == 0)` de DisableClockCorrection y el
//      `ctx.ExtCmd->Sequence_Shift = ctx.InitExtCmd->Sequence_Shift` de CorrectExtendedCommand.
//
//   3. `ApplyExtraScaling` con punto de llamada. En nuestra version quedo definida pero muerta
//      (comentado en RapidFire.h: "sin punto de llamada"). Aca se llama en su condicion real
//      (RF activo y SH apagado, en el primer comando del frame). Su factor de interpolacion es
//      un ajuste del original que nosotros no teniamos - se expone como slider y arranca en 0,
//      que deja la multiplicacion en 1.0 (o sea, inerte hasta que se suba a mano).
//
//   4. Sin `s_nCarriedDebt`. Ese arrastre entre frames es un invento de nuestro glue, no del
//      original; aca no esta.
//
// Las funciones del .cpp son el codigo tal cual. Unicos cambios: `CRapidFire` -> `CRapidFireRef`,
// `RFContext` -> `RFRefContext` y `g_var` -> `g_varRef`, todos obligatorios para que las dos
// copias puedan coexistir sin compartir estado.
//
// `OnInfectedNoVictim` se porta igual que en la version que ya teniamos: queda definida pero sin
// llamador, porque es logica de Versus/Infected y este proyecto es Survivor-only por ahora.
class CRapidFireRef
{
public:
	void DisableClockCorrection(__int32 queue, const RFRefContext &ctx);
	void CorrectExtendedCommand(const RFRefContext &ctx);
	void ApplyExtraScaling(void *localPlayer, Global_Variables_Structure *gv,
		__int32 &extraCommands, __int32 interpolateFactor);
	void RunSurvivor(const RFRefContext &ctx);
	void OnInfectedNoVictim(const RFRefContext &ctx);
	void ToggleAttackOnExtraTicks(const RFRefContext &ctx);
	void Reset();

private:
	__int32 m_AccumulativeCorrection = 0;
	bool m_bNetRestoreNeeded = false;
	bool m_bDisabledThisInvoke = false;
	__int32 m_nSavedNC16 = 0;
	__int32 m_nSavedNC28 = 0;
};

inline CRapidFireRef g_RapidFireRefEngine;

// Config propia. Igual que el RapidFire nuestro: la tecla se MANTIENE apretada (no es toggle).
namespace Features::Exploits::RapidFireRef
{
	inline bool bEnabled = false;
	inline int  nKey = 0;         // tecla a MANTENER; 0 = sin asignar
	inline int  nValue = 0;       // factor de comandos extra mientras este apretada
	inline int  nInterpolate = 0; // factor de ApplyExtraScaling; 0 = inerte (multiplicador 1.0)

	bool IsActive();
}
