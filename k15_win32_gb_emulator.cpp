#define _CRT_SECURE_NO_WARNINGS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

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

struct tagOFNA;
typedef tagOFNA OPENFILENAMEA;

DECLARE_HANDLE(HDROP);

typedef int			(WINAPI *PFNCHOOSEPIXELFORMATPROC)(HDC hdc, CONST PIXELFORMATDESCRIPTOR* ppfd);
typedef BOOL		(WINAPI *PFNSETPIXELFORMATPROC)(HDC hdc, int pixelFormat, CONST PIXELFORMATDESCRIPTOR* ppfd);
typedef BOOL		(WINAPI *PFNSWAPBUFFERSPROC)(HDC hdc);
typedef DWORD 		(WINAPI *PFNXINPUTGETSTATEPROC)(DWORD, XINPUT_STATE*);
typedef void 		(WINAPI *PFNPATHSTRIPPATHAPROC)(LPSTR pszPath);
typedef BOOL 		(WINAPI *PFNGETOPENFILENAMEAPROC)(OPENFILENAMEA*);
typedef UINT 		(WINAPI *PFNDRAGQUERYFILEAPROC)(HDROP hDrop, UINT iFile, LPSTR lpszFile, UINT cch);
typedef void 		(WINAPI *PFNDRAGFINISHPROC)(HDROP hDrop);
typedef UINT		(WINAPI *PFNNTDELAYEXECUTIONPROC)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);

PFNXINPUTGETSTATEPROC		w32XInputGetState 		= nullptr;
PFNCHOOSEPIXELFORMATPROC 	w32ChoosePixelFormat 	= nullptr;
PFNSETPIXELFORMATPROC 	 	w32SetPixelFormat 		= nullptr;
PFNSWAPBUFFERSPROC 		 	w32SwapBuffers 			= nullptr;
PFNGETOPENFILENAMEAPROC 	w32GetOpenFileNameA 	= nullptr;
PFNPATHSTRIPPATHAPROC		w32PathStripPathA		= nullptr;
PFNDRAGQUERYFILEAPROC		w32DragQueryFileA		= nullptr;
PFNDRAGFINISHPROC			w32DragFinish			= nullptr;
PFNNTDELAYEXECUTIONPROC		w32NtDelayExecution		= nullptr;

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <commdlg.h>
#include <comdef.h>

#include <math.h>
#include <stdio.h>
#include "k15_gb_emulator.h"
#include "k15_win32_opengl.h"

#define WIN32_PROFILE_FUNCTION(func) \
{\
	LARGE_INTEGER funcStart, funcEnd, freq;\
	QueryPerformanceFrequency(&freq);\
	QueryPerformanceCounter(&funcStart);\
	func;\
	QueryPerformanceCounter(&funcEnd);\
	const uint32_t funcTimeInMicroseconds = (uint32_t)(((funcEnd.QuadPart-funcStart.QuadPart)*1000000)/freq.QuadPart);\
	const float funcTimeInMilliseconds = (float)funcTimeInMicroseconds/1000.0f;\
	printf("'%s' took %.3fms.\n", #func, funcTimeInMilliseconds);\
}

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
//#	include "k15_gb_emulator_ui.cpp"
#endif

constexpr float pi 		= 3.14159f;
constexpr float twoPi 	= 6.28318f;

const IID 	IID_IAudioClient			= _uuidof(IAudioClient);
const IID 	IID_IAudioRenderClient		= _uuidof(IAudioRenderClient);
const IID 	IID_IMMDeviceEnumerator 	= _uuidof(IMMDeviceEnumerator);
const CLSID CLSID_MMDeviceEnumerator 	= _uuidof(MMDeviceEnumerator);

constexpr uint32_t gbMenuOpenRom 		= 1;
constexpr uint32_t gbMenuClose 			= 2;

constexpr uint32_t gbMenuScale0 		= 10;
constexpr uint32_t gbMenuFullscreen		= 25;

constexpr uint32_t gbMenuState1 		= 30;
constexpr uint32_t gbMenuState2 		= 31;
constexpr uint32_t gbMenuState3 		= 32;
constexpr uint32_t gbMenuSaveState		= 35;
constexpr uint32_t gbMenuLoadState		= 36;
constexpr uint32_t gbMenuSpeed1x		= 37;
constexpr uint32_t gbMenuSpeed4x		= 38;
constexpr uint32_t gbMenuSpeed8x		= 39;
constexpr uint32_t gbMenuSpeed16x		= 40;

constexpr uint32_t gbMenuResetEmulator 	= 50;

enum InputType
{
	Gamepad,
	Keyboard
};

struct Win32FileMapping
{
	uint8_t* pFileBaseAddress		= nullptr;
	HANDLE	 pFileMappingHandle		= nullptr;
	HANDLE   pFileHandle			= nullptr;
};

struct UserMessage
{
	char messageBuffer[256];
	size_t messageBufferLength;
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

enum class Win32EmulatorKeyboardDigipadBindings
{
	RIGHT = 0,
	LEFT,
	UP,
	DOWN,

	COUNT
};

enum class Win32EmulatorKeyboardActionButtonBindings
{
	A = 0,
	B,
	SELECT,
	START,
	COUNT
};

struct Win32EmulatorContext
{
	char				romBaseFileName[MAX_PATH];

	GBEmulatorInstance*	pEmulatorInstance 				= nullptr;
	
	Win32FileMapping	romMapping;
	Win32FileMapping	ramMapping;
	
	int 				digipadKeyboardMappings[ (size_t)Win32EmulatorKeyboardDigipadBindings::COUNT ];
	int 				actionButtonKeyboardMappings[ (size_t)Win32EmulatorKeyboardActionButtonBindings::COUNT ];
	uint32_t			cyclesPerHostFrame 			= gbCyclesPerFrame;
	uint32_t			cyclesPerHostFrameRest		= 0;
	InputType 			dominantInputType 			= Gamepad;
	uint8_t 			stateSlot 					= 1u;
	uint8_t				cyclePerHostFrameFactor		= 1u;
};

struct Win32ApplicationContext
{
	Win32EmulatorContext		emulatorContext;

	HINSTANCE					pInstanceHandle						= nullptr;
	HMONITOR					pMonitorHandle						= nullptr;
	HWND						pWindowHandle						= nullptr;
	HMENU						pFileMenuItems 						= nullptr;
	HMENU						pViewMenuItems 						= nullptr;
	HMENU						pScaleMenuItems 					= nullptr;
	HMENU						pStateMenuItems						= nullptr;
	HMENU						pMenuBar							= nullptr;
	HMENU						pContextMenu						= nullptr;
	HGLRC						pOpenGLContext						= nullptr;
	HDC							pDeviceContext						= nullptr;
	uint8_t* 					pGameboyRGBVideoBuffer 				= nullptr;
	UserMessage 				userMessages[8] 					= {};
	char 						gameTitle[16] 						= {};
	char						romBaseFileName[256]				= {};

	uint32_t					monitorWidth						= 0;
	uint32_t					monitorHeight						= 0;
	uint32_t					monitorPosX							= 0;
	uint32_t					monitorPosY							= 0;
	uint16_t					textVertexCount						= 0;
	uint16_t					monitorRefreshRate					= 60;

	uint32_t 					windowWidth							= gbHorizontalResolutionInPixels;
	uint32_t 					windowHeight						= gbVerticalResolutionInPixels;
	int32_t						windowPosX							= 0;
	int32_t						windowPosY							= 0;

	int16_t						leftMouseDownDeltaX					= 0;
	int16_t						leftMouseDownDeltaY					= 0;

	uint32_t 					emulatorDeltaTimeInMicroseconds 	= 0u;
	uint8_t						frameBufferScale					= 1;	
	uint8_t						menuScale							= 1; //FK: What has been selected in the menu
	uint8_t						maxScale							= 1;

	DWORD						windowStyle							= 0u;
	DWORD						windowStyleEx						= 0u;

	GLuint 						gameboyFrameBufferTexture;
	GLuint						gameboyFrameBufferShader;
	GLuint						gameboyFrameVertexBuffer;
	GLuint						gameboyVertexArray;

	uint8_t 					userMessageCount 					= 0;
	uint8_t   					leftMouseDown						= 0;
	uint8_t						fullscreen							= 0;
	uint8_t						vsyncEnabled						= 0;
	uint8_t						hasFocus							= 1;
};

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

struct Win32GBEmulatorArguments
{
	char romPath[MAX_PATH] = {0};
};

void parseCommandLineArguments( Win32GBEmulatorArguments* pOutArguments, LPSTR pCommandLineArguments )
{
	if( pCommandLineArguments == nullptr || *pCommandLineArguments == 0 )
	{
		return;
	}
	
	enum ParseState
	{
		LOOKING_FOR_NEXT_ARG,
		READ_ARG,
		READ_ARG_PARAM,
		EAT_WHITESPACES,
	} parseState = EAT_WHITESPACES, pendingParseState = LOOKING_FOR_NEXT_ARG;

	char arg = 0;
	char argParam[256] = {0};
	char* pArgParam = argParam;
	size_t lastSpacePos = 0;
	do
	{
		switch( parseState )
		{
		case EAT_WHITESPACES:
			if( !isspace( *pCommandLineArguments ) )
			{
				parseState = pendingParseState;
				pCommandLineArguments -= 1;
			}
			break;

		case LOOKING_FOR_NEXT_ARG:
			if( *pCommandLineArguments == '-' )
			{
				pendingParseState 	= READ_ARG;
				parseState 			= EAT_WHITESPACES;
			}
			break;
		
		case READ_ARG:
			if( isalpha( *pCommandLineArguments ) )
			{
				arg = *pCommandLineArguments;
				pendingParseState 	= READ_ARG_PARAM;
				parseState 			= EAT_WHITESPACES;
			}
			break;

		case READ_ARG_PARAM:
			if( *pCommandLineArguments == 0 )
			{
				*pArgParam = 0;

				if( arg == 'r' )
				{
					strcpy_s( pOutArguments->romPath, sizeof( pOutArguments->romPath ), argParam );
				}

				pArgParam 			= argParam;
				pendingParseState 	= LOOKING_FOR_NEXT_ARG;
				parseState 			= EAT_WHITESPACES;
			}
			else
			{
				*pArgParam++ = *pCommandLineArguments;
			}
		}

	} while( *pCommandLineArguments++ != 0 );
}

HMODULE getLibraryHandle( const char* pLibraryFileName )
{
	HMODULE pModuleHandle = GetModuleHandleA( pLibraryFileName );
	if( pModuleHandle != nullptr )
	{
		return pModuleHandle;
	}

	return LoadLibraryA( pLibraryFileName );
}

void updateGameboyFrameVertexBuffer( Win32ApplicationContext* pContext )
{
	const float l = (float)( pContext->windowWidth - gbHorizontalResolutionInPixels * pContext->frameBufferScale ) * 0.5f;
	const float r = (float)( pContext->windowWidth + gbHorizontalResolutionInPixels * pContext->frameBufferScale ) * 0.5f;	
	const float t = (float)( pContext->windowHeight + gbVerticalResolutionInPixels  * pContext->frameBufferScale ) * 0.5f;	
	const float b = (float)( pContext->windowHeight - gbVerticalResolutionInPixels  * pContext->frameBufferScale ) * 0.5f;	

	const float left  		= (((2.0f * l) - (2.0f * 0)) / pContext->windowWidth) - 1.0f;
	const float right 		= (((2.0f * r) - (2.0f * 0)) / pContext->windowWidth) - 1.0f;
	const float top	  		= (((2.0f * t) - (2.0f * 0)) / pContext->windowHeight) - 1.0f;
	const float bottom	    = (((2.0f * b) - (2.0f * 0)) / pContext->windowHeight) - 1.0f;

	glBindBuffer( GL_ARRAY_BUFFER, pContext->gameboyFrameVertexBuffer );
	float* pVertexData = ( float* )glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );

	*pVertexData++ = left; 	*pVertexData++ = bottom;
	*pVertexData++ = 0.0f;	*pVertexData++ = 1.0f;

	*pVertexData++ = right; *pVertexData++ = bottom;
	*pVertexData++ = 1.0f;	*pVertexData++ = 1.0f;

	*pVertexData++ = right; *pVertexData++ = top;
	*pVertexData++ = 1.0f;	*pVertexData++ = 0.0f;

	*pVertexData++ = right; *pVertexData++ = top;
	*pVertexData++ = 1.0f;	*pVertexData++ = 0.0f;

	*pVertexData++ = left; 	*pVertexData++ = top;
	*pVertexData++ = 0.0f;	*pVertexData++ = 0.0f;

	*pVertexData++ = left; 	*pVertexData++ = bottom;
	*pVertexData++ = 0.0f;	*pVertexData++ = 1.0f;

	glUnmapBuffer( GL_ARRAY_BUFFER );
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
	pUserMessage->messageBufferLength = vsnprintf_s( pUserMessage->messageBuffer, sizeof( pUserMessage->messageBuffer ), pFormattedMessage, argList );
	va_end(argList);

	QueryPerformanceCounter( &pUserMessage->startTime );
}

void allocateDebugConsole()
{
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);
	freopen("CONOUT$", "w", stdout);
}

