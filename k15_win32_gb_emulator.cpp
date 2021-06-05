#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <gl/GL.h>

#include "k15_gb_emulator.h"

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
#	include "imgui/imgui.cpp"
#	include "imgui/imgui_demo.cpp"
#	include "imgui/imgui_draw.cpp"
#	include "imgui/imgui_tables.cpp"
#	include "imgui/imgui_widgets.cpp"
#	include "imgui/imgui_impl_win32.cpp"
#	include "imgui/imgui_impl_opengl2.cpp"
#	include "k15_gb_emulator_ui.cpp"
#endif

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

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
	if( GetMonitorInfo(monitorHandle, &monitorInfo) == FALSE )
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

	if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
        return true;

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
	
	//set default gl state
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
}

const uint8_t* mapRomFile( const char* pRomFileName )
{
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

void setup( HWND hwnd )
{	
	createOpenGLContext( hwnd );
	//const uint8_t* pRomData = mapRomFile( "cpu_instrs.gb" );
	//const uint8_t* pRomData = mapRomFile( "BattleCity (Japan).gb" );
	//const uint8_t* pRomData = mapRomFile( "Super Mario Land (World).gb" );
	const uint8_t* pRomData = mapRomFile( "Bomb Jack (Europe).gb" );
	//const uint8_t* pRomData = mapRomFile( "Alleyway (World).gb" );
	//const uint8_t* pRomData = mapRomFile( "Tetris (Japan) (En).gb" );
	if( pRomData == nullptr )
	{
		return;
	}

	const uint16_t monitorRefreshRate = queryMonitorRefreshRate( hwnd );
	const size_t emulatorMemorySizeInBytes = calculateGBEmulatorInstanceMemoryRequirementsInBytes();

	uint8_t* pEmulatorInstanceMemory = (uint8_t*)malloc(emulatorMemorySizeInBytes);
	pEmulatorInstance = createGBEmulatorInstance(pEmulatorInstanceMemory);
	setGBEmulatorInstanceMonitorRefreshRate( pEmulatorInstance, monitorRefreshRate );

	pGameboyVideoBuffer = (uint8_t*)malloc(gbScreenHeight*gbScreenWidth*3);

	const GBCartridgeHeader header = getGBCartridgeHeader(pRomData);
	memcpy(gameTitle, header.gameTitle, sizeof(header.gameTitle) );

	glGenTextures(1, &gbScreenTexture);

	glBindTexture(GL_TEXTURE_2D, gbScreenTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL2_Init();

	loadGBEmulatorInstanceRom( pEmulatorInstance, pRomData );
}

void doFrame(HWND hwnd)
{
	char windowTitle[64];
	sprintf_s(windowTitle, sizeof(windowTitle), "%s - emulator: %.3f ms - ui: %.3f", gameTitle, (float)deltaTimeInMicroseconds/1000.f, (float)uiDeltaTimeInMicroseconds/1000.f );
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

void testCompression()
{
	uint8_t testData[128];
	memset(testData + 0, 1, 20);
	memset(testData + 20, 2, 40);
	memset(testData + 60, 3, 68);

	const size_t compressedTestSizeInBytes = calculateCompressedMemorySizeRLE( testData, sizeof( testData ) );
	uint8_t* pCompressedBuffer = (uint8_t*)malloc( compressedTestSizeInBytes );
	compressMemoryBlockRLE(pCompressedBuffer, testData, sizeof( testData ));
	memset( testData, 0, sizeof( testData ) );
	if(!uncompressMemoryBlockRLE( testData, pCompressedBuffer ))
	{
		printf("Compressed failed! FourCC check failed.\n");
		return;
	}

	for( size_t i = 0; i < 20; ++ i)
	{
		if( testData[i] != 1 )
		{
			printf("Compressed failed, test data is not the same as before compression.\n");
			return;
		}
	}

	for( size_t i = 20; i < 60; ++ i)
	{
		if( testData[i] != 2 )
		{
			printf("Compressed failed, test data is not the same as before compression.\n");
			return;
		}
	}

	for( size_t i = 60; i < 128; ++ i)
	{
		if( testData[i] != 3 )
		{
			printf("Compressed failed, test data is not the same as before compression.\n");
			return;
		}
	}

	printf("Compression test successful!\n");
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

	//FK: only for testing, duh
	//testCompression();

	setup(hwnd);

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