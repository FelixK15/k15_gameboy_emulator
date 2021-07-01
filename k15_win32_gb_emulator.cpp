#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct _XINPUT_STATE;
typedef _XINPUT_STATE XINPUT_STATE;

typedef int		(WINAPI *PFNCHOOSEPIXELFORMATPROC)(HDC hdc, CONST PIXELFORMATDESCRIPTOR* ppfd);
typedef BOOL	(WINAPI *PFNSETPIXELFORMATPROC)(HDC hdc, int pixelFormat, CONST PIXELFORMATDESCRIPTOR* ppfd);
typedef BOOL	(WINAPI *PFNSWAPBUFFERSPROC)(HDC hdc);
typedef DWORD 	(WINAPI *PFNXINPUTGETSTATEPROC)(DWORD, XINPUT_STATE*);

PFNXINPUTGETSTATEPROC		w32XInputGetState 		= nullptr;
PFNCHOOSEPIXELFORMATPROC 	w32ChoosePixelFormat 	= nullptr;
PFNSETPIXELFORMATPROC 	 	w32SetPixelFormat 		= nullptr;
PFNSWAPBUFFERSPROC 		 	w32SwapBuffers 			= nullptr;

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>

#include <stdio.h>
#include "k15_gb_emulator.h"
#include "k15_win32_opengl.h"

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
#	define IMGUI_IMPL_OPENGL_LOADER_CUSTOM "k15_win32_opengl.h"
#	include "imgui/imgui.cpp"
#	include "imgui/imgui_demo.cpp"
#	include "imgui/imgui_draw.cpp"
#	include "imgui/imgui_tables.cpp"
#	include "imgui/imgui_widgets.cpp"
#	include "imgui/imgui_impl_win32.cpp"
#	include "imgui/imgui_impl_opengl3.cpp"
#	include "k15_gb_emulator_ui.cpp"
#else
#	define XINPUT_GAMEPAD_DPAD_UP          0x0001
#	define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#	define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#	define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#	define XINPUT_GAMEPAD_START            0x0010
#	define XINPUT_GAMEPAD_BACK             0x0020
#	define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#	define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#	define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#	define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#	define XINPUT_GAMEPAD_A                0x1000
#	define XINPUT_GAMEPAD_B                0x2000
#	define XINPUT_GAMEPAD_X                0x4000
#	define XINPUT_GAMEPAD_Y                0x8000

