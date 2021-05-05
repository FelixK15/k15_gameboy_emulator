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

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")

#define K15_FALSE 0
#define K15_TRUE 1

typedef unsigned char bool8;
typedef unsigned char byte;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

void printErrorToFile(const char* pFileName)
{
	DWORD errorId = GetLastError();
	char* textBuffer = 0;
	DWORD writtenChars = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, errorId, 
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPSTR)&textBuffer, 512, 0);

	if (writtenChars > 0)
	{
		FILE* file = fopen(pFileName, "w");

		if (file)
		{
			fwrite(textBuffer, writtenChars, 1, file);			
			fflush(file);
			fclose(file);
		}
	}
}

void allocateDebugConsole()
{
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
}

uint32 screenWidth = 1024;
uint32 screenHeight = 768;
uint32 timePerFrameInMS = 16;

GLuint gbScreenTexture = 0;
uint32 gbScreenWidth = 160;
uint32 gbScreenHeight = 144;
uint8_t* pGameboyVideoBuffer = nullptr;

void K15_WindowCreated(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_WindowClosed(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

}

void K15_KeyInput(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{

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

void K15_WindowResized(HWND hwnd, UINT p_Messaeg, WPARAM wparam, LPARAM lparam)
{
	RECT clientRect = {0};
	GetClientRect(hwnd, &clientRect);

	screenWidth  = clientRect.right - clientRect.left;
	screenHeight = clientRect.bottom - clientRect.top;

	glViewport(0, 0, screenWidth, screenHeight);
}

LRESULT CALLBACK K15_WNDPROC(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	bool8 messageHandled = K15_FALSE;

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
		messageHandled = K15_TRUE;
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

	if (messageHandled == K15_FALSE)
	{
		return DefWindowProc(hwnd, message, wparam, lparam);
	}

	return 0;
}

HWND setupWindow(HINSTANCE p_Instance, int p_Width, int p_Height)
{
	WNDCLASS wndClass = {0};
	wndClass.style = CS_HREDRAW | CS_OWNDC | CS_VREDRAW;
	wndClass.hInstance = p_Instance;
	wndClass.lpszClassName = "K15_Win32Template";
	wndClass.lpfnWndProc = K15_WNDPROC;
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClass(&wndClass);

	HWND hwnd = CreateWindowA("K15_Win32Template", "Win32 Template",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		p_Width, p_Height, 0, 0, p_Instance, 0);

	if (hwnd == INVALID_HANDLE_VALUE)
		MessageBox(0, "Error creating Window.\n", "Error!", 0);
	else
		ShowWindow(hwnd, SW_SHOW);
	return hwnd;
}

uint32 getTimeInMilliseconds(LARGE_INTEGER p_PerformanceFrequency)
{
	LARGE_INTEGER appTime = {0};
	QueryPerformanceCounter(&appTime);

	appTime.QuadPart *= 1000; //to milliseconds

	return (uint32)(appTime.QuadPart / p_PerformanceFrequency.QuadPart);
}

uint8_t* pRomData = nullptr;
GBEmulatorInstance* pEmulatorInstance = nullptr;

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

	pGameboyVideoBuffer = (uint8*)malloc(gbScreenHeight*gbScreenWidth*3);

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

	pRomData = (uint8_t*)MapViewOfFile( pRomMapping, FILE_MAP_READ, 0u, 0u, 0u );
	
	const size_t emulatorMemorySizeInBytes = calculateEmulatorInstanceMemoryRequirementsInBytes();
	uint8_t* pEmulatorInstanceMemory = (uint8_t*)malloc(emulatorMemorySizeInBytes);
	pEmulatorInstance = createEmulatorInstance(pEmulatorInstanceMemory);

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

void doFrame(uint32 p_DeltaTimeInMS, HWND hwnd)
{
	#if 0
	glBegin(GL_TRIANGLES);
		glColor3f(1.0, 0.0, 0.0);
		glVertex3f(1.0f, -1.0f, 0.0f);

		glColor3f(0.0, 1.0f, 0.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);

		glColor3f(0.0, 0.0, 1.0);
		glVertex3f(-1.0f, -1.0f, 0.0f);
	glEnd();
	#endif

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gbScreenTexture);

#if 1
const float pixelUnitH = 1.0f/(float)screenWidth;
const float pixelUnitV = 1.0f/(float)screenHeight;
const float centerH = pixelUnitH*(0.5f*(float)screenWidth);
const float centerV = pixelUnitV*(0.5f*(float)screenHeight);

const float gbSizeH = pixelUnitH*gbScreenWidth;
const float gbSizeV = pixelUnitV*gbScreenHeight;

const float scale = 4.0f;

const float left  = -gbSizeH * 0.5f * scale;
const float right = +gbSizeH * 0.5f * scale;
const float top	  = +gbSizeV * 0.5f * scale;
const float bottom	  = -gbSizeV * 0.5f * scale;

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
	#endif

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	HDC deviceContext = GetDC(hwnd);
	SwapBuffers(deviceContext);

	glClear(GL_COLOR_BUFFER_BIT);
}

void renderVideoRam(const uint8_t* pVideoRam, const uint8_t* pBG)
{
	const uint32_t pixelMapping[4] = {
		RGB(255,255,255),
		RGB(192, 192, 192),
		RGB(128, 128, 128),
		RGB(64, 64, 64),
	};
	const size_t tileDataTableSizeInBytes = 0x1000;

	int x = 0;
	int y = 0;

	for( size_t tileIndex = 0; tileIndex < tileDataTableSizeInBytes; )
	{
		for( size_t pixely = 0; pixely < 8; ++pixely )
		{
				const uint8_t lineValues[2] = {
					pVideoRam[ tileIndex + 0 ],
					pVideoRam[ tileIndex + 1 ]
				};

				const uint8_t colorIdBitsL[8] = {
					(uint8_t)(lineValues[0] >> 7 & 0x1),
					(uint8_t)(lineValues[0] >> 6 & 0x1),
					(uint8_t)(lineValues[0] >> 5 & 0x1),
					(uint8_t)(lineValues[0] >> 4 & 0x1),
					(uint8_t)(lineValues[0] >> 3 & 0x1),
					(uint8_t)(lineValues[0] >> 2 & 0x1),
					(uint8_t)(lineValues[0] >> 1 & 0x1),
					(uint8_t)(lineValues[0] >> 0 & 0x1)
				};

				const uint8_t colorIdBitsR[8] = {
					(uint8_t)(lineValues[1] >> 7 & 0x1),
					(uint8_t)(lineValues[1] >> 6 & 0x1),
					(uint8_t)(lineValues[1] >> 5 & 0x1),
					(uint8_t)(lineValues[1] >> 4 & 0x1),
					(uint8_t)(lineValues[1] >> 3 & 0x1),
					(uint8_t)(lineValues[1] >> 2 & 0x1),
					(uint8_t)(lineValues[1] >> 1 & 0x1),
					(uint8_t)(lineValues[1] >> 0 & 0x1)
				};

				const uint8_t colorIds[8] = {
					(uint8_t)(colorIdBitsL[0] << 0 | colorIdBitsR[0] << 1),
					(uint8_t)(colorIdBitsL[1] << 0 | colorIdBitsR[1] << 1),
					(uint8_t)(colorIdBitsL[2] << 0 | colorIdBitsR[2] << 1),
					(uint8_t)(colorIdBitsL[3] << 0 | colorIdBitsR[3] << 1),
					(uint8_t)(colorIdBitsL[4] << 0 | colorIdBitsR[4] << 1),
					(uint8_t)(colorIdBitsL[5] << 0 | colorIdBitsR[5] << 1),
					(uint8_t)(colorIdBitsL[6] << 0 | colorIdBitsR[6] << 1),
					(uint8_t)(colorIdBitsL[7] << 0 | colorIdBitsR[7] << 1)
				};

				for( size_t pixelIndex = 0; pixelIndex < 8; ++pixelIndex )
				{
					const uint32_t vbIndex = x + pixelIndex + (y+pixely)*gbScreenWidth;

					const uint32_t pixel = pixelMapping[ colorIds[ pixelIndex ] ];
					pGameboyVideoBuffer[vbIndex * 3 + 0] = pixel >> 0;
					pGameboyVideoBuffer[vbIndex * 3 + 1] = pixel >> 8;
					pGameboyVideoBuffer[vbIndex * 3 + 2] = pixel >> 16;
				}
				
				tileIndex += 2;
		}

		x+=8;
		if( x==8*16)
		{
			x=0;
			y+=8;
		}
	}
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

	uint32 timeFrameStarted = 0;
	uint32 timeFrameEnded = 0;
	uint32 deltaMs = 0;

	bool8 loopRunning = K15_TRUE;
	MSG msg = {0};

	startEmulation( pEmulatorInstance, pRomData );

	uint32_t totalCycleCount = 0;

	while (loopRunning)
	{
		ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		timeFrameStarted = getTimeInMilliseconds(performanceFrequency);

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
		{
			if (msg.message == WM_QUIT)
			{
				loopRunning = K15_FALSE;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!loopRunning)
		{
			break;
		}

		const uint8_t cycleCount = runSingleInstruction( pEmulatorInstance );
		tickPPU( pEmulatorInstance, cycleCount );
	
		if( cycleCount == 0 )
		{
			__debugbreak();
		}

		totalCycleCount += cycleCount;
		if( totalCycleCount >= 70224 )
		{
			totalCycleCount -= 70224;
			const uint8_t* pVideoRam = pEmulatorInstance->pMemoryMapper->pVideoRAM;
			const uint8_t* pBG = pEmulatorInstance->pPpuState->pBackgroundOrWindowTiles[ 0 ];
			renderVideoRam( pVideoRam, pBG );
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, gbScreenWidth, gbScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pGameboyVideoBuffer);
		}

		ImGui::EndFrame();

		timeFrameEnded = getTimeInMilliseconds(performanceFrequency);
		deltaMs += timeFrameEnded - timeFrameStarted;

		if( deltaMs >= 16 )
		{
			doFrame( deltaMs, hwnd );
			deltaMs = 0;
		}
	}

	DestroyWindow(hwnd);

	return 0;
}