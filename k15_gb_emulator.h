#include "stdint.h"
#include "stdio.h"
#include "assert.h"

#define K15_PRINT_DISASSEMBLY
#define K15_BREAK_ON_UNKNOWN_INSTRUCTION 1

#ifdef K15_PRINT_DISASSEMBLY
#   include "k15_gb_opcodes.h"
#endif


#define Kbyte(x) (x)*1024
#define Mbyte(x) Kbyte(x)*1024

#define Kbit(x) Kbyte(x)/8
#define Mbit(x) Mbyte(x)/8

struct GBCpuState
{
    uint8_t* pIE;
    uint8_t* pIF;
    uint8_t* pStack;
    uint16_t programCounter;

    struct
    {
        uint8_t IME     : 1;
        uint8_t halt    : 1;
        uint8_t _pad    : 6;
    } flags;

    struct
    {
        union
        {
            struct
            {
                uint8_t F;
                uint8_t A;
            };

            uint16_t AF;
        };

        union
        {
            struct
            {
                uint8_t C;
                uint8_t B;
            };

            uint16_t BC;
        };

        union
        {
            struct
            {
                uint8_t E;
                uint8_t D;
            };

            uint16_t DE;
        };

        union
        {
            struct
            {
                uint8_t L;
                uint8_t H;
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

    } cpuRegisters;
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

enum GBInterruptFlags : uint8_t
{
    VBlankInterrupt     = (1<<0),
    LCDStatInterrupt    = (1<<1),
    TimerInterrupt      = (1<<2),
    SerialInterrupt     = (1<<3),
    JoypadInterrupt     = (1<<4)
};

struct GBObjectAttributeFlags
{
    uint8_t GBCpaletteNumber : 3;
    uint8_t GBCtileVramBank  : 1;
    uint8_t paletteNumber    : 1;
    uint8_t xflip            : 1;
    uint8_t yflip            : 1;
    uint8_t bgOverObj        : 1;
};

struct GBObjectAttributes
{
    uint8_t                 y;
    uint8_t                 x;
    uint8_t                 tileIndex;
    GBObjectAttributeFlags  flags;
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
    uint8_t         reserved[4];        
    uint8_t         nintendoLogo[48];
    uint8_t         gameTitle[16];
    uint8_t         colorCompatibility;
    uint8_t         licenseHigh;
    uint8_t         licenseLow;
    uint8_t         superGameBoyCompatibility;
    GBCartridgeType cartridgeType;
    uint8_t         romSize;
    uint8_t         ramSize;
    uint8_t         licenseCode;
    uint8_t         maskRomVersion;
    uint8_t         complementCheck;
    uint8_t         checksumHigher;
    uint8_t         checksumLower;
};

struct GBLcdControl
{
    uint8_t enable                  : 1;
    uint8_t windowTileMapArea       : 1;
    uint8_t windowEnable            : 1;
    uint8_t bgAndWindowTileDataArea : 1;
    uint8_t bgTileMapArea           : 1;
    uint8_t objSize                 : 1;
    uint8_t objEnable               : 1;
    uint8_t bgAndWindowEnable       : 1;
};

struct GBPalette
{
    uint8_t  color0 : 2;
    uint8_t  color1 : 2;
    uint8_t  color2 : 2;
    uint8_t  color3 : 2;
};

struct GBLcdStatus
{
    uint8_t enableLycEqLyInterrupt      : 1;
    uint8_t enableMode2OAMInterrupt     : 1;
    uint8_t enableMode1VBlankInterrupt  : 1;
    uint8_t enableMode0HBlankInterrupt  : 1;
    uint8_t LycEqLyFlag                 : 1;
    uint8_t mode                        : 2;
};

struct GBLcdRegisters
{
    GBLcdStatus*    pStatus;
    uint8_t*        pScy;
    uint8_t*        pScx;
    uint8_t*        pLy;
    uint8_t*        pLyc;
    uint8_t*        pWy;
    uint8_t*        pWx;
};

struct GBPpuState
{
    uint16_t            dotCounter;
    GBLcdRegisters      lcdRegisters;


    GBObjectAttributes* pOAM;
    GBLcdControl*       pLcdControl;
    GBPalette*          pPalettes;
    uint8_t*            pObjectTiles;
    uint8_t*            pBackgroundOrWindowTiles[2];

