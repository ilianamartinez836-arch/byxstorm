#include "WINAPI_WndProc.h"

#include "../Features/Menu/ImguiMenu.h"

LONG __stdcall Hooks::WINAPI_WndProc::Func(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (F::MenuImgui->IsOpen() && H::Input->IsGameFocused())
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
			return true;

		return 1;
	}

	return CallWindowProc(Original, hWnd, uMsg, wParam, lParam);
}

void Hooks::WINAPI_WndProc::Init()
{
	Original = (WNDPROC)SetWindowLongPtr(hwWindow = FindWindowA(0, "Left 4 Dead 2 - Direct3D 9"), GWL_WNDPROC, (LONG_PTR)Func);
}