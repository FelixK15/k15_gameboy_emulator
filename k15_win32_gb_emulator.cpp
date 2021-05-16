#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
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

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
#	include "k15_gb_emulator_ui.cpp"
#endif

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

char gameTitle[16] = {0};
constexpr uint32_t gbScreenWidth 			 	= 160;
constexpr uint32_t gbScreenHeight 			 	= 144;
constexpr uint32_t gbFrameTimeInMilliseconds 	= 16;
uint16_t breakpointAddress					 	= 0;
uint32_t screenWidth 						 	= 1920;
uint32_t screenHeight 						 	= 1080;
GLuint gbScreenTexture 						 	= 0;
bool   showUi								 	= true;
uint32_t deltaTimeInMilliSeconds 				= 0u;

uint8_t* pGameboyVideoBuffer 					= nullptr;

uint8_t directionInput							= 0x0F;
uint8_t buttonInput								= 0x0F;

GBEmulatorInstance* pEmulatorInstance 			= nullptr;

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
	GBEmulatorJoypadState joypadState;

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

	setEmulatorJoypadState( pEmulatorInstance, joypadState );
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

	const size_t emulatorMemorySizeInBytes = calculateEmulatorInstanceMemoryRequirementsInBytes();

	uint8_t* pEmulatorInstanceMemory = (uint8_t*)malloc(emulatorMemorySizeInBytes);
	pEmulatorInstance = createEmulatorInstance(pEmulatorInstanceMemory);

	uint8_t* pRomData = (uint8_t*)MapViewOfFile( pRomMapping, FILE_MAP_READ, 0u, 0u, 0u );

	pGameboyVideoBuffer = (uint8_t*)malloc(gbScreenHeight*gbScreenWidth*3);

	const GBRomHeader header = getGBRomHeader(pRomData);
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

	loadRom( pEmulatorInstance, pRomData );
}

void doFrame(HWND hwnd)
{
	char windowTitle[64];
	sprintf_s(windowTitle, sizeof(windowTitle), "%s - emulator: %d ms", gameTitle, deltaTimeInMilliSeconds );
	SetWindowText(hwnd, (LPCSTR)windowTitle);

	const float pixelUnitH = 1.0f/(float)screenWidth;
	const float pixelUnitV = 1.0f/(float)screenHeight;
	const float centerH = pixelUnitH*(0.5f*(float)screenWidth);
	const float centerV = pixelUnitV*(0.5f*(float)screenHeight);

	const float gbSizeH = pixelUnitH*gbScreenWidth;
	const float gbSizeV = pixelUnitV*gbScreenHeight;

	const float scale = 5.0f;

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

	setup(hwnd);

	uint8_t loopRunning = true;
	MSG msg = {0};

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	LARGE_INTEGER start;
	LARGE_INTEGER end;

	start.QuadPart = 0;
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
		runEmulator( pEmulatorInstance );

		if( pEmulatorInstance->flags.vblank )
		{
			renderGbFrameBuffer( pEmulatorInstance->pPpuState->pGBFrameBuffer );
			//FK: GB frame finished, upload gb framebuffer to texture
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);
		}

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
	
		doFrame(hwnd);

		QueryPerformanceCounter(&end);
		deltaTimeInMilliSeconds = (uint32_t)(((end.QuadPart-start.QuadPart)*1000)/freq.QuadPart);

		if( deltaTimeInMilliSeconds < gbFrameTimeInMilliseconds )
		{
			const uint32_t sleepTimeInMilliSeconds = gbFrameTimeInMilliseconds - deltaTimeInMilliSeconds;
			Sleep(sleepTimeInMilliSeconds);
		}
	}

	DestroyWindow(hwnd);

	return 0;
}