    uint8_t             objectAttributeCapacity;
};

struct GBEmulatorSettings
{
    uint8_t spriteCountPerScanline;
};

struct GBEmulatorInstance
{
    GBCpuState*         pCpuState;
    GBPpuState*         pPpuState;
    GBMemoryMapper*     pMemoryMapper;
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

void initCpuState( GBMemoryMapper* pMemoryMapper, GBCpuState* pState )
{
    memset(pState, 0, sizeof(GBCpuState));

    //FK: (Gameboy cpu manual) The GameBoy stack pointer 
    //is initialized to $FFFE on power up but a programmer 
    //should not rely on this setting and rather should 
    //explicitly set its value.
    pState->cpuRegisters.SP = 0xFFFE;

    //FK: TODO: Make the following values user defined
    //FK: A value of 11h indicates CGB (or GBA) hardware
    pState->cpuRegisters.A = 0x11;

    //FK: examine Bit 0 of the CPUs B-Register to separate between CGB (bit cleared) and GBA (bit set)
    pState->cpuRegisters.B = 0x1;

    pState->cpuRegisters.F = 0xB0;
    pState->cpuRegisters.C = 0x13;
    pState->cpuRegisters.DE = 0x00D8;
    pState->cpuRegisters.HL = 0x014D;

    pState->pIE = pMemoryMapper->pBaseAddress + 0xFFFF;
    pState->pIF = pMemoryMapper->pBaseAddress + 0xFF0F;

    pState->flags.IME   = 1;
    pState->flags.halt  = 0;
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

void increment8BitValue( GBCpuState* pCpuState, uint8_t* pValueAddress )
{
    ++*pValueAddress;
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
    pCpuState->cpuRegisters.Flags.N = 1;
}

void decrement8BitValue( GBCpuState* pCpuState, uint8_t* pValueAddress )
{
    --*pValueAddress;
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
    pCpuState->cpuRegisters.Flags.N = 1;
}

void increment16BitValue( GBCpuState* pCpuState, uint16_t* pValueAddress )
{
    ++*pValueAddress;
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
    pCpuState->cpuRegisters.Flags.N = 1;
}

void decrement16BitValue( GBCpuState* pCpuState, uint16_t* pValueAddress )
{
    --*pValueAddress;
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.adjust;
    pCpuState->cpuRegisters.Flags.N = 1;
}

uint8_t rotateThroughCarryLeft( GBCpuState* pCpuState, uint8_t value )
{
    __asm
    {
        mov bl, value 
        rcl bl, 1
        mov value, bl
    }

    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    return value;
}

uint8_t rotateThroughCarryRight( GBCpuState* pCpuState, uint8_t value)
{
    __asm
    {
        mov bl, value 
        rcr bl, 1
        mov value, bl
    }

    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    return value;
}

uint8_t rotateLeft( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.Flags.C = (value << 0);
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;

    __asm
    {
        mov bl, value
        rol bl, 1
        mov value, bl
    }
    
    return value;
}

uint8_t rotateRight( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.Flags.C = (value << 7);
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    
    __asm
    {
        mov bl, value
        ror bl, 1
        mov value, bl
    }
    
    return value;
}

uint8_t shiftLeft( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    pCpuState->cpuRegisters.Flags.C = (value << 0);

    value = (value << 1);
    return value;
}

uint8_t shiftRight( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    pCpuState->cpuRegisters.Flags.C = (value << 7);

    value = (value >> 1);
    return value;
}

uint8_t shiftRightKeepMSB( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 0;
    pCpuState->cpuRegisters.Flags.C = (value << 7);

    const uint8_t msb = value & (1 << 7);
    value = (value >> 1) | msb;
    return value;
}

uint8_t swapNibbles( GBCpuState* pCpuState, uint8_t value )
{
    value = ( value >> 4) | ( value << 4 );
    return value;
}

void testBit( GBCpuState* pCpuState, uint8_t value, uint8_t bitIndex )
{
    pCpuState->cpuRegisters.Flags.Z = (value >> bitIndex);
    pCpuState->cpuRegisters.Flags.N = 0;
    pCpuState->cpuRegisters.Flags.H = 1;
}

uint8_t resetBit( GBCpuState* pCpuState, uint8_t value, uint8_t bitIndex )
{
    value = value & ~(1 << bitIndex);
    return value;
}

uint8_t setBit( GBCpuState* pCpuState, uint8_t value, uint8_t bitIndex )
{
    value = value | (1 << bitIndex);
    return value;
}

void addValue8Bit( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.A += value;
    
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.C = flagsRegister.carry;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.parity;
    pCpuState->cpuRegisters.Flags.N = 0;
}

void addValue16Bit( GBCpuState* pCpuState, uint16_t value )
{
    pCpuState->cpuRegisters.HL += value;
    
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.C = flagsRegister.carry;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.parity;
    pCpuState->cpuRegisters.Flags.N = 0;
}

void subValue8Bit( GBCpuState* pCpuState, uint8_t value )
{
    pCpuState->cpuRegisters.A -= value;
    
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.C = flagsRegister.carry;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.parity;
    pCpuState->cpuRegisters.Flags.N = 0;
}

void subValue16Bit( GBCpuState* pCpuState, uint16_t value )
{
    pCpuState->cpuRegisters.HL -= value;
    
    const x86Flags flagsRegister = readX86CpuFlags();
    
    pCpuState->cpuRegisters.Flags.Z = flagsRegister.zero;
    pCpuState->cpuRegisters.Flags.C = flagsRegister.carry;
    pCpuState->cpuRegisters.Flags.H = flagsRegister.parity;
    pCpuState->cpuRegisters.Flags.N = 0;
}

void initMemoryMapper( GBMemoryMapper* pMapper, uint8_t* pMemory )
{
    memset(pMemory, 0, 0xFFFF);

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

void initPpuState(GBMemoryMapper* pMemoryMapper, GBPpuState* pPpuState)
{
    pPpuState->pOAM                         = (GBObjectAttributes*)(pMemoryMapper->pBaseAddress + 0xFE00);
    pPpuState->pLcdControl                  = (GBLcdControl*)(pMemoryMapper->pBaseAddress + 0xFF40);
    pPpuState->pPalettes                    = (GBPalette*)(pMemoryMapper->pBaseAddress + 0xFF47);
    pPpuState->pObjectTiles                 = pMemoryMapper->pBaseAddress + 0x8000;
    pPpuState->pBackgroundOrWindowTiles[0]  = pMemoryMapper->pBaseAddress + 0x9800;
    pPpuState->pBackgroundOrWindowTiles[1]  = pMemoryMapper->pBaseAddress + 0x9C00;
    pPpuState->objectAttributeCapacity      = 40;
    pPpuState->dotCounter                   = 0;

    pPpuState->lcdRegisters.pStatus = (GBLcdStatus*)(pMemoryMapper->pBaseAddress + 0xFF40);
    pPpuState->lcdRegisters.pScy    = pMemoryMapper->pBaseAddress + 0xFF42;
    pPpuState->lcdRegisters.pScx    = pMemoryMapper->pBaseAddress + 0xFF43;
    pPpuState->lcdRegisters.pLy     = pMemoryMapper->pBaseAddress + 0xFF44;
    pPpuState->lcdRegisters.pLyc    = pMemoryMapper->pBaseAddress + 0xFF45;
    pPpuState->lcdRegisters.pWy     = pMemoryMapper->pBaseAddress + 0xFF4A;
    pPpuState->lcdRegisters.pWx     = pMemoryMapper->pBaseAddress + 0xFF4B;
}

size_t calculateEmulatorInstanceMemoryRequirementsInBytes()
{
    const size_t memoryRequirementsInBytes = sizeof(GBEmulatorInstance) + sizeof(GBCpuState) + sizeof(GBMemoryMapper) + sizeof(GBPpuState) + 0xFFFF;
    return memoryRequirementsInBytes;
}

GBEmulatorInstance* createEmulatorInstance( uint8_t* pEmulatorInstanceMemory )
{
    GBEmulatorInstance* pEmulatorInstance = (GBEmulatorInstance*)pEmulatorInstanceMemory;
    pEmulatorInstance->pCpuState     = (GBCpuState*)(pEmulatorInstance + 1);
    pEmulatorInstance->pMemoryMapper = (GBMemoryMapper*)(pEmulatorInstance->pCpuState + 1);
    pEmulatorInstance->pPpuState     = (GBPpuState*)(pEmulatorInstance->pMemoryMapper + 1);
    
    uint8_t* pGBMemory = (uint8_t*)(pEmulatorInstance->pPpuState + 1);
    initMemoryMapper(pEmulatorInstance->pMemoryMapper, pGBMemory );
    initCpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pCpuState);
    initPpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pPpuState);
    return pEmulatorInstance;
}

uint8_t* getVideoRam( GBEmulatorInstance* pEmulator)
{
    return pEmulator->pMemoryMapper->pVideoRAM;
}

void startEmulation( GBEmulatorInstance* pEmulator, const uint8_t* pRomMemory )
{
    mapRomMemory(pEmulator->pMemoryMapper, pRomMemory);

    GBCpuState* pCpuState           = pEmulator->pCpuState;
    GBMemoryMapper* pMemoryMapper   = pEmulator->pMemoryMapper;

    //FK: Start fetching instructions at address $1000
    pCpuState->programCounter    = 0x0100;
}

void pushSpritesToScanline( uint8_t* pScanline, GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    const uint8_t objHeight = pPpuState->pLcdControl->objSize == 0 ? 8 : 16;

    uint8_t spriteCounter = 0;
    GBObjectAttributes participatingSprites[10] = {};
    for( size_t spriteIndex = 0u; spriteIndex < pPpuState->objectAttributeCapacity; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = pPpuState->pOAM + spriteIndex;
        if( pSprite->y <= scanlineYCoordinate && pSprite->y + objHeight >= scanlineYCoordinate )
        {
            participatingSprites[spriteCounter++] = *pSprite;
            
            if( spriteCounter == 10 )
            {
                break;
            }
        }
    }

    for( size_t spriteIndex = 0; spriteIndex < spriteCounter; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = participatingSprites + spriteIndex;
        const uint8_t* pTile = pPpuState->pObjectTiles + ( pSprite->tileIndex * 0x10 );
        const uint8_t tileLine = *(pTile + ( pSprite->y - scanlineYCoordinate ));
        //FK: TODO
        
    }
}

void pushBackgroundOrWindowToScanline( uint8_t* pScanline, GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    const uint8_t* pWindowTiles     = pPpuState->pBackgroundOrWindowTiles[ pPpuState->pLcdControl->windowTileMapArea ];
    const uint8_t* pBackgroundTiles = pPpuState->pBackgroundOrWindowTiles[ pPpuState->pLcdControl->bgTileMapArea ];
    const uint8_t tileCount = 32;
    const uint8_t wy = *pPpuState->lcdRegisters.pWy;
    uint8_t backgroundTileIndices[32] = {};

    if( wy <= scanlineYCoordinate )
    {
        const uint8_t wx = *pPpuState->lcdRegisters.pWx;
        for( uint8_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
        {
            const uint8_t x = tileIndex * 8;
            const uint8_t tileMapIndex = scanlineYCoordinate * 32 + tileIndex;
            if( x >= wx )
            {
                //FK: get tile from window tile map
                backgroundTileIndices[tileIndex] = pWindowTiles[tileMapIndex];
            }
            else
            {
                //FK: get tile from background tile map
                backgroundTileIndices[tileIndex] = pBackgroundTiles[tileMapIndex];
            }
        }
    }
    else
    {
        for( uint8_t tileIndex = 0; tileIndex < tileCount; ++tileIndex )
        {
            //FK: get tile from background tile map
            const uint8_t x = tileIndex * 8;
            const uint8_t tileMapIndex = scanlineYCoordinate * 32 + tileIndex;
            backgroundTileIndices[tileIndex] = pBackgroundTiles[tileMapIndex];
        }
    }
}

void pushScanline( GBEmulatorInstance* pEmulator, uint8_t scanlineYCoordinate )
{
    GBPpuState* pPpuState = pEmulator->pPpuState;
    uint8_t scanlineInGBPixels[20]; //FK: 20byte = 160gb pixels (1gb pixel = 2bit)

    if( pPpuState->pLcdControl->bgAndWindowEnable )
    {
        pushBackgroundOrWindowToScanline( scanlineInGBPixels, pPpuState, scanlineYCoordinate );
    }

    if( pPpuState->pLcdControl->objEnable )
    {
        pushSpritesToScanline( scanlineInGBPixels, pPpuState, scanlineYCoordinate );
    }
}

void triggerInterrupt( GBCpuState* pCpuState, uint8_t interruptFlag )
{
    *pCpuState->pIE |= interruptFlag;
}

void tickPPU( GBEmulatorInstance* pEmulator, const uint8_t cycleCount )
{
    GBPpuState* pPpuState = pEmulator->pPpuState;
    GBCpuState* pCpuState = pEmulator->pCpuState;

    GBLcdStatus* pLcdStatus = pPpuState->lcdRegisters.pStatus;
    uint8_t ly = *pPpuState->lcdRegisters.pLy;
    uint8_t lcdMode = pLcdStatus->mode;
    const uint8_t lyc = *pPpuState->lcdRegisters.pLyc;

    uint8_t triggerLCDStatInterrupt = 0;

    if( lcdMode == 2 && pPpuState->dotCounter >= 80 )
    {
        lcdMode = 3;
        pushScanline(pEmulator, ly);
    }
    else if( lcdMode == 3 && pPpuState->dotCounter >= 172 )
    {
        lcdMode = 0;
        triggerLCDStatInterrupt = pLcdStatus->enableMode0HBlankInterrupt;

        ++ly;

        if( pLcdStatus->enableLycEqLyInterrupt )
        {
            if( pLcdStatus->LycEqLyFlag == 1 && ly == lyc )
            {
                triggerLCDStatInterrupt = 1;
            }
            else if ( pLcdStatus->LycEqLyFlag == 0 && ly != lyc )
            {
                triggerLCDStatInterrupt = 1;
            }
        }
    }
    else if( lcdMode == 0 && pPpuState->dotCounter >= 204 )
    {
        if( ly == 145 )
        {
            lcdMode = 1;
            triggerLCDStatInterrupt = pLcdStatus->enableMode1VBlankInterrupt;
            triggerInterrupt( pCpuState, VBlankInterrupt );
        }
        else
        {
            lcdMode = 2;
        }
    }
    else if( lcdMode == 1 && ly == 155 )
    {
        lcdMode = 2;
        triggerLCDStatInterrupt = pLcdStatus->enableMode2OAMInterrupt;
    }

    if( triggerLCDStatInterrupt )
    {
        triggerInterrupt( pCpuState, LCDStatInterrupt );
    }

    pPpuState->dotCounter += cycleCount;

    *pPpuState->lcdRegisters.pLy    = ly;
    pLcdStatus->mode                = lcdMode;
}

void push16BitValueToStack( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint16_t value )
{
    //FK: TODO: Better error handling
    assert( pCpuState->cpuRegisters.SP < 0xFFFE );

    pCpuState->cpuRegisters.SP -= 2;
    
    uint8_t* pStack = pMemoryMapper->pBaseAddress + pCpuState->cpuRegisters.SP;
    pStack[0] = (uint8_t)( value >> 8 );
    pStack[1] = (uint8_t)( value >> 0 );
}

uint16_t pop16BitValueFromStack( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper )
{
    uint8_t* pStack = pMemoryMapper->pBaseAddress + pCpuState->cpuRegisters.SP;
    pCpuState->cpuRegisters.SP += 2;

    const uint16_t value = (uint16_t)pStack[0] << 8 | 
                           (uint16_t)pStack[1] << 0;
    return value;
}

void triggerPendingInterrupts( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper )
{
    if( !pCpuState->flags.IME )
    {
        return;
    }

    const uint8_t interruptEnable       = *pCpuState->pIE;
    const uint8_t interruptFlags        = *pCpuState->pIF;
    const uint8_t interruptHandleMask   = ( interruptEnable & interruptFlags );
    if( interruptHandleMask > 0u )
    {
        push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->programCounter);
        pCpuState->flags.halt = 0;
    }
}

void printOpcodeDisassembly(GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode)
{
    uint16_t programCounter = pCpuState->programCounter - 1;
    const uint16_t index = ( opcode == 0xCB ) ? opcode + 0xFF : opcode;
    const GBOpcode* pOpcode = opcodeDisassembly + index;

    printf("0x%.4hX: ", programCounter);
    for( uint8_t byteCount = 0u; byteCount < pOpcode->arguments; ++byteCount )
    {
        printf( "0x%.2hhX ", read8BitValueFromAddress(pMemoryMapper, programCounter++) );
    }

    printf(" - %s\n", pOpcode->pMnemonic);
} 

uint8_t handleCbOpcode(GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode)
{
    uint8_t* pTarget = nullptr;
    uint8_t cycleCost = 0u;
    uint8_t target = (opcode & 0x0F);
    const uint8_t operation = ( opcode & 0xF0 );
    const uint8_t bitIndex  = (operation / 0x40) + (target > 0x07);
    target = target > 0x07 ? target - 0x07 : target;
    switch( target )
    {
        case 0x00:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.B;
            break;
        }
        case 0x01:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.C;
            break;
        }
        case 0x02:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.D;
            break;
        }
        case 0x03:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.E;
            break;
        }
        case 0x04:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.H;
            break;
        }
        case 0x05:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.L;
            break;
        }
        case 0x06:
        {
            cycleCost = operation == 0x0E || operation == 0x06 ? 12 : 16;
            pTarget = getMemoryAddress( pMemoryMapper, pCpuState->cpuRegisters.HL );
            break;
        }
        case 0x07:
        {
            cycleCost = 8u;
            pTarget = &pCpuState->cpuRegisters.A;
            break;
        }
    }

    //FK: Refer to https://gbdev.io/gb-opcodes//optables/
    switch( operation )
    {
        case 0x00:
        {
            if( target <= 0x07 )
            {
                //RLC
                *pTarget = rotateThroughCarryLeft(pCpuState, *pTarget);
            }
            else
            {
                //RRC
                *pTarget = rotateThroughCarryRight(pCpuState, *pTarget);
            }
            break;
        }
        case 0x10:
        {
            if( target <= 0x07 )
            {
                //RL
                *pTarget = rotateLeft(pCpuState, *pTarget);
            }
            else
            {
                //RR
                *pTarget = rotateRight(pCpuState, *pTarget);
            }
            break;
        }
        case 0x20:
        {
            if( target <= 0x07 )
            {
                //SLA
                *pTarget = shiftLeft(pCpuState, *pTarget);
            }
            else
            {
                //SRA
                *pTarget = shiftRightKeepMSB(pCpuState, *pTarget);
            }
            break;
        }
        case 0x30:
        {
            if( target <= 0x07 )
            {
                //SWAP
                *pTarget = swapNibbles(pCpuState, *pTarget);
            }
            else
            {
                //SRL
                *pTarget = shiftRight(pCpuState, *pTarget);
            }
            break;
        }
        //BIT
        case 0x40:
        case 0x50:
        case 0x60:
        case 0x70:
        {
            testBit(pCpuState, *pTarget, bitIndex);
            break;
        }
        //RES
        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        {
            *pTarget = resetBit(pCpuState, *pTarget, bitIndex);
            break;
        }
        //SET
        case 0xC0:
        case 0xD0:
        case 0xE0:
        case 0xF0:
        {
            *pTarget = setBit(pCpuState, *pTarget, bitIndex);
            break;
        }
    }

    return cycleCost;
}

