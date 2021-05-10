#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <gl/GL.h>

#include "imgui/imgui.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_impl_opengl2.cpp"

#include "k15_gb_emulator.h"
#include "k15_gb_emulator_ui.cpp"

typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
typedef const char *(WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC) (void);

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

constexpr uint32_t gbScreenWidth 	= 160;
constexpr uint32_t gbScreenHeight 	= 144;
uint32_t screenWidth 				= 1024;
uint32_t screenHeight 				= 768;
GLuint gbScreenTexture 				= 0;

uint8_t* pGameboyVideoBuffer 		= nullptr;
HANDLE gbThreadHandle 				= INVALID_HANDLE_VALUE;
HANDLE gbFrameSyncEvent 			= INVALID_HANDLE_VALUE;
HANDLE gbFrameFinishedEvent 		= INVALID_HANDLE_VALUE;
HANDLE gbEmulatorResetEvent			= INVALID_HANDLE_VALUE;

uint8_t directionInput				= 0x0F;
uint8_t buttonInput					= 0x0F;

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

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
	GBEmulatorJoypad joypad;

	if( message == WM_KEYDOWN )
	{
		if((lparam & (1 >> 30)) == 0)
		{
			switch(wparam)
			{
				case VK_DOWN:
					joypad.down = 1;
					break;
				case VK_UP:
					joypad.up = 1;
					break;
				case VK_LEFT:
					joypad.left = 1;
					break;
				case VK_RIGHT:
					joypad.right = 1;
					break;

				case VK_CONTROL:
					joypad.start = 1;
					break;
				case VK_MENU:
					joypad.select = 1;
					break;
				case 'A':
					joypad.a = 1;
					break;
				case 'S':
					joypad.b = 1;
					break;
			}
		}
	}
	else if( message == WM_KEYUP )
	{
		switch(wparam)
		{
			case VK_DOWN:
				joypad.down = 0;
				break;
			case VK_UP:
				joypad.up = 0;
				break;
			case VK_LEFT:
				joypad.left = 0;
				break;
			case VK_RIGHT:
				joypad.right = 0;
				break;

			case VK_CONTROL:
				joypad.start = 0;
				break;
			case VK_MENU:
				joypad.select = 0;
				break;
			case 'A':
				joypad.a = 0;
				break;
			case 'S':
				joypad.b = 0;
				break;
		}
	}

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
		MessageBox(0, "Error creating Window.\n", "Error!", 0);
	else
		ShowWindow(hwnd, SW_SHOW);
	return hwnd;
}

void renderGbFrameBuffer(const uint8_t* pFrameBuffer)
{
	//FK: greenish hue of gameboy lcd
	constexpr float gbRGBMapping[3] = {
		(float)0xB0,
		(float)0xCE,
		(float)0x48,
	};

	const float pixelIntensity[4] = {
		1.0f, 
		0.75f,
		0.5f,
		0.25f
	};

	int x = 0;
	int y = 0;

	#if 1

	for( size_t y = 0; y < gbVerticalResolutionInPixels; ++y )
	{
		for( size_t x = 0; x < gbHorizontalResolutionInPixels; x += 4 )
		{
			const size_t frameBufferPixelIndex 	= (x + y*gbHorizontalResolutionInPixels)/4;
			const uint8_t pixels	 			= pFrameBuffer[frameBufferPixelIndex];
			
			for( uint8_t pixelIndex = 0; pixelIndex < 4; ++pixelIndex )
			{
				const uint8_t pixelValue = pixels >> ( ( 3 - pixelIndex ) * 2 ) & 0x3;

				//FK: Map gameboy pixels to rgb pixels
				const float intensity = pixelIntensity[ pixelValue ];
				const float r = intensity * gbRGBMapping[0];
				const float g = intensity * gbRGBMapping[1];
				const float b = intensity * gbRGBMapping[2];

				const size_t videoBufferPixelIndex = ( x + pixelIndex + y * gbHorizontalResolutionInPixels ) * 3;
				pGameboyVideoBuffer[videoBufferPixelIndex + 0] = (uint8_t)r;
				pGameboyVideoBuffer[videoBufferPixelIndex + 1] = (uint8_t)g;
				pGameboyVideoBuffer[videoBufferPixelIndex + 2] = (uint8_t)b;
			}
		}
	}

	#endif
}

GBEmulatorInstance* pEmulatorInstance = nullptr;