typedef struct _XINPUT_GAMEPAD
{
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
    DWORD                               dwPacketNumber;
    XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;
#endif

const IID 	IID_IAudioClient			= _uuidof(IAudioClient);
const IID 	IID_IAudioRenderClient		= _uuidof(IAudioRenderClient);
const IID 	IID_IMMDeviceEnumerator 	= _uuidof(IMMDeviceEnumerator);
const CLSID CLSID_MMDeviceEnumerator 	= _uuidof(MMDeviceEnumerator);

enum InputType
{
	Gamepad,
	Keyboard
};

struct UserMessage
{
	char messageBuffer[256];
	LARGE_INTEGER startTime;
};

#define K15_RETURN_ON_HRESULT_ERROR(comFunction) \
{	\
	_com_error comFunctionResult(comFunction); \
	if( comFunctionResult.Error() != S_OK ) \
	{ \
		printf("COM function '%s' failed with '%s'.\n", #comFunction, comFunctionResult.ErrorMessage()); \
		return; \
	} \
}

struct Win32ApplicationContext
{
	static constexpr uint32_t gbFrameTimeInMicroseconds 		= 16700;

	HWND					pWindowHandle						= nullptr;
	HGLRC					pOpenGLContext						= nullptr;
	uint8_t* 				pGameboyRGBVideoBuffer 				= nullptr;
	GBEmulatorInstance* 	pEmulatorInstance 					= nullptr;
	UserMessage 			userMessages[8] 					= {};
	char 					gameTitle[16] 						= {};

	uint32_t 				screenWidth							= 1920;
	uint32_t 				screenHeight						= 1080;
	uint32_t 				emulatorDeltaTimeInMicroseconds 	= 0u;
	uint32_t 				deltaTimeInMicroseconds 			= 0u;
	uint32_t 				uiDeltaTimeInMicroseconds 			= 0u;

	LARGE_INTEGER 			perfCounterFrequency;

	GLuint 					gameboyFrameBufferTexture;
	GLuint 					bitmapFontTexture;
	GLuint					vertexBuffer;
	GLuint					vertexArray;
	
	GBEmulatorJoypadState 	joypadState;

	InputType 				dominantInputType 					= Gamepad;
	uint8_t 				userMessageCount 					= 0;
	bool   					showUi								= true;
};

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

HMODULE getLibraryHandle( const char* pLibraryFileName )
{
	HMODULE pModuleHandle = GetModuleHandleA( pLibraryFileName );
	if( pModuleHandle != nullptr )
	{
		return pModuleHandle;
	}

	return LoadLibraryA( pLibraryFileName );
}

void pushUserMessage( Win32ApplicationContext* pContext, const char* pFormattedMessage, ... )
{
	UserMessage* pUserMessage = &pContext->userMessages[ pContext->userMessageCount++ ];
	if( pContext->userMessageCount == 8 )
	{
		//FK: Order is important here
		pContext->userMessageCount = 7;
		memmove( pContext->userMessages, pContext->userMessages + 1, sizeof(UserMessage) * pContext->userMessageCount );
	}

	va_list argList;
	va_start( argList, pFormattedMessage );
	vsprintf( pUserMessage->messageBuffer, pFormattedMessage, argList );
	va_end(argList);

	QueryPerformanceCounter( &pUserMessage->startTime );
}

void updateVertexBuffer( Win32ApplicationContext* pContext )
{
	const float pixelUnitH = 1.0f/(float)pContext->screenWidth;
	const float pixelUnitV = 1.0f/(float)pContext->screenHeight;
	const float centerH = pixelUnitH*(0.5f*(float)pContext->screenWidth);
	const float centerV = pixelUnitV*(0.5f*(float)pContext->screenHeight);

	const float gbSizeH = pixelUnitH*gbHorizontalResolutionInPixels;
	const float gbSizeV = pixelUnitV*gbVerticalResolutionInPixels;

	//FK: division using integer to keep integer scaling (minus 2 to not fill out the whole screen just yet)
	const float scale 		= (float)(pContext->screenHeight / gbVerticalResolutionInPixels) - 2;

	const float l = (float)( pContext->screenWidth - gbHorizontalResolutionInPixels * scale ) * 0.5f;
	const float r = (float)( pContext->screenWidth + gbHorizontalResolutionInPixels * scale ) * 0.5f;	
	const float t = (float)( pContext->screenHeight + gbVerticalResolutionInPixels * scale ) * 0.5f;	
	const float b = (float)( pContext->screenHeight - gbVerticalResolutionInPixels * scale ) * 0.5f;	

	const float left  		= (((2.0f * l) - (2.0f * 0)) / pContext->screenWidth) - 1.0f;
	const float right 		= (((2.0f * r) - (2.0f * 0)) / pContext->screenWidth) - 1.0f;
	const float top	  		= (((2.0f * t) - (2.0f * 0)) / pContext->screenHeight) - 1.0f;
	const float bottom	    = (((2.0f * b) - (2.0f * 0)) / pContext->screenHeight) - 1.0f;

	const float vertexBufferData[] = {
		left, bottom,
		0.0f, 1.0f,

		right, bottom,
		1.0f,  1.0f,

		right, top,
		1.0f,  0.0f,

		right, top,
		1.0f,  0.0f,

		left, top,
		0.0f, 0.0f,

		left, bottom,
		0.0f, 1.0f
	};

	glBufferData( GL_ARRAY_BUFFER, sizeof(vertexBufferData), vertexBufferData, GL_STATIC_DRAW );
}

uint16_t queryMonitorRefreshRate( HWND hwnd )
{
	HMONITOR monitorHandle = MonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY );

	MONITORINFOEX monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if( GetMonitorInfo( monitorHandle, &monitorInfo ) == FALSE )
	{
		return 60; //FK: assume 60hz as default
	}

	DEVMODE displayInfo;
	if( EnumDisplaySettings( monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &displayInfo ) == FALSE )
	{
		return 60; //FK: assume 60hz as default
	}

	return (uint16_t)displayInfo.dmDisplayFrequency;
}

void allocateDebugConsole()
{
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
}

void K15_KeyInput(Win32ApplicationContext* pContext, UINT message, WPARAM wparam, LPARAM lparam)
{
	pContext->dominantInputType = InputType::Keyboard;

	if( message == WM_KEYDOWN )
	{
		if((lparam & (1 >> 30)) == 0)
		{
			switch(wparam)
			{
				case VK_F2:
					pushUserMessage(pContext, "State saved successfully...");
					break;
					
				case VK_DOWN:
					pContext->joypadState.down = 1;
					break;
				case VK_UP:
					pContext->joypadState.up = 1;
					break;
				case VK_LEFT:
					pContext->joypadState.left = 1;
					break;
				case VK_RIGHT:
					pContext->joypadState.right = 1;
					break;

				case VK_CONTROL:
					pContext->joypadState.start = 1;
					break;
				case VK_MENU:
					pContext->joypadState.select = 1;
					break;
				case 'A':
					pContext->joypadState.a = 1;
					break;
				case 'S':
					pContext->joypadState.b = 1;
					break;

				case VK_F1:
					pContext->showUi = !pContext->showUi;
					break;
			}
		}
	}
	else if( message == WM_KEYUP )
	{
		switch(wparam)
		{
			case VK_DOWN:
				pContext->joypadState.down = 0;
				break;
			case VK_UP:
				pContext->joypadState.up = 0;
				break;
			case VK_LEFT:
				pContext->joypadState.left = 0;
				break;
			case VK_RIGHT:
				pContext->joypadState.right = 0;
				break;

			case VK_CONTROL:
				pContext->joypadState.start = 0;
				break;
			case VK_MENU:
				pContext->joypadState.select = 0;
				break;
			case 'A':
				pContext->joypadState.a = 0;
				break;
			case 'S':
				pContext->joypadState.b = 0;
				break;
		}
	}

	setGBEmulatorInstanceJoypadState( pContext->pEmulatorInstance, pContext->joypadState );
}

void K15_WindowResized(Win32ApplicationContext* pContext, UINT message, WPARAM wparam, LPARAM lparam)
{
	RECT clientRect = {0};
	GetClientRect(pContext->pWindowHandle, &clientRect);

	pContext->screenWidth  = clientRect.right - clientRect.left;
	pContext->screenHeight = clientRect.bottom - clientRect.top;

	if( pContext->pOpenGLContext != nullptr )
	{
		updateVertexBuffer( pContext );
		glViewport(0, 0, pContext->screenWidth, pContext->screenHeight);
	}

	//FK: Requery monitor refresh rate in case the monitor changed
	const uint16_t monitorRefreshRate = queryMonitorRefreshRate(pContext->pWindowHandle);
	if( pContext->pEmulatorInstance != nullptr )
	{
		setGBEmulatorInstanceMonitorRefreshRate( pContext->pEmulatorInstance, monitorRefreshRate );
	}
}

LRESULT CALLBACK K15_WNDPROC(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	uint8_t messageHandled = false;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
        return true;
#endif

	Win32ApplicationContext* pContext = (Win32ApplicationContext*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
	switch (message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		messageHandled = true;
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		K15_KeyInput(pContext, message, wparam, lparam);
		break;

	case WM_SIZE:
		K15_WindowResized(pContext, message, wparam, lparam);
		break;
	}

	if (messageHandled == false)
	{
		return DefWindowProc(hwnd, message, wparam, lparam);
	}

	return false;
}

bool setupWindow( HINSTANCE pInstance, Win32ApplicationContext* pContext )
{
	WNDCLASSA wndClass 		= {0};
	wndClass.style 			= CS_HREDRAW | CS_OWNDC | CS_VREDRAW;
	wndClass.hInstance 		= pInstance;
	wndClass.lpszClassName 	= "K15_Win32Template";
	wndClass.lpfnWndProc 	= K15_WNDPROC;
	wndClass.hCursor 		= LoadCursor(NULL, IDC_ARROW);
	RegisterClassA(&wndClass);

	pContext->pWindowHandle = CreateWindowA("K15_Win32Template", "Win32 Template",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		pContext->screenWidth, pContext->screenHeight, 
		0, 0, pInstance, 0);

	if (pContext->pWindowHandle == INVALID_HANDLE_VALUE)
	{
		MessageBoxA(0, "Error creating Window.\n", "Error!", 0);
		return false;
	}

	SetWindowLongPtrA( pContext->pWindowHandle, GWLP_USERDATA, (LONG_PTR)pContext );
	ShowWindow(pContext->pWindowHandle, SW_SHOW);
	return true;
}

void generateOpenGLShaders()
{
	constexpr char vertexShaderSource[] = R"(
		#version 410
		layout(location=0) in vec2 vs_pos;
		layout(location=1) in vec2 vs_uv;

		out vec2 ps_uv;

		void main()
		{
			gl_Position = vec4(vs_pos.x, vs_pos.y, 0, 1);
			ps_uv = vs_uv;
		}
	)";

	constexpr char pixelShaderSource[] = R"(
		#version 410
		in vec2 ps_uv;
		out vec4 color;

		uniform sampler2D gameboyFramebuffer;

		void main()
		{
			color = texture( gameboyFramebuffer, ps_uv );
		}
	)";

	const GLuint vertexShader 	= glCreateShader( GL_VERTEX_SHADER );
	const GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
	const GLuint gpuProgram		= glCreateProgram();

	compileOpenGLShader( vertexShader, vertexShaderSource, sizeof( vertexShaderSource ) );
	compileOpenGLShader( fragmentShader, pixelShaderSource, sizeof( pixelShaderSource ) );

	glAttachShader( gpuProgram, vertexShader );
	glAttachShader( gpuProgram, fragmentShader );
	glLinkProgram( gpuProgram );
	glUseProgram( gpuProgram );
}

void openGLDebugMessageCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* pMessage, const GLvoid* pUserParam )
{
	printf(pMessage);
}

void createOpenGLContext( Win32ApplicationContext* pContext )
{
	const HMODULE pOpenGL32Module = getLibraryHandle("opengl32.dll");

	loadWin32OpenGLFunctionPointers( pOpenGL32Module );
	createOpenGLDummyContext( pContext->pWindowHandle );
	loadWGLOpenGLFunctionPointers();
	createOpenGL4Context( pContext->pWindowHandle );
	loadOpenGL4FunctionPointers();

	//glDebugMessageCallbackARB( openGLDebugMessageCallback, nullptr );

	glGenVertexArrays( 1, &pContext->vertexArray );
	glBindVertexArray( pContext->vertexArray );

	generateOpenGLShaders();

	//FK: Enable v-sync
	w32glSwapIntervalEXT(1);
}

void generateOpenGLTextures(Win32ApplicationContext* pContext)
{
	glGenTextures(1, &pContext->gameboyFrameBufferTexture);
	glGenTextures(1, &pContext->bitmapFontTexture);

	glBindTexture(GL_TEXTURE_2D, pContext->gameboyFrameBufferTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, gbHorizontalResolutionInPixels, gbVerticalResolutionInPixels);
	
	glBindTexture(GL_TEXTURE_2D, pContext->bitmapFontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, fontPixelDataWidthInPixels, fontPixelDataHeightInPixels);
}