void enableRomMenuItems( Win32ApplicationContext* pContext )
{
	EnableMenuItem( pContext->pContextMenu, gbMenuLoadState, MF_ENABLED );
	EnableMenuItem( pContext->pContextMenu, gbMenuSaveState, MF_ENABLED );

	EnableMenuItem( pContext->pStateMenuItems, gbMenuLoadState, MF_ENABLED );
	EnableMenuItem( pContext->pStateMenuItems, gbMenuSaveState, MF_ENABLED );
}

void changeStateSlot( Win32ApplicationContext* pContext, uint8_t stateSlot )
{
	//FK: TODO: Check if this is atomic
	pContext->emulatorContext.stateSlot = stateSlot;

	const int32_t stateCommand = stateSlot + ( gbMenuState1 - 1 );
	CheckMenuItem( pContext->pStateMenuItems, gbMenuState1, gbMenuState1 == stateCommand ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( pContext->pStateMenuItems, gbMenuState2, gbMenuState2 == stateCommand ? MF_CHECKED : MF_UNCHECKED  );
	CheckMenuItem( pContext->pStateMenuItems, gbMenuState3, gbMenuState3 == stateCommand ? MF_CHECKED : MF_UNCHECKED  );
}

void generateStateFileName( char* pStateFileNameBuffer, size_t stateFileNameLength, uint8_t stateSlotIndex, const char* pRomBaseFileName )
{
	sprintf_s( pStateFileNameBuffer, stateFileNameLength, "%s_%d%s", pRomBaseFileName, stateSlotIndex, gbStateFileExtension );
}

void generateRamFileName( char* pRamFileNameBuffer, size_t ramFileNameLength, const char* pRomBaseFileName )
{
	sprintf_s( pRamFileNameBuffer, ramFileNameLength, "%s%s", pRomBaseFileName, gbRamFileExtension );
}

void unmapFileMapping( Win32FileMapping* pFileMapping )
{
	if( pFileMapping->pFileBaseAddress != nullptr )
	{
		UnmapViewOfFile( pFileMapping->pFileBaseAddress );
		pFileMapping->pFileBaseAddress = nullptr;
	}

	if( pFileMapping->pFileMappingHandle != nullptr )
	{
		CloseHandle( pFileMapping->pFileMappingHandle );
		pFileMapping->pFileMappingHandle = nullptr;
	}

	if( pFileMapping->pFileHandle != nullptr )
	{
		CloseHandle( pFileMapping->pFileHandle );
		pFileMapping->pFileHandle = nullptr;
	}
}

uint8_t mapFileForReading( Win32FileMapping* pOutFileMapping, const char* pFileName )
{
	const HANDLE pFileHandle = CreateFileA( pFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0u, nullptr );
	if( pFileHandle== INVALID_HANDLE_VALUE )
	{
		const DWORD lastError = GetLastError();
		printf("Could not open file handle to '%s'. CreateFileA() error = %d\n", pFileName, lastError );
		return 0;
	}

	const HANDLE pFileMappingHandle = CreateFileMapping( pFileHandle, nullptr, PAGE_READONLY, 0u, 0u, nullptr );
	if( pFileMappingHandle == INVALID_HANDLE_VALUE )
	{
		const DWORD lastError = GetLastError();
		printf("Could not create file mapping handle for file '%s'. CreateFileMapping() error = %d.\n", pFileName, lastError );

		CloseHandle( pFileHandle );
		return 0;
	}

	uint8_t* pFileBaseAddress = ( uint8_t* )MapViewOfFile( pFileMappingHandle, FILE_MAP_READ, 0u, 0u, 0u );
	if( pFileBaseAddress == nullptr )
	{
		const DWORD lastError = GetLastError();
		printf("Could not create map view of file '%s'. MapViewOfFile() error = %d.\n", pFileName, lastError );

		CloseHandle( pFileHandle );
		CloseHandle( pFileMappingHandle );
		return 0;
	}

	pOutFileMapping->pFileHandle 		= pFileHandle;
	pOutFileMapping->pFileMappingHandle = pFileMappingHandle;
	pOutFileMapping->pFileBaseAddress	= pFileBaseAddress;

	return 1;
}

uint8_t mapFileForWriting( Win32FileMapping* pOutFileMapping, const char* pFileName, const size_t fileSizeInBytes )
{
	const HANDLE pFileHandle = CreateFileA( pFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0u, nullptr );
	if( pFileHandle == INVALID_HANDLE_VALUE )
	{
		const DWORD lastError = GetLastError();
		printf("Could not open file handle to '%s'. CreateFileA() error = %d\n", pFileName, lastError );
		return 0;
	}

	const DWORD fileSizeLow  = fileSizeInBytes & 0xFFFFFFFF;
	HANDLE pFileMappingHandle = CreateFileMappingA( pFileHandle, nullptr, PAGE_READWRITE, 0u, fileSizeLow, nullptr );
	if( pFileMappingHandle == nullptr )
	{
		const DWORD lastError = GetLastError();
		printf("Could not create file mapping handle for file '%s'. CreateFileMapping() error = %d.\n", pFileName, lastError );

		CloseHandle( pFileHandle );
		return 0;
	}

	uint8_t* pFileBaseAddress = ( uint8_t* )MapViewOfFile( pFileMappingHandle, FILE_MAP_WRITE, 0u, 0u, 0u );
	if( pFileBaseAddress == nullptr )
	{
		const DWORD lastError = GetLastError();
		printf("Could not create map view of file '%s'. MapViewOfFile() error = %d.\n", pFileName, lastError );

		CloseHandle( pFileHandle );
		CloseHandle( pFileMappingHandle );
		return 0;
	}

	pOutFileMapping->pFileHandle 		= pFileHandle;
	pOutFileMapping->pFileMappingHandle = pFileMappingHandle;
	pOutFileMapping->pFileBaseAddress	= pFileBaseAddress;

	return 1;
}

uint8_t loadStateInSlot( GBEmulatorInstance* pEmulatorInstance, const char* pStateFileName, uint8_t stateSlot )
{
	Win32FileMapping stateFileMapping;
	if( mapFileForReading( &stateFileMapping, pStateFileName ) == 0 )
	{
		return 0;
	}


	loadGBEmulatorState( pEmulatorInstance, stateFileMapping.pFileBaseAddress );
	unmapFileMapping( &stateFileMapping );
	return 1;
}

uint8_t saveStateInSlot( GBEmulatorInstance* pEmulatorInstance, const char* pStateFileName, uint8_t stateSlot )
{
	const size_t stateSizeInBytes = calculateGBEmulatorStateSizeInBytes( pEmulatorInstance );

	Win32FileMapping stateFileMapping;
	if( mapFileForWriting( &stateFileMapping, pStateFileName, stateSizeInBytes ) == 0 )
	{
		return 0;
	}

	storeGBEmulatorState( pEmulatorInstance, stateFileMapping.pFileBaseAddress, stateSizeInBytes );
	unmapFileMapping( &stateFileMapping );

	return 1;
}

void updateMonitorSettings( Win32ApplicationContext* pContext )
{
	pContext->pMonitorHandle = MonitorFromWindow( pContext->pWindowHandle, MONITOR_DEFAULTTOPRIMARY );

	MONITORINFOEX monitorInfo = {};
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfoA( pContext->pMonitorHandle, &monitorInfo );

	DEVMODE displayInfo;
	if( EnumDisplaySettings( monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &displayInfo ) == FALSE )
	{
		pContext->monitorRefreshRate = 60u;
	}

	pContext->monitorWidth  	 = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
	pContext->monitorHeight 	 = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
	pContext->monitorRefreshRate = (uint16_t)displayInfo.dmDisplayFrequency;

	//FK: max scale is directly tied to monitor resolution
	pContext->maxScale = pContext->monitorHeight / gbVerticalResolutionInPixels;

	const float cyclesPerHostFrame = ( ( float )(gbCyclesPerFrame*60) / ( float )pContext->monitorRefreshRate );
	const float cylcesPerHostFrameRest = ( ( cyclesPerHostFrame - ( uint32_t )cyclesPerHostFrame ) * ( float )pContext->monitorRefreshRate );
	pContext->emulatorContext.cyclesPerHostFrame 		= ( uint32_t )cyclesPerHostFrame;
	pContext->emulatorContext.cyclesPerHostFrameRest 	= ( uint32_t )( cylcesPerHostFrameRest + 0.5f );
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

void getRomBaseFileName( char* pRomBaseFileNameBuffer, size_t romBaseFileNameBufferSizeInBytes, char* pRomPath )
{
	//FK: Temporary remove the file extension to save base file name
	char* pRomPathFileExtension = strrchr( pRomPath, '.' );
	*pRomPathFileExtension = 0;
	
	strcpy_s( pRomBaseFileNameBuffer, romBaseFileNameBufferSizeInBytes, pRomPath );
	w32PathStripPathA( pRomBaseFileNameBuffer );

	*pRomPathFileExtension = '.';
}

void loadRomFile( Win32ApplicationContext* pContext, char* pRomPath )
{
	Win32EmulatorContext* pEmulatorContext = &pContext->emulatorContext;

	char fixedRomPath[ MAX_PATH ];
	strcpy_s( fixedRomPath, sizeof( fixedRomPath ), pRomPath );
	fixRomFileName( fixedRomPath );
	
	char romBaseFileName[ MAX_PATH ];
	CompiletimeAssert( sizeof( romBaseFileName ) == sizeof( Win32EmulatorContext::romBaseFileName ) );

	getRomBaseFileName( romBaseFileName, sizeof( romBaseFileName ), fixedRomPath );
		
	unmapFileMapping( &pEmulatorContext->romMapping );
	unmapFileMapping( &pEmulatorContext->ramMapping );

	if( mapFileForReading( &pEmulatorContext->romMapping, fixedRomPath ) == 0 )
	{
		//pushUserMessage( pContext, "Couldn't open rom file '%s'", pRomPath );
		return;
	}

	if( !isValidGBRomData( pEmulatorContext->romMapping.pFileBaseAddress ) )
	{
		//pushUserMessage( pContext, "Rom file '%s' is not a valid rom file.", pRomPath );
		unmapFileMapping( &pEmulatorContext->romMapping );
		return;
	}

	const GBCartridgeHeader header = getGBCartridgeHeader( pEmulatorContext->romMapping.pFileBaseAddress );
	//memcpy( pContext->gameTitle, header.gameTitle, sizeof( header.gameTitle ) );

	loadGBEmulatorRom( pEmulatorContext->pEmulatorInstance, pEmulatorContext->romMapping.pFileBaseAddress );

	const size_t ramSizeInBytes = mapRamSizeToByteSize( header.ramSize );
	if( ramSizeInBytes > 0 )
	{
		char ramFileName[MAX_PATH] = {0};
		generateRamFileName( ramFileName, sizeof( ramFileName ), romBaseFileName );

		if( mapFileForWriting( &pEmulatorContext->ramMapping, ramFileName, ramSizeInBytes ) == 0 )
		{
			//pushUserMessage( pContext, "Couldn't open ram file '%s'", ramFileName );
			unmapFileMapping( &pEmulatorContext->romMapping );
			return;
		}

		setGBEmulatorRamData( pEmulatorContext->pEmulatorInstance, pEmulatorContext->ramMapping.pFileBaseAddress );
	}

	strcpy_s( pEmulatorContext->romBaseFileName, sizeof( pEmulatorContext->romBaseFileName ), romBaseFileName );

	//FK: TODO: only enable if the thread verified the rom and successfully loaded the rom data
	enableRomMenuItems( pContext );
}

void openRomFile( Win32ApplicationContext* pContext )
{
	char romPath[MAX_PATH] = { 0 };
	OPENFILENAMEA parameters = {};
	parameters.lStructSize 		= sizeof(OPENFILENAMEA);
	parameters.hwndOwner   		= pContext->pWindowHandle;
	parameters.lpstrFilter		= "GameBoy Rom (*.gb)\0*.gb\0GameBoy Color Rom (*.gbc)\0*.gbc\0\0";
	parameters.lpstrFile 		= romPath;
	parameters.nMaxFile			= sizeof( romPath );
	parameters.lpstrTitle		= "Load GameBoy/GameBoy Color rom file.";
	parameters.Flags			= OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	parameters.lpstrDefExt		= "gb";

	const BOOL result = w32GetOpenFileNameA( &parameters );
	if( result == FALSE )
	{
		return;
	}
	
	loadRomFile( pContext, romPath );
}

void enableFullscreen( Win32ApplicationContext* pContext )
{
	pContext->fullscreen 		= 1;
	pContext->frameBufferScale 	= pContext->maxScale;

	const LONG fullscreenWindowStyle 	=  pContext->windowStyle & ~( WS_CAPTION | WS_THICKFRAME );
	const LONG fullscreenWindowStyleEx 	=  pContext->windowStyleEx & ~( WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE ) ;

	SetWindowLongA( pContext->pWindowHandle, GWL_STYLE,   fullscreenWindowStyle);
	SetWindowLongA( pContext->pWindowHandle, GWL_EXSTYLE, fullscreenWindowStyleEx);

	//FK: Hide menu
	SetMenu( pContext->pWindowHandle, nullptr );
	SetWindowPos( pContext->pWindowHandle, HWND_TOP, 0u, 0u, pContext->monitorWidth, pContext->monitorHeight, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED );
	updateGameboyFrameVertexBuffer( pContext );
}

void disableFullscreen( Win32ApplicationContext* pContext )
{
	pContext->fullscreen 		= 0;
	pContext->frameBufferScale 	= pContext->menuScale;

	SetWindowLongA( pContext->pWindowHandle, GWL_STYLE,   pContext->windowStyle );
	SetWindowLongA( pContext->pWindowHandle, GWL_EXSTYLE, pContext->windowStyleEx );

	SetMenu( pContext->pWindowHandle, pContext->pMenuBar );

	RECT windowRect = {};
	windowRect.right = gbHorizontalResolutionInPixels * pContext->frameBufferScale;
	windowRect.bottom = gbVerticalResolutionInPixels * pContext->frameBufferScale;
	AdjustWindowRect( &windowRect, pContext->windowStyle, TRUE );

	const int32_t windowWidth 	= windowRect.right - windowRect.left;
	const int32_t windowHeight 	= windowRect.bottom - windowRect.top;
	SetWindowPos( pContext->pWindowHandle, nullptr, 0, 0, windowWidth, windowHeight, 0u );
}

void toggleFullscreen( Win32ApplicationContext* pContext )
{
	if( pContext->fullscreen )
	{
		disableFullscreen( pContext );
	}
	else
	{
		enableFullscreen( pContext );
	}
}

void setFrameBufferScale( Win32ApplicationContext* pContext, uint8_t scale )
{
	RuntimeAssert( scale <= pContext->maxScale );

	for( uint8_t scaleIndex = 1; scaleIndex <= pContext->maxScale; ++scaleIndex )
	{
		CheckMenuItem( pContext->pScaleMenuItems, gbMenuScale0 + scaleIndex, scale == scaleIndex ? MF_CHECKED : MF_UNCHECKED );
	}

	pContext->frameBufferScale = scale;

	RECT windowRect = {};
	windowRect.right 	= gbHorizontalResolutionInPixels * pContext->frameBufferScale;
	windowRect.bottom 	= gbVerticalResolutionInPixels 	 * pContext->frameBufferScale;
	AdjustWindowRect( &windowRect, pContext->windowStyle, TRUE );

	const int32_t windowWidth 	= windowRect.right - windowRect.left;
	const int32_t windowHeight 	= windowRect.bottom - windowRect.top;

	SetWindowPos( pContext->pWindowHandle, nullptr, pContext->windowPosX, pContext->windowPosY, windowWidth, windowHeight, 0u );
	updateGameboyFrameVertexBuffer( pContext );
}

void handleWindowMinMaxInfo( LPARAM lparam )
{
	MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lparam;
	pMinMaxInfo->ptMinTrackSize.x = gbHorizontalResolutionInPixels;
	pMinMaxInfo->ptMinTrackSize.y = gbVerticalResolutionInPixels;
}

void loadEmulatorState( Win32EmulatorContext* pContext )
{
	if( !isGBEmulatorRomMapped( pContext->pEmulatorInstance ) )
	{
		//FK: TODO error reporting
		return;
	}

	char stateFileName[MAX_PATH];
	generateStateFileName( stateFileName, sizeof( stateFileName ), pContext->stateSlot, pContext->romBaseFileName );

	if( !loadStateInSlot( pContext->pEmulatorInstance, stateFileName, pContext->stateSlot ) )
	{
		//FK: TODO: error reporting
		return;
	}
}

void saveEmulatorState( Win32EmulatorContext* pContext )
{
	if( !isGBEmulatorRomMapped( pContext->pEmulatorInstance ) )
	{
		return;
	}

	char stateFileName[MAX_PATH];
	generateStateFileName( stateFileName, sizeof( stateFileName ), pContext->stateSlot, pContext->romBaseFileName );

	if( !saveStateInSlot( pContext->pEmulatorInstance, stateFileName, pContext->stateSlot ) )
	{
		//FK: TODO: error reporting
	}
}

void setEmulatorSpeedFactor( Win32ApplicationContext* pContext, uint8_t speedFactor )
{
	RuntimeAssert( speedFactor > 0u );
	pContext->emulatorContext.cyclePerHostFrameFactor = speedFactor;

	CheckMenuItem( pContext->pStateMenuItems,  gbMenuSpeed1x, MF_UNCHECKED );
	CheckMenuItem( pContext->pStateMenuItems,  gbMenuSpeed4x, MF_UNCHECKED );
	CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed8x, MF_UNCHECKED );
	CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed16x, MF_UNCHECKED );

	if( speedFactor == 1 )
	{
		CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed1x, MF_CHECKED );
	}
	else if( speedFactor == 4 )
	{
		CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed4x, MF_CHECKED );
	}
	else if( speedFactor == 16 )
	{
		CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed8x, MF_CHECKED );
	}
	else if( speedFactor == 32 )
	{
		CheckMenuItem( pContext->pStateMenuItems, gbMenuSpeed8x, MF_CHECKED );
	}
}

