#include "../../SDK/SDK.h"
#include "EndScene.h"
#include "../Features/Menu/ImguiMenu.h"
#include "../../Utils/DebugLog.h"


// Los dos vertex/index buffers de ImGui y su atlas de fuentes viven en D3DPOOL_DEFAULT
// (imgui_impl_dx9.cpp: CreateVertexBuffer / CreateIndexBuffer / CreateTexture). D3D9 exige que
// TODO recurso de ese pool este liberado antes de un Reset: si queda uno solo vivo, Reset devuelve
// D3DERR_INVALIDCALL. Y el material system de Source no se rinde ante eso - reintenta el reset
// frame tras frame - asi que la imagen se queda congelada para siempre y solo se sale reiniciando.
//
// Sin este hook el bug estaba latente: una partida normal nunca pierde el device, asi que Reset
// nunca se llamaba. Con ReShade recargando efectos si se llama (su pico de VRAM manda el device a
// lost), y ahi el ImGui del cheat bloqueaba la recuperacion. Por eso solo se colgaba con los dos.
MAKE_HOOK(
	IDirect3DDevice9_Reset, Memory::GetVFunc(I::D3D9Device, 16),
	HRESULT, __stdcall, IDirect3DDevice9* ecx, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (Hooks::EndScene::bImGuiInitialised)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}

	const HRESULT hr = CALL_ORIGINAL(ecx, pPresentationParameters);

	Debug::Log("[Byx] D3D9 Reset -> 0x%08lX (%dx%d, windowed=%d, imgui=%d)\n",
		static_cast<unsigned long>(hr),
		pPresentationParameters ? (int)pPresentationParameters->BackBufferWidth : -1,
		pPresentationParameters ? (int)pPresentationParameters->BackBufferHeight : -1,
		pPresentationParameters ? (int)pPresentationParameters->Windowed : -1,
		(int)Hooks::EndScene::bImGuiInitialised);

	// Solo recrear si el device volvio: sobre un Reset fallido el engine reintenta, y crear
	// recursos en el medio seria volver a plantarle el bloqueo que acabamos de quitar.
	if (SUCCEEDED(hr) && Hooks::EndScene::bImGuiInitialised)
	{
		ImGui_ImplDX9_CreateDeviceObjects();
	}

	return hr;
}