char* fixRomFileName( char* pRomFileName )
{
	//FK: This needs to be done purely because visual studio code wraps the argument in double quotes for whatever reason...
	while( *pRomFileName == '\"' )
	{
		++pRomFileName;
	}

	char* pFixedRomFileName = pRomFileName;

	while( *pRomFileName != '\"' && *pRomFileName != 0 )
	{
		++pRomFileName;
	}

	if( *pRomFileName == '\"' )
	{
		*pRomFileName = 0;
	}

	return pFixedRomFileName;
}

uint8_t* mapCartridgeRamFile( const char* pRamFileName, const size_t fileSizeInBytes )
{
	HANDLE pRomHandle = CreateFileA( pRamFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0u, nullptr );
	if( pRomHandle == INVALID_HANDLE_VALUE )
	{
		const DWORD createFileError = GetLastError();
		printf("Could not open '%s'.\n", pRamFileName);
		return nullptr;
	}

	const DWORD fileSizeLow  = fileSizeInBytes & 0xFFFFFFFF;
	HANDLE pRomMapping = CreateFileMappingA( pRomHandle, nullptr, PAGE_READWRITE, 0u, fileSizeLow, nullptr );
	if( pRomMapping == nullptr )
	{
		const DWORD lastError = GetLastError();
		printf("Could not map '%s' with PAGE_WRITECOPY. Win32 ErrorCode=%x.\n", pRamFileName, lastError );
		return nullptr;
	}

	return (uint8_t*)MapViewOfFile( pRomMapping, FILE_MAP_WRITE, 0u, 0u, 0u );
}

const uint8_t* mapRomFile( const char* pRomFileName )
{
	HANDLE pRomHandle = CreateFileA( pRomFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0u, nullptr );
	if( pRomHandle == INVALID_HANDLE_VALUE )
	{
		printf("Could not open '%s'.\n", pRomFileName);
		return nullptr;
	}

	HANDLE pRomMapping = CreateFileMapping( pRomHandle, nullptr, PAGE_READONLY, 0u, 0u, nullptr );
	if( pRomMapping == INVALID_HANDLE_VALUE )
	{
		printf("Could not map '%s'.\n", pRomFileName);
		return nullptr;
	}

	return (const uint8_t*)MapViewOfFile( pRomMapping, FILE_MAP_READ, 0u, 0u, 0u );
}

void queryXInputController( Win32ApplicationContext* pContext )
{
	XINPUT_STATE state;
	const DWORD result = w32XInputGetState(0, &state);
	if( result != ERROR_SUCCESS )
	{
		return;
	}

	const WORD gamepadButtons = state.Gamepad.wButtons;

	if( pContext->dominantInputType != InputType::Gamepad && gamepadButtons == 0)
	{
		return;
	}

	pContext->dominantInputType = InputType::Gamepad;
	pContext->joypadState.a 		= ( gamepadButtons & XINPUT_GAMEPAD_A ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_B ) > 0;
	pContext->joypadState.b 		= ( gamepadButtons & XINPUT_GAMEPAD_X ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_Y ) > 0;
	pContext->joypadState.start 	= ( gamepadButtons & XINPUT_GAMEPAD_START ) > 0;
	pContext->joypadState.select 	= ( gamepadButtons & XINPUT_GAMEPAD_BACK ) > 0;
	pContext->joypadState.left 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_LEFT ) > 0;
	pContext->joypadState.right 	= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) > 0;
	pContext->joypadState.down 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_DOWN ) > 0;
	pContext->joypadState.up 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_UP ) > 0;

	setGBEmulatorInstanceJoypadState( pContext->pEmulatorInstance, pContext->joypadState );
}