void handleWindowCommand( Win32ApplicationContext* pContext, WPARAM wparam )
{
	uint8_t changeFrameBufferScale = 0;
	switch( wparam )
	{
		case gbMenuOpenRom:
			openRomFile( pContext );
			break;

		case gbMenuClose:
			PostQuitMessage(0);
			break;

		case gbMenuFullscreen:
			toggleFullscreen( pContext );
			break;

		case gbMenuState1:
			changeStateSlot( pContext, 1 );
			break;
		
		case gbMenuState2:
			changeStateSlot( pContext, 2 );
			break;
		
		case gbMenuState3:
			changeStateSlot( pContext, 3 );
			break;

		case gbMenuLoadState:
			loadEmulatorState( &pContext->emulatorContext );
			break;

		case gbMenuSaveState:
			saveEmulatorState( &pContext->emulatorContext );
			break;

		case gbMenuResetEmulator:
			resetGBEmulator( pContext->emulatorContext.pEmulatorInstance );
			break;

		default:
			if( wparam > gbMenuScale0 && wparam < gbMenuFullscreen )
			{
				pContext->menuScale = (uint8_t)wparam - gbMenuScale0;
				setFrameBufferScale( pContext, pContext->menuScale );
				break;
			}
			else if( wparam >= gbMenuSpeed1x && wparam <= gbMenuSpeed16x )
			{
				uint8_t speedFactor = 1;
				if( wparam == gbMenuSpeed4x )
				{
					speedFactor = 4;
				}
				else if( wparam == gbMenuSpeed8x )
				{
					speedFactor = 8;
				}
				else if( wparam == gbMenuSpeed16x )
				{
					speedFactor = 16;
				}
				
				setEmulatorSpeedFactor( pContext, speedFactor );
				break;
			}
			
			printf("Unknown window command '%d'\n", (int32_t)wparam);
			break;
	}
}

