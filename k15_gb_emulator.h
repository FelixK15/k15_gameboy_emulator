#include "stdint.h"
#include "stdio.h"
#include "assert.h"

#define K15_ASSERT_UNKNOWN_INSTRUCTION 1

struct GBZ80CpuState
{
    uint16_t cycleCount;
    uint16_t programCounter;
    struct registers
    {
        union
        {
            struct
            {
                uint8_t A;
                uint8_t F;
            };

            uint16_t AF;
        };

        union
        {
            struct
            {
                uint8_t B;
                uint8_t C;
            };

            uint16_t BC;
        };

        union
        {
            struct
            {
                uint8_t D;
                uint8_t E;
            };

            uint16_t DE;
        };

        union
        {
            struct
            {
                uint8_t H;
                uint8_t L;
            };

            uint16_t HL;
        };

        union
        {
            struct 
            {
                uint8_t Z   : 1;
                uint8_t N   : 1;
                uint8_t H   : 1;
                uint8_t C   : 1;
                uint8_t NOP : 4;
            };
            
            uint8_t value;
        } Flags;

        uint16_t SP;
        uint16_t PC;

    } cpuRegisters;
};

enum GBZ80SpriteAttributeFlags
{
    GBZ80SpriteAttributeFlags_Priority      = (1 << 7),
    GBZ80SpriteAttributeFlags_YFlip         = (1 << 6),
    GBZ80SpriteAttributeFlags_XFlip         = (1 << 5),
    GBZ80SpriteAttributeFlags_PaletteNumber = (1 << 4),
};

enum CartridgeType : uint8_t
{
    ROM_ONLY                    = 0x00,
    ROM_MBC1                    = 0x01,
    ROM_MBC1_RAM                = 0x02,
    ROM_MBC1_RAM_BATT           = 0x03,
    ROM_MBC2                    = 0x05,
    ROM_MBC2_BATT               = 0x06,
    ROM_RAM                     = 0x08,
    ROM_RAM_BATT                = 0x09,
    ROM_MMMD1                   = 0x0B,
    ROM_MMMD1_SRAM              = 0x0C,
    ROM_MMMD1_SRAM_BATT         = 0x0D,
    ROM_MBC3_TIMER_BATT         = 0x0F,
    ROM_MBC3_TIMER_RAM_BATT     = 0x10,
    ROM_MBC3                    = 0x11,
    ROM_MBC3_RAM                = 0x12,
    ROM_MBC3_RAM_BATT           = 0x13,
    ROM_MBC5                    = 0x19,
    ROM_MBC5_RAM                = 0x1A,
    ROM_MBC5_RAM_BATT           = 0x1B,
    ROM_MBC5_RUMBLE             = 0x1C,
    ROM_MBC5_RUMBLE_SRAM        = 0x1D,
    ROM_MBC5_RUMBLE_SRAM_BATT   = 0x1E,
    POCKET_CAMERA               = 0x1F,
    BANDAI_TAMA5                = 0xFD,
    HUDSON_HUC_3                = 0xFE,
};

struct GBZ80SpriteAttributes
{
    uint8_t x;
    uint8_t y;
    uint8_t pattern;
    uint8_t flags;
};

struct GBZ80MemoryMapper
{
    uint8_t*        pBaseAddress;
    const uint8_t*  pRomBaseAddress;
};

uint16_t read16BitValueFromAddress( const uint8_t* pAddress )
{
    const uint8_t ls = *pAddress++;
    const uint8_t hs = *pAddress++;
    return (hs << 8u) | (ls << 0u);
}

uint8_t read8BitValueFromAddress( const uint8_t* pAddress )
{
    return *pAddress;
}