void initXInput()
{
	HMODULE pXinputModule = getLibraryHandle("xinput1_4.dll");
	if( pXinputModule == nullptr )
	{
		pXinputModule = getLibraryHandle("xinput1_3.dll");
	}

	if( pXinputModule == nullptr )
	{
		return;
	}

	w32XInputGetState = (PFNXINPUTGETSTATEPROC)GetProcAddress( pXinputModule, "XInputGetState");
	K15_RUNTIME_ASSERT(w32XInputGetState != nullptr);
}

void initializeImGui( HWND hwnd )
{
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init( hwnd );
    ImGui_ImplOpenGL3_Init( "#version 410" );
#endif
}

#include <math.h>
constexpr float pi = 3.14159f;
constexpr float twoPi = 2.0f*pi;
float getSineWaveSample(const float t, const float a, const float f)
{
	const float ft = f*t;
	return a * sinf(twoPi*ft);
}

float getSquareWaveSample(const float t, const float a, const float f )
{
	const float s = getSineWaveSample(t, 1.0f, f);
	return a * (s < 0.0f ? -1.f : 1.f);
}

float getAnalogSquareWaveSample(const float t, const float a, const float f )
{	
	const float ft = f*t;
	float s = 0.0f;
	for( uint8_t i = 1; i < 22; ++i )
	{	
		const float k = (float)i;
		s += sinf(twoPi*(2.0f*k-1.0f)*ft)/(2.0f*k-1);
	}

	return a*s;
}

float getTriangleWaveSample(const float t, const float a, const float f)
{
	return asinf(getSineWaveSample(t, a, f) * 2.0f / pi);
}

float getNoiseSample(const float t, const float a, const float f)
{
	return a * ( 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f );
}

void initWasapi()
{
#if 1
	return;
#else
	IMMDeviceEnumerator* pEnumerator = nullptr;
	K15_RETURN_ON_HRESULT_ERROR( CoCreateInstance( CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator) );

	IMMDevice* pDevice = nullptr;
	K15_RETURN_ON_HRESULT_ERROR( pEnumerator->GetDefaultAudioEndpoint( eRender, eConsole, &pDevice ) );

	IAudioClient* pAudioClient = nullptr;
	constexpr PROPVARIANT* pActivationParams = nullptr; //FK: Set to NULL to activate an IAudioClient
	K15_RETURN_ON_HRESULT_ERROR( pDevice->Activate( IID_IAudioClient, CLSCTX_ALL, pActivationParams, (void**)&pAudioClient ) );

	WAVEFORMATEX* pWaveFormat = nullptr;
	K15_RETURN_ON_HRESULT_ERROR( pAudioClient->GetMixFormat( &pWaveFormat ) );

	constexpr uint32_t nanosecondsToMilliseconds = 1000000;
	constexpr REFERENCE_TIME bufferDuration = 16 * nanosecondsToMilliseconds / 100; //FK: Each REFERENCE_TIME unit is 100ns ¯\_(ツ)_/¯
	
	constexpr DWORD streamFlags = 0; //FK: There are some interesting flags to set 	https://docs.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
							     	 //												https://docs.microsoft.com/en-us/windows/win32/coreaudio/audclnt-sessionflags-xxx-constants
	
	constexpr REFERENCE_TIME periodicity = 0; //FK: This parameter can be nonzero only in exclusive mode.
	constexpr LPCGUID pSessionGuid = nullptr; //FK: Add stream to existing session (what exactly is a session?)
	K15_RETURN_ON_HRESULT_ERROR( pAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED, streamFlags, bufferDuration, periodicity, pWaveFormat, pSessionGuid ) );

	// The size in bytes of an audio frame is calculated as the number of channels in the stream multiplied by the sample size per channel
	UINT32 bufferFrameCount = 0;
	K15_RETURN_ON_HRESULT_ERROR( pAudioClient->GetBufferSize( &bufferFrameCount ) );

	IAudioRenderClient* pAudioRenderClient = nullptr;
	K15_RETURN_ON_HRESULT_ERROR( pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pAudioRenderClient) );
	K15_RETURN_ON_HRESULT_ERROR( pAudioClient->Start() );

	LARGE_INTEGER start = {};
	LARGE_INTEGER end	= {};
	const float step = 1.0f/(float)bufferFrameCount;
	float t = 0.0f;
	float a = 0.1f;
	float f = 20.0f;
	uint8_t type = 0;
	while( true )
	{	
		QueryPerformanceCounter(&start);

		UINT32 padding = 0;

		// A rendering application can use the padding value to determine how much new data it can safely write to the endpoint 
		// buffer without overwriting previously written data that the audio engine has not yet read from the buffer.
		K15_RETURN_ON_HRESULT_ERROR( pAudioClient->GetCurrentPadding( &padding ) );

		const UINT32 framesToRender = bufferFrameCount - padding;

		BYTE* pAudioRenderBuffer = nullptr;
		K15_RETURN_ON_HRESULT_ERROR( pAudioRenderClient->GetBuffer( framesToRender, &pAudioRenderBuffer ) );
		float* pBuffer = (float*)pAudioRenderBuffer;

		if( GetAsyncKeyState(VK_F1) & 0x8000 )
		{
			f = 5000.f;
		}
		else if( GetAsyncKeyState(VK_F2) & 0x8000 )
		{
			f = 20.0f;
		}
		else if( GetAsyncKeyState(VK_F3) & 0x8000 )
		{
			f = 10000.0f;
		}
		
		if( GetAsyncKeyState(VK_UP) & 0x8000 )
		{
			a += 0.01f;
		}
		else if( GetAsyncKeyState(VK_DOWN) & 0x8000 )
		{
			a -= 0.01f;
		}

		if( GetAsyncKeyState('0') & 0x8000 )
		{
			type = 0;
		}

		if( GetAsyncKeyState('1') & 0x8000 )
		{
			type = 1;
		}

		if( GetAsyncKeyState('2') & 0x8000 )
		{
			type = 2;
		}

		if( GetAsyncKeyState('3') & 0x8000 )
		{
			type = 3;
		}

		if( GetAsyncKeyState('4') & 0x8000 )
		{
			type = 4;
		}

		f = max( 1.0f, f );
		a = max( 0.0f, a );

		for( size_t i = 0; i < framesToRender; ++i )
		{	
			//const float sample = getSinusWaveSample(t, a, f);
			float sample = 0.0f;
			switch( type )
			{
				case 0:
					sample = getSineWaveSample(t, a, f);
					break;
				case 1:
					sample = getSquareWaveSample(t, a, f);
					break;
				case 2:
					sample = getTriangleWaveSample(t, a, f);
					break;
				case 3:
					sample = getNoiseSample(t, a, f);
					break;
				case 4:
					sample = getAnalogSquareWaveSample(t, a, f);
					break;
			}

			for( size_t c = 0; c < pWaveFormat->nChannels; ++c )
			{
				*pBuffer++ = sample;
			}

			t += step;
		}

		K15_RETURN_ON_HRESULT_ERROR( pAudioRenderClient->ReleaseBuffer( framesToRender, 0 ) );

		QueryPerformanceCounter(&end);

		deltaTimeInMicroseconds = (uint32_t)(((end.QuadPart-start.QuadPart)*1000000)/perfCounterFrequency.QuadPart);
		if( deltaTimeInMicroseconds < 8000)
		{
			Sleep(8-deltaTimeInMicroseconds/1000);
		}
	}
