#ifndef K15_GB_NETWORK
#define K15_GB_NETWORK

#include "k15_types.h"

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

inline EmulatorPacket createPingEmulatorPacket( EmulatorHostPlatform hostPlatform )
{
	EmulatorPacket pingPacket = {};
	pingPacket.header.fourcc 	= EmulatorPacketFourCC;
	pingPacket.header.type 		= EmulatorPacketType::PING;
	pingPacket.payload.pingPayload.platform = hostPlatform;

	return pingPacket;
}

#endif //K15_GB_NETWORK