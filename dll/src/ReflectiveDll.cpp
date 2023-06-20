//===============================================================================================//
// This is a stub for the actuall functionality of the DLL.
//===============================================================================================//
#include "ReflectiveLoader.h"
#include "hook.h"
#include <polyhook2\Detour\x64Detour.hpp>
// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;
//===============================================================================================//

D3D11PresentHook* D3D11PresentHook::Get() {
	static D3D11PresentHook hook;
	return &hook;
}

D3D11PresentHook::D3D11PresentHook() {
	// TODO
}

D3D11PresentHook::~D3D11PresentHook() {
	// TODO
}

HRESULT D3D11PresentHook::Hook() {
	// "Present" has index 8 because:
	// - "Present" is the first original virtual method of IDXGISwapChain.
	// - IDXGISwapChain is based on IDXGIDeviceSubObject
	//   which has 1 original virtual method (GetDevice).
	// - IDXGIDeviceSubObject is based on IDXGIObject
	//   which has 4 original virtual methods (SetPrivateData, SetPrivateDataInterface, GetPrivateData, GetParent).
	// - IDXGIObject is based on IUnknown
	//   which has 3 original virtual methods (QueryInterface, AddRef, Release). 
	// So 0 + 1 + 4 + 3 = 8.
	//presentPointer_ = static_cast<std::uint64_t>(virtualTablePointer[8]);
	//PLH::x64Detour* detour = new PLH::x64Detour(presentPointer_, presentCallback, &presentTrampoline_);

	// This will be called instead of the original "Present".
	//std::uint64_t presentCallback = reinterpret_cast<std::uint64_t>(&D3D11PresentHook::SwapChainPresentWrapper);

	// Hook!
	//detour->hook();
	return S_OK;
}

DWORD  WINAPI run_thread(LPVOID param) {
	HRESULT hr;
	hr = D3D11PresentHook::Get()->Hook();
	return 0;
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved )
{
    BOOL bReturnValue = TRUE;
	switch( dwReason ) 
    { 
		case DLL_QUERY_HMODULE:
			if( lpReserved != NULL )
				*(HMODULE *)lpReserved = hAppInstance;
			break;
		case DLL_PROCESS_ATTACH:
			hAppInstance = hinstDLL;
			MessageBoxA( NULL, "Hello from DllMain!", "Reflective Dll Injection", MB_OK );
			DWORD tmp;
			CreateThread(NULL, 0, run_thread, NULL, 0, &tmp);
			break;
		case DLL_PROCESS_DETACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
            break;
    }
	return bReturnValue;
}