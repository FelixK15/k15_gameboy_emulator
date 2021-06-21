#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <gl/GL.h>
#include "k15_gb_emulator.h"

typedef BOOL(*wglSwapIntervalEXTProc)(int);
wglSwapIntervalEXTProc wglSwapIntervalEXT = nullptr;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
#	include "imgui/imgui.cpp"
#	include "imgui/imgui_demo.cpp"
#	include "imgui/imgui_draw.cpp"
#	include "imgui/imgui_tables.cpp"
#	include "imgui/imgui_widgets.cpp"
#	include "imgui/imgui_impl_win32.cpp"
#	include "imgui/imgui_impl_opengl2.cpp"
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

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

enum InputType
{
	Gamepad,
	Keyboard
};

typedef void (WINAPI *XInputEnableProc)(BOOL);
typedef DWORD (WINAPI *XInputGetStateProc)(DWORD, XINPUT_STATE*);

XInputGetStateProc	w32XInputGetState	= nullptr;
XInputEnableProc	w32XInputEnable 	= nullptr;

InputType dominantInputType = Gamepad;

char gameTitle[16] = {0};
constexpr uint32_t gbScreenWidth 			 	= 160;
constexpr uint32_t gbScreenHeight 			 	= 144;
constexpr uint32_t gbFrameTimeInMicroseconds 	= 16700;
uint16_t breakpointAddress					 	= 0;
uint32_t screenWidth 						 	= 1920;
uint32_t screenHeight 						 	= 1080;
GLuint gbScreenTexture 						 	= 0;
bool   showUi								 	= true;
uint32_t emulatorDeltaTimeInMicroseconds 		= 0u;
uint32_t deltaTimeInMicroseconds 				= 0u;
uint32_t uiDeltaTimeInMicroseconds 				= 0u;

uint8_t* pGameboyVideoBuffer 					= nullptr;

uint8_t directionInput							= 0x0F;
uint8_t buttonInput								= 0x0F;

GBEmulatorInstance* pEmulatorInstance 			= nullptr;
GBEmulatorJoypadState joypadState;

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

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
		return 60; 
	}

	return (uint16_t)displayInfo.dmDisplayFrequency;
}

void allocateDebugConsole()
{
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
}