void handleWindowResize( Win32ApplicationContext* pContext, WPARAM wparam )
{
	RECT clientRect = {0};
	GetClientRect(pContext->pWindowHandle, &clientRect);

	pContext->windowWidth  = clientRect.right - clientRect.left;
	pContext->windowHeight = clientRect.bottom - clientRect.top;
	pContext->maxScale = pContext->windowHeight / gbVerticalResolutionInPixels;

	updateMonitorSettings( pContext );

	if( wparam == SIZE_MAXIMIZED )
	{
		pContext->frameBufferScale = pContext->maxScale;
	}
	else if( wparam == SIZE_RESTORED )
	{
		pContext->frameBufferScale = pContext->menuScale;
	}

	if( pContext->pOpenGLContext != nullptr )
	{
		updateGameboyFrameVertexBuffer( pContext );
		glViewport(0, 0, pContext->windowWidth, pContext->windowHeight);
	}
}

void handleDropFiles( Win32ApplicationContext* pContext, WPARAM wparam )
{
	HDROP pDropInfo = (HDROP)wparam;

	char filePathBuffer[MAX_PATH];
	if( w32DragQueryFileA( pDropInfo, 0u, filePathBuffer, sizeof( filePathBuffer ) ) == 0 )
	{
		return;
	}

	char* pFileExtension = strrchr( filePathBuffer, '.' );
	if( strcmp( pFileExtension, ".gb") != 0 &&
		strcmp( pFileExtension, ".gbc" ) != 0 )
	{
		pushUserMessage( pContext, "Not a valid rom file." );
		return;	
	}

	loadRomFile( pContext, filePathBuffer );
	w32DragFinish( pDropInfo );
}

