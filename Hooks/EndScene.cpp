#include <mutex>

#include "../../SDK/SDK.h"
#include "EndScene.h"
#include "WINAPI_WndProc.h"
#include "../Features/Menu/ImguiMenu.h"


MAKE_HOOK(
	EndScene, Memory::GetVFunc(I::D3D9Device, 42), 
	HRESULT, __stdcall, IDirect3DDevice9* ecx)
{
	static void* fAddr = __builtin_return_address(0);

	if (fAddr != __builtin_return_address(0))
	{
		return CALL_ORIGINAL(ecx);
	}

	// Si el device esta perdido (o perdido-pero-reseteable) no se puede crear NADA: cualquier
	// recurso D3DPOOL_DEFAULT que dejemos vivo hace que el Reset del engine falle. Y ImGui recrea
	// su atlas solo dentro de NewFrame, asi que sin este candado le devolveriamos al engine el
	// mismo bloqueo justo despues de haberlo soltado en el hook de Reset -> reset loop infinito.
	if (ecx->TestCooperativeLevel() != D3D_OK)
	{
		return CALL_ORIGINAL(ecx);
	}

	static std::once_flag initFlag;
	std::call_once(initFlag, [&]
	{
		ImGui::CreateContext();

		// Sin esto ImGui cae en ProggyClean, su bitmap de 13px. Va aca y no en
		// el menu porque el atlas se construye al inicializar el backend DX9.
		CMenuImgui::LoadFonts();

		ImGui_ImplWin32_Init(Hooks::WINAPI_WndProc::hwWindow);

		ImGui_ImplDX9_Init(ecx);

		Hooks::EndScene::bImGuiInitialised = true;
	});

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	F::MenuImgui->Run();

	ImGui::Render();

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	return CALL_ORIGINAL(ecx);
}