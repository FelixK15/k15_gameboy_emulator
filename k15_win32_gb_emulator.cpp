#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>

#include "k15_gb_emulator.h"

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

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

void resizeBackbuffer(HWND hwnd, uint32 p_Width, uint32 p_Height);

HDC backbufferDC = 0;
HBITMAP backbufferBitmap = 0;
uint32 screenWidth = 1024;
uint32 screenHeight = 768;
uint32 timePerFrameInMS = 16;
uint32_t* pBackbuffer = nullptr;

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
	WORD newWidth = (WORD)(lparam);
	WORD newHeight = (WORD)(lparam >> 16);

	resizeBackbuffer(hwnd, newWidth, newHeight);
}

LRESULT CALLBACK K15_WNDPROC(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	bool8 messageHandled = K15_FALSE;

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

void resizeBackbuffer(HWND hwnd, uint32 p_Width, uint32 p_Height)
{
	DeleteObject(backbufferBitmap);
	DeleteDC(backbufferDC);

	BITMAPINFO info;
	memset(&info, 0, sizeof(info));
	info.bmiHeader.biSize = sizeof(info.bmiHeader);
	info.bmiHeader.biWidth = screenWidth;
	info.bmiHeader.biHeight = screenHeight;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	info.bmiHeader.biHeight = -info.bmiHeader.biHeight;

	HDC originalDC = GetDC(hwnd);
	backbufferDC = CreateCompatibleDC(originalDC);
	backbufferBitmap = CreateDIBSection(backbufferDC, &info, DIB_RGB_COLORS, (VOID**)&pBackbuffer, nullptr, 0u);

	screenWidth = p_Width;
	screenHeight = p_Height;
	
	SelectObject(backbufferDC, backbufferBitmap);
}

uint8_t* pRomData = nullptr;
GBEmulatorInstance* pEmulatorInstance = nullptr;

void setup(HWND hwnd)
{	
	resizeBackbuffer(hwnd, screenWidth, screenHeight);

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
}

void swapBuffers(HWND hwnd)
{
	HDC originalDC = GetDC(hwnd);
	
	//blit to front buffer
	BitBlt(originalDC, 0, 0, screenWidth, screenHeight, backbufferDC, 0, 0, SRCCOPY);

	//clear backbuffer
	BitBlt(backbufferDC, 0, 0, screenWidth, screenHeight, backbufferDC, 0, 0, BLACKNESS);
}

void drawDeltaTime(uint32 p_DeltaTimeInMS)
{
	RECT textRect;
	textRect.left = 70;
	textRect.top = 70;
	textRect.bottom = screenHeight;
	textRect.right = screenWidth;

	char messageBuffer[64];
	SetTextColor(backbufferDC, RGB(255, 255, 255));
	SetBkColor(backbufferDC, RGB(0, 0, 0));

	sprintf_s(messageBuffer, 64, "MS: %d", p_DeltaTimeInMS);
	DrawTextA(backbufferDC, messageBuffer, -1, &textRect, DT_LEFT | DT_TOP);
}

void doFrame(uint32 p_DeltaTimeInMS, HWND hwnd)
{
	drawDeltaTime(p_DeltaTimeInMS);
	swapBuffers(hwnd);
}

void renderVideoRam(const uint8_t* pVideoRam)
{
	const uint32_t pixelMapping[4] = {
		RGB(255,255,255),
		RGB(192, 192, 192),
		RGB(128, 128, 128),
		RGB(64, 64, 64),
	};
	const size_t videoRamSizeInBytes = Kbyte(8);
	const size_t tileDataTableSizeInBytes = 0x1000;

	int x = 0;
	int y = 0;

	for( size_t tileDataByteIndex = 0; tileDataByteIndex < tileDataTableSizeInBytes; )
	{
		for( int tileY = 0; tileY < 8; ++tileY )
		{
			const uint16_t tileData = ( pVideoRam[tileDataByteIndex + tileY * 2 + 0] << 0 | 
										pVideoRam[tileDataByteIndex + tileY * 2 + 1] << 8 );

			pBackbuffer[x + 0 + (y + tileY) * screenWidth] = pixelMapping[((tileData <<  0) & 0x3)];
			pBackbuffer[x + 1 + (y + tileY) * screenWidth] = pixelMapping[((tileData <<  2) & 0x3)];
			pBackbuffer[x + 2 + (y + tileY) * screenWidth] = pixelMapping[((tileData <<  4) & 0x3)];
			pBackbuffer[x + 3 + (y + tileY) * screenWidth] = pixelMapping[((tileData <<  6) & 0x3)];
			pBackbuffer[x + 4 + (y + tileY) * screenWidth] = pixelMapping[((tileData <<  8) & 0x3)];
			pBackbuffer[x + 5 + (y + tileY) * screenWidth] = pixelMapping[((tileData << 10) & 0x3)];
			pBackbuffer[x + 6 + (y + tileY) * screenWidth] = pixelMapping[((tileData << 12) & 0x3)];
			pBackbuffer[x + 7 + (y + tileY) * screenWidth] = pixelMapping[((tileData << 14) & 0x3)];
		}

		x += 8;
		if( x == 256 )
		{
			x = 0;
			y += 8;
		}
		tileDataByteIndex += 16;
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

	while (loopRunning)
	{
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

		const uint8_t* pVideoRam = getVideoRam(pEmulatorInstance);
		renderVideoRam(pVideoRam);
		swapBuffers(hwnd);
	}

	DestroyWindow(hwnd);

	return 0;
}