void handleWindowPosChanged( Win32ApplicationContext* pContext, LPARAM lparam )
{
	WINDOWPOS* pWindowPos = (WINDOWPOS*)lparam;
	pContext->windowPosX = pWindowPos->x;
	pContext->windowPosY = pWindowPos->y;

	updateMonitorSettings( pContext );
}

void handleLeftButtonDown( Win32ApplicationContext* pContext, LPARAM lparam )
{
	pContext->leftMouseDown = 1;

	POINT mousePos;
	mousePos.x = ( ( lparam >>  0 ) & 0xFFFF );
	mousePos.y = ( ( lparam >> 16 ) & 0xFFFF );

	RECT windowRect = {};
	GetWindowRect(pContext->pWindowHandle, &windowRect);

	ClientToScreen(pContext->pWindowHandle, &mousePos);
	pContext->leftMouseDownDeltaX = (int16_t)(windowRect.left - mousePos.x);
	pContext->leftMouseDownDeltaY = (int16_t)(windowRect.top  - mousePos.y);

	SetCapture(pContext->pWindowHandle);
}

void handleRightButtonDown( Win32ApplicationContext* pContext, LPARAM lparam )
{
	POINT mousePos;
	mousePos.x = ( ( lparam >>  0 ) & 0xFFFF );
	mousePos.y = ( ( lparam >> 16 ) & 0xFFFF );

	ClientToScreen( pContext->pWindowHandle, &mousePos );
	TrackPopupMenu( pContext->pContextMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_VERPOSANIMATION, mousePos.x, mousePos.y, 0, pContext->pWindowHandle, nullptr );
}

void handleLeftButtonUp( Win32ApplicationContext* pContext )
{
	pContext->leftMouseDown = 0;
	ReleaseCapture();
}

void handleMouseMove( Win32ApplicationContext* pContext, LPARAM lparam )
{
	if( pContext->leftMouseDown == 0 || pContext->fullscreen == 1 )
	{
		return;
	}

	POINT pos;
	pos.x = (int16_t)( ( lparam >>  0 ) & 0xFFFF);
	pos.y = (int16_t)( ( lparam >> 16 ) & 0xFFFF);

	pos.x += pContext->leftMouseDownDeltaX;
	pos.y += pContext->leftMouseDownDeltaY;

	RECT windowRect = {};
	windowRect.right 	= pContext->windowWidth;
	windowRect.bottom 	= pContext->windowHeight;

	AdjustWindowRect(&windowRect, pContext->windowStyle, TRUE);

	const uint16_t windowWidth 	= (uint16_t)(windowRect.right - windowRect.left);
	const uint16_t windowHeight = (uint16_t)(windowRect.bottom - windowRect.top);

	ClientToScreen(pContext->pWindowHandle, &pos);
	MoveWindow(pContext->pWindowHandle, pos.x, pos.y, windowWidth, windowHeight, TRUE);
}

void drawGBFrameBuffer( HDC pDeviceContext )
{
	//FK: Draw gb frame to backbuffer
	glDrawArrays( GL_TRIANGLES, 0, 6 );
}

