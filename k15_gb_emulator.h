#include "stdint.h"
#include "stdio.h"
#include "assert.h"

#define K15_ASSERT_UNKNOWN_INSTRUCTION 1

#define Kbyte(x) (x)*1024
#define Mbyte(x) Kbyte(x)*1024

#define Kbit(x) Kbyte(x)/8
#define Mbit(x) Mbyte(x)/8

struct GBCpuState
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

enum GBSpriteAttributeFlags : uint8_t
{
    Priority      = (1 << 7),
    YFlip         = (1 << 6),
    XFlip         = (1 << 5),
    PaletteNumber = (1 << 4),
};

enum GBCartridgeType : uint8_t
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

struct GBSpriteAttributes
{
    uint8_t x;
    uint8_t y;
    uint8_t pattern;
    uint8_t flags;
};

struct GBMemoryMapper
{
    uint8_t*    pBaseAddress;
    uint8_t*    pRomBank0;
    uint8_t*    pRomBankSwitch;
    uint8_t*    pVideoRAM;
    uint8_t*    pRamBankSwitch;
    uint8_t*    pLowRam;
    uint8_t*    pLowRamEcho;
    uint8_t*    pSpriteAttributes;
    uint8_t*    IOPorts;
    uint8_t*    pHighRam;
    uint8_t*    pInterruptRegister;
};

struct GBRomHeader
{
    uint8_t reserved[4];        
    uint8_t nintendoLogo[48];
    char    gameTitle[16];
    uint8_t colorCompatibility;
    uint8_t licenseHigh;
    uint8_t licenseLow;
    uint8_t superGameBoyCompatibility;
    GBCartridgeType cartridgeType;
    uint8_t romSize;
    uint8_t ramSize;
    uint8_t licenseCode;
    uint8_t maskRomVersion;
    uint8_t complementCheck;
    uint8_t checksumHigher;
    uint8_t checksumLower;
};

struct GBEmulatorInstance
{
    GBCpuState* pCpuState;
    GBMemoryMapper* pMemoryMapper;
};

uint8_t read8BitValueFromAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    return pMemoryMapper->pBaseAddress[addressOffset];
}

uint16_t read16BitValueFromAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    const uint8_t ls = read8BitValueFromAddress( pMemoryMapper, addressOffset + 0);
    const uint8_t hs = read8BitValueFromAddress( pMemoryMapper, addressOffset + 1);
    return (hs << 8u) | (ls << 0u);
}

uint8_t* getMemoryAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset)
{
    return pMemoryMapper->pBaseAddress + addressOffset;
}

void write8BitValueToMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset, uint8_t value )
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

GBRomHeader getGBRomHeader(const uint8_t* pRomMemory)
{
    GBRomHeader header;
    memcpy(&header, pRomMemory + 0x0100, sizeof(GBRomHeader));
    return header;
}

size_t mapRomSizeToByteSize(uint8_t romSize)
{
    switch(romSize)
    {
        case 0x00:
            return Kbit(256u);
        case 0x01:
            return Kbit(512u);
        case 0x02:
            return Mbit(1u);
        case 0x03:
            return Mbit(2u);
        case 0x04:
            return Mbit(4u);
        case 0x05:
            return Mbit(8u);
        case 0x06:
            return Mbit(16u);
        case 0x52:
            return Mbit(9);
        case 0x53:
            return Mbit(10);
        case 0x54:
            return Mbit(12);
    }

    assert(false);
    return 0;
}

void mapRomMemory( GBMemoryMapper* pMemoryMapper, const uint8_t* pRomMemory )
{
    const GBRomHeader romHeader = getGBRomHeader(pRomMemory);
    const size_t romSizeInBytes = mapRomSizeToByteSize(romHeader.romSize);
    switch(romHeader.cartridgeType)
    {
        case ROM_ONLY:
        {
            //FK: Map rom memory to 0x0000-0x7FFF
            memset(pMemoryMapper->pBaseAddress, 0, 0x8000);
            memcpy(pMemoryMapper->pBaseAddress, pRomMemory, romSizeInBytes);
            break;
        }
        default:
        {
            printf("Cartridge type '%d' not supported.\n", romHeader.cartridgeType);
            assert(false);
        }
    }
}

void initGBCpuState( GBCpuState* pState )
{
    memset(pState, 0, sizeof(GBCpuState));

    //FK: (Gameboy cpu manual) The GameBoy stack pointer 
    //is initialized to $FFFE on power up but a programmer 
    //should not rely on this setting and rather should 
    //explicitly set its value.

    pState->cpuRegisters.SP = 0xFFFE;
}