#endif
}

void generateRamFileName( char* pRamFileNameBuffer, size_t ramFileNameLength, char* pRomPath )
{
	const char ramFileExtension[] = ".k15_ram_state";
	char* pRomFileExtension = strrchr( pRomPath, '.' );
	*pRomFileExtension = 0;
	strcat_s( pRamFileNameBuffer, ramFileNameLength, pRomPath );
	strcat_s( pRamFileNameBuffer, ramFileNameLength, ramFileExtension );
	*pRomFileExtension = '.';
}

void loadWin32FunctionPointers()
{
	const HMODULE pGDIModule = getLibraryHandle("gdi32.dll");
	K15_RUNTIME_ASSERT( pGDIModule != nullptr );

	w32ChoosePixelFormat = (PFNCHOOSEPIXELFORMATPROC)GetProcAddress( pGDIModule, "ChoosePixelFormat" );
	w32SetPixelFormat	 = (PFNSETPIXELFORMATPROC)GetProcAddress( pGDIModule, "SetPixelFormat" );
	w32SwapBuffers		 = (PFNSWAPBUFFERSPROC)GetProcAddress( pGDIModule, "SwapBuffers");

	//FK: TODO Turn into user friendly error messages
	K15_RUNTIME_ASSERT( w32ChoosePixelFormat != nullptr );
	K15_RUNTIME_ASSERT( w32SetPixelFormat != nullptr );
	K15_RUNTIME_ASSERT( w32SwapBuffers != nullptr );
}