LRESULT CALLBACK K15_WNDPROC(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	uint8_t messageHandled = false;

	Win32ApplicationContext* pContext = (Win32ApplicationContext*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
	switch (message)
	{
	case WM_CLOSE:
		PostQuitMessage(0);
		messageHandled = true;
		break;

	case WM_PAINT:
	{
		messageHandled = true;

		PAINTSTRUCT ps;
		HDC pDeviceContext = BeginPaint( hwnd, &ps );
		drawGBFrameBuffer( pDeviceContext );
		w32SwapBuffers( pDeviceContext );
		EndPaint( hwnd, &ps );
		break;
	}

	case WM_SETFOCUS:
		pContext->hasFocus = 1;
		printf("has focus\n");
		break;

	case WM_KILLFOCUS:
		pContext->hasFocus = 0;
		printf("lost focus\n");
		break;

	case WM_LBUTTONDOWN:
		handleLeftButtonDown( pContext, lparam );
		break;

	case WM_RBUTTONDOWN:
		handleRightButtonDown( pContext, lparam );
		break;

	case WM_LBUTTONUP:
		handleLeftButtonUp( pContext );
		break;

	case WM_MOUSEMOVE:
		handleMouseMove( pContext, lparam );
		break;

	case WM_GETMINMAXINFO:
		handleWindowMinMaxInfo( lparam );
		break;

	case WM_COMMAND:
		handleWindowCommand( pContext, wparam );
		break;
		
	case WM_WINDOWPOSCHANGED:
		handleWindowPosChanged( pContext, lparam );
		break;

	case WM_SIZE:
		handleWindowResize( pContext, wparam ); 
		break;
	
	case WM_DROPFILES:
		handleDropFiles( pContext, wparam );
		break;
	}

	if (messageHandled == false)
	{
		return DefWindowProc(hwnd, message, wparam, lparam);
	}

	return false;
}

uint8_t setupWindow( Win32ApplicationContext* pContext )
{
	constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME;

	RECT windowRect = {};
	windowRect.right 	= gbHorizontalResolutionInPixels;
	windowRect.bottom 	= gbVerticalResolutionInPixels;
	AdjustWindowRect( &windowRect, windowStyle, TRUE );

	pContext->windowWidth 	= windowRect.right  - windowRect.left;
	pContext->windowHeight 	= windowRect.bottom - windowRect.top;

	WNDCLASSA wndClass 		= {0};
	wndClass.style 			= CS_HREDRAW | CS_OWNDC | CS_VREDRAW;
	wndClass.hInstance 		= pContext->pInstanceHandle;
	wndClass.lpszClassName 	= "K15_Win32Template";
	wndClass.lpfnWndProc 	= K15_WNDPROC;
	wndClass.hCursor 		= LoadCursor(NULL, IDC_ARROW);
	RegisterClassA(&wndClass);

	pContext->pWindowHandle = CreateWindowExA( WS_EX_ACCEPTFILES,
		"K15_Win32Template", "K15 GB Emulator", 
		windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
		pContext->windowWidth, pContext->windowHeight, 
		0, 0, pContext->pInstanceHandle, 0);

	if( pContext->pWindowHandle == INVALID_HANDLE_VALUE )
	{
		MessageBoxA(0, "Error creating Window.\n", "Error!", 0);
		return 0;
	}

	pContext->pDeviceContext = GetDC( pContext->pWindowHandle );
	updateMonitorSettings( pContext );

	return 1;
}

uint8_t setupContextMenu( Win32ApplicationContext* pContext )
{
	pContext->pContextMenu = CreatePopupMenu();
	if( pContext->pContextMenu == nullptr )
	{
		return 0;
	}

	uint8_t result = 0;
	result |= InsertMenuA( pContext->pContextMenu, 0, MF_BYCOMMAND, 				gbMenuOpenRom, 			"&Open Rom...");
	result |= InsertMenuA( pContext->pContextMenu, 0, MF_BYCOMMAND,					gbMenuResetEmulator,	"Reset Emulator");
	result |= InsertMenuA( pContext->pContextMenu, 0, MF_BYCOMMAND | MF_GRAYED, 	gbMenuSaveState, 		"State Quicksave\tF6");
	result |= InsertMenuA( pContext->pContextMenu, 0, MF_BYCOMMAND | MF_GRAYED, 	gbMenuLoadState, 		"State Quickload\tF9");
	result |= InsertMenuA( pContext->pContextMenu, 0, MF_BYCOMMAND, 				gbMenuFullscreen,		"Fullscreen\tF11" );

	return result;
}

uint8_t setupMenu( Win32ApplicationContext* pContext )
{
	pContext->pFileMenuItems 	= CreateMenu();
	pContext->pViewMenuItems 	= CreateMenu();
	pContext->pScaleMenuItems 	= CreateMenu();
	pContext->pStateMenuItems	= CreateMenu();
	pContext->pMenuBar			= CreateMenu();

	if( pContext->pFileMenuItems 	== nullptr ||
	 	pContext->pViewMenuItems 	== nullptr ||
		pContext->pScaleMenuItems 	== nullptr ||
		pContext->pStateMenuItems 	== nullptr ||
		pContext->pMenuBar 			== nullptr )
	{
		return 0;	
	}

	uint8_t result = 0;

	//FK: File menu
	result |= AppendMenuA( pContext->pFileMenuItems, MF_STRING, gbMenuOpenRom, "&Open Rom..." );
	result |= AppendMenuA( pContext->pFileMenuItems, MF_STRING, gbMenuClose, "&Quit" );

	//FK: State menu
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_CHECKED, gbMenuState1, "State Slot 1\tF2" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_UNCHECKED, gbMenuState2, "State Slot 2\tF3" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_UNCHECKED, gbMenuState3, "State Slot 3\tF4" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_SEPARATOR, 0, nullptr );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_GRAYED, gbMenuSaveState, "Quicksave State\tF6" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_GRAYED, gbMenuLoadState, "Quickload State\tF9" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_SEPARATOR, 0, nullptr );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_CHECKED,	gbMenuSpeed1x,  "Run Emulator in 1x Speed" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_UNCHECKED, gbMenuSpeed4x,  "Run Emulator in 4x Speed" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_UNCHECKED, gbMenuSpeed8x, "Run Emulator in 16x Speed" );
	result |= AppendMenuA( pContext->pStateMenuItems, MF_STRING | MF_UNCHECKED, gbMenuSpeed16x, "Run Emulator in 32x Speed" );

	//FK: Scale settings
	char menuScaleText[] = "Scale 1x";
	for( uint8_t scaleIndex = 1; scaleIndex <= pContext->maxScale; ++scaleIndex )
	{
		UINT flags = scaleIndex == 1 ? MF_CHECKED : MF_UNCHECKED;
		flags |= MF_STRING;

		//FK: Set ascii value for decimal number
		menuScaleText[6] = scaleIndex + 48;
		result |= AppendMenuA( pContext->pScaleMenuItems, flags, gbMenuScale0 + scaleIndex, menuScaleText );
	}

	//FK: View menu
	result |= AppendMenuA( pContext->pViewMenuItems , MF_POPUP, (UINT_PTR)pContext->pScaleMenuItems, "&Scale" );
	result |= AppendMenuA( pContext->pViewMenuItems , MF_STRING, gbMenuFullscreen, "Fullscreen\tF11");

	result |= AppendMenuA( pContext->pMenuBar, MF_POPUP, (UINT_PTR)pContext->pFileMenuItems,  "&File" );
	result |= AppendMenuA( pContext->pMenuBar, MF_POPUP, (UINT_PTR)pContext->pViewMenuItems,  "&View" );
	result |= AppendMenuA( pContext->pMenuBar, MF_POPUP, (UINT_PTR)pContext->pStateMenuItems, "&State" );

	return result;
}

void loadXInputFunctionPointers()
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
	RuntimeAssert(w32XInputGetState != nullptr);
}

void loadWin32FunctionPointers()
{
	//FK: TODO Turn the asserts into user friendly error messages
	const HMODULE pGDIModule = getLibraryHandle("gdi32.dll");
	RuntimeAssert( pGDIModule != nullptr );

	const HMODULE pComDlg32Module = getLibraryHandle("Comdlg32.dll");
	RuntimeAssert( pComDlg32Module != nullptr );

	const HMODULE pShlwapiModule = getLibraryHandle("Shlwapi.dll");
	RuntimeAssert( pShlwapiModule != nullptr );

	const HMODULE pShell32Module = getLibraryHandle("Shell32.dll");
	RuntimeAssert( pShell32Module != nullptr );

	const HMODULE pNtModule = getLibraryHandle("ntdll.dll");
	RuntimeAssert( pNtModule != nullptr );

	w32ChoosePixelFormat = (PFNCHOOSEPIXELFORMATPROC)GetProcAddress( pGDIModule, "ChoosePixelFormat" );
	w32SetPixelFormat	 = (PFNSETPIXELFORMATPROC)GetProcAddress( pGDIModule, "SetPixelFormat" );
	w32SwapBuffers		 = (PFNSWAPBUFFERSPROC)GetProcAddress( pGDIModule, "SwapBuffers");
	w32GetOpenFileNameA  = (PFNGETOPENFILENAMEAPROC)GetProcAddress( pComDlg32Module, "GetOpenFileNameA" );
	w32PathStripPathA	 = (PFNPATHSTRIPPATHAPROC)GetProcAddress( pShlwapiModule, "PathStripPathA" );
	w32DragQueryFileA	 = (PFNDRAGQUERYFILEAPROC)GetProcAddress( pShell32Module, "DragQueryFileA" );
	w32DragFinish		 = (PFNDRAGFINISHPROC)GetProcAddress( pShell32Module, "DragFinish" );
	w32NtDelayExecution	 = (PFNNTDELAYEXECUTIONPROC)GetProcAddress( pNtModule, "NtDelayExecution" );

	RuntimeAssert( w32ChoosePixelFormat != nullptr );
	RuntimeAssert( w32SetPixelFormat != nullptr );
	RuntimeAssert( w32SwapBuffers != nullptr );
	RuntimeAssert( w32GetOpenFileNameA != nullptr );
	RuntimeAssert( w32PathStripPathA != nullptr );
	RuntimeAssert( w32DragQueryFileA != nullptr );
	RuntimeAssert( w32DragFinish != nullptr );
	RuntimeAssert( w32NtDelayExecution != nullptr );
}

void generateOpenGLShaders( Win32ApplicationContext* pContext  )
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
	const GLuint pixelShader 	= glCreateShader( GL_FRAGMENT_SHADER );

	pContext->gameboyFrameBufferShader 	= glCreateProgram();

	compileOpenGLShader( vertexShader, vertexShaderSource, sizeof( vertexShaderSource ) );
	compileOpenGLShader( pixelShader, pixelShaderSource, sizeof( pixelShaderSource ) );

	glAttachShader( pContext->gameboyFrameBufferShader, vertexShader );
	glAttachShader( pContext->gameboyFrameBufferShader, pixelShader );
	glLinkProgram( pContext->gameboyFrameBufferShader );
	glDetachShader( pContext->gameboyFrameBufferShader, vertexShader );
	glDetachShader( pContext->gameboyFrameBufferShader, pixelShader );
	glDeleteShader( vertexShader );
	glDeleteShader( pixelShader );

	glUseProgram( pContext->gameboyFrameBufferShader );
}