DWORD WINAPI emulatorThreadEntryPoint(LPVOID pParameter)
{
	const size_t emulatorMemorySizeInBytes = calculateEmulatorInstanceMemoryRequirementsInBytes();

	uint8_t* pRomData = (uint8_t*)pParameter;
	uint8_t* pEmulatorInstanceMemory = (uint8_t*)malloc(emulatorMemorySizeInBytes);
	pEmulatorInstance = createEmulatorInstance(pEmulatorInstanceMemory);

	loadRom( pEmulatorInstance, pRomData );

	uint32_t totalCycleCount = 0;

	while(true)
	{
		const uint8_t cycleCount = runSingleInstruction( pEmulatorInstance );
		if( cycleCount == 0 )
		{
			__debugbreak();
		}

		const DWORD waitResult = WaitForSingleObject(gbEmulatorResetEvent, 0);
		if( waitResult == WAIT_OBJECT_0 )
		{
			resetEmulatorInstance( pEmulatorInstance );
			totalCycleCount = 0;
			continue;
		}

		totalCycleCount += cycleCount;
		if( totalCycleCount >= gbCyclerPerFrame )
		{
			totalCycleCount -= gbCyclerPerFrame;

			renderGbFrameBuffer( pEmulatorInstance->pPpuState->pGBFrameBuffer );

			//FK: Tell main thread that the GB frame is finished
			SetEvent(gbFrameFinishedEvent);

			//FK: Wait for main thread to render latest frame so that frame buffer can be written to
			WaitForSingleObject(gbFrameSyncEvent, INFINITE);
		}
	}
}

void setup(HWND hwnd)
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
	
	//FK: it is assumed that these extension are always available
	PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");;
	PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");;
	assert(wglSwapIntervalEXT != nullptr);
	assert(wglGetSwapIntervalEXT != nullptr);

	//FK: Enable vsync
	wglSwapIntervalEXT(1);

	//set default gl state
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	HANDLE pRomHandle = CreateFile("rom.gb", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0u, nullptr);
	if( pRomHandle == INVALID_HANDLE_VALUE )
	{
		printf("Could not open 'rom.gb'.\n");
		return;
	}

	HANDLE pRomMapping = CreateFileMapping( pRomHandle, nullptr, PAGE_READONLY, 0u, 0u, nullptr );
	if( pRomMapping == INVALID_HANDLE_VALUE )
	{
		printf("Could not map 'rom.gb'.\n");
		return;
	}

	uint8_t* pRomData = (uint8_t*)MapViewOfFile( pRomMapping, FILE_MAP_READ, 0u, 0u, 0u );

	gbFrameFinishedEvent = CreateEvent(nullptr, FALSE, FALSE, "GameBoyFrameFinishedEvent");
	assert( gbFrameFinishedEvent != INVALID_HANDLE_VALUE );

	gbFrameSyncEvent = CreateEvent(nullptr, FALSE, FALSE, "GameBoyFrameSyncEvent");
	assert( gbFrameSyncEvent != INVALID_HANDLE_VALUE );

	gbEmulatorResetEvent = CreateEvent(nullptr, FALSE, FALSE, "GameBoyResetEvent");
	assert( gbEmulatorResetEvent != INVALID_HANDLE_VALUE );

	gbThreadHandle = CreateThread( nullptr, 0, emulatorThreadEntryPoint, pRomData, 0, nullptr );
	assert( gbThreadHandle != INVALID_HANDLE_VALUE );
	
	pGameboyVideoBuffer = (uint8_t*)malloc(gbScreenHeight*gbScreenWidth*3);

	const GBRomHeader header = getGBRomHeader(pRomData);
	SetWindowText(hwnd, (LPCSTR)header.gameTitle);

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
}

void doFrame(HWND hwnd)
{
	const float pixelUnitH = 1.0f/(float)screenWidth;
	const float pixelUnitV = 1.0f/(float)screenHeight;
	const float centerH = pixelUnitH*(0.5f*(float)screenWidth);
	const float centerV = pixelUnitV*(0.5f*(float)screenHeight);

	const float gbSizeH = pixelUnitH*gbScreenWidth;
	const float gbSizeV = pixelUnitV*gbScreenHeight;

	const float scale = 4.0f;

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

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	HDC deviceContext = GetDC(hwnd);
	SwapBuffers(deviceContext);

	glClear(GL_COLOR_BUFFER_BIT);
	SetEvent(gbFrameSyncEvent);
}

int CALLBACK WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nShowCmd)
{
	LARGE_INTEGER performanceFrequency;
	QueryPerformanceFrequency(&performanceFrequency);

	allocateDebugConsole();

	HWND hwnd = setupWindow(hInstance, screenWidth, screenHeight);

	if (hwnd == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	setup(hwnd);

	uint8_t loopRunning = true;
	MSG msg = {0};

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


		const DWORD waitResult = WaitForSingleObject(gbFrameFinishedEvent, 0);
		if( waitResult == WAIT_OBJECT_0 )
		{
			//FK: GB frame finished, upload gb framebuffer to texture
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);
		}

		//FK: UI
		{
			GBUiData uiData = {};
			ImGui_ImplOpenGL2_NewFrame();
			ImGui_ImplWin32_NewFrame();
			doUiFrame(pEmulatorInstance, &uiData);

			if( uiData.resetEmulator )
			{
				SetEvent( gbEmulatorResetEvent );
			}
		}
	
		doFrame(hwnd);
	}

	DestroyWindow(hwnd);

	return 0;
}