struct x86Flags
{
    uint8_t carry               : 1;
    uint8_t reserved0           : 1;
    uint8_t parity              : 1;
    uint8_t reserved1           : 1;
    uint8_t adjust              : 1;
    uint8_t reserved2           : 1;
    uint8_t zero                : 1;
    uint8_t sign                : 1;
    uint8_t trap                : 1;
    uint8_t interruptEnable     : 1;
    uint8_t direction           : 1;
    uint8_t overflow            : 1;
    uint8_t ioPrivilegeLevel    : 1;
    uint8_t nestedTask          : 1;
    uint8_t reserved3           : 1;
};

x86Flags readX86CpuFlags()
{
    uint16_t cpuFlagsValue = 0;
     __asm
    {
        pushf                   ; push 16bit flags onto stack
        pop bx                  ; pop stack value into bx (bx = flags)
        mov cpuFlagsValue, bx   ; store value of bx register into 'cpuFlagsValue'
    };
    
    x86Flags cpuFlags;
    memcpy(&cpuFlags, &cpuFlagsValue, sizeof(x86Flags));
    return cpuFlags;
}

void compareValue( GBCpuState* pCpuState, uint8_t value )
{
    const uint8_t compareResult = pCpuState->cpuRegisters.A - value;
    const x86Flags flagsRegister = readX86CpuFlags();

    pCpuState->cpuRegisters.Flags.N = 1;
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.C = flagsRegister.carry;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
}

void decrementValue( GBCpuState* pCpuState, uint8_t* pValueAddress )
{
    --*pValueAddress;
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
    pCpuState->cpuRegisters.Flags.N = 1;
}

void initMemoryMapper( GBMemoryMapper* pMapper, uint8_t* pMemory )
{
    pMapper->pBaseAddress           = pMemory;
    pMapper->pRomBank0              = pMemory;
    pMapper->pRomBankSwitch         = pMemory + 0x3FFF;
    pMapper->pVideoRAM              = pMemory + 0x7FFF;
    pMapper->pRamBankSwitch         = pMemory + 0x9FFF;
    pMapper->pLowRam                = pMemory + 0xBFFF;
    pMapper->pLowRamEcho            = pMemory + 0xDFFF;
    pMapper->pSpriteAttributes      = pMemory + 0xFDFF;
    pMapper->IOPorts                = pMemory + 0xFEFF;
    pMapper->pHighRam               = pMemory + 0xFF7F;
    pMapper->pInterruptRegister     = pMemory + 0xFFFE;
}

size_t calculateEmulatorInstanceMemoryRequirementsInBytes()
{
    const size_t memoryRequirementsInBytes = sizeof(GBEmulatorInstance) + sizeof(GBCpuState) + sizeof(GBMemoryMapper) + 0xFFFF;
    return memoryRequirementsInBytes;
}

GBEmulatorInstance* createEmulatorInstance( uint8_t* pEmulatorInstanceMemory )
{
    GBEmulatorInstance* pEmulatorInstance = (GBEmulatorInstance*)pEmulatorInstanceMemory;
    pEmulatorInstance->pCpuState     = (GBCpuState*)(pEmulatorInstance + 1);
    pEmulatorInstance->pMemoryMapper = (GBMemoryMapper*)(pEmulatorInstance->pCpuState + 1);

    initGBCpuState(pEmulatorInstance->pCpuState);

    uint8_t* pGBMemory = (uint8_t*)(pEmulatorInstance->pMemoryMapper + 1);
    initMemoryMapper(pEmulatorInstance->pMemoryMapper, pGBMemory );

    return pEmulatorInstance;
}

