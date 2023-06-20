//===============================================================================================//
// This is a stub for the actuall functionality of the DLL.
//===============================================================================================//
//#include "ReflectiveLoader.h"
#include "hook.h"
#include "base-window.h"
#include "d3d11-base-helper.h"
#include <iostream>
#include <string>
#include <polyhook2\Detour\x64Detour.hpp>
#include <MinHook.h>
#pragma comment(lib, "libMinHook.x64.lib")

#ifndef BUILD_CONFIG
#define BUILD_CONFIG "RelWithDebInfo"
#endif

constexpr int c_strcmp(char const* lhs, char const* rhs) {
	return (('\0' == lhs[0]) && ('\0' == rhs[0])) ? 0
		: (lhs[0] != rhs[0]) ? (lhs[0] - rhs[0])
		: c_strcmp(lhs + 1, rhs + 1);
}
// Note: REFLECTIVEDLLINJECTION_VIA_LOADREMOTELIBRARYR and REFLECTIVEDLLINJECTION_CUSTOM_DLLMAIN are
// defined in the project properties (Properties->C++->Preprocessor) so as we can specify our own 
// DllMain and use the LoadRemoteLibraryR() API to inject this DLL.

// You can use this value as a pseudo hinstDLL value (defined and set via ReflectiveLoader.c)
extern HINSTANCE hAppInstance;
//===============================================================================================//

typedef HRESULT(WINAPI* D3D11PresentPointer)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

LPVOID swapchain_present_vtable_lookup() {
	// Credits: https://www.unknowncheats.me/forum/d3d-tutorials-and-source/88369-universal-d3d11-hook.html
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = GetForegroundWindow();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, &featureLevel, 1,
		D3D11_SDK_VERSION, &swapChainDesc, &pSwapChain, &pDevice, NULL, &pContext))) {
		std::cout << "D3D11CreateDeviceAndSwapChain failed" << std::endl;
		return nullptr;
	}

	DWORD_PTR* pSwapChainVtable;
	pSwapChainVtable = (DWORD_PTR*)pSwapChain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];
	LPVOID ret = reinterpret_cast<LPVOID>(pSwapChainVtable[8]);

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

	return ret;
}

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
	////DWORD_PTR hDxgi = (DWORD_PTR)GetModuleHandle("dxgi.dll");
	////std::uint64_t presentPointer_ = reinterpret_cast<std::uint64_t>(swapchain_present_vtable_lookup());

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

	std::cout << "Start hooking" << std::endl;
	MH_Initialize();
	MH_CreateHook(&presentPointer_, &presentCallback, reinterpret_cast<LPVOID*>(&presentTrampoline_));
	MH_EnableHook(&presentPointer_);
	std::cout << "Done hooking" << std::endl;

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
	std::cout << "D3D11PresentHook::SwapChainPresentWrapper called" << std::endl;
	return D3D11PresentHook::Get()->SwapChainPresent(swapChain, syncInterval, flags);
}

DWORD  WINAPI run_thread(LPVOID param) {
	if constexpr (c_strcmp(BUILD_CONFIG, "RelWithDebInfo") == 0) {
		AllocConsole();
		SetConsoleTitle("Sekiro Practice Tool DLL by johndisandonato");
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		freopen("CONIN$", "r", stdin);
	}
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