BOOL setup( Win32ApplicationContext* pContext, LPSTR romPath )
{	
	QueryPerformanceFrequency(&pContext->perfCounterFrequency);

	loadWin32FunctionPointers();
	initWasapi();
	initXInput();
	createOpenGLContext( pContext );

	glGenBuffers( 1, &pContext->vertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, pContext->vertexBuffer );

	updateVertexBuffer( pContext );

	const size_t posOffset = 0;
	const size_t uvOffset = 2*sizeof(float);
	const size_t strideInBytes = 4*sizeof(float);

	glVertexAttribPointer(0, 2, GL_FLOAT, 0, strideInBytes, (const GLvoid*)posOffset );
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, 0, strideInBytes, (const GLvoid*)uvOffset );
	glEnableVertexAttribArray(1);

	char* pRomPath = fixRomFileName( romPath );
	const uint8_t* pRomData = mapRomFile( pRomPath );
	if( pRomData == nullptr )
	{
		return FALSE;
	}

	const uint16_t monitorRefreshRate = queryMonitorRefreshRate( pContext->pWindowHandle );
	const size_t emulatorMemorySizeInBytes = calculateGBEmulatorInstanceMemoryRequirementsInBytes();

	//FK: at address 0x100000000 for better debugging of memory errors (eg: access violation > 0x100000000 indicated emulator instance memory is at fault)
	uint8_t* pEmulatorInstanceMemory = (uint8_t*)VirtualAlloc( ( LPVOID )0x100000000, emulatorMemorySizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	pContext->pEmulatorInstance = createGBEmulatorInstance(pEmulatorInstanceMemory);

	setGBEmulatorInstanceMonitorRefreshRate( pContext->pEmulatorInstance, monitorRefreshRate );
	loadGBEmulatorInstanceRom( pContext->pEmulatorInstance, pRomData );

	//FK: at address 0x200000000 for better debugging of memory errors (eg: access violation > 0x200000000 indicated emulator instance memory is at fault)
	pContext->pGameboyRGBVideoBuffer = (uint8_t*)VirtualAlloc( ( LPVOID )0x200000000, (gbVerticalResolutionInPixels*gbHorizontalResolutionInPixels*3), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );

	const GBCartridgeHeader header = getGBCartridgeHeader(pRomData);
	memcpy(pContext->gameTitle, header.gameTitle, sizeof(header.gameTitle) );

	generateOpenGLTextures( pContext );
	initializeImGui( pContext->pWindowHandle );

	const size_t ramSizeInBytes = mapRamSizeToByteSize(header.ramSize);
	if( ramSizeInBytes > 0 )
	{
		char ramFileName[MAX_PATH] = {0};
		generateRamFileName( ramFileName, sizeof( ramFileName ), pRomPath );

		uint8_t* pRamData = mapCartridgeRamFile( ramFileName, ramSizeInBytes );
		setGBEmulatorRamData( pContext->pEmulatorInstance, pRamData );
	}

	return TRUE;
}

void drawGBFrameBuffer( Win32ApplicationContext* pContext )
{
	glBindTexture(GL_TEXTURE_2D, pContext->gameboyFrameBufferTexture);
	glBindBuffer( GL_ARRAY_BUFFER, pContext->vertexBuffer );
	glBindVertexArray( pContext->vertexArray );

	glDrawArrays( GL_TRIANGLES, 0, 6 );
}

void drawUserMessage( Win32ApplicationContext* pContext, const char* pText, float x, float y)
{
	#if 0
	const float pixelUnitH = 1.0f/(float)pContext->screenWidth;
	const float pixelUnitV = 1.0f/(float)pContext->screenHeight;

	const size_t textLength = strlen(pText);
	if( textLength == 0 )
	{
		return;
	}

	const float xStep = pixelUnitH * glyphWidthInPixels * 4;
	const float yStep = pixelUnitV * glyphHeightInPixels * 4;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, pContext->bitmapFontTexture);

	glBegin(GL_TRIANGLES);
		for( size_t textIndex = 0; textIndex < textLength; ++textIndex )
		{
			const char textCharacter = pText[ textIndex ];
			const char fontCharacterIndex = textCharacter - 32;
			if( fontCharacterIndex < 0 || fontCharacterIndex > 95 )
			{
				//FK: non renderable glyphs
				continue;
			}

			const uint8_t rowIndex 		= ( fontCharacterIndex / glyphRowCount );
			const uint8_t columnIndex 	= ( fontCharacterIndex % glyphRowCount );

			const uint16_t glyphX = columnIndex * glyphWidthInPixels;
			const uint16_t glyphY = rowIndex * glyphHeightInPixels;

			const float glyphU = ( (float)glyphX / (float)fontPixelDataWidthInPixels );
			const float glyphV = ( (float)glyphY / (float)fontPixelDataHeightInPixels );

			const float glyphUStep = (1.0f / fontPixelDataWidthInPixels) * (float)glyphWidthInPixels;
			const float glyphVStep = (1.0f / fontPixelDataHeightInPixels) * (float)glyphHeightInPixels;

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU, glyphV);
			glVertex2f(x, y + yStep);

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU + glyphUStep, glyphV);
			glVertex2f(x + xStep, y + yStep);

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU + glyphUStep, glyphV + glyphVStep);
			glVertex2f(x + xStep, y);

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU + glyphUStep, glyphV + glyphVStep);
			glVertex2f(x + xStep, y);

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU, glyphV + glyphVStep);
			glVertex2f(x, y);

			glColor3f(1.0f, 1.0f, 1.0f);
			glTexCoord2d(glyphU, glyphV);
			glVertex2f(x, y + yStep);

			x += xStep;
		}
	glEnd();
	#endif
}

