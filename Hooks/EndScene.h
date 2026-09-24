#pragma once

namespace Hooks
{
	namespace EndScene
	{
		// ImGui se inicializa perezosamente en el primer EndScene. El hook de Reset y App::Shutdown
		// necesitan saber si eso ya paso: llamar a los backends antes revienta en un IM_ASSERT.
		inline bool bImGuiInitialised = false;
	}
}