void createOpenGLContext( Win32ApplicationContext* pContext )
{
	const HMODULE pOpenGL32Module = getLibraryHandle("opengl32.dll");

	loadWin32OpenGLFunctionPointers( pOpenGL32Module );
	createOpenGLDummyContext( pContext->pWindowHandle, pContext->pDeviceContext );
	loadWGLOpenGLFunctionPointers();
	
	pContext->pOpenGLContext = createOpenGL4Context( pContext->pWindowHandle, pContext->pDeviceContext );
	
	loadOpenGL4FunctionPointers();

	constexpr uint8_t enableVsync = 0;
	//FK: Try to disable v-sync...
	w32glSwapIntervalEXT( enableVsync );

	//FK: 	Since vsync behavior can be overriden by the gpu driver, we have to
	//		check if vsync is actually disabled or not
	pContext->vsyncEnabled = w32glGetSwapIntervalEXT();
}

void generateOpenGLTextures(Win32ApplicationContext* pContext)
{
	glGenTextures(1, &pContext->gameboyFrameBufferTexture);

	glBindTexture(GL_TEXTURE_2D, pContext->gameboyFrameBufferTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, gbHorizontalResolutionInPixels, gbVerticalResolutionInPixels);
}

uint8_t setupUi( Win32ApplicationContext* pContext )
{
	if( setupWindow( pContext ) == 0 )
	{
		//FK: TODO: User friendly error message
		return 0;
	}

	if( !setupMenu( pContext ) ) 
	{
		return 0;
	}

	if( !setupContextMenu( pContext ) )
	{
		return 0;
	}
	
	SetWindowLongPtrA( pContext->pWindowHandle, GWLP_USERDATA, (LONG_PTR)pContext );
	ShowWindow(pContext->pWindowHandle, SW_SHOW);

	pContext->windowStyle 	= GetWindowLongA( pContext->pWindowHandle, GWL_STYLE );
	pContext->windowStyleEx = GetWindowLongA( pContext->pWindowHandle, GWL_EXSTYLE );

	SetMenu( pContext->pWindowHandle, pContext->pMenuBar );

	return 1;
}

void setupOpenGL( Win32ApplicationContext* pContext )
{
	createOpenGLContext( pContext );
	generateOpenGLShaders( pContext );

	constexpr size_t gameboyFrameVertexBufferSizeInBytes = sizeof(float) * 4 * 6;

	glGenBuffers( 1, &pContext->gameboyFrameVertexBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, pContext->gameboyFrameVertexBuffer );
	glBufferStorage( GL_ARRAY_BUFFER, gameboyFrameVertexBufferSizeInBytes, nullptr, GL_MAP_WRITE_BIT );

	glGenVertexArrays( 1, &pContext->gameboyVertexArray );
	glBindVertexArray( pContext->gameboyVertexArray );

	const size_t posOffset = 0;
	const size_t uvOffset = 2*sizeof(float);
	const size_t strideInBytes = 4*sizeof(float);

	glVertexAttribPointer(0, 2, GL_FLOAT, 0, strideInBytes, (const GLvoid*)posOffset );
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, 0, strideInBytes, (const GLvoid*)uvOffset );
	glEnableVertexAttribArray(1);

	updateGameboyFrameVertexBuffer( pContext );

	//FK: at address 0x200000000 for better debugging of memory errors (eg: access violation > 0x200000000 indicated emulator instance memory is at fault)
	pContext->pGameboyRGBVideoBuffer = (uint8_t*)VirtualAlloc( ( LPVOID )0x200000000, (gbVerticalResolutionInPixels*gbHorizontalResolutionInPixels*3), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	generateOpenGLTextures( pContext );
}

uint8_t queryControllerInput( GBEmulatorJoypadState* pJoypadState, const InputType dominantInputType )
{
	XINPUT_STATE state;
	const DWORD result = w32XInputGetState(0, &state);
	if( result != ERROR_SUCCESS )
	{
		return 0;
	}

	const WORD gamepadButtons = state.Gamepad.wButtons;
	if( dominantInputType != InputType::Gamepad && gamepadButtons == 0)
	{
		return 0;
	}

	pJoypadState->a 		= ( gamepadButtons & XINPUT_GAMEPAD_A ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_B ) > 0;
	pJoypadState->b 		= ( gamepadButtons & XINPUT_GAMEPAD_X ) > 0 || ( gamepadButtons & XINPUT_GAMEPAD_Y ) > 0;
	pJoypadState->start 	= ( gamepadButtons & XINPUT_GAMEPAD_START ) > 0;
	pJoypadState->select 	= ( gamepadButtons & XINPUT_GAMEPAD_BACK ) > 0;
	pJoypadState->left 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_LEFT ) > 0;
	pJoypadState->right 	= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) > 0;
	pJoypadState->down 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_DOWN ) > 0;
	pJoypadState->up 		= ( gamepadButtons & XINPUT_GAMEPAD_DPAD_UP ) > 0;

	return 1;
}

uint8_t queryKeyboardInput( GBEmulatorJoypadState* pJoypadState, const int* pDigipadKeyboardMappings, const int* pActionButtonKeyboardMappings, const InputType dominantInputType )
{
	uint8_t keyboardPressed = 0;
	GBEmulatorJoypadState joypadState;
	for( size_t keyboardMappingIndex = 0u; keyboardMappingIndex < ( size_t )Win32EmulatorKeyboardDigipadBindings::COUNT; ++keyboardMappingIndex )
	{
		if( GetAsyncKeyState( pDigipadKeyboardMappings[ keyboardMappingIndex ] ) & 0x8000 )
		{
			joypadState.dpadButtonMask |= (1<<keyboardMappingIndex);
			keyboardPressed = 1;
		}
	}

	for( size_t keyboardMappingIndex = 0u; keyboardMappingIndex < ( size_t )Win32EmulatorKeyboardActionButtonBindings::COUNT; ++keyboardMappingIndex )
	{
		if( GetAsyncKeyState( pActionButtonKeyboardMappings[ keyboardMappingIndex ] ) & 0x8000 )
		{
			joypadState.actionButtonMask |= (1<<keyboardMappingIndex);
			keyboardPressed = 1;
		}
	}

	if( keyboardPressed == 0 && dominantInputType != InputType::Keyboard )
	{
		return 0;
	}

	*pJoypadState = joypadState;
	return 1;
}

void setDefaultKeyboardBinding( int* pDigipadMappings, int* pActionButtonMappings )
{
	pDigipadMappings[(uint8_t)Win32EmulatorKeyboardDigipadBindings::UP] 	= VK_UP;
	pDigipadMappings[(uint8_t)Win32EmulatorKeyboardDigipadBindings::LEFT] 	= VK_LEFT;
	pDigipadMappings[(uint8_t)Win32EmulatorKeyboardDigipadBindings::RIGHT] 	= VK_RIGHT;
	pDigipadMappings[(uint8_t)Win32EmulatorKeyboardDigipadBindings::DOWN] 	= VK_DOWN;

	pActionButtonMappings[(uint8_t)Win32EmulatorKeyboardActionButtonBindings::A] 		= 'A';
	pActionButtonMappings[(uint8_t)Win32EmulatorKeyboardActionButtonBindings::B] 		= 'S';
	pActionButtonMappings[(uint8_t)Win32EmulatorKeyboardActionButtonBindings::START] 	= VK_RETURN;
	pActionButtonMappings[(uint8_t)Win32EmulatorKeyboardActionButtonBindings::SELECT] 	= VK_SPACE;
}