void drawUserMessages( Win32ApplicationContext* pContext )
{
	const float pixelUnitH = 1.0f/(float)pContext->screenWidth;
	const float pixelUnitV = 1.0f/(float)pContext->screenHeight;

	for( uint8_t userMessageIndex = 0; userMessageIndex < pContext->userMessageCount; ++userMessageIndex )
	{
		const UserMessage* pUserMessage = pContext->userMessages + userMessageIndex;
		const float x = 1.0f - ( pixelUnitH * 4 * glyphWidthInPixels * strlen( pUserMessage->messageBuffer ) );
		const float y = ( pixelUnitV * ( ( 1 + userMessageIndex ) * glyphHeightInPixels * 4 ) ) - 1.0f;
		drawUserMessage( pContext, pUserMessage->messageBuffer, x, y);
	}
}

void drawUserInterface()
{
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

void doFrame( Win32ApplicationContext* pContext )
{
	char windowTitle[128];
	sprintf_s(windowTitle, sizeof(windowTitle), "%s - emulator: %.3f ms - ui: %.3f ms - %d hz monitor refresh rate", pContext->gameTitle, (float)pContext->deltaTimeInMicroseconds/1000.f, (float)pContext->uiDeltaTimeInMicroseconds/1000.f, pContext->pEmulatorInstance->monitorRefreshRate );
	SetWindowTextA(pContext->pWindowHandle, (LPCSTR)windowTitle);

	drawGBFrameBuffer(pContext);
	drawUserMessages(pContext);
	drawUserInterface();
	
	HDC deviceContext = GetDC(pContext->pWindowHandle);
	w32SwapBuffers(deviceContext);

	glClear(GL_COLOR_BUFFER_BIT);
}

void updateUserMessages( Win32ApplicationContext* pContext )
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);

	for( size_t userMessageIndex = 0; userMessageIndex < pContext->userMessageCount; ++userMessageIndex )
	{
		const UserMessage* pUserMessage = pContext->userMessages + userMessageIndex;
		const uint32_t deltaInMilliseconds = (uint32_t)(((time.QuadPart-pUserMessage->startTime.QuadPart)*1000)/pContext->perfCounterFrequency.QuadPart);
		if( deltaInMilliseconds > 5000 )
		{
			pContext->userMessages[userMessageIndex] = pContext->userMessages[userMessageIndex+1];
			--pContext->userMessageCount;
		}
	}
}

int CALLBACK WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nShowCmd)
{
	Win32ApplicationContext appContext;
	allocateDebugConsole();

	setupWindow(hInstance, &appContext);

	setup( &appContext, lpCmdLine );

	uint8_t loopRunning = true;
	MSG msg = {0};

	LARGE_INTEGER start;
	LARGE_INTEGER uiStart;
	LARGE_INTEGER end;

	start.QuadPart = 0;
	uiStart.QuadPart = 0;
	end.QuadPart = 0;

	while (loopRunning)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
		{
			if (msg.message == WM_QUIT)
			{
				loopRunning = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!loopRunning)
		{
			break;
		}

		queryXInputController(&appContext);

		QueryPerformanceCounter(&start);
		const GBEmulatorInstanceEventMask emulatorEventMask = runGBEmulatorInstance( appContext.pEmulatorInstance );

		if( emulatorEventMask & K15_GB_VBLANK_EVENT_FLAG )
		{
			const uint8_t* pFrameBuffer = getGBEmulatorInstanceFrameBuffer( appContext.pEmulatorInstance );
			convertGBFrameBufferToRGB8Buffer( appContext.pGameboyRGBVideoBuffer, pFrameBuffer );
			
			//FK: GB frame finished, upload gb framebuffer to texture
			glBindTexture(GL_TEXTURE_2D, appContext.gameboyFrameBufferTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gbHorizontalResolutionInPixels, gbVerticalResolutionInPixels,
				GL_RGB, GL_UNSIGNED_BYTE, appContext.pGameboyRGBVideoBuffer);
		}

		if( emulatorEventMask & K15_GB_STATE_SAVED_EVENT_FLAG )
		{
			pushUserMessage(&appContext, "State saved successfully...");
		}

		if( emulatorEventMask & K15_GB_STATE_LOADED_EVENT_FLAG )
		{
			pushUserMessage(&appContext, "State loaded successfully...");
		}

		updateUserMessages(&appContext);
		QueryPerformanceCounter(&uiStart);

		//FK: UI
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();

		    ImGui::NewFrame();
			if( appContext.showUi )
			{
				doUiFrame( appContext.pEmulatorInstance );
			}
			ImGui::EndFrame();
		}
#endif

		QueryPerformanceCounter(&end);
	
		doFrame(&appContext);

		appContext.deltaTimeInMicroseconds 	= (uint32_t)(((uiStart.QuadPart-start.QuadPart)*1000000)/appContext.perfCounterFrequency.QuadPart);
		appContext.uiDeltaTimeInMicroseconds 	= (uint32_t)(((end.QuadPart-uiStart.QuadPart)*1000000)/appContext.perfCounterFrequency.QuadPart);
	}

	DestroyWindow(appContext.pWindowHandle);

	return 0;
}