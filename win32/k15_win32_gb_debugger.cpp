#pragma comment(lib, "Ws2_32.lib")

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "WinSock2.h"
#include "ws2ipdef.h"

#include "stdio.h"

#include "../k15_gb_debugger_interface.h"

#include "k15_win32_opengl.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "../thirdparty/imgui/imgui.cpp"
#include "../thirdparty/imgui/imgui_draw.cpp"
#include "../thirdparty/imgui/imgui_widgets.cpp"
#include "../thirdparty/imgui/imgui_tables.cpp"
#include "../thirdparty/imgui/backends/imgui_impl_win32.cpp"
#include "../thirdparty/imgui/backends/imgui_impl_opengl3.cpp"
#include "../thirdparty/imgui_club/imgui_memory_editor/imgui_memory_editor.h"
#include "../thirdparty/tlsf/tlsf.c"
#include "../tools/debugger/k15_gb_debugger.h"

typedef BOOL ( WINAPI *PFNSWAPBUFFERSPROC )( HDC hdc );
typedef int	 ( WSAAPI *PFNWSASTARTUP )( WORD wVersionRequested, LPWSADATA lpWSAData );

PFNSWAPBUFFERSPROC 	w32SwapBuffers = nullptr;
PFNWSASTARTUP	  	w32WSAStartup  = nullptr;

constexpr size_t MaxBroadcastAddresses = 8;
constexpr float  BroadcastTimeIntervallInSeconds = 2.0f;

struct Win32Context
{
	void*				pEmulatorMemory;

	HWND 				pMainWindowHandle;
	HDC  				pMainWindowDeviceContext;
	HGLRC				pOpenGLContext;

	LARGE_INTEGER 		pcFrequency;
	LARGE_INTEGER 		pcOld;

	SOCKET 				broadcastSocket;
	SOCKET 				debugSocket;
	uint32_t			broadcastAddresses[ MaxBroadcastAddresses ];

	uint32_t 			windowWidth;
	uint32_t 			windowHeight;
	int32_t 			windowPosX;
	int32_t 			windowPosY;

	uint8_t 			broadcastAddressCount;
	bool8_t 			vsyncEnabled;
};

struct DebuggerTarget
{
	char name[K15_MAX_COMPUTER_NAME_LENGTH];
	char address[K15_IPV4_ADDRESS_MAX_STRING_LENGTH];
};

#define K15_MAX_DEBUGGER_TARGETS 16

struct EmulatorDebuggerContext
{
	void* pMemory;
};

struct DebuggerContext
{
	DebuggerTarget 			debuggerTargets[K15_MAX_DEBUGGER_TARGETS];
	EmulatorDebuggerContext emulatorContext;
	uint32_t* 				pBroadcastAddresses;
	SOCKET 					broadcastSocket;
	SOCKET 					emulatorSocket;
	float 					timeUntilBroadcastInSeconds = BroadcastTimeIntervallInSeconds;
	uint8_t 				broadcastAddressCount;
	uint8_t	 				debuggerTargetCount;
	bool8_t 				connectedToEmulator;
};

struct Win32Settings
{
	uint32_t 	windowWidth;
	uint32_t 	windowHeight;
	int32_t 	windowPosX;
	int32_t 	windowPosY;
	char 		bindAddress[K15_IPV4_ADDRESS_MAX_STRING_LENGTH];
};

const char* pSettingsFormatting = R"(
windowPosX=%d
windowPosY=%d
windowWidth=%u
windowHeight=%u
bindAddress=%s
)";


typedef LRESULT( CALLBACK* WNDPROC )( HWND, UINT, WPARAM, LPARAM );

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

bool8_t deserializeSettings( Win32Settings* pOutSettings, const char* pSettingsTextBuffer, size_t settingsTextBufferSizeInBytes )
{
	if( sscanf_s( pSettingsTextBuffer, pSettingsFormatting, &pOutSettings->windowPosX, &pOutSettings->windowPosY, 
		&pOutSettings->windowWidth, &pOutSettings->windowHeight, &pOutSettings->bindAddress ) == 0 )
	{
		return false;
	}

	return true;
}

