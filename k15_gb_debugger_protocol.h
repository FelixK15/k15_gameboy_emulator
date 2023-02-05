#include "k15_gb_emulator_types.h"

#define K15_MAX_COMPUTER_NAME_LENGTH  64
#define K15_DEBUGGER_MAX_MESSAGE_SIZE_IN_BYTES sizeof( EmulatorMemoryMessage )
#define K15_DEBUGGER_SEND_BUFFER_SIZE_IN_BYTES Mbyte(1)
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

struct DatagramSocket
{
	SOCKET socket;
};

struct StreamSocket
{
	SOCKET socket;
};

enum class EmulatorMessageType : uint8_t
{
	CONNECT,
	CONNECT_ACK,
	PING,
	CPU_REGISTERS,
	CPU_INSTRUCTION,
	MEMORY,
	ROM_HEADER,
};

struct EmulatorMessageHeader
{
	fourcc32_t 				fourcc;
	EmulatorMessageType 	type;
};

enum class EmulatorHostPlatform : uint8_t
{
	Windows,
	Linux,
	OSX,
	Android,
	iOS
};

struct BaseEmulatorMessage
{
	EmulatorMessageHeader 	header;
};

struct EmulatorPingMessage : public BaseEmulatorMessage
{
	EmulatorHostPlatform 	platform;
	uint8_t 				protocolVersion;
	char 					computerName[ K15_MAX_COMPUTER_NAME_LENGTH ];
};

struct EmulatorCpuRegisterMessage : public BaseEmulatorMessage
{
	GBCpuRegisters 			cpuRegisters;
};

struct EmulatorMemoryMessage : public BaseEmulatorMessage
{
	uint8_t 				memory[ gbMappedMemorySizeInBytes ];
};

struct EmulatorRomHeaderMessage : public BaseEmulatorMessage
{
	GBRomHeader 			romHeader;
};

struct EmulatorCpuInstructionMessage : public BaseEmulatorMessage
{
	GBEmulatorCpuInstruction instruction;
};

constexpr uint16_t 			DebuggerBroadcastPort 	= 4066;
constexpr uint16_t 			DebuggerPort 			= 5066;
constexpr uint8_t 			ProtocolMinorVersion 	= 0;
constexpr uint8_t 			ProtocolMajorVersion 	= 1u << 4u;
constexpr uint8_t 			ProtocolVersion 		= (ProtocolMajorVersion | ProtocolMinorVersion );
constexpr fourcc32_t        DebuggerPacketFourCC = K15_FOUR_CC('G', 'B', 'D', 'B');
constexpr DebuggerPacket    BroadcastPacket = { DebuggerPacketFourCC, DebuggerPacketType::BROADCAST };

constexpr fourcc32_t 		EmulatorMessageFourCC = K15_FOUR_CC('G', 'B', 'E', 'M');

inline EmulatorMessageHeader createEmulatorMessageHeader( EmulatorMessageType messageType )
{
	EmulatorMessageHeader header = {};
	header.fourcc = EmulatorMessageFourCC;
	header.type = messageType;

	return header;
}

inline bool8_t isValidDebuggerPacket( const DebuggerPacket* pPacket )
{
    return pPacket->header.fourcc == DebuggerPacketFourCC;
}

inline bool8_t isValidEmulatorMessageHeader( const EmulatorMessageHeader* pHeader )
{
    return pHeader->fourcc == EmulatorMessageFourCC;
}

inline EmulatorPingMessage createEmulatorPingMessage( EmulatorHostPlatform hostPlatform, const char* pComputerName )
{
	EmulatorPingMessage pingMessage = {};
	pingMessage.header 			= createEmulatorMessageHeader( EmulatorMessageType::PING );
	pingMessage.platform 		= hostPlatform;
	pingMessage.protocolVersion = ProtocolVersion;
	strcpy_s( pingMessage.computerName, K15_MAX_COMPUTER_NAME_LENGTH, pComputerName );

	return pingMessage;
}

inline EmulatorCpuRegisterMessage createEmulatorCpuRegistersMessage( GBCpuRegisters cpuRegisters )
{
	EmulatorCpuRegisterMessage registersMessage = {};
	registersMessage.header = createEmulatorMessageHeader( EmulatorMessageType::CPU_REGISTERS );
	registersMessage.cpuRegisters = cpuRegisters;

	return registersMessage;
}

inline EmulatorMemoryMessage createEmulatorMemoryMessage( const uint8_t* pMemory )
{
	EmulatorMemoryMessage memoryMessage = {};
	memoryMessage.header = createEmulatorMessageHeader( EmulatorMessageType::MEMORY );
	memcpy( memoryMessage.memory, pMemory, gbMappedMemorySizeInBytes );

	return memoryMessage;
}

inline EmulatorCpuInstructionMessage createEmulatorCpuInstructionMessage( uint16_t address, uint8_t opcode, uint8_t opcodeByteCount )
{
	EmulatorCpuInstructionMessage cpuInstructionMessage = {};
	cpuInstructionMessage.header 							= createEmulatorMessageHeader( EmulatorMessageType::CPU_INSTRUCTION );
	cpuInstructionMessage.instruction.address				= address;
	cpuInstructionMessage.instruction.opcode 				= opcode;
	cpuInstructionMessage.instruction.argumentByteCount 	= opcodeByteCount;

	return cpuInstructionMessage;
}

bool8_t receiveFromSocket( SOCKET socket, void* pBuffer, int bytesToReceive, IN_ADDR* pOutAddress )
{
	char* pBufferPtr = ( char* )pBuffer;
	int receivedBytes = 0;

	struct sockaddr_in senderAddress = {};
	int senderAddressSizeInBytes = sizeof(senderAddress);
	while( receivedBytes != bytesToReceive )
	{
		const int recvReturnValue = recvfrom( socket, pBufferPtr + receivedBytes, bytesToReceive - receivedBytes, 0, (struct sockaddr*)&senderAddress, &senderAddressSizeInBytes );
		if( recvReturnValue != SOCKET_ERROR )
		{
			receivedBytes += recvReturnValue;
			continue;
		}

		return false;
	};

	if( pOutAddress != nullptr )
	{
		*pOutAddress = senderAddress.sin_addr;
	}

	return true;
}

bool8_t sendToSocket(SOCKET socket, uint16_t port, IN_ADDR address, const void* pBuffer, int bufferSizeInBytes )
{
	const char* pBufferPtr = ( const char* )pBuffer;
    int bytesSent = 0;
	struct sockaddr_in receivingAddress = {};
	receivingAddress.sin_addr = address;
	receivingAddress.sin_family = AF_INET;
	receivingAddress.sin_port = port;
    while( bytesSent != bufferSizeInBytes )
    {
        const int sendReturnValue = sendto( socket, pBufferPtr + bytesSent, bufferSizeInBytes - bytesSent, 0u, ( const struct sockaddr* )&receivingAddress, sizeof( receivingAddress ) );
        if( sendReturnValue == SOCKET_ERROR )
        {
            return false;
        }

        bytesSent += sendReturnValue;
    }

    return true;
}