void K15_WindowCreated(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_WindowClosed(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_KeyInput(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	dominantInputType = InputType::Keyboard;

	if( message == WM_KEYDOWN )
	{
		if((lparam & (1 >> 30)) == 0)
		{
			switch(wparam)
			{
				case VK_DOWN:
					joypadState.down = 1;
					break;
				case VK_UP:
					joypadState.up = 1;
					break;
				case VK_LEFT:
					joypadState.left = 1;
					break;
				case VK_RIGHT:
					joypadState.right = 1;
					break;

				case VK_CONTROL:
					joypadState.start = 1;
					break;
				case VK_MENU:
					joypadState.select = 1;
					break;
				case 'A':
					joypadState.a = 1;
					break;
				case 'S':
					joypadState.b = 1;
					break;

				case VK_F1:
					showUi = !showUi;
					break;
			}
		}
	}
	else if( message == WM_KEYUP )
	{
		switch(wparam)
		{
			case VK_DOWN:
				joypadState.down = 0;
				break;
			case VK_UP:
				joypadState.up = 0;
				break;
			case VK_LEFT:
				joypadState.left = 0;
				break;
			case VK_RIGHT:
				joypadState.right = 0;
				break;

			case VK_CONTROL:
				joypadState.start = 0;
				break;
			case VK_MENU:
				joypadState.select = 0;
				break;
			case 'A':
				joypadState.a = 0;
				break;
			case 'S':
				joypadState.b = 0;
				break;
		}
	}

	setGBEmulatorInstanceJoypadState( pEmulatorInstance, joypadState );
}

void K15_MouseButtonInput(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_MouseMove(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_MouseWheel(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_WindowResized(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	RECT clientRect = {0};
	GetClientRect(hwnd, &clientRect);

	screenWidth  = clientRect.right - clientRect.left;
	screenHeight = clientRect.bottom - clientRect.top;

	glViewport(0, 0, screenWidth, screenHeight);

	//FK: Requery monitor refresh rate in case the monitor changed
	const uint16_t monitorRefresRate = queryMonitorRefreshRate(hwnd);
	if( pEmulatorInstance != nullptr )
	{
		setGBEmulatorInstanceMonitorRefreshRate( pEmulatorInstance, monitorRefresRate );
	}
}

LRESULT CALLBACK K15_WNDPROC(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	uint8_t messageHandled = false;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
        return true;
#endif

	switch (message)
	{
	case WM_CREATE:
		K15_WindowCreated(hwnd, message, wparam, lparam);
		break;

	case WM_CLOSE:
		K15_WindowClosed(hwnd, message, wparam, lparam);
		PostQuitMessage(0);
		messageHandled = true;
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		K15_KeyInput(hwnd, message, wparam, lparam);
		break;

	case WM_SIZE:
		K15_WindowResized(hwnd, message, wparam, lparam);
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_XBUTTONUP:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		K15_MouseButtonInput(hwnd, message, wparam, lparam);
		break;

	case WM_MOUSEMOVE:
		K15_MouseMove(hwnd, message, wparam, lparam);
		break;

	case WM_MOUSEWHEEL:
		K15_MouseWheel(hwnd, message, wparam, lparam);
		break;
	}

	if (messageHandled == false)
	{
		return DefWindowProc(hwnd, message, wparam, lparam);
	}

	return false;
}

HWND setupWindow(HINSTANCE pInstance, int width, int height)
{
	WNDCLASS wndClass = {0};
	wndClass.style = CS_HREDRAW | CS_OWNDC | CS_VREDRAW;
	wndClass.hInstance = pInstance;
	wndClass.lpszClassName = "K15_Win32Template";
	wndClass.lpfnWndProc = K15_WNDPROC;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClass(&wndClass);

	HWND hwnd = CreateWindowA("K15_Win32Template", "Win32 Template",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		width, height, 0, 0, pInstance, 0);

	if (hwnd == INVALID_HANDLE_VALUE)
	{
		MessageBox(0, "Error creating Window.\n", "Error!", 0);
		return nullptr;
	}

	ShowWindow(hwnd, SW_SHOW);
	return hwnd;
}

void createOpenGLContext(HWND hwnd)
{
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
		32,                        //Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,                        //Number of bits for the depthbuffer
		8,                        //Number of bits for the stencilbuffer
		0,                        //Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	HDC mainDC = GetDC(hwnd);

	int pixelFormat = ChoosePixelFormat(mainDC, &pfd); 
	SetPixelFormat(mainDC,pixelFormat, &pfd);

	HGLRC glContext = wglCreateContext(mainDC);

	wglMakeCurrent(mainDC, glContext);
	
	wglSwapIntervalEXT = ( wglSwapIntervalEXTProc )wglGetProcAddress( "wglSwapIntervalEXT" );
	K15_RUNTIME_ASSERT( wglSwapIntervalEXT != nullptr );

	wglSwapIntervalEXT(1);

	//set default gl state
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}

const char* fixRomFileName( char* pRomFileName )
{
	//FK: This needs to be done purely because visual studio code wraps the argument in double quotes for whatever reason...
	while( *pRomFileName == '\"' )
	{
		++pRomFileName;
	}

	const char* pFixedRomFileName = pRomFileName;

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

const uint8_t* mapRomFile( const char* pRomFileName )
{
	char fixedRomFileName[MAX_PATH] = {0};
	strcpy_s( fixedRomFileName, pRomFileName );

	pRomFileName = fixRomFileName( fixedRomFileName );
	HANDLE pRomHandle = CreateFile( pRomFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0u, nullptr );
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

void queryXInputController()
{
	if( w32XInputGetState == nullptr )
	{
		return;
	}

	XINPUT_STATE state;
	const DWORD result = w32XInputGetState(0, &state);
	if( result != ERROR_SUCCESS )
	{
		return;
	}

	const WORD gamepadButtons = state.Gamepad.wButtons;

	if( dominantInputType != InputType::Gamepad && gamepadButtons == 0)
	{
		return;
	}

	dominantInputType = InputType::Gamepad;
	joypadState.a 		= ( gamepadButtons & XINPUT_GAMEPAD_A ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_B ) > 0;
	joypadState.b 		= ( gamepadButtons & XINPUT_GAMEPAD_X ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_Y ) > 0;
	joypadState.start 	= ( gamepadButtons & XINPUT_GAMEPAD_START ) > 0;
	joypadState.select 	= ( gamepadButtons & XINPUT_GAMEPAD_BACK ) > 0;
	joypadState.left 	= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_LEFT ) > 0;
	joypadState.right 	= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) > 0;
	joypadState.down 	= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_DOWN ) > 0;
	joypadState.up 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_UP ) > 0;

	setGBEmulatorInstanceJoypadState( pEmulatorInstance, joypadState );
}

void loadXInput()
{
	HMODULE pXInput = LoadLibraryA("xinput1_4.dll");
	if( pXInput == nullptr )
	{
		pXInput = LoadLibraryA("xinput1_3.dll");
	}

	if( pXInput == nullptr )
	{
		return;
	}

	w32XInputGetState = (XInputGetStateProc)GetProcAddress( pXInput, "XInputGetState");
	w32XInputEnable = (XInputEnableProc)GetProcAddress( pXInput, "XInputEnable");

	K15_RUNTIME_ASSERT(w32XInputGetState != nullptr);
	K15_RUNTIME_ASSERT(w32XInputEnable != nullptr);

	w32XInputEnable(TRUE);
}

void initializeImGui( HWND hwnd )
{
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL2_Init();
#endif
}

BOOL setup( HWND hwnd, LPSTR romPath )
{	
	loadXInput();
	createOpenGLContext( hwnd );

	const uint8_t* pRomData = mapRomFile( romPath );
	if( pRomData == nullptr )
	{
		return FALSE;
	}

	const uint16_t monitorRefreshRate = queryMonitorRefreshRate( hwnd );
	const size_t emulatorMemorySizeInBytes = calculateGBEmulatorInstanceMemoryRequirementsInBytes();

	//FK: at address 0x100000000 for better debugging of memory errors (eg: access violation > 0x100000000 indicated emulator instance memory is at fault)
	uint8_t* pEmulatorInstanceMemory = (uint8_t*)VirtualAlloc( ( LPVOID )0x100000000, emulatorMemorySizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	pEmulatorInstance = createGBEmulatorInstance(pEmulatorInstanceMemory);

	setGBEmulatorInstanceMonitorRefreshRate( pEmulatorInstance, monitorRefreshRate );
	loadGBEmulatorInstanceRom( pEmulatorInstance, pRomData );

	//FK: at address 0x200000000 for better debugging of memory errors (eg: access violation > 0x200000000 indicated emulator instance memory is at fault)
	pGameboyVideoBuffer = (uint8_t*)VirtualAlloc( ( LPVOID )0x200000000, (gbScreenHeight*gbScreenWidth*3), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );

	const GBCartridgeHeader header = getGBCartridgeHeader(pRomData);
	memcpy(gameTitle, header.gameTitle, sizeof(header.gameTitle) );

	glGenTextures(1, &gbScreenTexture);

	glBindTexture(GL_TEXTURE_2D, gbScreenTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);

	initializeImGui( hwnd );

	return TRUE;
}

void doFrame(HWND hwnd)
{
	char windowTitle[128];
	sprintf_s(windowTitle, sizeof(windowTitle), "%s - emulator: %.3f ms - ui: %.3f ms - %d hz monitor refresh rate", gameTitle, (float)deltaTimeInMicroseconds/1000.f, (float)uiDeltaTimeInMicroseconds/1000.f, pEmulatorInstance->monitorRefreshRate );
	SetWindowText(hwnd, (LPCSTR)windowTitle);

	const float pixelUnitH = 1.0f/(float)screenWidth;
	const float pixelUnitV = 1.0f/(float)screenHeight;
	const float centerH = pixelUnitH*(0.5f*(float)screenWidth);
	const float centerV = pixelUnitV*(0.5f*(float)screenHeight);

	const float gbSizeH = pixelUnitH*gbScreenWidth;
	const float gbSizeV = pixelUnitV*gbScreenHeight;

	//FK: division using integer to keep integer scaling (minus 2 to not fill out the whole screen just yet)
	const float scale 		= (float)(screenHeight / gbVerticalResolutionInPixels) - 2;

	const float left  		= -gbSizeH * scale;
	const float right 		= +gbSizeH * scale;
	const float top	  		= +gbSizeV * scale;
	const float bottom	    = -gbSizeV * scale;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gbScreenTexture);

	glBegin(GL_TRIANGLES);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(left, bottom);

		glTexCoord2f(1.0f, 1.0f);
		glVertex2f(right, bottom);

		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(right, top);

		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(right, top);

		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(left, top);

		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(left, bottom);
	glEnd();

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif

	HDC deviceContext = GetDC(hwnd);
	SwapBuffers(deviceContext);

	glClear(GL_COLOR_BUFFER_BIT);
}

int CALLBACK WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nShowCmd)
{
	allocateDebugConsole();

	HWND hwnd = setupWindow(hInstance, screenWidth, screenHeight);

	if (hwnd == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	setup( hwnd, lpCmdLine );

	uint8_t loopRunning = true;
	MSG msg = {0};

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

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

		queryXInputController();

		QueryPerformanceCounter(&start);
		runGBEmulatorInstance( pEmulatorInstance );

		if( hasGBEmulatorInstanceHitVBlank( pEmulatorInstance ) )
		{
			const uint8_t* pFrameBuffer = getGBEmulatorInstanceFrameBuffer( pEmulatorInstance );
			convertGBFrameBufferToRGB8Buffer( pGameboyVideoBuffer, pFrameBuffer );

			//FK: GB frame finished, upload gb framebuffer to texture
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);
		}

		QueryPerformanceCounter(&uiStart);

		//FK: UI
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
		{
			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplWin32_NewFrame();

		    ImGui::NewFrame();
			if( showUi )
			{
				doUiFrame( pEmulatorInstance );
			}
			ImGui::EndFrame();
		}
#endif

		QueryPerformanceCounter(&end);
	
		doFrame(hwnd);

		deltaTimeInMicroseconds 	= (uint32_t)(((uiStart.QuadPart-start.QuadPart)*1000000)/freq.QuadPart);
		uiDeltaTimeInMicroseconds 	= (uint32_t)(((end.QuadPart-uiStart.QuadPart)*1000000)/freq.QuadPart);
	}

	DestroyWindow(hwnd);

	return 0;
}