DWORD serializeSettings( const Win32Settings* pSettings, char* pSettingsTextBuffer, size_t settingsTextBufferSizeInBytes )
{
	return sprintf_s( pSettingsTextBuffer, settingsTextBufferSizeInBytes, pSettingsFormatting, pSettings->windowPosX, pSettings->windowPosY, 
		pSettings->windowWidth, pSettings->windowHeight, pSettings->bindAddress );
}

bool8_t writeSettingsToFile( const Win32Settings* pSettings, const char* pSettingsPath )
{
	const HANDLE pSettingsHandle = CreateFileA( pSettingsPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr );
	if( pSettingsHandle == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	char settingsBuffer[512] = {0};
	const DWORD settingsBufferLength = serializeSettings( pSettings, settingsBuffer, sizeof( settingsBuffer ) );

	DWORD bytesWritten = 0;
	const BOOL settingsWritten = WriteFile( pSettingsHandle, settingsBuffer, settingsBufferLength, &bytesWritten, nullptr );
	CloseHandle( pSettingsHandle );

	return settingsWritten == TRUE;
}

bool8_t readSettingsFromFile( Win32Settings* pOutSettings, const char* pSettingsPath )
{
	const HANDLE pSettingsHandle = CreateFileA( pSettingsPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr );
	if( pSettingsHandle == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	char settingsBuffer[512] = {0};
	DWORD bytesRead = 0;

	const BOOL readSettingsFileContent = ReadFile( pSettingsHandle, settingsBuffer, sizeof(settingsBuffer), &bytesRead, nullptr );
	CloseHandle(pSettingsHandle);

	if( !readSettingsFileContent)
	{
		return false;
	}

	if( !deserializeSettings( pOutSettings, settingsBuffer, bytesRead ) )
	{
		return false;
	}

	return true;
}	

bool8_t createSettingsFileName( char* pSettingsFileNameBuffer, size_t bufferSizeInBytes )
{
	const DWORD pathLength = GetModuleFileNameA(nullptr, pSettingsFileNameBuffer, MAX_PATH);
	
	if( pathLength == 0 )
	{
		return false;
	}

	const DWORD lastError = GetLastError();
	if( lastError == ERROR_INSUFFICIENT_BUFFER )
	{
		return false;
	}

	const char* pModuleFileName = findLastInString(pSettingsFileNameBuffer, pathLength, '\\');
	if( pModuleFileName == nullptr )
	{
		pModuleFileName = findLastInString(pSettingsFileNameBuffer, pathLength, '/');
		if( pModuleFileName == nullptr )
		{
			return false;
		}
	}

	const size_t modulePathPosition = pModuleFileName - pSettingsFileNameBuffer;
	pSettingsFileNameBuffer[modulePathPosition + 1] = 0;
	if( strcat_s(pSettingsFileNameBuffer, MAX_PATH, "settings.ini") != 0 )
	{
		return false;
	}

	return true;
}

bool8_t tryToLoadSettings( Win32Settings* pOutSettings )
{
	char settingsPath[MAX_PATH] = {};
	if( !createSettingsFileName( settingsPath, MAX_PATH ) )
	{
		return false;
	}

	if( !readSettingsFromFile( pOutSettings, settingsPath ) )
	{
		return false;
	}

	return true;
}

void storeSettings( Win32Settings* pOutSettings, Win32Context* pContext )
{
	pOutSettings->windowPosX 	= pContext->windowPosX;
	pOutSettings->windowPosY 	= pContext->windowPosY;
	pOutSettings->windowWidth 	= pContext->windowWidth;
	pOutSettings->windowHeight 	= pContext->windowHeight;
}

void applyDefaultSettings( Win32Settings* pOutSettings )
{
	pOutSettings->windowPosX = 0;
	pOutSettings->windowPosY = 0;
	pOutSettings->windowWidth = 1024;
	pOutSettings->windowHeight = 768;
	strcpy( pOutSettings->bindAddress, "127.0.0.1" );
}

constexpr size_t imguiMemoryBlockSizeInBytes = Mbyte(4);
void* pImGuiMemoryBlock = nullptr;
tlsf_t globalTlsf = 0;

void* K15ImGuiMallocWrapper( size_t size, void* user_data )
{
	K15_UNUSED_VAR(user_data);
	return tlsf_malloc(globalTlsf, size);
}

void K15ImgUIFreeWrapper( void* ptr, void* user_data )
{
	K15_UNUSED_VAR(user_data);
	tlsf_free(globalTlsf, ptr);
}

void handleWindowResize( Win32Context* pContext, WPARAM wparam )
{
	RECT clientRect = {0};
	GetClientRect(pContext->pMainWindowHandle, &clientRect);

	pContext->windowWidth  = clientRect.right - clientRect.left;
	pContext->windowHeight = clientRect.bottom - clientRect.top;

	if( pContext->pOpenGLContext != nullptr )
	{
		glViewport(0, 0, pContext->windowWidth, pContext->windowHeight);
	}
}

void handleWindowPosChanged( Win32Context* pContext, LPARAM lparam )
{
	WINDOWPOS* pWindowPos = (WINDOWPOS*)lparam;
	pContext->windowPosX = pWindowPos->x;
	pContext->windowPosY = pWindowPos->y;
}

LRESULT CALLBACK TOOLWNDPROC(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	bool8_t messageHandled = false;

	if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}

	Win32Context* pContext = ( Win32Context* )GetWindowLongPtrA( hwnd, GWLP_USERDATA );
	switch (msg)
	{
	case WM_CREATE:
		break;

	case WM_CLOSE:
	{
		char settingsFilePath[512];
		if( createSettingsFileName( settingsFilePath, sizeof(settingsFilePath) ) )
		{	
			Win32Settings settings;
			storeSettings(&settings, pContext);
			writeSettingsToFile( &settings, settingsFilePath );
		}
		break;
	}

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}

	case WM_SIZE:
	{
		handleWindowResize( pContext, wparam );
		break;
	}

	case WM_WINDOWPOSCHANGED:
	{
		handleWindowPosChanged( pContext, lparam );
		break;
	}

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_XBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		break;

	case WM_MOUSEMOVE:
		break;

	case WM_MOUSEWHEEL:
		break;
	}

	if (messageHandled == false)
	{
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

HWND setupWindow( HINSTANCE pInstance, const Win32Settings* pSettings )
{
	WNDCLASSA wndClass = {0};
	wndClass.style 			= CS_HREDRAW | CS_OWNDC | CS_VREDRAW;
	wndClass.hInstance 		= pInstance;
	wndClass.lpszClassName 	= "K15_ToolWindowClass";
	wndClass.lpfnWndProc 	= TOOLWNDPROC;
	wndClass.hCursor 		= LoadCursor(NULL, IDC_ARROW);
	RegisterClassA(&wndClass);

	HWND hwnd = CreateWindowA("K15_ToolWindowClass", "K15 GB Debugger", WS_OVERLAPPEDWINDOW, 
		pSettings->windowPosX, pSettings->windowPosY, pSettings->windowWidth, pSettings->windowHeight, 
		nullptr, nullptr, pInstance, nullptr);

	if (hwnd == INVALID_HANDLE_VALUE)
	{
		char errorMsgBuffer[512] = {};
		sprintf_s(errorMsgBuffer, sizeof( errorMsgBuffer ), "Error creating Window - Errorcode: %d", GetLastError() );
		MessageBoxA(0, errorMsgBuffer, "Error!", 0);
	}

	return hwnd;
}

bool8_t setupOpenGL( Win32Context* pContext )
{
	HGLRC pOpenGLContext = setupOpenGLAndCreateOpenGL4Context( pContext->pMainWindowHandle, pContext->pMainWindowDeviceContext );
	if( pOpenGLContext == nullptr )
	{
		return false;
	}

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	constexpr bool8_t enableVsync = true;
	
	//FK: Try to enable v-sync...
	w32glSwapIntervalEXT( enableVsync );

	//FK: 	Since vsync behavior can be overriden by the gpu driver, we have to
	//		check if vsync is actually disabled or not
	pContext->vsyncEnabled 		= w32glGetSwapIntervalEXT();
	pContext->pOpenGLContext 	= pOpenGLContext;

	return true;
}

bool8_t setupImgui(Win32Context* pContext)
{
	pImGuiMemoryBlock = VirtualAlloc( ( LPVOID )0x300000000, imguiMemoryBlockSizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if( pImGuiMemoryBlock == nullptr )
	{
		return false;
	}

	globalTlsf = tlsf_create_with_pool(pImGuiMemoryBlock, imguiMemoryBlockSizeInBytes);
	if( globalTlsf == 0 )
	{
		return false;
	}

	ImGui::SetAllocatorFunctions(K15ImGuiMallocWrapper, K15ImgUIFreeWrapper);

	ImGuiContext* pImGuiContext = ImGui::CreateContext();
	if( pImGuiContext == nullptr )
	{
		return false;
	}

	if( !ImGui_ImplWin32_Init(pContext->pMainWindowHandle) )
	{
		return false;
	}

	if( !ImGui_ImplOpenGL3_Init() )
	{
		return false;
	}

	return true;
}

bool8_t setupNetworking( Win32Context* pContext )
{
	WORD wsaVersion = MAKEWORD( 2, 2 );
	WSADATA wsaData = {};
	const int wsaResult = WSAStartup(wsaVersion, &wsaData);
	if( wsaResult != 0 )
	{
		return false;
	}

	pContext->broadcastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if( pContext->broadcastSocket == INVALID_SOCKET )
	{
		DWORD error = GetLastError();
		return false;
	}

	pContext->debugSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( pContext->debugSocket == INVALID_SOCKET )
	{
		return false;
	}

	char broadcast = 1;
	if( setsockopt(pContext->broadcastSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, 1) != 0 )
	{
		closesocket(pContext->broadcastSocket);
		pContext->broadcastSocket = INVALID_SOCKET;
		return false;
	}

	u_long mode = 1;  // 1 to enable non-blocking socket
	ioctlsocket( pContext->broadcastSocket, FIONBIO, &mode );

	INTERFACE_INFO interfaceInfos[16] = {};
	DWORD bytesReturned = 0;
	if( WSAIoctl( pContext->broadcastSocket, SIO_GET_INTERFACE_LIST, 0, 0, &interfaceInfos, sizeof(interfaceInfos), &bytesReturned, nullptr, nullptr ) == SOCKET_ERROR )
	{
		return false;
	}

	const DWORD interfaceCount = GetMin( MaxBroadcastAddresses, bytesReturned / sizeof( INTERFACE_INFO ) );
	for( DWORD interfaceIndex = 0; interfaceIndex < interfaceCount; ++interfaceIndex )
	{
		networkaddress_t maskAddress = interfaceInfos[interfaceIndex].iiNetmask.AddressIn.sin_addr.S_un.S_addr;
		networkaddress_t address = interfaceInfos[interfaceIndex].iiAddress.AddressIn.sin_addr.S_un.S_addr;

		networkaddress_t broadcastAddress = ( maskAddress & address ) | ( 0xFFFFFFFFu & ~maskAddress );
		pContext->broadcastAddresses[interfaceIndex] = broadcastAddress;
	}

	pContext->broadcastAddressCount = ( uint8_t )interfaceCount;

	return true;
}

bool8_t tryToConnectToEmulatorInstance( SOCKET socket, const DebuggerTarget* pTarget )
{
	struct sockaddr_in targetAddress;
	targetAddress.sin_family = AF_INET;
	targetAddress.sin_port = DebuggerPort;
	targetAddress.sin_addr = convertFromIPv4AddressString( pTarget->address, K15_IPV4_ADDRESS_MAX_STRING_LENGTH );
	return connect( socket, (struct sockaddr*)&targetAddress, sizeof( targetAddress ) ) == 0;
}

void doEmulatorSelectUi( DebuggerContext* pContext )
{
	ImGui::Begin("Connect to a target");
	for( uint8_t index = 0; index < pContext->debuggerTargetCount; ++index )
	{
		ImGui::LabelText( "", "%s - %s", pContext->debuggerTargets[ index ].name, pContext->debuggerTargets[ index ].address );
		ImGui::SameLine();
		
		if( ImGui::Button("Connect") )
		{
			if( tryToConnectToEmulatorInstance( pContext->emulatorSocket, &pContext->debuggerTargets[ index ] ) )
			{
				pContext->connectedToEmulator = true;
			}
		}
	}

	ImGui::End();
}

void doEmulatorMemoryViewUi( DebuggerContext* pContext )
{
	static MemoryEditor s_MemoryEditor;

	ImGui::Begin("Memory Debug View");
	s_MemoryEditor.DrawContents( pContext->emulatorContext.pMemory, 0xFFFF, 1u );
	ImGui::End();
}

void doEmulatorDebugUi( DebuggerContext* pContext )
{
	doEmulatorMemoryViewUi( pContext );
}

void updateEmulatorState( DebuggerContext* pContext )
{
	recv( pContext->emulatorSocket, (char*)pContext->emulatorContext.pMemory, 0x10000, 0 );
}

void doDebuggerUiFrame( DebuggerContext* pContext )
{
	if( !pContext->connectedToEmulator )
	{
		doEmulatorSelectUi( pContext );
	}
	else 
	{
		updateEmulatorState( pContext );
		doEmulatorDebugUi( pContext );
	}
}

void renderDebuggerUiFrame(HDC dc)
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	w32SwapBuffers( dc );

	glClear(GL_COLOR_BUFFER_BIT);
}

void sendEmulatorDetectionBroadcast( SOCKET socket, const networkaddress_t* pBroadcastAddresses, size_t broadcastAddressCount )
{
	struct sockaddr_in broadcastAddr;
	broadcastAddr.sin_family 		= AF_INET;
	broadcastAddr.sin_port 			= DebuggerBroadcastPort;

	for( uint8_t broadcastIndex = 0; broadcastIndex < broadcastAddressCount; ++broadcastIndex )
	{
		broadcastAddr.sin_addr.S_un.S_addr = pBroadcastAddresses[ broadcastIndex ];
		sendto( socket, (const char*)&BroadcastPacket, sizeof( BroadcastPacket ), 0, ( const sockaddr* )&broadcastAddr, sizeof( broadcastAddr ) );
	}
}

bool8_t targetAnsweredBroadcast( SOCKET socket, DebuggerTarget* pOutDebuggerTarget )
{
	struct sockaddr_in emulatorAddr = {};
	EmulatorPacket emulatorPacket = {};
	int packetLength = sizeof( emulatorAddr );
	if( recvfrom( socket, (char*)&emulatorPacket, sizeof( emulatorPacket ), 0, (sockaddr*)&emulatorAddr, &packetLength ) != -1 )
	{
		if( isValidEmulatorPacket( &emulatorPacket ) && emulatorPacket.header.type == EmulatorPacketType::PING )
		{
			strcpy_s( pOutDebuggerTarget->name, K15_MAX_COMPUTER_NAME_LENGTH, emulatorPacket.payload.pingPayload.computerName );
			convertToIPv4AddressString( emulatorAddr.sin_addr, pOutDebuggerTarget->address, K15_IPV4_ADDRESS_MAX_STRING_LENGTH );
			return true;
		}
	}

	return false;
}

void searchForEmulatorsToConnectTo( DebuggerContext* pContext, float deltaTimeInSeconds )
{
	pContext->timeUntilBroadcastInSeconds -= deltaTimeInSeconds;
	if( pContext->timeUntilBroadcastInSeconds <= 0.0f )
	{
		sendEmulatorDetectionBroadcast( pContext->broadcastSocket, pContext->pBroadcastAddresses, pContext->broadcastAddressCount );
		pContext->timeUntilBroadcastInSeconds = BroadcastTimeIntervallInSeconds; 
	}

	DebuggerTarget potentialTarget = {};
	if( targetAnsweredBroadcast( pContext->broadcastSocket, &potentialTarget ) )
	{
		bool8_t targetFoundEarlier = false;
		for( uint8_t targetIndex = 0; targetIndex < pContext->debuggerTargetCount; ++targetIndex )
		{
			if( strcmp( pContext->debuggerTargets[ targetIndex ].address, potentialTarget.address ) == 0 )
			{
				targetFoundEarlier = true;
				break;
			}
		}

		if( !targetFoundEarlier && pContext->debuggerTargetCount + 1 < K15_MAX_DEBUGGER_TARGETS )
		{
			pContext->debuggerTargets[ pContext->debuggerTargetCount ] = potentialTarget;
			++pContext->debuggerTargetCount;
		}
	}
}

void tickDebugger( DebuggerContext* pContext, float deltaTimeInSeconds )
{
	if( !pContext->connectedToEmulator )
	{
		searchForEmulatorsToConnectTo( pContext, deltaTimeInSeconds );
	}
	
	doDebuggerUiFrame( pContext );
}

float calculateDeltaTimeInSeconds( const LARGE_INTEGER* pFrequency, LARGE_INTEGER* pCounter )
{
	LARGE_INTEGER current;
	QueryPerformanceCounter( &current );

	LARGE_INTEGER delta;
	delta.QuadPart = { current.QuadPart - pCounter->QuadPart };
	delta.QuadPart *= 10; //convert to ms

	pCounter->QuadPart = current.QuadPart;

	return ( float )delta.QuadPart / ( float )pFrequency->QuadPart;
}

void runDebuggerMainLoop( Win32Context* pContext )
{
	DebuggerContext debuggerContext;
	debuggerContext.broadcastSocket 		= pContext->broadcastSocket;
	debuggerContext.pBroadcastAddresses 	= pContext->broadcastAddresses;
	debuggerContext.broadcastAddressCount 	= pContext->broadcastAddressCount;
	debuggerContext.emulatorSocket			= pContext->debugSocket;
	debuggerContext.emulatorContext.pMemory	= pContext->pEmulatorMemory;

	bool loopRunning = true;
	while( loopRunning )
	{
		MSG msg;
		if( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) > 0 )
		{
			if (msg.message == WM_QUIT)
			{
				loopRunning = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if( !loopRunning )
		{
			break;
		}

		ImGui_ImplWin32_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();
		
		const float deltaTimeInSeconds = calculateDeltaTimeInSeconds( &pContext->pcFrequency, &pContext->pcOld );
		tickDebugger( &debuggerContext, deltaTimeInSeconds );
		renderDebuggerUiFrame( pContext->pMainWindowDeviceContext );
	}
}

void loadWin32FunctionPointers()
{
	const HMODULE pGDIModule = LoadLibraryA("gdi32.dll");
	RuntimeAssert( pGDIModule != nullptr );

	const HMODULE pWSock2Module = LoadLibraryA("Ws2_32.dll");
	RuntimeAssert( pWSock2Module != nullptr );

	w32WSAStartup = (PFNWSASTARTUP)GetProcAddress(pWSock2Module, "WSAStartup");
	w32SwapBuffers = (PFNSWAPBUFFERSPROC)GetProcAddress( pGDIModule, "SwapBuffers");
	RuntimeAssert( w32WSAStartup != nullptr );
	RuntimeAssert( w32SwapBuffers != nullptr );
} 

int CALLBACK WinMain(HINSTANCE pInstance, HINSTANCE pPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	loadWin32FunctionPointers();

	Win32Settings settings = {};
	if( !tryToLoadSettings( &settings ) )
	{
		applyDefaultSettings( &settings );
	}

	Win32Context context = {};
	context.pMainWindowHandle = setupWindow( pInstance, &settings );
	if( context.pMainWindowHandle == INVALID_HANDLE_VALUE )
	{
		return 1;
	}

	SetWindowLongPtrA( context.pMainWindowHandle, GWLP_USERDATA, (LONG_PTR)&context );
	ShowWindow(context.pMainWindowHandle, SW_SHOW);

	context.pMainWindowDeviceContext = GetDC(context.pMainWindowHandle);
	QueryPerformanceFrequency( &context.pcFrequency );
	QueryPerformanceCounter( &context.pcOld );

	if( !setupOpenGL( &context ) )
	{
		return 1;
	}

	if( !setupImgui( &context ) )
	{
		return 1;
	}

	if( !setupNetworking( &context ) )
	{
		return 1;
	}

	context.pEmulatorMemory = VirtualAlloc( ( LPVOID )0x400000000, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if( context.pEmulatorMemory == nullptr )
	{
		return 1;
	}

	runDebuggerMainLoop( &context );

	return 0;
}