uint8_t setupEmulator( Win32EmulatorContext* pContext )
{
	const size_t emulatorMemorySizeInBytes = calculateGBEmulatorMemoryRequirementsInBytes();

	//FK: at address 0x100000000 for better debugging of memory errors (eg: access violation > 0x100000000 indicated emulator instance memory is at fault)
	uint8_t* pEmulatorInstanceMemory = (uint8_t*)VirtualAlloc( ( LPVOID )0x100000000, emulatorMemorySizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if( pEmulatorInstanceMemory == nullptr )
	{
		return 0;
	}

	constexpr size_t gbFrameBufferSizeInBytes = calculateGBEmulatorFrameBuffersSizeInBytes();
	uint8_t* pEmulatorBackBufferMemory = (uint8_t*)VirtualAlloc( nullptr, gbFrameBufferSizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	if( pEmulatorBackBufferMemory == nullptr )
	{
		return 0;
	}

	pContext->pEmulatorInstance = createGBEmulatorInstance( pEmulatorInstanceMemory );
	setDefaultKeyboardBinding( pContext->digipadKeyboardMappings, pContext->actionButtonKeyboardMappings );

	return 1;
}

void uploadGBFrameBuffer( HDC pDeviceContext, uint8_t* pGameBoyRGBVideoBuffer, const uint8_t* pGameBoyNativeFrameBuffer )
{
	convertGBFrameBufferToRGB8Buffer( pGameBoyRGBVideoBuffer, pGameBoyNativeFrameBuffer );

	//FK: GB frame finished, upload gb framebuffer to texture
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, gbHorizontalResolutionInPixels, gbVerticalResolutionInPixels,
					 GL_RGB, GL_UNSIGNED_BYTE, pGameBoyRGBVideoBuffer );
}

#if 0
void updateUserMessages( Win32ApplicationContext* pContext )
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);

	const uint8_t oldUserMessageCount = pContext->userMessageCount;
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
#endif

void queryWin32SystemKeys( Win32ApplicationContext* pContext )
{
	Win32EmulatorContext* pEmulatorContext = &pContext->emulatorContext;

	if( GetAsyncKeyState( VK_F2 ) & 0x8000 )
	{
		changeStateSlot( pContext, 1 );
	}

	if( GetAsyncKeyState( VK_F3 ) & 0x8000 )
	{
		changeStateSlot( pContext, 1 );
	}

	if( GetAsyncKeyState( VK_F4 ) & 0x8000 )
	{
		changeStateSlot( pContext, 1 );
	}

	//FK: QuickSave
	if( GetAsyncKeyState( VK_F6 ) & 0x8000 )
	{
		saveEmulatorState( pEmulatorContext );
	}

	//FK: QuickLoad
	if( GetAsyncKeyState( VK_F9 ) & 0x8000 )
	{
		loadEmulatorState( pEmulatorContext );
	}
	
	//FK: Fullscreen
	if( GetAsyncKeyState( VK_F11 ) & 0x8000 )
	{
		toggleFullscreen( pContext );
	}

	if( ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) && pContext->fullscreen == 1 )
	{
		disableFullscreen( pContext );
	}
}

void queryGBEmulatorJoypadState( GBEmulatorJoypadState* pJoypadState, Win32EmulatorContext* pEmulatorContext )
{
	printf("queryGBEmulatorJoypadState\n");
	if( queryControllerInput( pJoypadState, pEmulatorContext->dominantInputType ) )
	{
		pEmulatorContext->dominantInputType = InputType::Gamepad;
	}

	if( queryKeyboardInput( pJoypadState, pEmulatorContext->digipadKeyboardMappings, pEmulatorContext->actionButtonKeyboardMappings, pEmulatorContext->dominantInputType ) )
	{
		pEmulatorContext->dominantInputType = InputType::Keyboard;
	}
}

void runVsyncMainLoop( Win32ApplicationContext* pContext )
{
	uint8_t loopRunning = true;
	MSG msg = {0};

	Win32EmulatorContext* pEmulatorContext = &pContext->emulatorContext;
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

		if( pContext->hasFocus )
		{
			queryWin32SystemKeys( pContext );

			GBEmulatorJoypadState joypadState;
			queryGBEmulatorJoypadState( &joypadState, pEmulatorContext );
			setGBEmulatorJoypadState( pEmulatorContext->pEmulatorInstance, joypadState );
		}

		const uint32_t gbCyclesPerSecond = gbCyclesPerFrame * gbEmulatorFrameRate;

		//FK: TODO: Consider `rest cycles` if refresh rate is not evenly divisible. 
		const uint32_t cyclesPerFrame = gbCyclesPerSecond / pContext->monitorRefreshRate;
		const uint32_t cycleCountForThisHostFrame = cyclesPerFrame * pEmulatorContext->cyclePerHostFrameFactor;
		const GBEmulatorInstanceEventMask emulatorEventMask = runGBEmulatorForCycles( pEmulatorContext->pEmulatorInstance, cycleCountForThisHostFrame );
		if( emulatorEventMask & K15_GB_VBLANK_EVENT_FLAG )
		{
			const uint8_t* pGameBoyNativeFrameBuffer = getGBEmulatorFrameBuffer( pEmulatorContext->pEmulatorInstance );
			uploadGBFrameBuffer( pContext->pDeviceContext, pContext->pGameboyRGBVideoBuffer, pGameBoyNativeFrameBuffer );
		}

		drawGBFrameBuffer( pContext->pDeviceContext );
		w32SwapBuffers( pContext->pDeviceContext );
	}
}

void runNonVsyncMainLoop( Win32ApplicationContext* pContext )
{
	uint8_t loopRunning = true;
	MSG msg = {0};

	LARGE_INTEGER start;
	LARGE_INTEGER end;
	LARGE_INTEGER freq;

	QueryPerformanceFrequency( &freq );

	start.QuadPart = 0;
	end.QuadPart = 0;

	Win32EmulatorContext* pEmulatorContext = &pContext->emulatorContext;
	while (loopRunning)
	{
		QueryPerformanceCounter( &start );

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

		if( pContext->hasFocus )
		{
			queryWin32SystemKeys( pContext );

			GBEmulatorJoypadState joypadState;
			queryGBEmulatorJoypadState( &joypadState, pEmulatorContext );
			setGBEmulatorJoypadState( pEmulatorContext->pEmulatorInstance, joypadState );
		}

		const uint32_t cycleCountForThisHostFrame = gbCyclesPerFrame * pEmulatorContext->cyclePerHostFrameFactor;
		const GBEmulatorInstanceEventMask emulatorEventMask = runGBEmulatorForCycles( pEmulatorContext->pEmulatorInstance, gbCyclesPerFrame );

		//FK: Since we're running with locked 60hz in non-vsync the vblank flag should *always* be set.
		if( emulatorEventMask & K15_GB_VBLANK_EVENT_FLAG )
		{
			const uint8_t* pGameBoyNativeFrameBuffer = getGBEmulatorFrameBuffer( pEmulatorContext->pEmulatorInstance );
			uploadGBFrameBuffer( pContext->pDeviceContext, pContext->pGameboyRGBVideoBuffer, pGameBoyNativeFrameBuffer);
		}

		drawGBFrameBuffer( pContext->pDeviceContext );
		w32SwapBuffers( pContext->pDeviceContext );

		QueryPerformanceCounter( &end );

		//FK: run with 60hz when vsync is disabled
		const uint32_t fixedFrameTimeInMicroseconds = 16666u;
		const uint32_t frameTimeInMicroseconds		= (uint32_t)( ( ( end.QuadPart - start.QuadPart ) * 1000000u )/freq.QuadPart );
		if( frameTimeInMicroseconds < fixedFrameTimeInMicroseconds )
		{
			LARGE_INTEGER sleepTime;
			sleepTime.QuadPart = (LONGLONG)(( fixedFrameTimeInMicroseconds - frameTimeInMicroseconds ) ) * -1;
			w32NtDelayExecution( FALSE, &sleepTime );
		}	
	}
}

int CALLBACK WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nShowCmd)
{
	Win32ApplicationContext appContext;
	appContext.pInstanceHandle = hInstance;

	allocateDebugConsole();

	loadWin32FunctionPointers();
	loadXInputFunctionPointers();

	if( !setupUi( &appContext ) )
	{
		DebugBreak();
		return 1;
	}

	setupOpenGL( &appContext );
	setupEmulator( &appContext.emulatorContext );
	
	Win32GBEmulatorArguments args;
	parseCommandLineArguments( &args, lpCmdLine );

	if( args.romPath[0] != 0 )
	{
		loadRomFile( &appContext, args.romPath );
	}

	if( appContext.vsyncEnabled )
	{
		runVsyncMainLoop( &appContext );
	}
	else
	{
		runNonVsyncMainLoop( &appContext );
	}

	return 0;
}

#if 0
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
}
#endif