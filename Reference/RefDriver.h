#pragma once
#include "../../../../SDK/SDK.h"

// ── Glue del par de referencia (SpeedHackRef + RapidFireRef) ──────────────────
//
// Existe UN solo driver para las dos features porque en el codigo original son una sola cosa:
// comparten `g_var` y comparten el mismo `extraCommands`. `CRapidFire::RunSurvivor` combina
// `iFactorSpeedHack + iFactorRapidFire` en UNA cifra cuando los dos estan activos - no las suma
// como buckets separados. Reproducir eso pide un unico punto de decision.
//
// Diferencia directa contra lo nuestro: en CL_Move.cpp, SpeedHack / CustomSpeedHack / RapidFire
// aportan tres contadores independientes que se SUMAN. Este par aporta uno solo. Con SpeedHack=10
// y RapidFire=10, lo nuestro pide 20 llamadas extra por dos caminos distintos; el de referencia
// pide `max(minTicks, 20)` por un solo camino, con el pacing de disparo de
// ToggleAttackOnExtraTicks aplicado a TODAS. Eso es exactamente lo que el A/B tiene que mostrar.
namespace Features::Exploits::Reference
{
	// Una vez por frame real, desde el hook de CL_Move, antes del CALL_ORIGINAL real.
	void BeginFrame();

	// Por cada comando generado este frame (el real y cada uno sintetico), desde Copy_Command.
	// Es el equivalente del `Redirected_Copy_Command` del original.
	void OnCommand(CUserCmd *pCmd);

	// Llamadas extra a CL_Move que pide el par este frame. 0 si los dos estan apagados.
	int GetExtraCalls();

	// True cuando quien decidio el contador fue el RapidFire de referencia (no el SpeedHack).
	// CL_Move.cpp lo usa para decidir si el bucket de este par necesita el reset incondicional de
	// clientstate+19016 (el acumulador de RapidFire es fragil contra un backlog inflado - ver el
	// comentario largo del loop en CL_Move.cpp) o si puede recibir el valor de Bleed/Choke como
	// los buckets de SpeedHack.
	bool IsRapidFireDriving();

	// True si alguno de los dos esta activo ahora mismo.
	bool IsAnyActive();
}
