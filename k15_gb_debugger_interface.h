#ifndef K15_GB_NETWORK
#define K15_GB_NETWORK

#include "k15_types.h"

#define K15_MAX_COMPUTER_NAME_LENGTH  64
#define K15_DEBUGGER_SENT_BUFFER_SIZE_IN_BYTES Mbyte(1)
#define K15_DEBUGGER_RECV_BUFFER_SIZE_IN_BYTES Mbyte(1)

enum class DebuggerPacketType : uint8_t
{
	BROADCAST,
};

struct DebuggerPacketHeader
{
	fourcc32_t 			fourcc;
	DebuggerPacketType 	type;
};

struct DebuggerPacket
{
	DebuggerPacketHeader header;
};

enum class EmulatorPacketType : uint8_t
{
	PING,
	EMULATOR_SETUP,
	EMULATOR_MEMORY,
	EMULATOR_OPCODE
};

struct EmulatorPacketHeader
{
	fourcc32_t 			fourcc;
	EmulatorPacketType 	type;
};

enum class EmulatorHostPlatform : uint8_t
{
	Windows,
	Linux,
	OSX,
	Android,
	iOS
};

union EmulatorPacketPayload
{
	struct {
		EmulatorHostPlatform platform;	
		char 				 computerName[K15_MAX_COMPUTER_NAME_LENGTH];
	} pingPayload;
};

struct EmulatorPacket
{
	EmulatorPacketHeader 	header;
	EmulatorPacketPayload 	payload;
};

constexpr uint16_t 			DebuggerBroadcastPort 	= 4066;
constexpr uint16_t 			DebuggerPort 			= 5066;
constexpr fourcc32_t        DebuggerPacketFourCC = K15_FOUR_CC('G', 'B', 'D', 'B');
constexpr DebuggerPacket    BroadcastPacket = { DebuggerPacketFourCC, DebuggerPacketType::BROADCAST };

constexpr fourcc32_t 		EmulatorPacketFourCC = K15_FOUR_CC('G', 'B', 'E', 'M');

inline bool8_t isValidDebuggerPacket( const DebuggerPacket* pPacket )
{
    return pPacket->header.fourcc == DebuggerPacketFourCC;
}

inline bool8_t isValidEmulatorPacket( const EmulatorPacket* pPacket )
{
    return pPacket->header.fourcc == EmulatorPacketFourCC;
}

inline EmulatorPacket createPingEmulatorPacket( EmulatorHostPlatform hostPlatform, const char* pComputerName )
{
	EmulatorPacket pingPacket = {};
	pingPacket.header.fourcc 	= EmulatorPacketFourCC;
	pingPacket.header.type 		= EmulatorPacketType::PING;
	pingPacket.payload.pingPayload.platform 	= hostPlatform;
	strcpy_s( pingPacket.payload.pingPayload.computerName, K15_MAX_COMPUTER_NAME_LENGTH, pComputerName );

	return pingPacket;
}

inline EmulatorPacket createEmulatorSetupPacket( )

#endif //K15_GB_NETWORK