void startEmulation( GBEmulatorInstance* pEmulator, const uint8_t* pRomMemory )
{
    mapRomMemory(pEmulator->pMemoryMapper, pRomMemory);

    GBCpuState* pCpuState           = pEmulator->pCpuState;
    GBMemoryMapper* pMemoryMapper   = pEmulator->pMemoryMapper;

    //FK: Start fetching instructions at address $1000
    pCpuState->programCounter    = 0x0100;
    pCpuState->cycleCount        = 0u;

    while(true)
    {
        const uint8_t opcode = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
        switch( opcode )
        {
            //NOP
            case 0x00:
                break;

            //LD A,(BC)
            case 0x0A:
                pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.BC);
                pCpuState->cycleCount += 8;
                break;

            //LD A,(DE)
            case 0x1A:
                pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.DE);
                pCpuState->cycleCount += 8;
                break;

            //LD A,(nn)
            case 0xFA:
            {
                const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, address);
                pCpuState->programCounter += 2;
                pCpuState->cycleCount     += 16;
                break;     
            }
                
            //LD A,#
            case 0x3E:
                pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                pCpuState->cycleCount += 8;
                break;

            //LD B,n
            case 0x06:  
                pCpuState->cpuRegisters.B = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                break;

             //LD C,n
            case 0x0E:
                pCpuState->cpuRegisters.C = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                break;

            //LD D,n
            case 0x16: 
                pCpuState->cpuRegisters.D = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                break;
                
            //LD E,n
            case 0x1E: 
                pCpuState->cpuRegisters.E = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                break;
            
             //LD H,n
            case 0x26:
                pCpuState->cpuRegisters.H = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                break;

            //LD L,n
            case 0x2E:
                pCpuState->cpuRegisters.L = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
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
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.A;
                        cycleCost       = 4;
                        break;
                    //LD A, B
                    case 0x78:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD A, C
                    case 0x79:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD A, D
                    case 0x7A:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD A, E
                    case 0x7B:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD A, H
                    case 0x7C:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD A, L
                    case 0x7D:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD A, (HL)
                    case 0x7E:
                        pTargetRegister = &pCpuState->cpuRegisters.A;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD B, B
                    case 0x40:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD B, C
                    case 0x41:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD B, D
                    case 0x42:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD B, E
                    case 0x43:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD B, H
                    case 0x44:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD B, L
                    case 0x45:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD B, (HL)
                    case 0x46:
                        pTargetRegister = &pCpuState->cpuRegisters.B;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD C, B
                    case 0x48:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD C, C
                    case 0x49:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD C, D
                    case 0x4A:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD C, E
                    case 0x4B:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD C, H
                    case 0x4C:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD C, L
                    case 0x4D:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD C, (HL)
                    case 0x4E:
                        pTargetRegister = &pCpuState->cpuRegisters.C;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD D, B
                    case 0x50:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD D, C
                    case 0x51:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD D, D
                    case 0x52:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD D, E
                    case 0x53:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD D, H
                    case 0x54:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD D, L
                    case 0x55:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD B, (HL)
                    case 0x56:
                        pTargetRegister = &pCpuState->cpuRegisters.D;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD E, B
                    case 0x58:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD E, C
                    case 0x59:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD E, D
                    case 0x5A:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD E, E
                    case 0x5B:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD E, H
                    case 0x5C:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD E, L
                    case 0x5D:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD E, (HL)
                    case 0x5E:
                        pTargetRegister = &pCpuState->cpuRegisters.E;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD H, B
                    case 0x60:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD H, C
                    case 0x61:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD H, D
                    case 0x62:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD H, E
                    case 0x63:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD H, H
                    case 0x64:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD H, L
                    case 0x65:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD H, (HL)
                    case 0x66:
                        pTargetRegister = &pCpuState->cpuRegisters.H;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;

                    //LD L, B
                    case 0x68:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 4;
                        break;
                    //LD L, C
                    case 0x69:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 4;
                        break;
                    //LD L, D
                    case 0x6A:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 4;
                        break;
                    //LD L, E
                    case 0x6B:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 4;
                        break;
                    //LD L, H
                    case 0x6C:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 4;
                        break;
                    //LD L, L
                    case 0x6D:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 4;
                        break;
                    //LD L, (HL)
                    case 0x6E:
                        pTargetRegister = &pCpuState->cpuRegisters.L;
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        cycleCost       = 8;
                        break;
                        
                    //LD (HL), B
                    case 0x70:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.B;
                        cycleCost       = 8;
                        break;
                    //LD (HL), C
                    case 0x71:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.C;
                        cycleCost       = 8;
                        break;
                    //LD (HL), D
                    case 0x72:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.D;
                        cycleCost       = 8;
                        break;
                    //LD (HL), E
                    case 0x73:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.E;
                        cycleCost       = 8;
                        break;
                    //LD (HL), H
                    case 0x74:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.H;
                        cycleCost       = 8;
                        break;
                    //LD (HL), L
                    case 0x75:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = pCpuState->cpuRegisters.L;
                        cycleCost       = 8;
                        break;
                    //LD (HL), n
                    case 0x36:
                        pTargetRegister = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                        cycleCost       = 12;
                        break;
                }

                *pTargetRegister = value;
                pCpuState->cycleCount += cycleCost;
            }

            //JP nn
            case 0xC3:
            {
                pCpuState->cycleCount += 12u;

                const uint16_t addressToJumpTo = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter = addressToJumpTo;
                break;
            }

            //JR n
            case 0x20:
            case 0x28:
            case 0x30:
            case 0x38:
            {
                int8_t addressOffset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
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

                if( ( checkMask & pCpuState->cpuRegisters.Flags.value ) == bitValue )
                {
                    pCpuState->programCounter += addressOffset;
                }

                pCpuState->cycleCount += 8;
                break;
            }

            //XOR n
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
                        operand = pCpuState->cpuRegisters.A;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xA8:
                        operand = pCpuState->cpuRegisters.B;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xA9:
                        operand = pCpuState->cpuRegisters.C;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xAA:
                        operand = pCpuState->cpuRegisters.D;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xAB:
                        operand = pCpuState->cpuRegisters.E;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xAC:
                        operand = pCpuState->cpuRegisters.H;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xAD:
                        operand = pCpuState->cpuRegisters.L;
                        pCpuState->cycleCount += 4;
                        break;
                    case 0xAE:
                        operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        pCpuState->cycleCount += 8;
                        break;
                    case 0xEE:
                        operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                        pCpuState->cycleCount += 8;
                        break;
                }
                
                pCpuState->cpuRegisters.A            = pCpuState->cpuRegisters.A ^ operand;
                pCpuState->cpuRegisters.Flags.value  = 0x0;
                pCpuState->cpuRegisters.Flags.Z      = (pCpuState->cpuRegisters.A == 0);
               
                break;
            }

            //LD n,nn (16bit loads)
            case 0x01:
                pCpuState->cpuRegisters.BC = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter += 2u;
                pCpuState->cycleCount += 12;
                break;
            case 0x11:
                pCpuState->cpuRegisters.DE = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter += 2u;
                pCpuState->cycleCount += 12;
                break;
            case 0x21:
                pCpuState->cpuRegisters.HL = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter += 2u;
                pCpuState->cycleCount += 12;
                break;
            case 0x31:
                pCpuState->cpuRegisters.SP = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter += 2u;
                pCpuState->cycleCount += 12;
                break;

            //LDD (HL),A
            case 0x32:
            {
                write8BitValueToMappedMemory(pMemoryMapper, pCpuState->cpuRegisters.HL, pCpuState->cpuRegisters.A);
                --pCpuState->cpuRegisters.HL;
                pCpuState->cycleCount += 8;
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
                uint8_t* pRegister = nullptr;
                switch(opcode)
                {
                    //DEC A
                    case 0x3D:
                        pRegister = &pCpuState->cpuRegisters.A;
                        break;
                    //DEC B
                    case 0x05:
                        pRegister = &pCpuState->cpuRegisters.B;
                        break;
                    //DEC C
                    case 0x0D:
                        pRegister = &pCpuState->cpuRegisters.C;
                        break;
                    //DEC D
                    case 0x15:
                        pRegister = &pCpuState->cpuRegisters.D;
                        break;
                    //DEC E
                    case 0x1D:
                        pRegister = &pCpuState->cpuRegisters.E;
                        break;
                    //DEC H
                    case 0x25:
                        pRegister = &pCpuState->cpuRegisters.H;
                        break;
                    //DEC L
                    case 0x2D:
                        pRegister = &pCpuState->cpuRegisters.L;
                        break;
                }

                decrementValue(pCpuState, pRegister);
                pCpuState->cycleCount += 4;
                break;
            }

            //LDH (n),A
            case 0xE0:
            {
                const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->cpuRegisters.A);
                pCpuState->cycleCount += 12;
                break;
            }

            //LDH A,(n)
            case 0xF0:
            {
                const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, address);
                pCpuState->cycleCount += 12;
                break;
            }

            //DI
            case 0xF3:
            {
                //pCpuState->disableInterruptPendingCounter = 2;
                pCpuState->cycleCount += 4;
                break;
            }

            //EI
            case 0xFB:
            {   
                //pCpuState->enableInterruptPendingCounter = 2;
                pCpuState->cycleCount += 4;
                break;
            }

            //DEC (HL)
            case 0x35:
            {
                uint8_t* pValue = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                decrementValue(pCpuState, pValue);
                pCpuState->cycleCount += 12;
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
                        value = pCpuState->cpuRegisters.A;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP B
                    case 0xB8:
                        value = pCpuState->cpuRegisters.B;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP C
                    case 0xB9:
                        value = pCpuState->cpuRegisters.C;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP D
                    case 0xBA:
                        value = pCpuState->cpuRegisters.D;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP E
                    case 0xBB:
                        value = pCpuState->cpuRegisters.E;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP H
                    case 0xBC:
                        value = pCpuState->cpuRegisters.H;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP L
                    case 0xBD:
                        value = pCpuState->cpuRegisters.L;
                        pCpuState->cycleCount += 4;
                        break;
                    //CP (HL)
                    case 0xBE:
                        value = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                        pCpuState->cycleCount += 8;
                        break;
                    //CP #
                    case 0xFE:
                        value = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                        pCpuState->cycleCount += 8;
                        break;
                }

                compareValue(pCpuState, value);
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