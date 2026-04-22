#include <Windows.h>
#include "Core/Core.h"
#include "Utils/ExceptionHandler/ExceptionHandler.h"

DWORD WINAPI MainThread(LPVOID lpParam)
{
	U::ExceptionHandler.Initialize(lpParam);

	U::Core.Load();
	U::Core.Loop();
	U::Core.Unload();

	U::ExceptionHandler.Unload();

#ifdef _WINDLL
	FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), EXIT_SUCCESS);
#endif
	return 0;
}

#ifdef _WINDLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (const auto hThread = CreateThread(nullptr, 0, MainThread, hinstDLL, 0, nullptr))
			CloseHandle(hThread);
	}

	return TRUE;
}
#else
int main()
{
	MainThread(GetModuleHandle(NULL));
	return 0;
}
#endif
