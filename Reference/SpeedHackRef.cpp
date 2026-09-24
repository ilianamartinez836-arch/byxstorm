#include "SpeedHackRef.h"
#include "../../Misc/Binds/Binds.h"
#include <algorithm>
using std::max;
using std::min;

void CSpeedHackRef::OnFirstTick(Extended_Command_Structure* extCmd,
                                Global_Variables_Structure* gv,
                                __int32& extraCommands,
                                void* localPlayer)
{
    (void)gv; // solo lo usaba la rama comentada de abajo

    if (g_varRef.bSHActive)
    {
        // BUG previo: std::clamp(factor, minTicks, factor) con factor < minTicks daba lo > hi = UB
        // (con factor=1 forzaba 2-4 comandos extra igual, y descuadraba el timing del aimbot rage).
        // Ahora el factor es DIRECTO: el valor que pones = comandos extra reales (1 = speedhack minimo).
        extCmd->Extra_Commands = extraCommands = max(0, g_varRef.iFactorSpeedHack);

        // m_flSimulationTime = 0 fuerza al engine a catch-up con muchos ticks de prediccion.
        // Antes este offset se escribia como int (strict-aliasing UB sobre el float), que via
        // bit-pattern producia un denormal cercano a 0 — mismo efecto observable, ahora sin UB.
        *(float*)((uintptr_t)localPlayer + 16) = 0.f;

        G::shouldForceSendPackets = true;
    }
    // (El "Strafe Turbo" se quito: era un mini-speedhack. El AutoStrafe ahora solo optimiza el angulo
    // del strafe para tomar impulso, sin alterar ticks ni velocidad de forma artificial.)
    // Branch inactivo: sim_time queda en Interval_Per_Tick (escrito por Copy_Command.hpp:69
    // antes de esta funcion), valor normal sin speed hack.
}

// ── Features::Exploits::SpeedHackRef ──────────────────────────────────────────
// Solo config + toggle. Quien llama a OnFirstTick es RefDriver.cpp (el par SpeedHackRef/
// RapidFireRef comparte un unico contador de comandos extra, igual que en el original).
namespace Features::Exploits::SpeedHackRef
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
}