void write8BitValueToMappedMemory( GBZ80MemoryMapper* pMemoryMapper, uint16_t addressOffset, uint8_t value )
{
    pMemoryMapper->pBaseAddress[addressOffset] = value;

    //FK: Echo 8kB internal Ram
    if( addressOffset >= 0xC000 && addressOffset < 0xDE00 )
    {
        addressOffset += 0x2000;
        pMemoryMapper->pBaseAddress[addressOffset] = value;
    }
    else if( addressOffset >= 0xE000 && addressOffset < 0xFE00 )
    {
        addressOffset -= 0x2000;
        pMemoryMapper->pBaseAddress[addressOffset] = value;
    }
}

void mapRomMemory( GBZ80MemoryMapper* pMemoryMapper, const uint8_t* pRomMemory, size_t romMemorySizeInBytes )
{
    pMemoryMapper->pRomBaseAddress = pRomMemory;

    CartridgeType cartridgeType = (CartridgeType)*(pMemoryMapper->pRomBaseAddress + 0x147);
    switch(cartridgeType)
    {
        case ROM_ONLY:
        {
            //FK: Map rom memory to 0x0000-0x7FFF
            memset(pMemoryMapper->pBaseAddress, 0, 0x8000);
            memcpy(pMemoryMapper->pBaseAddress, pMemoryMapper->pRomBaseAddress, romMemorySizeInBytes);
            break;
        }
        default:
        {
            printf("Cartridge type '%d' not supported.\n", cartridgeType);
            assert(false);
        }
    }
}