uint8_t runSingleInstruction( GBEmulatorInstance* pEmulator )
{
    GBMemoryMapper* pMemoryMapper   = pEmulator->pMemoryMapper;
    GBCpuState* pCpuState           = pEmulator->pCpuState;

    triggerPendingInterrupts( pCpuState, pMemoryMapper );

    if( pCpuState->flags.halt )
    {
        return 0u;
    }

    uint8_t cycleCost = 0u;
    const uint8_t opcode = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);

#ifdef K15_PRINT_DISASSEMBLY
    printOpcodeDisassembly( pCpuState, pMemoryMapper, opcode );
#endif

    switch( opcode )
    {
        //NOP
        case 0x00:
            break;

        //CALL nn
        case 0xCD:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->programCounter + 2);
            pCpuState->programCounter = address;
            break;
        }

        case 0xC4:
        case 0xCC:
        case 0xD4:
        case 0xDC:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter += 2;

            uint8_t condition = 0;
            switch( opcode )
            {
                //CALL nz, nn
                case 0xC4:
                    condition = pCpuState->cpuRegisters.Flags.Z == 0;
                    break;
                //CALL nc, nn
                case 0xCC:
                    condition = pCpuState->cpuRegisters.Flags.C == 0;
                    break;
                //CALL z, nn
                case 0xD4:
                    condition = pCpuState->cpuRegisters.Flags.Z == 1;
                    break;
                //CALL c, nn
                case 0xDC:
                    condition = pCpuState->cpuRegisters.Flags.C == 1;
                    break;
            }

            if( condition )
            {
                push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->programCounter);
                pCpuState->programCounter = address;
            }
            break;
        }
  

        //LD A,(BC)
        case 0x0A:
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.BC);
            cycleCost = 8;
            break;

        //LD A,(DE)
        case 0x1A:
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.DE);
            cycleCost = 8;
            break;

        //LD A,(nn)
        case 0xFA:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, address);
            pCpuState->programCounter += 2;
            cycleCost = 16;
            break;     
        }

        //LD (BC), A
        case 0x02:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->cpuRegisters.BC, pCpuState->cpuRegisters.A);
            cycleCost = 8;
            break;

        //LD (DE), A
        case 0x12:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->cpuRegisters.DE, pCpuState->cpuRegisters.A);
            cycleCost = 8;
            break;

        //LD (HL), A
        case 0x77:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->cpuRegisters.DE, pCpuState->cpuRegisters.A);
            cycleCost = 8;
            break;

        //LD (nn), A
        case 0xEA:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->cpuRegisters.A);
            pCpuState->programCounter += 2;
            cycleCost = 16;
            break;
        }

        //LD A,n
        case 0x3E:
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;

        //LD B,n
        case 0x06:  
            pCpuState->cpuRegisters.B = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;

        //LD C,n
        case 0x0E:
            pCpuState->cpuRegisters.C = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;

        //LD D,n
        case 0x16: 
            pCpuState->cpuRegisters.D = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;
            
        //LD E,n
        case 0x1E: 
            pCpuState->cpuRegisters.E = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;
        
        //LD H,n
        case 0x26:
            pCpuState->cpuRegisters.H = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;

        //LD L,n
        case 0x2E:
            pCpuState->cpuRegisters.L = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = 8;
            break;
        
        //LD a,(HL++)
        case 0x2A:
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL++);
            cycleCost = 8;
            break;

        //LD (C),a
        case 0xE2:
        {
            const uint16_t address = 0xFF00 + pCpuState->cpuRegisters.C;
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->cpuRegisters.A);
            cycleCost = 8;
            break;
        }

        //LD b,a
        case 0x47:
        {
            pCpuState->cpuRegisters.B = pCpuState->cpuRegisters.A;
            cycleCost = 4;
            break;
        }

        //LD c,a
        case 0x4F:
        {
            pCpuState->cpuRegisters.C = pCpuState->cpuRegisters.A;
            cycleCost = 4;
            break;
        }

        //LD E, A
        case 0x5F:
        {
            pCpuState->cpuRegisters.E = pCpuState->cpuRegisters.A;
            cycleCost = 4;
            break;
        }

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
            uint8_t* pTarget            = nullptr;
            uint8_t value               = 0;
            switch(opcode)
            {
                //LD A, A
                case 0x7F:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.A;
                    cycleCost       = 4;
                    break;
                //LD A, B
                case 0x78:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD A, C
                case 0x79:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD A, D
                case 0x7A:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD A, E
                case 0x7B:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD A, H
                case 0x7C:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD A, L
                case 0x7D:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD A, (HL)
                case 0x7E:
                    pTarget         = &pCpuState->cpuRegisters.A;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD B, B
                case 0x40:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD B, C
                case 0x41:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD B, D
                case 0x42:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD B, E
                case 0x43:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD B, H
                case 0x44:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD B, L
                case 0x45:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD B, (HL)
                case 0x46:
                    pTarget         = &pCpuState->cpuRegisters.B;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD C, B
                case 0x48:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD C, C
                case 0x49:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD C, D
                case 0x4A:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD C, E
                case 0x4B:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD C, H
                case 0x4C:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD C, L
                case 0x4D:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD C, (HL)
                case 0x4E:
                    pTarget         = &pCpuState->cpuRegisters.C;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD D, B
                case 0x50:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD D, C
                case 0x51:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD D, D
                case 0x52:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD D, E
                case 0x53:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD D, H
                case 0x54:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD D, L
                case 0x55:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD B, (HL)
                case 0x56:
                    pTarget         = &pCpuState->cpuRegisters.D;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD E, B
                case 0x58:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD E, C
                case 0x59:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD E, D
                case 0x5A:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD E, E
                case 0x5B:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD E, H
                case 0x5C:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD E, L
                case 0x5D:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD E, (HL)
                case 0x5E:
                    pTarget         = &pCpuState->cpuRegisters.E;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD H, B
                case 0x60:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD H, C
                case 0x61:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD H, D
                case 0x62:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD H, E
                case 0x63:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD H, H
                case 0x64:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD H, L
                case 0x65:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD H, (HL)
                case 0x66:
                    pTarget         = &pCpuState->cpuRegisters.H;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;

                //LD L, B
                case 0x68:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 4;
                    break;
                //LD L, C
                case 0x69:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 4;
                    break;
                //LD L, D
                case 0x6A:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 4;
                    break;
                //LD L, E
                case 0x6B:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 4;
                    break;
                //LD L, H
                case 0x6C:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 4;
                    break;
                //LD L, L
                case 0x6D:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 4;
                    break;
                //LD L, (HL)
                case 0x6E:
                    pTarget         = &pCpuState->cpuRegisters.L;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost       = 8;
                    break;
                    
                //LD (HL), B
                case 0x70:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.B;
                    cycleCost       = 8;
                    break;
                //LD (HL), C
                case 0x71:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.C;
                    cycleCost       = 8;
                    break;
                //LD (HL), D
                case 0x72:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.D;
                    cycleCost       = 8;
                    break;
                //LD (HL), E
                case 0x73:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.E;
                    cycleCost       = 8;
                    break;
                //LD (HL), H
                case 0x74:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.H;
                    cycleCost       = 8;
                    break;
                //LD (HL), L
                case 0x75:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = pCpuState->cpuRegisters.L;
                    cycleCost       = 8;
                    break;
                //LD (HL), n
                case 0x36:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost       = 12;
                    break;
            }

            *pTarget         = value;
            break;
        }

        //JP nn
        case 0xC3:
        {
            cycleCost = 12u;

            const uint16_t addressToJumpTo = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter = addressToJumpTo;
            break;
        }

        //JP (HL)
        case 0xE9:
        {
            cycleCost = 4u;

            const uint16_t addressToJumpTo = read16BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
            pCpuState->programCounter = addressToJumpTo;
            break;
        }

        //JR n
        case 0x18:
        {
            const int8_t addressOffset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            pCpuState->programCounter += addressOffset;
            cycleCost = 8;
            break;
        }

        //JR
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38:
        {
            const int8_t addressOffset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            uint8_t condition = 0;
            switch(opcode)
            {
                //JR nz
                case 0x20:
                    condition = pCpuState->cpuRegisters.Flags.Z == 0;
                    break;
                //JR nc
                case 0x30:
                    condition = pCpuState->cpuRegisters.Flags.C == 0;
                    break;
                //JR z
                case 0x28:
                    condition = pCpuState->cpuRegisters.Flags.Z == 1;
                    break;
                //JR c
                case 0x38:
                    condition = pCpuState->cpuRegisters.Flags.C == 1;
                    break;
            }

            if( condition )
            {
                pCpuState->programCounter += addressOffset;
            }

            cycleCost = 8;
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
                    cycleCost = 4;
                    break;
                case 0xA8:
                    operand = pCpuState->cpuRegisters.B;
                    cycleCost = 4;
                    break;
                case 0xA9:
                    operand = pCpuState->cpuRegisters.C;
                    cycleCost = 4;
                    break;
                case 0xAA:
                    operand = pCpuState->cpuRegisters.D;
                    cycleCost = 4;
                    break;
                case 0xAB:
                    operand = pCpuState->cpuRegisters.E;
                    cycleCost = 4;
                    break;
                case 0xAC:
                    operand = pCpuState->cpuRegisters.H;
                    cycleCost = 4;
                    break;
                case 0xAD:
                    operand = pCpuState->cpuRegisters.L;
                    cycleCost = 4;
                    break;
                case 0xAE:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost = 8;
                    break;
                case 0xEE:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost = 8;
                    break;
            }
            
            pCpuState->cpuRegisters.A            = pCpuState->cpuRegisters.A ^ operand;
            pCpuState->cpuRegisters.Flags.value  = 0x0;
            pCpuState->cpuRegisters.Flags.Z      = (pCpuState->cpuRegisters.A == 0);
            
            break;
        }

        //OR n
        case 0xB7:
        case 0xB0:
        case 0xB1:
        case 0xB2:
        case 0xB3:
        case 0xB4:
        case 0xB5:
        case 0xB6:
        case 0xF6:
        {
            uint8_t operand = 0;
            switch( opcode )
            {
                //OR A
                case 0xB7:
                    operand = pCpuState->cpuRegisters.A;
                    cycleCost += 4;
                    break;
                //OR B
                case 0xB0:
                    operand = pCpuState->cpuRegisters.B;
                    cycleCost += 4;
                    break;
                //OR C
                case 0xB1:
                    operand = pCpuState->cpuRegisters.C;
                    cycleCost += 4;
                    break;
                //OR D
                case 0xB2:
                    operand = pCpuState->cpuRegisters.D;
                    cycleCost += 4;
                    break;
                //OR E
                case 0xB3:
                    operand = pCpuState->cpuRegisters.E;
                    cycleCost += 4;
                    break;
                //OR H
                case 0xB4:
                    operand = pCpuState->cpuRegisters.H;
                    cycleCost += 4;
                    break;
                //OR L
                case 0xB5:
                    operand = pCpuState->cpuRegisters.L;
                    cycleCost += 4;
                    break;
                //OR (HL)
                case 0xB6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost += 8;
                    break;
                //OR #
                case 0xF6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost += 8;
                    break;
            }

            pCpuState->cpuRegisters.A = pCpuState->cpuRegisters.A | operand;
            pCpuState->cpuRegisters.Flags.value  = 0x0;
            pCpuState->cpuRegisters.Flags.Z      = (pCpuState->cpuRegisters.A == 0);
            break;
        }

        //AND n
        case 0xA7:
        case 0xA0:
        case 0xA1:
        case 0xA2:
        case 0xA3:
        case 0xA4:
        case 0xA5:
        case 0xA6:
        case 0xE6:
        {
            uint8_t operand = 0;
            switch( opcode )
            {
                //AND A
                case 0xA7:
                    operand = pCpuState->cpuRegisters.A;
                    cycleCost += 4;
                    break;
                //AND B
                case 0xA0:
                    operand = pCpuState->cpuRegisters.B;
                    cycleCost += 4;
                    break;
                //AND C
                case 0xA1:
                    operand = pCpuState->cpuRegisters.C;
                    cycleCost += 4;
                    break;
                //AND D
                case 0xA2:
                    operand = pCpuState->cpuRegisters.D;
                    cycleCost += 4;
                    break;
                //AND E
                case 0xA3:
                    operand = pCpuState->cpuRegisters.E;
                    cycleCost += 4;
                    break;
                //AND H
                case 0xA4:
                    operand = pCpuState->cpuRegisters.H;
                    cycleCost += 4;
                    break;
                //AND L
                case 0xA5:
                    operand = pCpuState->cpuRegisters.L;
                    cycleCost += 4;
                    break;
                //AND (HL)
                case 0xA6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost += 8;
                    break;
                //AND #
                case 0xE6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost += 8;
                    break;
            }

            pCpuState->cpuRegisters.A = pCpuState->cpuRegisters.A & operand;
            pCpuState->cpuRegisters.Flags.value  = 0x0;
            pCpuState->cpuRegisters.Flags.Z      = (pCpuState->cpuRegisters.A == 0);
            break;
        }

        //LD n,nn (16bit loads)
        case 0x01:
            pCpuState->cpuRegisters.BC = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter += 2u;
            cycleCost = 12;
            break;
        case 0x11:
            pCpuState->cpuRegisters.DE = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter += 2u;
            cycleCost = 12;
            break;
        case 0x21:
            pCpuState->cpuRegisters.HL = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter += 2u;
            cycleCost = 12;
            break;
        case 0x31:
            pCpuState->cpuRegisters.SP = read16BitValueFromAddress(pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter += 2u;
            cycleCost = 12;
            break;

        //LDD (HL),A
        case 0x32:
        {
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->cpuRegisters.HL, pCpuState->cpuRegisters.A);
            --pCpuState->cpuRegisters.HL;
            cycleCost = 8;
            break;
        }

        //INC n
        case 0x3C:
        case 0x04:
        case 0x0C:
        case 0x14:
        case 0x1C:
        case 0x24:
        case 0x2C:
        {
            uint8_t* pRegister = nullptr;
            switch(opcode)
            {
                //INC A
                case 0x3C:
                    pRegister = &pCpuState->cpuRegisters.A;
                    break;
                //INC B
                case 0x04:
                    pRegister = &pCpuState->cpuRegisters.B;
                    break;
                //INC C
                case 0x0C:
                    pRegister = &pCpuState->cpuRegisters.C;
                    break;
                //INC D
                case 0x14:
                    pRegister = &pCpuState->cpuRegisters.D;
                    break;
                //INC E
                case 0x1C:
                    pRegister = &pCpuState->cpuRegisters.E;
                    break;
                //INC H
                case 0x24:
                    pRegister = &pCpuState->cpuRegisters.H;
                    break;
                //INC L
                case 0x2C:
                    pRegister = &pCpuState->cpuRegisters.L;
                    break;
            }

            increment8BitValue(pCpuState, pRegister);
            cycleCost = 4;
            break;
        }

        //INC nn
        case 0x03:
        case 0x13:
        case 0x23:
        case 0x33:
        {
            uint16_t* pRegister = nullptr;
            switch( opcode )
            {
                //INC BC
                case 0x03:
                    pRegister = &pCpuState->cpuRegisters.BC;
                    break;
                //INC DE
                case 0x13:
                    pRegister = &pCpuState->cpuRegisters.DE;
                    break;
                //INC HL
                case 0x23:
                    pRegister = &pCpuState->cpuRegisters.HL;
                    break;
                //INC SP
                case 0x33:
                    pRegister = &pCpuState->cpuRegisters.SP;
                    break;
            }

            increment16BitValue(pCpuState, pRegister);
            cycleCost = 8;
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

            decrement8BitValue(pCpuState, pRegister);
            cycleCost = 4;
            break;
        }

        //DEC nn
        case 0x0B:
        case 0x1B:
        case 0x2B:
        case 0x3B:
        {
            uint16_t* pRegister = nullptr;
            switch( opcode )
            {
                //DEC BC
                case 0x0B:
                    pRegister = &pCpuState->cpuRegisters.BC;
                    break;
                //DEC DE
                case 0x1B:
                    pRegister = &pCpuState->cpuRegisters.DE;
                    break;
                //DEC HL
                case 0x2B:
                    pRegister = &pCpuState->cpuRegisters.HL;
                    break;
                //DEC SP
                case 0x3B:
                    pRegister = &pCpuState->cpuRegisters.SP;
                    break;
            }

            decrement16BitValue(pCpuState, pRegister);
            cycleCost = 8;
            break;
        }

        //HALT
        case 0x76:
        {
            pCpuState->flags.halt = 1;
            break;
        }

        //LDH (n),A
        case 0xE0:
        {
            const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->cpuRegisters.A);
            cycleCost = 12;
            break;
        }

        //LDH A,(n)
        case 0xF0:
        {
            const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            pCpuState->cpuRegisters.A = read8BitValueFromAddress(pMemoryMapper, address);
            cycleCost = 12;
            break;
        }

        //DI
        case 0xF3:
        {
            pCpuState->flags.IME = 0;
            cycleCost = 4;
            break;
        }

        //EI
        case 0xFB:
        {   
            pCpuState->flags.IME = 1;
            cycleCost = 4;
            break;
        }

        //ADD HL, n
        case 0x09:
        case 0x19:
        case 0x29:
        case 0x39:
        {
            uint16_t operand = 0;
            switch(opcode)
            {
                //ADD HL, BC
                case 0x09:
                    operand = pCpuState->cpuRegisters.BC;
                    cycleCost = 8;
                    break;
                //ADD HL, DE
                case 0x19:
                    operand = pCpuState->cpuRegisters.DE;
                    cycleCost = 8;
                    break;
                //ADD HL, HL
                case 0x29:
                    operand = pCpuState->cpuRegisters.HL;
                    cycleCost = 8;
                    break;
                //ADD HL, SP
                case 0x39:
                    operand = pCpuState->cpuRegisters.SP;
                    cycleCost = 8;
                    break;
            }

            addValue16Bit(pCpuState, operand);
            break;
        }

        //ADD SP, n
        case 0xE8:
        {
            const int8_t value = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            pCpuState->cpuRegisters.SP += value;
            break;
        }

        //ADD A, n
        case 0x87:
        case 0x80:
        case 0x81:
        case 0x82:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0xC6:
        {
            uint8_t operand = 0;
            switch(opcode)
            {
                //ADD A, A
                case 0x87:
                    operand = pCpuState->cpuRegisters.A;
                    cycleCost = 4;
                    break;
                //ADD A, B
                case 0x80:
                    operand = pCpuState->cpuRegisters.B;
                    cycleCost = 4;
                    break;
                //ADD A, C
                case 0x81:
                    operand = pCpuState->cpuRegisters.C;
                    cycleCost = 4;
                    break;
                //ADD A, D
                case 0x82:
                    operand = pCpuState->cpuRegisters.D;
                    cycleCost = 4;
                    break;
                //ADD A, E
                case 0x83:
                    operand = pCpuState->cpuRegisters.E;
                    cycleCost = 4;
                    break;
                //ADD A, H
                case 0x84:
                    operand = pCpuState->cpuRegisters.H;
                    cycleCost = 4;
                    break;
                //ADD A, L
                case 0x85:
                    operand = pCpuState->cpuRegisters.L;
                    cycleCost = 4;
                    break;
                //ADD A, (HL)
                case 0x86:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost = 8;
                    break;
                //ADD A, #
                case 0xC6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost = 8;
                    break;
            }

            addValue8Bit(pCpuState, operand);
            break;
        }

        //SUB A, n
        case 0x97:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0xD6:
        {
            uint8_t operand = 0;
            switch(opcode)
            {
                //SUB A, A
                case 0x87:
                    operand = pCpuState->cpuRegisters.A;
                    cycleCost = 4;
                    break;
                //SUB A, B
                case 0x80:
                    operand = pCpuState->cpuRegisters.B;
                    cycleCost = 4;
                    break;
                //SUB A, C
                case 0x81:
                    operand = pCpuState->cpuRegisters.C;
                    cycleCost = 4;
                    break;
                //SUB A, D
                case 0x82:
                    operand = pCpuState->cpuRegisters.D;
                    cycleCost = 4;
                    break;
                //SUB A, E
                case 0x83:
                    operand = pCpuState->cpuRegisters.E;
                    cycleCost = 4;
                    break;
                //SUB A, H
                case 0x84:
                    operand = pCpuState->cpuRegisters.H;
                    cycleCost = 4;
                    break;
                //SUB A, L
                case 0x85:
                    operand = pCpuState->cpuRegisters.L;
                    cycleCost = 4;
                    break;
                //SUB A, (HL)
                case 0x86:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost = 8;
                    break;
                //SUB A, #
                case 0xC6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost = 8;
                    break;
            }

            subValue8Bit(pCpuState, operand);
            break;
        }

        case 0xC0:
        case 0xD0:
        case 0xC8:
        case 0xD8:
        {
            uint8_t condition = 0;
            switch( opcode )
            {
                //RET NZ
                case 0xC0:
                    condition = pCpuState->cpuRegisters.Flags.Z == 0;
                    break;

                //RET NC
                case 0xD0:
                    condition = pCpuState->cpuRegisters.Flags.C == 0;
                    break;

                //RET NZ
                case 0xC8:
                    condition = pCpuState->cpuRegisters.Flags.Z == 1;
                    break;

                //RET NC
                case 0xD8:
                    condition = pCpuState->cpuRegisters.Flags.C == 1;
                    break;
            }

            if( condition )
            {
                pCpuState->programCounter = pop16BitValueFromStack(pCpuState, pMemoryMapper);
                cycleCost = 20;
            }
            else
            {
                cycleCost = 8;
            }
        }

        //RETI
        case 0xD9:
        {
            pCpuState->flags.IME = 1;
            //FK: no break, fall through to 0xC9
        }

        //RET
        case 0xC9:
        {
            pCpuState->programCounter = pop16BitValueFromStack(pCpuState, pMemoryMapper);
            cycleCost = 16;
            break;
        }

        //INC (HL)
        case 0x34:
        {
            uint8_t* pValue = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
            increment8BitValue(pCpuState, pValue);
            cycleCost = 12;
            break;
        }

        //DEC (HL)
        case 0x35:
        {
            uint8_t* pValue = getMemoryAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
            decrement8BitValue(pCpuState, pValue);
            cycleCost = 12;
            break;
        }

        // CPL
        case 0x2F:
        {
            pCpuState->cpuRegisters.A = pCpuState->cpuRegisters.A ^ 0xFF;
            pCpuState->cpuRegisters.Flags.N = 1;
            pCpuState->cpuRegisters.Flags.H = 1;
            cycleCost = 4;
            break;
        }

        //CCF
        case 0x3F:
        {
            pCpuState->cpuRegisters.Flags.C = !pCpuState->cpuRegisters.Flags.C;
            pCpuState->cpuRegisters.Flags.N = 0;
            pCpuState->cpuRegisters.Flags.H = 0;
            cycleCost = 4;
            break;
        }

        //SCF
        case 0x37:
        {
            pCpuState->cpuRegisters.Flags.C = 1;
            pCpuState->cpuRegisters.Flags.N = 0;
            pCpuState->cpuRegisters.Flags.H = 0;
            cycleCost = 4;
            break;
        }

        //PUSH nn
        case 0xF5:
        case 0xC5:
        case 0xD5:
        case 0xE5:
        {
            uint16_t value = 0;
            switch( opcode )
            {
                //PUSH AF
                case 0xF5:
                    value = pCpuState->cpuRegisters.AF;
                    break;
                //PUSH BC
                case 0xC5:
                    value = pCpuState->cpuRegisters.BC;
                    break;
                //PUSH DE
                case 0xD5:
                    value = pCpuState->cpuRegisters.DE;
                    break;
                //PUSH HL
                case 0xE5:
                    value = pCpuState->cpuRegisters.HL;
                    break;
            }
            push16BitValueToStack( pCpuState, pMemoryMapper, value );
            cycleCost = 16;
            break;
        }

        //POP nn
        case 0xF1:
        case 0xC1:
        case 0xD1:
        case 0xE1:
        {
            uint16_t* pTarget = nullptr;
            switch( opcode )
            {
                //POP AF
                case 0xF1:
                    pTarget = &pCpuState->cpuRegisters.AF;
                    break;
                //POP BC
                case 0xC1:
                    pTarget = &pCpuState->cpuRegisters.BC;
                    break;
                //POP DE
                case 0xD1:
                    pTarget = &pCpuState->cpuRegisters.DE;
                    break;
                //POP HL
                case 0xE1:
                    pTarget = &pCpuState->cpuRegisters.HL;
                    break;
            }
            *pTarget = pop16BitValueFromStack( pCpuState, pMemoryMapper );
            cycleCost = 12;
            break;
        }

        // SWAP n
        // SRL n
        // SRA n
        // SLA n
        // RR n
        // RRC n
        // RL n
        // RLC n
        case 0xCB:
        {
            const uint8_t cbOpcode = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
            cycleCost = handleCbOpcode(pCpuState, pMemoryMapper, cbOpcode);
            break;
        }

        //RST n
        case 0xC7:
        case 0xCF:
        case 0xD7:
        case 0xDF:
        case 0xE7:
        case 0xEF:
        case 0xF7:
        case 0xFF:
        {
            const uint16_t address = (uint16_t)(opcode - 0xC7);
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->programCounter);
            pCpuState->programCounter = address;
            cycleCost = 32;
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
                    cycleCost = 4;
                    break;
                //CP B
                case 0xB8:
                    value = pCpuState->cpuRegisters.B;
                    cycleCost = 4;
                    break;
                //CP C
                case 0xB9:
                    value = pCpuState->cpuRegisters.C;
                    cycleCost = 4;
                    break;
                //CP D
                case 0xBA:
                    value = pCpuState->cpuRegisters.D;
                    cycleCost = 4;
                    break;
                //CP E
                case 0xBB:
                    value = pCpuState->cpuRegisters.E;
                    cycleCost = 4;
                    break;
                //CP H
                case 0xBC:
                    value = pCpuState->cpuRegisters.H;
                    cycleCost = 4;
                    break;
                //CP L
                case 0xBD:
                    value = pCpuState->cpuRegisters.L;
                    cycleCost = 4;
                    break;
                //CP (HL)
                case 0xBE:
                    value = read8BitValueFromAddress(pMemoryMapper, pCpuState->cpuRegisters.HL);
                    cycleCost = 8;
                    break;
                //CP #
                case 0xFE:
                    value = read8BitValueFromAddress(pMemoryMapper, pCpuState->programCounter++);
                    cycleCost = 8;
                    break;
            }

            compareValue(pCpuState, value);
            break;
        }

        default:
#ifdef K15_PRINT_DISASSEMBLY
            printf( "not implemented opcode '0x%.2hhX'\n", opcode );
#endif

#ifdef K15_BREAK_ON_UNKNOWN_INSTRUCTION
            __debugbreak();
#endif
    }

    return cycleCost;
}