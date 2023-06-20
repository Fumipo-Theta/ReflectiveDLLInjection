//===============================================================================================//
// This is a stub for the actuall functionality of the DLL.
//===============================================================================================//
//#include "ReflectiveLoader.h"
#include "hook.h"
#include "base-window.h"
#include "d3d11-base-helper.h"
#include <polyhook2\Detour\x64Detour.hpp>
// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;
//===============================================================================================//

typedef HRESULT(WINAPI* D3D11PresentPointer)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

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
	HRESULT hr;
	// A temporary window to create a sample swap chain.
	BaseWindow temporaryWindow("DirectX 11 Temporary Window");
	hr = temporaryWindow.Initialize(800, 600);
	if (FAILED(hr)) {
		return hr;
	}

	// A derived class to access the swap chain.
	class D3D11PresentHookHelper : public D3D11BaseHelper {
	public:
		inline IDXGISwapChain1* GetSwapChain() {
			return swapChain1_.Get();
		}
	};
	// Create and initialize the helper.
	D3D11PresentHookHelper d3d11Helper;
	hr = d3d11Helper.Initialize(temporaryWindow.GetHandle(), 800, 600);
	if (FAILED(hr)) {
		return hr;
	}
	// Pointer to the swap chain.
	std::uintptr_t* swapChainPointer = static_cast<std::uintptr_t*>(static_cast<void*>(d3d11Helper.GetSwapChain()));
	// Pointer to the swap chain virtual table.
	std::uintptr_t* virtualTablePointer = reinterpret_cast<std::uintptr_t*>(swapChainPointer[0]);
	// "Present" has index 8 because:
	// - "Present" is the first original virtual method of IDXGISwapChain.
	// - IDXGISwapChain is based on IDXGIDeviceSubObject
	//   which has 1 original virtual method (GetDevice).
	// - IDXGIDeviceSubObject is based on IDXGIObject
	//   which has 4 original virtual methods (SetPrivateData, SetPrivateDataInterface, GetPrivateData, GetParent).
	// - IDXGIObject is based on IUnknown
	//   which has 3 original virtual methods (QueryInterface, AddRef, Release). 
	// So 0 + 1 + 4 + 3 = 8.
	presentPointer_ = static_cast<std::uint64_t>(virtualTablePointer[8]);
	// This will be called instead of the original "Present".
	std::uint64_t presentCallback = reinterpret_cast<std::uint64_t>(&D3D11PresentHook::SwapChainPresentWrapper);

	PLH::x64Detour* detour = new PLH::x64Detour(presentPointer_, presentCallback, &presentTrampoline_);
	// Hook!
	detour->hook();
	return S_OK;
}

void D3D11PresentHook::CaptureFrame(IDXGISwapChain* swapChain) {
	MessageBoxA(NULL, "Hello from DllMain!", "Reflective Dll Injection", MB_OK);
}

HRESULT D3D11PresentHook::SwapChainPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
	// Check if we need to capture the window.
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT hr = swapChain->GetDesc(&swapChainDesc);
	if (SUCCEEDED(hr)) {
		if (windowHandleToCapture_ == swapChainDesc.OutputWindow) {
			CaptureFrame(swapChain);
		}
	}
	// Call the original "Present".
	D3D11PresentPointer presentPtr = reinterpret_cast<D3D11PresentPointer>(presentPointer_);
	return PLH::FnCast(presentTrampoline_, presentPtr)(swapChain, syncInterval, flags);
}

HRESULT D3D11PresentHook::SwapChainPresentWrapper(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
	return D3D11PresentHook::Get()->SwapChainPresent(swapChain, syncInterval, flags);
}

DWORD  WINAPI run_thread(LPVOID param) {
	MessageBoxA(NULL, "Dll injecting...", "Reflective Dll Injection", MB_OK);
	HRESULT hr;
	hr = D3D11PresentHook::Get()->Hook();
	MessageBoxA(NULL, "Dll injected!", "Reflective Dll Injection", MB_OK);
	return 0;
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved )
{
    BOOL bReturnValue = TRUE;
	switch( dwReason ) 
    { 
		//case DLL_QUERY_HMODULE:
		//	if( lpReserved != NULL )
		//		*(HMODULE *)lpReserved = hAppInstance;
		//	break;
		case DLL_PROCESS_ATTACH:
		//	hAppInstance = hinstDLL;
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