void startEmulator( const uint8_t* pRomMemory, size_t romMemorySizeInBytes )
{
    GBZ80CpuState state;
    memset(&state, 0, sizeof(state));

    GBZ80MemoryMapper memoryMapper;
    memoryMapper.pBaseAddress = (uint8_t*)malloc(0xFFFF);
    mapRomMemory(&memoryMapper, pRomMemory, romMemorySizeInBytes);

    //FK: Start fetching instructions at address $1000
    state.programCounter    = 0x0100;
    state.cycleCount        = 0u;

    while(true)
    {
        const uint8_t opcode = memoryMapper.pBaseAddress[state.programCounter++];
        switch( opcode )
        {
            //NOP
            case 0x00:
                break;

            //LD A,(BC)
            case 0x0A:
                state.cpuRegisters.A = memoryMapper.pBaseAddress[state.cpuRegisters.BC];
                state.cycleCount += 8;
                break;

            //LD A,(DE)
            case 0x1A:
                state.cpuRegisters.A = memoryMapper.pBaseAddress[state.cpuRegisters.DE];
                state.cycleCount += 8;
                break;

            //LD A,(nn)
            case 0xFA:
            {
                const uint16_t address = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                state.cpuRegisters.A = memoryMapper.pBaseAddress[address];
                state.programCounter += 2;
                state.cycleCount     += 16;
                break;     
            }
                
            //LD A,#
            case 0x3E:
                state.cpuRegisters.A = memoryMapper.pBaseAddress[state.programCounter++];
                state.cycleCount += 8;
                break;

            //LD B,n
            case 0x06:  
                state.cpuRegisters.B = memoryMapper.pBaseAddress[state.programCounter++];
                break;

             //LD C,n
            case 0x0E:
                state.cpuRegisters.C = memoryMapper.pBaseAddress[state.programCounter++];
                break;

            //LD D,n
            case 0x16: 
                state.cpuRegisters.D = memoryMapper.pBaseAddress[state.programCounter++];
                break;
                
            //LD E,n
            case 0x1E: 
                state.cpuRegisters.E = memoryMapper.pBaseAddress[state.programCounter++];
                break;
            
             //LD H,n
            case 0x26:
                state.cpuRegisters.H = memoryMapper.pBaseAddress[state.programCounter++];
                break;

            //LD L,n
            case 0x2E:
                state.cpuRegisters.L = memoryMapper.pBaseAddress[state.programCounter++];
                break;
            
            //LD n, n
            case 0x70:
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x7F:
            case 0x78:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7E:
            case 0x36:
            case 0x40:
            case 0x41:
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66:
            case 0x68:
            case 0x69:
            case 0x6A:
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E:
            {
                uint8_t* pTargetRegister    = nullptr;
                uint8_t value               = 0;
                uint8_t cycleCost           = 0;
                switch(opcode)
                {
                    //LD A, A
                    case 0x7F:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.A;
                        cycleCost       = 4;
                        break;
                    //LD A, B
                    case 0x78:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD A, C
                    case 0x79:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD A, D
                    case 0x7A:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD A, E
                    case 0x7B:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD A, H
                    case 0x7C:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD A, L
                    case 0x7D:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD A, (HL)
                    case 0x7E:
                        pTargetRegister = &state.cpuRegisters.A;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD B, B
                    case 0x40:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD B, C
                    case 0x41:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD B, D
                    case 0x42:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD B, E
                    case 0x43:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD B, H
                    case 0x44:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD B, L
                    case 0x45:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD B, (HL)
                    case 0x46:
                        pTargetRegister = &state.cpuRegisters.B;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD C, B
                    case 0x48:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD C, C
                    case 0x49:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD C, D
                    case 0x4A:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD C, E
                    case 0x4B:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD C, H
                    case 0x4C:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD C, L
                    case 0x4D:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD C, (HL)
                    case 0x4E:
                        pTargetRegister = &state.cpuRegisters.C;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD D, B
                    case 0x50:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD D, C
                    case 0x51:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD D, D
                    case 0x52:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD D, E
                    case 0x53:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD D, H
                    case 0x54:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD D, L
                    case 0x55:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD B, (HL)
                    case 0x56:
                        pTargetRegister = &state.cpuRegisters.D;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD E, B
                    case 0x58:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD E, C
                    case 0x59:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD E, D
                    case 0x5A:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD E, E
                    case 0x5B:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD E, H
                    case 0x5C:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD E, L
                    case 0x5D:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD E, (HL)
                    case 0x5E:
                        pTargetRegister = &state.cpuRegisters.E;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD H, B
                    case 0x60:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD H, C
                    case 0x61:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD H, D
                    case 0x62:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD H, E
                    case 0x63:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD H, H
                    case 0x64:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD H, L
                    case 0x65:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD H, (HL)
                    case 0x66:
                        pTargetRegister = &state.cpuRegisters.H;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;

                    //LD L, B
                    case 0x68:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD L, C
                    case 0x69:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD L, D
                    case 0x6A:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD L, E
                    case 0x6B:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD L, H
                    case 0x6C:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD L, L
                    case 0x6D:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = state.cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD L, (HL)
                    case 0x6E:
                        pTargetRegister = &state.cpuRegisters.L;
                        value           = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        cycleCost       = 8;
                        break;
                        
                    //LD (HL), B
                    case 0x70:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.B;
                        cycleCost       = 8;
                        break;
                    //LD (HL), C
                    case 0x71:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.C;
                        cycleCost       = 8;
                        break;
                    //LD (HL), D
                    case 0x72:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.D;
                        cycleCost       = 8;
                        break;
                    //LD (HL), E
                    case 0x73:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.E;
                        cycleCost       = 8;
                        break;
                    //LD (HL), H
                    case 0x74:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.H;
                        cycleCost       = 8;
                        break;
                    //LD (HL), L
                    case 0x75:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = state.cpuRegisters.L;
                        cycleCost       = 8;
                        break;
                    //LD (HL), n
                    case 0x36:
                        pTargetRegister = &memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        value           = memoryMapper.pBaseAddress[state.programCounter++];
                        cycleCost       = 12;
                        break;
                }

                *pTargetRegister = value;
                state.cycleCount += cycleCost;
            }

            //JP nn
            case 0xC3:
            {
                state.cycleCount += 12u;

                const uint16_t addressToJumpTo = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                state.programCounter = addressToJumpTo;
                break;
            }

            //JR n
            case 0x20:
            case 0x28:
            case 0x30:
            case 0x38:
            {
                int8_t addressOffset = (int8_t)memoryMapper.pBaseAddress[state.programCounter++];
                uint8_t checkMask   = 0;
                uint8_t bitValue    = 0;
                switch(opcode)
                {
                    case 0x20:
                        checkMask = (1<<0);
                        bitValue  = 0;
                        break;
                    case 0x28:
                        checkMask = (1<<0);
                        bitValue  = (1<<0);
                        break;
                    case 0x30:
                        checkMask = (1<<3);
                        bitValue  = 0;
                        break;
                    case 0x38:
                        checkMask = (1<<3);
                        bitValue  = (1<<3);
                        break;
                }

                if( ( checkMask & state.cpuRegisters.Flags.value ) == bitValue )
                {
                    state.programCounter += addressOffset;
                }

                state.cycleCount += 8;
                break;
            }

            //XOR A, n
            case 0xAF:
            case 0xA8:
            case 0xA9:
            case 0xAA:
            case 0xAB:
            case 0xAC:
            case 0xAD:
            case 0xAE:
            case 0xEE:
            {
                uint8_t operand = 0;
                switch(opcode)
                {
                    case 0xAF:
                        operand = state.cpuRegisters.A;
                        state.cycleCount += 4;
                        break;
                    case 0xA8:
                        operand = state.cpuRegisters.B;
                        state.cycleCount += 4;
                        break;
                    case 0xA9:
                        operand = state.cpuRegisters.C;
                        state.cycleCount += 4;
                        break;
                    case 0xAA:
                        operand = state.cpuRegisters.D;
                        state.cycleCount += 4;
                        break;
                    case 0xAB:
                        operand = state.cpuRegisters.E;
                        state.cycleCount += 4;
                        break;
                    case 0xAC:
                        operand = state.cpuRegisters.H;
                        state.cycleCount += 4;
                        break;
                    case 0xAD:
                        operand = state.cpuRegisters.L;
                        state.cycleCount += 4;
                        break;
                    case 0xAE:
                        operand = read8BitValueFromAddress(memoryMapper.pBaseAddress + state.cpuRegisters.HL);
                        state.cycleCount += 8;
                        break;
                    case 0xEE:
                        operand = memoryMapper.pBaseAddress[state.programCounter++];
                        state.cycleCount += 8;
                        break;
                }
                
                state.cpuRegisters.A            = state.cpuRegisters.A ^ operand;
                state.cpuRegisters.Flags.value  = 0x0;
                state.cpuRegisters.Flags.Z      = (state.cpuRegisters.A == 0);
               
                break;
            }

            //LD n,nn (16bit loads)
            case 0x01:
            case 0x11:
            case 0x21:
            case 0x31:
            {
                switch(opcode)
                {
                    case 0x01:
                        state.cpuRegisters.BC = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                        break;
                    case 0x11:
                        state.cpuRegisters.DE = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                        break;
                    case 0x21:
                        state.cpuRegisters.HL = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                        break;
                    case 0x31:
                        state.cpuRegisters.SP = read16BitValueFromAddress(memoryMapper.pBaseAddress + state.programCounter);
                        break;
                }
                state.programCounter += 2u;
                state.cycleCount += 12;
            }
            break;

            //LDD (HL),A
            case 0x32:
            {
                write8BitValueToMappedMemory(&memoryMapper, state.cpuRegisters.HL, state.cpuRegisters.A);
                --state.cpuRegisters.HL;
                state.cycleCount += 8;
                break;
            }

            //DEC n
            case 0x3D:
            case 0x05:
            case 0x0D:
            case 0x15:
            case 0x1D:
            case 0x25:
            case 0x2D:
            {
                uint8_t* pRegisterToDecrement = nullptr;
                switch(opcode)
                {
                    //DEC A
                    case 0x3D:
                        pRegisterToDecrement = &state.cpuRegisters.A;
                        break;
                    //DEC B
                    case 0x05:
                        pRegisterToDecrement = &state.cpuRegisters.B;
                        break;
                    //DEC C
                    case 0x0D:
                        pRegisterToDecrement = &state.cpuRegisters.C;
                        break;
                    //DEC D
                    case 0x15:
                        pRegisterToDecrement = &state.cpuRegisters.D;
                        break;
                    //DEC E
                    case 0x1D:
                        pRegisterToDecrement = &state.cpuRegisters.E;
                        break;
                    //DEC H
                    case 0x25:
                        pRegisterToDecrement = &state.cpuRegisters.H;
                        break;
                    //DEC L
                    case 0x2D:
                        pRegisterToDecrement = &state.cpuRegisters.L;
                        break;
                }
                --*pRegisterToDecrement;

                state.cpuRegisters.Flags.Z = (*pRegisterToDecrement == 0);
                state.cpuRegisters.Flags.N = 1;
                state.cycleCount += 4;
                break;
            }

            //LDH (n),A
            case 0xE0:
            {
                const uint16_t address = 0xFF00 + memoryMapper.pBaseAddress[state.programCounter++];
                write8BitValueToMappedMemory(&memoryMapper, address, state.cpuRegisters.A);
                state.cycleCount += 12;
                break;
            }

            //LDH A,(n)
            case 0xF0:
            {
                const uint16_t address = 0xFF00 + memoryMapper.pBaseAddress[state.programCounter++];
                state.cpuRegisters.A = memoryMapper.pBaseAddress[address];
                state.cycleCount += 12;
                break;
            }

            //DI
            case 0xF3:
            {
                //state.disableInterruptPendingCounter = 2;
                state.cycleCount += 4;
                break;
            }

            //EI
            case 0xFB:
            {   
                //state.enableInterruptPendingCounter = 2;
                state.cycleCount += 4;
                break;
            }

            //DEC (HL)
            case 0x35:
            {
                uint8_t* pValue = memoryMapper.pBaseAddress + state.cpuRegisters.HL;
                --*pValue;
                state.cpuRegisters.Flags.Z = (*pValue == 0);
                state.cpuRegisters.Flags.N = 1;
                state.cycleCount += 12;
                break;
            }

            // CP n
            case 0xBF:
            case 0xB8:
            case 0xB9:
            case 0xBA:
            case 0xBB:
            case 0xBC:
            case 0xBD:
            case 0xBE:
            case 0xFE:
            {
                uint8_t value = 0;
                switch(opcode)
                {
                    //CP A
                    case 0xBF:
                        value = state.cpuRegisters.A;
                        state.cycleCount += 4;
                        break;
                    //CP B
                    case 0xB8:
                        value = state.cpuRegisters.B;
                        state.cycleCount += 4;
                        break;
                    //CP C
                    case 0xB9:
                        value = state.cpuRegisters.C;
                        state.cycleCount += 4;
                        break;
                    //CP D
                    case 0xBA:
                        value = state.cpuRegisters.D;
                        state.cycleCount += 4;
                        break;
                    //CP E
                    case 0xBB:
                        value = state.cpuRegisters.E;
                        state.cycleCount += 4;
                        break;
                    //CP H
                    case 0xBC:
                        value = state.cpuRegisters.H;
                        state.cycleCount += 4;
                        break;
                    //CP L
                    case 0xBD:
                        value = state.cpuRegisters.L;
                        state.cycleCount += 4;
                        break;
                    //CP (HL)
                    case 0xBE:
                        value = memoryMapper.pBaseAddress[state.cpuRegisters.HL];
                        state.cycleCount += 8;
                        break;
                    //CP #
                    case 0xFE:
                        value = memoryMapper.pBaseAddress[state.programCounter++];
                        state.cycleCount += 8;
                        break;
                }

                state.cpuRegisters.Flags.N = 1;
                state.cpuRegisters.Flags.Z = (value == state.cpuRegisters.A);
                state.cpuRegisters.Flags.C = (value < state.cpuRegisters.A);
                break;
            }

            default:
                printf( "Unknown opcode '%hhX", opcode );

#if K15_ASSERT_UNKNOWN_INSTRUCTION == 1
                assert(false);
#endif
        }
    }
}