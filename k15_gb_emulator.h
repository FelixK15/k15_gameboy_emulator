#include "stdint.h"
#include "stdio.h"

#define K15_ENABLE_EMULATOR_DEBUG_FEATURES      1
#define K15_BREAK_ON_UNKNOWN_INSTRUCTION        1

#include "k15_gb_opcodes.h"

#define Kbyte(x) (x)*1024
#define Mbyte(x) Kbyte(x)*1024

#define Kbit(x) Kbyte(x)/8
#define Mbit(x) Mbyte(x)/8

#define FourCC(a, b, c, d) ((uint32_t)((a) << 0) | (uint32_t)((b) << 8) | (uint32_t)((c) << 16) | (uint32_t)((d) << 24))

//FK: Compiler specific functions
#ifdef _MSC_VER
#   define breakPointHook() \
__asm\
{\
    nop\
}
#   define debugBreak __debugbreak
#else
#   define breakPointHook()
#   define debugBreak
#endif

static constexpr uint32_t   gbStateFourCC                      = FourCC( 'K', 'G', 'B', 'C' ); //FK: FourCC of state files
static constexpr uint32_t   gbCpuFrequency                     = 4194304u;
static constexpr uint32_t   gbCyclesPerFrame                   = 70224u;
static constexpr uint8_t    gbOAMSizeInBytes                   = 0x9Fu;
static constexpr uint8_t    gbDMACycleCount                    = 160u;
static constexpr uint8_t    gbSpriteHeight                     = 16u;
static constexpr uint8_t    gbTileResolution                   = 8u;  //FK: Tiles are 8x8 pixels
static constexpr uint8_t    gbHalfTileResolution               = gbTileResolution / 2;
static constexpr uint8_t    gbHorizontalResolutionInPixels     = 160u;
static constexpr uint8_t    gbVerticalResolutionInPixels       = 144u;
static constexpr uint8_t    gbHorizontalResolutionInTiles      = gbHorizontalResolutionInPixels / gbTileResolution;
static constexpr uint8_t    gbVerticalResolutuionInTiles       = gbVerticalResolutionInPixels / gbTileResolution;
static constexpr uint8_t    gbBGTileCount                      = 32u; //FK: BG maps are 32x32 tiles
static constexpr uint8_t    gbTileSizeInBytes                  = 16u; //FK: 8x8 pixels with 2bpp
static constexpr uint8_t    gbOpcodeHistoryBufferCapacity      = 255u;
static constexpr uint8_t    gbObjectAttributeCapacity          = 40u;
static constexpr uint8_t    gbBackgroundTileCountPerScanline   = 20u; //FK: 20 Background Tiles fit on one scanline (horizontally)
static constexpr uint8_t    gbSpritesPerScanline               = 10u; //FK: the hardware allowed no more than 10 sprites per scanline
static constexpr uint8_t    gbFrameBufferScanlineSizeInBytes   = gbHorizontalResolutionInPixels / 4;
static constexpr size_t     gbFrameBufferSizeInBytes           = gbFrameBufferScanlineSizeInBytes * gbVerticalResolutionInPixels;
static constexpr size_t     gbMappedMemorySizeInBytes          = 0x10000;
static constexpr size_t     gbCompressionTokenSizeInBytes      = 1;

struct GBEmulatorJoypadState
{
    union
    {
        struct 
        {
            uint8_t a           : 1;
            uint8_t b           : 1;
            uint8_t select      : 1;
            uint8_t start       : 1;
            uint8_t _padding    : 4;
        };

        uint8_t actionButtonMask = 0;
    };

    union
    {
        struct 
        {
            uint8_t right       : 1;
            uint8_t left        : 1;
            uint8_t up          : 1;
            uint8_t down        : 1;
            uint8_t _padding    : 4;
        };

        uint8_t dpadButtonMask = 0;
    };
};

struct GBCpuFlags
{
    union
    {
        struct
        {
            uint8_t NOP : 4;
            uint8_t C   : 1;
            uint8_t H   : 1;
            uint8_t N   : 1;
            uint8_t Z   : 1;
        };

        uint8_t value;
    };
};

struct GBCpuRegisters
{
    union
    {
        struct
        {
            GBCpuFlags  F;
            uint8_t     A;
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

    uint16_t SP;
    uint16_t PC;
};

struct GBCpuStateFlags
{
    uint8_t IME     : 1;
    uint8_t halt    : 1;
    uint8_t dma     : 1;
};

struct GBCpuState
{
    uint8_t*        pIE;
    uint8_t*        pIF;

    GBCpuRegisters  registers;

    uint32_t        cycleCounter;
    uint32_t        targetCycleCountPerUpdate;
    uint16_t        lastOpcodeAddress;
    uint16_t        dmaAddress;
    uint8_t         dmaCycleCounter;
    uint8_t         lastOpcode;
    GBCpuStateFlags flags;          //FK: not to be confused with GBCpuRegisters::F
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

struct GBLcdControl
{
    uint8_t bgAndWindowEnable       : 1;
    uint8_t objEnable               : 1;
    uint8_t objSize                 : 1;
    uint8_t bgTileMapArea           : 1;
    uint8_t bgAndWindowTileDataArea : 1;
    uint8_t windowEnable            : 1;
    uint8_t windowTileMapArea       : 1;
    uint8_t enable                  : 1;
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
    uint8_t mode                        : 2;
    uint8_t LycEqLyFlag                 : 1;
    uint8_t enableMode0HBlankInterrupt  : 1;
    uint8_t enableMode1VBlankInterrupt  : 1;
    uint8_t enableMode2OAMInterrupt     : 1;
    uint8_t enableLycEqLyInterrupt      : 1;
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
    uint32_t            dotCounter;
    uint32_t            cycleCounter;
    GBLcdRegisters      lcdRegisters;

    GBObjectAttributes* pOAM;
    GBLcdControl*       pLcdControl;
    GBPalette*          pPalettes;
    uint8_t*            pBackgroundOrWindowTileIds[2];
    uint8_t*            pTileBlocks[3];
    uint8_t*            pGBFrameBuffer;
};

struct GBMemoryMapper
{
    const uint8_t*      pRomMemoryBaseAddress;
    uint8_t*            pBaseAddress;
    uint8_t*            pRomBank0;
    uint8_t*            pRomBankSwitch;
    uint8_t*            pVideoRAM;
    uint8_t*            pRamBankSwitch;
    uint8_t*            pLowRam;
    uint8_t*            pLowRamEcho;
    uint8_t*            pSpriteAttributes;
    uint8_t*            IOPorts;
    uint8_t*            pHighRam;
    uint8_t*            pInterruptRegister;
    uint16_t            lastAddressWrittenTo;
    uint16_t            lastAddressReadFrom;
    uint8_t             lastValueWritten;

    GBLcdStatus         lcdStatus;  //FK: Mirror lcd status to check wether we can read from VRAM and/or OAM 
    uint8_t             lcdEnabled; //FK: Mirror lcd enabled flag to check wether we can read from VRAM and/or OAM
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

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
struct GBEmulatorDebugSettings
{
    uint8_t enableBreakAtProgramCounter         : 1;
    uint8_t enableBreakAtOpcode                 : 1;
    uint8_t enableBreakAtMemoryReadFromAddress  : 1;
    uint8_t enableBreakAtMemoryWriteToAddress   : 1;
};

struct GBOpcodeHistoryElement
{
    uint8_t         opcode;
    uint16_t        address;
    GBCpuRegisters  registers;
};

struct GBEmulatorDebug
{
    GBOpcodeHistoryElement      opcodeHistory[ gbOpcodeHistoryBufferCapacity ];

    uint8_t                     pauseExecution          : 1;
    uint8_t                     continueExecution       : 1;
    uint8_t                     runForOneInstruction    : 1;
    uint8_t                     runSingleFrame          : 1;
    uint8_t                     pauseAtBreakpoint       : 1;

    uint16_t                    breakpointAddress;
    uint8_t                     opcodeHistorySize;
};
#endif

struct GBEmulatorInstanceFlags
{
    union
    {
        struct 
        {
            uint8_t vblank : 1;
        };

        uint8_t value;
    };
};

struct GBTimerState
{
    uint16_t    dividerCounter;     //FK: overflow = increment divider
    uint16_t    counterValue;   //FK: how many cpu cycles until TIMA is increased?
    uint16_t    counterTarget;
    uint8_t     enableCounter;
    uint8_t*    pDivider;
    uint8_t*    pCounter;
    uint8_t*    pModulo;
    uint8_t*    pControl;
};

struct GBEmulatorInstance
{
    GBCpuState*             pCpuState;
    GBPpuState*             pPpuState;
    GBTimerState*           pTimerState;
    GBMemoryMapper*         pMemoryMapper;

    GBEmulatorJoypadState   joypadState;
    GBEmulatorInstanceFlags flags;
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
    GBEmulatorDebug         debug;
#endif
};

struct GBEmulatorState
{
    GBCpuStateFlags     cpuFlags;
    GBCpuRegisters      cpuRegisters;
    uint32_t            lcdDotCounter;
    uint32_t            ppuCycleCounter;
    uint32_t            cpuCycleCounter;
    uint16_t            dmaAddress;
    uint16_t            timerCounterTarget;
    uint16_t            timerCounterValue;
    uint16_t            timerDividerCounter;
    uint8_t             timerEnableCounter;
    uint8_t             dmaCycleCounter;
    uint8_t             interruptEnableRegisters;
};

uint8_t isInVRAMAddressRange( const uint16_t address )
{
    return address >= 0x8000 && address < 0xA000;
}

uint8_t isInOAMAddressRange( const uint16_t address )
{
    return address >= 0xFE00 && address < 0xFEA0;
}

uint8_t isInCartridgeROMAddressRange( const uint16_t address )
{
    return address >= 0x0000 && address < 0x8000;
}

size_t calculateCompressedMemorySizeRLE( const uint8_t* pMemory, size_t memorySizeInBytes )
{
    size_t compressedMemorySizeInBytes = sizeof( uint32_t ); //FK: Compressed memory size in bytes
    uint16_t tokenCounter = 1;
    uint8_t token = pMemory[0];

    for( size_t offset = 1; offset < memorySizeInBytes; ++offset )
    {
        if( pMemory[offset] == token )
        {
            ++tokenCounter;
            continue;
        }

        compressedMemorySizeInBytes += sizeof(uint16_t); //FK: token counter
        compressedMemorySizeInBytes += gbCompressionTokenSizeInBytes; //FK: token
        token = pMemory[offset];
        tokenCounter = 1;
    }
    
    compressedMemorySizeInBytes += sizeof(uint16_t); //FK: token counter
    compressedMemorySizeInBytes += gbCompressionTokenSizeInBytes; //FK: token

    return compressedMemorySizeInBytes;
}

size_t calculateGBEmulatorStateSizeInBytes( const GBEmulatorInstance* pEmulatorInstance )
{
    const size_t compressedVRAMSizeInBytes = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->pVideoRAM,            0x2000 );
    const size_t compressedLRAMSizeInBytes = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->pLowRam,              0x2000 );
    const size_t compressedOAMSizeInBytes  = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->pSpriteAttributes,    0x00A0 );
    const size_t compressedHRAMSizeInBytes = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->pHighRam,             0x007F );
    const size_t compressedIOSizeInBytes   = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->IOPorts,              0x0080 );
    const size_t stateSizeInBytes = sizeof( GBEmulatorState ) + sizeof( gbStateFourCC ) +
        compressedVRAMSizeInBytes + compressedLRAMSizeInBytes + 
        compressedOAMSizeInBytes + compressedHRAMSizeInBytes +
        compressedIOSizeInBytes;

    return stateSizeInBytes;
}

size_t compressMemoryBlockRLE( uint8_t* pDestination, const uint8_t* pSource, size_t memorySizeInBytes )
{
    size_t compressedMemorySizeInBytes = sizeof( uint32_t );
    uint16_t tokenCounter = 1;
    uint8_t token = pSource[0];

    uint8_t* pDestinationBaseAddress = pDestination;
    pDestination += sizeof( uint32_t ); //FK: store compressed memory size at beginning of compressed memory block

    for( size_t offset = 1; offset < memorySizeInBytes; ++offset )
    {
        if( pSource[offset] == token )
        {
            ++tokenCounter;
            continue;
        }

        compressedMemorySizeInBytes += sizeof( uint16_t ); //FK: token counter
        compressedMemorySizeInBytes += 1; //FK: token

        memcpy(pDestination, &tokenCounter, sizeof( uint16_t ) );

        pDestination[2] = token;
        pDestination += 3;

        token = pSource[offset];
        tokenCounter = 1;
    }

    compressedMemorySizeInBytes += sizeof( uint16_t );  
    compressedMemorySizeInBytes += gbCompressionTokenSizeInBytes;                   
    memcpy(pDestination, &tokenCounter, sizeof( uint16_t ) );
    pDestination[2] = token;

    //FK: write compressed memory size in bytes at the beginning of the memory block
    memcpy( pDestinationBaseAddress, &compressedMemorySizeInBytes, sizeof( uint32_t ) );

    return compressedMemorySizeInBytes;
}

void storeGBEmulatorInstanceState( const GBEmulatorInstance* pEmulatorInstance, uint8_t* pStateMemory, size_t stateMemorySizeInBytes )
{
    const GBCpuState* pCpuState         = pEmulatorInstance->pCpuState;
    const GBPpuState* pPpuState         = pEmulatorInstance->pPpuState;
    const GBTimerState* pTimerState     = pEmulatorInstance->pTimerState;
    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;

    GBEmulatorState state;
    state.cpuRegisters                  = pCpuState->registers;
    state.cpuFlags                      = pCpuState->flags;
    state.cpuCycleCounter               = pCpuState->cycleCounter;
    state.dmaAddress                    = pCpuState->dmaAddress;
    state.dmaCycleCounter               = pCpuState->dmaCycleCounter;
    state.ppuCycleCounter               = pPpuState->cycleCounter;
    state.lcdDotCounter                 = pPpuState->dotCounter;
    state.interruptEnableRegisters      = pMemoryMapper->pBaseAddress[0xFFFF];
    state.timerCounterTarget            = pTimerState->counterTarget;
    state.timerCounterValue             = pTimerState->counterValue;
    state.timerDividerCounter           = pTimerState->dividerCounter;
    state.timerEnableCounter            = pTimerState->enableCounter;

    uint8_t* pCompressedMemory = pStateMemory + sizeof( GBEmulatorState ) + sizeof( gbStateFourCC );
    
    //FK: Store cartridge data as well...?
    size_t offset = 0;
    offset += compressMemoryBlockRLE( pCompressedMemory + offset, pMemoryMapper->pVideoRAM,           0x2000 );
    offset += compressMemoryBlockRLE( pCompressedMemory + offset, pMemoryMapper->pLowRam,             0x2000 );
    offset += compressMemoryBlockRLE( pCompressedMemory + offset, pMemoryMapper->pSpriteAttributes,   0x00A0 );
    offset += compressMemoryBlockRLE( pCompressedMemory + offset, pMemoryMapper->pHighRam,            0x007F );
    offset += compressMemoryBlockRLE( pCompressedMemory + offset, pMemoryMapper->IOPorts,             0x0080 );

    memcpy( pStateMemory + 0, &gbStateFourCC, sizeof( gbStateFourCC ) );
    memcpy( pStateMemory + sizeof( gbStateFourCC ), &state, sizeof( GBEmulatorState ) );
}

size_t uncompressMemoryBlockRLE( uint8_t* pDestination, const uint8_t* pSource )
{
    uint32_t compressedMemorySizeInBytes;
    memcpy( &compressedMemorySizeInBytes, pSource, sizeof( uint32_t ) );

    size_t sourceMemoryIndex = sizeof( uint32_t );
    size_t destinationMemoryIndex = 0;
    while( sourceMemoryIndex < compressedMemorySizeInBytes )
    {
        uint16_t tokenCount;
        memcpy( &tokenCount, pSource + sourceMemoryIndex, sizeof( uint16_t ) );

        const uint8_t token = pSource[sourceMemoryIndex + 2];
        memset( pDestination + destinationMemoryIndex, token, tokenCount );

        sourceMemoryIndex += sizeof( uint16_t ) + gbCompressionTokenSizeInBytes;
        destinationMemoryIndex += tokenCount;
    }

    return compressedMemorySizeInBytes;
}

bool loadGBEmulatorInstanceState( GBEmulatorInstance* pEmulatorInstance, const uint8_t* pStateMemory )
{
    uint32_t fourCC;
    memcpy( &fourCC, pStateMemory, sizeof( gbStateFourCC ) );

    if( fourCC != gbStateFourCC )
    {
        return false;
    }

    GBCpuState* pCpuState         = pEmulatorInstance->pCpuState;
    GBPpuState* pPpuState         = pEmulatorInstance->pPpuState;
    GBTimerState* pTimerState       = pEmulatorInstance->pTimerState;
    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;

    GBEmulatorState state;
    memcpy( &state, pStateMemory + sizeof( gbStateFourCC ), sizeof( GBEmulatorState ) );

    pCpuState->registers        = state.cpuRegisters;
    pCpuState->cycleCounter     = state.cpuCycleCounter;
    pCpuState->flags            = state.cpuFlags;
    pCpuState->dmaAddress       = state.dmaAddress;
    pCpuState->dmaCycleCounter  = state.dmaCycleCounter;

    pPpuState->dotCounter   = state.lcdDotCounter;
    pPpuState->cycleCounter = state.ppuCycleCounter;

    pTimerState->counterTarget  = state.timerCounterTarget;
    pTimerState->counterValue   = state.timerCounterValue;
    pTimerState->dividerCounter = state.timerDividerCounter;
    pTimerState->enableCounter  = state.timerEnableCounter;

    pMemoryMapper->pBaseAddress[0xFFFF] = state.interruptEnableRegisters;

    const uint8_t* pCompressedMemory = ( uint8_t* )( pStateMemory + sizeof( GBEmulatorState ) + sizeof( gbStateFourCC ) );

    size_t offset = 0;
    offset += uncompressMemoryBlockRLE( pMemoryMapper->pVideoRAM,          pCompressedMemory + offset );
    offset += uncompressMemoryBlockRLE( pMemoryMapper->pLowRam,            pCompressedMemory + offset );
    offset += uncompressMemoryBlockRLE( pMemoryMapper->pSpriteAttributes,  pCompressedMemory + offset );
    offset += uncompressMemoryBlockRLE( pMemoryMapper->pHighRam,           pCompressedMemory + offset );
    offset += uncompressMemoryBlockRLE( pMemoryMapper->IOPorts,            pCompressedMemory + offset );

    return true;
}

void setGBEmulatorInstanceMonitorRefreshRate( GBEmulatorInstance* pEmulatorInstance, uint16_t monitorRefreshRate )
{
    pEmulatorInstance->pCpuState->targetCycleCountPerUpdate = gbCpuFrequency / monitorRefreshRate;
}

uint8_t read8BitValueFromAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    if( isInVRAMAddressRange( addressOffset ) || 
        isInOAMAddressRange( addressOffset ) )
    {
        if( pMemoryMapper->lcdEnabled && pMemoryMapper->lcdStatus.mode == 3 )
        {
            //FK: can't read from VRAM and/or OAM during lcd mode 3 
            return 0xFF;
        }
    }

    pMemoryMapper->lastAddressReadFrom = addressOffset;
    return pMemoryMapper->pBaseAddress[addressOffset];
}

uint16_t read16BitValueFromAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    pMemoryMapper->lastAddressReadFrom = addressOffset;
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
    pMemoryMapper->lastValueWritten     = value;
    pMemoryMapper->lastAddressWrittenTo = addressOffset;

    if( pMemoryMapper->lcdStatus.mode == 3 && pMemoryMapper->lcdEnabled )
    {
        if( isInVRAMAddressRange( addressOffset ) || 
            isInOAMAddressRange( addressOffset ) )
        {
            //FK: can't read from VRAM and/or OAM during lcd mode 3 
            return;
        }
    }

    if( isInCartridgeROMAddressRange( addressOffset ) )
    {
        return;
    }

    switch( addressOffset )
    {
        //FK: Joypad input
        case 0xFF00:
        {
            const uint8_t currentValue = pMemoryMapper->pBaseAddress[addressOffset];
            pMemoryMapper->pBaseAddress[addressOffset] = ( value & 0xF0 ) | ( currentValue & 0x0F );
            break;
        }

        case 0xFF40:
        {
            //FK: LCD control - will be evaluated and written to memory in updatePPULcdControl()
            break;
        }

        case 0xFF41:
        {
            //FK: LCD Status - only bit 3:6 are writeable
            value &= 0x78;
            pMemoryMapper->pBaseAddress[addressOffset] |= value;
            break;
        }

        case 0xFF44:
        {
            //FK: Read only
            break;
        }

        default:
        {
            pMemoryMapper->pBaseAddress[addressOffset] = value;
            break;
        }
    }

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

void write16BitValueToMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset, uint16_t value )
{
    write8BitValueToMappedMemory( pMemoryMapper, addressOffset + 0, (uint8_t)(value & 0x0F >> 0) );
    write8BitValueToMappedMemory( pMemoryMapper, addressOffset + 1, (uint8_t)(value & 0xF0 >> 8) );
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
    }

    printf("Unsupported romSize identifier '%.2hhx'\n", romSize);
    debugBreak();
    return 0;
}

void mapRomMemory( GBMemoryMapper* pMemoryMapper, const uint8_t* pRomMemory )
{
    pMemoryMapper->pRomMemoryBaseAddress = pRomMemory;

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
            printf("Cartridge type '%.2hhx' not supported.\n", romHeader.cartridgeType);
            debugBreak();
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
    pState->registers.SP = 0xFFFE;

    //FK: TODO: Make the following values user defined
    //FK: A value of 11h indicates CGB (or GBA) hardware
    //pState->registers.A = 0x11;

    pState->registers.A = 0x1;

    //FK: examine Bit 0 of the CPUs B-Register to separate between CGB (bit cleared) and GBA (bit set)
    //pState->registers.B = 0x1;
    pState->registers.B = 0x0;

    //FK: Start fetching instructions at address $1000
    pState->registers.PC = 0x0100;

    pState->registers.F.value   = 0x80;
    pState->registers.C         = 0x13;
    pState->registers.DE        = 0x00D8;
    pState->registers.HL        = 0x014D;

    pState->pIE = pMemoryMapper->pBaseAddress + 0xFFFF;
    pState->pIF = pMemoryMapper->pBaseAddress + 0xFF0F;

    pState->dmaCycleCounter = 0;

    pState->targetCycleCountPerUpdate = ( uint16_t )( gbCpuFrequency / 60 ); //FK: Assume 60hz output as default

    pState->flags.dma   = 0;
    pState->flags.IME   = 1;
    pState->flags.halt  = 0;
}

void resetMemoryMapper( GBMemoryMapper* pMapper )
{
    memset(pMapper->pBaseAddress, 0, gbMappedMemorySizeInBytes);
}

void initMemoryMapper( GBMemoryMapper* pMapper, uint8_t* pMemory )
{
    pMapper->pRomMemoryBaseAddress  = 0x0000;
    pMapper->pBaseAddress           = pMemory;
    pMapper->pRomBank0              = pMemory;
    pMapper->pRomBankSwitch         = pMemory + 0x4000;
    pMapper->pVideoRAM              = pMemory + 0x8000;
    pMapper->pRamBankSwitch         = pMemory + 0xA000;
    pMapper->pLowRam                = pMemory + 0xC000;
    pMapper->pLowRamEcho            = pMemory + 0xE000;
    pMapper->pSpriteAttributes      = pMemory + 0xFE00;
    pMapper->IOPorts                = pMemory + 0xFF00;
    pMapper->pHighRam               = pMemory + 0xFF80;
    pMapper->pInterruptRegister     = pMemory + 0xFFFF;

    resetMemoryMapper( pMapper );
}

void initTimerState( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimerState )
{
    pTimerState->dividerCounter = 0;
    pTimerState->counterValue   = 0;
    pTimerState->counterTarget  = 0;
    pTimerState->enableCounter  = 0;
    pTimerState->pDivider       = pMemoryMapper->pBaseAddress + 0xFF04;
    pTimerState->pCounter       = pMemoryMapper->pBaseAddress + 0xFF05;
    pTimerState->pModulo        = pMemoryMapper->pBaseAddress + 0xFF06;
    pTimerState->pControl       = pMemoryMapper->pBaseAddress + 0xFF07;
}

void clearGBFrameBuffer( uint8_t* pGBFrameBuffer )
{
    memset( pGBFrameBuffer, 0, gbFrameBufferSizeInBytes );
}

void initPpuFrameBuffer( GBPpuState* pPpuState, uint8_t* pMemory )
{
    pPpuState->pGBFrameBuffer = pMemory;
    clearGBFrameBuffer( pPpuState->pGBFrameBuffer );
}

void initPpuState( GBMemoryMapper* pMemoryMapper, GBPpuState* pPpuState )
{
    pPpuState->pOAM                             = (GBObjectAttributes*)(pMemoryMapper->pBaseAddress + 0xFE00);
    pPpuState->pLcdControl                      = (GBLcdControl*)(pMemoryMapper->pBaseAddress + 0xFF40);
    pPpuState->pPalettes                        = (GBPalette*)(pMemoryMapper->pBaseAddress + 0xFF47);
    pPpuState->pTileBlocks[0]                   = pMemoryMapper->pBaseAddress + 0x8000;
    pPpuState->pTileBlocks[1]                   = pMemoryMapper->pBaseAddress + 0x8080;
    pPpuState->pTileBlocks[2]                   = pMemoryMapper->pBaseAddress + 0x9000;
    pPpuState->pBackgroundOrWindowTileIds[0]    = pMemoryMapper->pBaseAddress + 0x9800;
    pPpuState->pBackgroundOrWindowTileIds[1]    = pMemoryMapper->pBaseAddress + 0x9C00;
    pPpuState->dotCounter                       = 0;

    pPpuState->lcdRegisters.pStatus = (GBLcdStatus*)(pMemoryMapper->pBaseAddress + 0xFF41);
    pPpuState->lcdRegisters.pScy    = pMemoryMapper->pBaseAddress + 0xFF42;
    pPpuState->lcdRegisters.pScx    = pMemoryMapper->pBaseAddress + 0xFF43;
    pPpuState->lcdRegisters.pLy     = pMemoryMapper->pBaseAddress + 0xFF44;
    pPpuState->lcdRegisters.pLyc    = pMemoryMapper->pBaseAddress + 0xFF45;
    pPpuState->lcdRegisters.pWy     = pMemoryMapper->pBaseAddress + 0xFF4A;
    pPpuState->lcdRegisters.pWx     = pMemoryMapper->pBaseAddress + 0xFF4B;

    //FK: set default state
    pPpuState->pLcdControl->enable              = 1;
    pPpuState->pLcdControl->bgAndWindowEnable   = 1;
    pPpuState->pLcdControl->bgTileMapArea       = 1;
    
    pPpuState->lcdRegisters.pStatus->enableLycEqLyInterrupt = 1;
    clearGBFrameBuffer( pPpuState->pGBFrameBuffer );
}

size_t calculateGBEmulatorInstanceMemoryRequirementsInBytes()
{
    const size_t memoryRequirementsInBytes = sizeof(GBEmulatorInstance) + sizeof(GBCpuState) + sizeof(GBMemoryMapper) + sizeof(GBPpuState) + sizeof(GBTimerState) + gbMappedMemorySizeInBytes + gbFrameBufferSizeInBytes;
    return memoryRequirementsInBytes;
}

void resetGBEmulatorInstance( GBEmulatorInstance* pEmulatorInstance )
{
    resetMemoryMapper(pEmulatorInstance->pMemoryMapper );
    initCpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pCpuState);
    initPpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pPpuState);
    initTimerState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pTimerState);

    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    if( pMemoryMapper->pRomMemoryBaseAddress != nullptr )
    {
        mapRomMemory( pMemoryMapper, pMemoryMapper->pRomMemoryBaseAddress );
    }

    pEmulatorInstance->debug.continueExecution     = 1;
    pEmulatorInstance->joypadState.actionButtonMask  = 0;
    pEmulatorInstance->joypadState.dpadButtonMask    = 0;

    //FK: Reset joypad value
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF00] = 0x0F;
}

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES
void setGBEmulatorInstanceBreakpoint( GBEmulatorInstance* pEmulatorInstance, uint8_t pauseAtBreakpoint, uint16_t breakpointAddress )
{
    pEmulatorInstance->debug.pauseAtBreakpoint = pauseAtBreakpoint;
    pEmulatorInstance->debug.breakpointAddress = breakpointAddress;
}

void continueGBEmulatorInstanceExecution( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.continueExecution = 1;
}

void pauseGBEmulatorInstanceExecution( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.pauseExecution = 1;
}

void runGBEmulatorInstanceForOneInstruction( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.runForOneInstruction = 1;
}

void runGBEmulatorInstanceForOneFrame( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.runSingleFrame = 1;
}
#endif

GBEmulatorInstance* createGBEmulatorInstance( uint8_t* pEmulatorInstanceMemory )
{
    GBEmulatorInstance* pEmulatorInstance = (GBEmulatorInstance*)pEmulatorInstanceMemory;
    pEmulatorInstance->pCpuState     = (GBCpuState*)(pEmulatorInstance + 1);
    pEmulatorInstance->pMemoryMapper = (GBMemoryMapper*)(pEmulatorInstance->pCpuState + 1);
    pEmulatorInstance->pPpuState     = (GBPpuState*)(pEmulatorInstance->pMemoryMapper + 1);
    pEmulatorInstance->pTimerState   = (GBTimerState*)(pEmulatorInstance->pPpuState + 1);

    uint8_t* pGBMemory = (uint8_t*)(pEmulatorInstance->pTimerState + 1);
    initMemoryMapper( pEmulatorInstance->pMemoryMapper, pGBMemory );

    uint8_t* pFramebufferMemory = (uint8_t*)(pGBMemory + gbMappedMemorySizeInBytes);
    initPpuFrameBuffer( pEmulatorInstance->pPpuState, pFramebufferMemory );

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES
    pEmulatorInstance->debug.breakpointAddress    = 0x0000;
    pEmulatorInstance->debug.pauseAtBreakpoint    = 0;
    pEmulatorInstance->debug.pauseExecution       = 0;
    pEmulatorInstance->debug.runForOneInstruction = 0;
    pEmulatorInstance->debug.runSingleFrame       = 0;
    pEmulatorInstance->debug.continueExecution    = 0;
    pEmulatorInstance->debug.opcodeHistorySize    = 0;
    memset( pEmulatorInstance->debug.opcodeHistory, 0, sizeof( pEmulatorInstance->debug.opcodeHistory ) );
#endif

    resetGBEmulatorInstance(pEmulatorInstance);
    return pEmulatorInstance;
}

void loadGBEmulatorInstanceRom( GBEmulatorInstance* pEmulator, const uint8_t* pRomMemory )
{
    mapRomMemory(pEmulator->pMemoryMapper, pRomMemory);
}

int qsort_sortSpritesByXCoordinate( void const* pSpriteA, void const* pSpriteB )
{
    const GBObjectAttributes* pOAMs[2] = {
        (const GBObjectAttributes*)pSpriteA,
        (const GBObjectAttributes*)pSpriteB
    };

    return (int)pOAMs[0]->x - (int)pOAMs[1]->x;
}

void sortSpritesByXCoordinate( GBObjectAttributes* pSprites, uint8_t spriteCount )
{
    qsort( pSprites, spriteCount, sizeof(GBObjectAttributes), qsort_sortSpritesByXCoordinate );
}

void pushSpritePixelsToScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    const uint8_t objHeight = pPpuState->pLcdControl->objSize == 0 ? 8 : 16;

    uint8_t spriteCounter = 0;
    GBObjectAttributes scanlineSprites[10] = {}; //FK: Max 10 sprites per scanline
    for( size_t spriteIndex = 0u; spriteIndex < gbObjectAttributeCapacity; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = pPpuState->pOAM + spriteIndex;
        if( pSprite->y < gbSpriteHeight || pSprite->y > gbVerticalResolutionInPixels + gbSpriteHeight )
        {
            //FK: Sprite is invisible
            continue;
        }

        const uint8_t spriteTopPosition = pSprite->y - gbSpriteHeight;
        if( spriteTopPosition <= scanlineYCoordinate && spriteTopPosition + objHeight >= scanlineYCoordinate )
        {
            scanlineSprites[spriteCounter++] = *pSprite;
            
            if( spriteCounter == gbSpritesPerScanline )
            {
                break;
            }
        }
    }

    if( spriteCounter == 0 )
    {
        return;
    }

    if( spriteCounter > 1 )
    {
        //FK: TODO: This shouldn't happen in CGB mode
        sortSpritesByXCoordinate( scanlineSprites, spriteCounter );
    }

    const uint8_t scanlineYCoordinateInTileSpace = scanlineYCoordinate / gbTileResolution;

    uint8_t* pScanlinePixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
    for( size_t spriteIndex = 0; spriteIndex < spriteCounter; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = scanlineSprites + spriteIndex;
        if( pSprite->x < 8 || pSprite->x >= 168 )
        {
            //FK: Sprite is off screen
            continue;
        }

        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pPpuState->pTileBlocks[0] + pSprite->tileIndex * gbTileSizeInBytes;
        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + ( scanlineYCoordinate - scanlineYCoordinateInTileSpace * 8 ) * 2;
        //FK: Get pixel data of this tile for the current scanline
        const uint8_t pixelDataMSB = pTileScanlinePixelData[0];
        const uint8_t pixelDataLSB = pTileScanlinePixelData[1];

        //FK: Pixel will be written interleaved here
        uint8_t interleavedScanlinePixelData[2] = {0};
        uint8_t scanlinePixelMask[2] = {0};
        for( uint8_t scanlinePixelDataIndex = 0; scanlinePixelDataIndex < 2; ++scanlinePixelDataIndex )
        {
            const uint8_t startPixelIndex = scanlinePixelDataIndex * gbHalfTileResolution;
            const uint8_t endPixelIndex = startPixelIndex + gbHalfTileResolution;
            for( uint8_t pixelIndex = startPixelIndex; pixelIndex < endPixelIndex; ++pixelIndex )
            {
                const uint8_t scanlinePixelShift        = 6 - (pixelIndex-startPixelIndex)*2;
                const uint8_t pixelShift                = 7 - pixelIndex;
                const uint8_t pixelMSB                  = ( pixelDataMSB >> pixelShift ) & 0x1;
                const uint8_t pixelLSB                  = ( pixelDataLSB >> pixelShift ) & 0x1;
                const uint8_t pixelData                 = pixelMSB << 1 | pixelLSB << 0;
                scanlinePixelMask[ scanlinePixelDataIndex ]             |= ( pixelData == 0 ? 0x3 : 0x0 ) << scanlinePixelShift;
                interleavedScanlinePixelData[ scanlinePixelDataIndex ]  |= ( pixelData << scanlinePixelShift );
            }
        }

        const uint8_t scanlinePixelIndex = ( pSprite->x - 8u ) / 4;
        pScanlinePixelData[ scanlinePixelIndex + 0 ] = interleavedScanlinePixelData[0] | ( pScanlinePixelData[ scanlinePixelIndex + 0 ] & scanlinePixelMask[0] );
        pScanlinePixelData[ scanlinePixelIndex + 1 ] = interleavedScanlinePixelData[1] | ( pScanlinePixelData[ scanlinePixelIndex + 1 ] & scanlinePixelMask[1] );
    }
}

void pushBackgroundOrWindowPixelsToScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    const uint8_t* pWindowTileIds      = pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->windowTileMapArea ];
    const uint8_t* pBackgroundTileIds  = pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->bgTileMapArea ];
    const uint8_t wy = *pPpuState->lcdRegisters.pWy;
    const uint8_t sx = *pPpuState->lcdRegisters.pScx;
    const uint8_t sy = *pPpuState->lcdRegisters.pScy;

    //FK: Calculate the tile row that intersects with the current scanline
    //FK: TODO: implement sx/sy here
    const uint8_t scanlineYCoordinateInTileSpace = scanlineYCoordinate / gbTileResolution;
    const uint16_t startTileIndex = scanlineYCoordinateInTileSpace * gbBGTileCount;
    const uint16_t endTileIndex = startTileIndex + gbBGTileCount;

    uint8_t scanlineBackgroundTilesIds[ gbBGTileCount ] = { 0 };

    if( wy <= scanlineYCoordinate && pPpuState->pLcdControl->windowEnable )
    {
        const uint8_t wx = *pPpuState->lcdRegisters.pWx;
        const uint8_t wy = *pPpuState->lcdRegisters.pWy;

        for( uint16_t tileIndex = startTileIndex; tileIndex < endTileIndex; ++tileIndex )
        {
            const uint8_t x = tileIndex * 8;
            if( x >= wx )
            {
                //FK: get tile from window tile map
                scanlineBackgroundTilesIds[ tileIndex - startTileIndex ] = pWindowTileIds[ tileIndex ];
            }
            else if( x >= sx )
            {
                //FK: get tile from background tile map
                scanlineBackgroundTilesIds[ tileIndex - startTileIndex ] = pBackgroundTileIds[ tileIndex ];
            }
        }
    }
    else
    {
        for( uint16_t tileIndex = startTileIndex; tileIndex < endTileIndex; ++tileIndex )
        {
            //FK: get tile from background tile map
            scanlineBackgroundTilesIds[ tileIndex - startTileIndex ] = pBackgroundTileIds[ tileIndex ];
        }
    }

    //FK: Get the y position of the top most pixel inside the tile row that the current scanline is in
    const uint8_t tileRowStartYPos  = startTileIndex / gbBGTileCount * gbTileResolution;

    //FK: Get the y offset inside the tile row to where the scanline currently is.
    const uint8_t tileRowYOffset    = scanlineYCoordinate - tileRowStartYPos;

    //FK: Assume 0x8000 addressing mode
    const uint8_t* pTileData = pPpuState->pTileBlocks[0];

    //FK: Determine tile addressing mode
    if( !pPpuState->pLcdControl->bgAndWindowTileDataArea )
    {
        //FK: TODO: 0x8800 addressing mode  (actually uses 0x9000 as its base ptr)
        pTileData = pPpuState->pTileBlocks[2];
    }

    uint8_t* pScanlinePixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );

    for( uint8_t tileIdIndex = 0; tileIdIndex < gbBackgroundTileCountPerScanline; ++tileIdIndex )
    {
        const uint8_t tileIndex = scanlineBackgroundTilesIds[ tileIdIndex ];
        
        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pTileData + tileIndex * gbTileSizeInBytes;
        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + ( scanlineYCoordinate - scanlineYCoordinateInTileSpace * 8 ) * 2;
        //FK: Get pixel data of this tile for the current scanline
        const uint8_t pixelDataMSB = pTileScanlinePixelData[0];
        const uint8_t pixelDataLSB = pTileScanlinePixelData[1];

        //FK: Pixel will be written interleaved here
        uint8_t interleavedScanlinePixelData[2] = {0};
        for( uint8_t scanlinePixelDataIndex = 0; scanlinePixelDataIndex < 2; ++scanlinePixelDataIndex )
        {
            const uint8_t startPixelIndex = scanlinePixelDataIndex * gbHalfTileResolution;
            const uint8_t endPixelIndex = startPixelIndex + gbHalfTileResolution;
            for( uint8_t pixelIndex = startPixelIndex; pixelIndex < endPixelIndex; ++pixelIndex )
            {
                const uint8_t scanlinePixelShift        = 6 - (pixelIndex-startPixelIndex)*2;
                const uint8_t pixelShift                = 7 - pixelIndex;
                const uint8_t pixelMSB                  = ( pixelDataMSB >> pixelShift ) & 0x1;
                const uint8_t pixelLSB                  = ( pixelDataLSB >> pixelShift ) & 0x1;
                const uint8_t pixelData                 = pixelMSB << 1 | pixelLSB << 0;
                interleavedScanlinePixelData[ scanlinePixelDataIndex ] |= ( pixelData << scanlinePixelShift );
            }
        }

        const uint8_t scanlinePixelIndex = tileIdIndex * 2;
        pScanlinePixelData[ scanlinePixelIndex + 0 ] = interleavedScanlinePixelData[0];
        pScanlinePixelData[ scanlinePixelIndex + 1 ] = interleavedScanlinePixelData[1];
    }
}

void clearGBFrameBufferScanline( uint8_t* pGBFrameBuffer, uint8_t scanlineYCoordinate )
{
    memset( pGBFrameBuffer + scanlineYCoordinate * gbFrameBufferScanlineSizeInBytes, 0, gbFrameBufferScanlineSizeInBytes );
}

void pushScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    //FK: Clear scanline first, so we know what we're working with
    clearGBFrameBufferScanline( pPpuState->pGBFrameBuffer, scanlineYCoordinate );

    if( pPpuState->pLcdControl->bgAndWindowEnable )
    {
        pushBackgroundOrWindowPixelsToScanline( pPpuState, scanlineYCoordinate );
    }

    if( pPpuState->pLcdControl->objEnable )
    {
        pushSpritePixelsToScanline( pPpuState, scanlineYCoordinate );
    }
}

void triggerInterrupt( GBCpuState* pCpuState, uint8_t interruptFlag )
{
    *pCpuState->pIF |= interruptFlag;
}

void updatePPULcdControl( GBPpuState* pPpuState, GBLcdControl lcdControlValue )
{
    if( lcdControlValue.enable != pPpuState->pLcdControl->enable )
    {
        if( !lcdControlValue.enable )
        {
            clearGBFrameBuffer( pPpuState->pGBFrameBuffer );
        }
    }

    *pPpuState->pLcdControl = lcdControlValue;
}

uint16_t readTimerControlFrequency( const uint8_t timerControlValue )
{
    const uint8_t frequency = timerControlValue & 0x3;
    switch( frequency )
    {
        case 0b00:
            return 1024u;
        case 0xb01:
            return 16u;
        case 0xb10:
            return 64;
    }

    return 256;
}

void updateTimerRegisters( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimer )
{
    //FK: Timer Divider
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF04 )
    {
        pMemoryMapper->pBaseAddress[0xFF04] = 0x00;
    }

    //FK: Timer Control
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF07 )
    {
        const uint8_t timerControlValue = pMemoryMapper->lastValueWritten;
        pTimer->enableCounter = (timerControlValue >> 2) & 0x1;
        pTimer->counterTarget = readTimerControlFrequency( timerControlValue );
    }
}

void updateTimer( GBCpuState* pCpuState, GBTimerState* pTimer, const uint8_t cycleCount )
{
    //FK: timer divier runs at 16384hz (update counter every 256 cpu cycles to be accurate)
    pTimer->dividerCounter += cycleCount;
    if( pTimer->dividerCounter > 0xFF )
    {
        pTimer->dividerCounter -= 0xFF;
        *pTimer->pDivider += 1;
    }

    if( !pTimer->enableCounter )
    {
        return;
    }

    pTimer->counterValue += cycleCount;
    if( pTimer->counterValue > pTimer->counterTarget )
    {
        if( *pTimer->pCounter == 0xFF )
        {
            *pTimer->pCounter = *pTimer->pModulo;
            triggerInterrupt( pCpuState, TimerInterrupt );
        }

        *pTimer->pCounter += 1;
    }
}

void updatePPU( GBCpuState* pCpuState, GBPpuState* pPpuState, const uint8_t cycleCount )
{
    GBLcdStatus* pLcdStatus = pPpuState->lcdRegisters.pStatus;

    if( !pPpuState->pLcdControl->enable )
    {
        return;
    }

    pPpuState->cycleCounter += cycleCount;

    uint8_t lcdMode         = pPpuState->lcdRegisters.pStatus->mode;
    uint8_t ly              = *pPpuState->lcdRegisters.pLy;
    uint16_t lcdDotCounter  = pPpuState->dotCounter;
    const uint8_t lyc       = *pPpuState->lcdRegisters.pLyc;

    uint8_t triggerLCDStatInterrupt = 0;
    lcdDotCounter += cycleCount;

    if( lcdMode == 2 && lcdDotCounter >= 80 )
    {
        lcdDotCounter -= 80;

        lcdMode = 3;
        pushScanline( pPpuState, ly );
    }
    else if( lcdMode == 3 && lcdDotCounter >= 172 )
    {
        lcdDotCounter -= 172;

        lcdMode = 0;
        triggerLCDStatInterrupt = pLcdStatus->enableMode0HBlankInterrupt;
    }
    else if( lcdMode == 0 && lcdDotCounter >= 204 )
    {
        lcdDotCounter -= 204;
        ++ly;

        if( ly == 144 )
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
    else if( lcdMode == 1 )
    {
        if( lcdDotCounter >= 456 )
        {
            lcdDotCounter -= 456;
            ++ly;
            if( ly == 154 )
            {
                ly = 0;
                lcdMode = 2;
                triggerLCDStatInterrupt = pLcdStatus->enableMode2OAMInterrupt;
            }
        }
    }

    if( pLcdStatus->enableLycEqLyInterrupt )
    {
        if( pLcdStatus->LycEqLyFlag == 1 && ly == lyc )
        {
            triggerLCDStatInterrupt = 1;
        }

#if 0
        //FK: TODO: Check if this accurate...
        else if ( pLcdStatus->LycEqLyFlag == 0 && ly != lyc )
        {
            triggerLCDStatInterrupt = 1;
        }
#endif
    }

    if( triggerLCDStatInterrupt )
    {
        triggerInterrupt( pCpuState, LCDStatInterrupt );
    }

    //FK: update ppu state
    *pPpuState->lcdRegisters.pLy = ly;
    pPpuState->dotCounter = lcdDotCounter;
    pPpuState->lcdRegisters.pStatus->mode = lcdMode;
}

void push16BitValueToStack( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint16_t value )
{
    pCpuState->registers.SP -= 2;
    
    uint8_t* pStack = pMemoryMapper->pBaseAddress + pCpuState->registers.SP;
    pStack[0] = (uint8_t)( value >> 0 );
    pStack[1] = (uint8_t)( value >> 8 );
}

uint16_t pop16BitValueFromStack( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper )
{
    uint8_t* pStack = pMemoryMapper->pBaseAddress + pCpuState->registers.SP;
    pCpuState->registers.SP += 2;

    const uint16_t value = (uint16_t)pStack[0] << 0 | 
                           (uint16_t)pStack[1] << 8;
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
        for( uint8_t interruptIndex = 0u; interruptIndex < 5u; ++interruptIndex )
        {
            //FK:   start with lowest prio interrupt, that way the program counter will eventually be overwritten with the higher prio interrupts
            //      with the lower prio interrupts being pushed to the stack
            const uint8_t interruptFlag = 1 << ( 4u - interruptIndex );
            if( interruptHandleMask & interruptFlag )
            {
                push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
                pCpuState->registers.PC = 0x40 + 0x08 * ( 4u - interruptIndex );
                *pCpuState->pIF &= ~interruptFlag;
                break;
            }
        }

        pCpuState->flags.halt = 0;
    }
}

void handleCbOpcode(GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode)
{
    uint8_t* pTarget = nullptr;
    uint8_t target = (opcode & 0x0F);
    const uint8_t operation = ( opcode & 0xF0 );
    uint8_t bitIndex = ((operation%0x40)/0x10) * 2 + (target > 0x07);
    target = target > 0x07 ? target - 0x08 : target;
    switch( target )
    {
        case 0x00:
        {
            pTarget = &pCpuState->registers.B;
            break;
        }
        case 0x01:
        {
            pTarget = &pCpuState->registers.C;
            break;
        }
        case 0x02:
        {
            pTarget = &pCpuState->registers.D;
            break;
        }
        case 0x03:
        {
            pTarget = &pCpuState->registers.E;
            break;
        }
        case 0x04:
        {
            pTarget = &pCpuState->registers.H;
            break;
        }
        case 0x05:
        {
            pTarget = &pCpuState->registers.L;
            break;
        }
        case 0x06:
        {
            pTarget = getMemoryAddress( pMemoryMapper, pCpuState->registers.HL );
            break;
        }
        case 0x07:
        {
            pTarget = &pCpuState->registers.A;
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
                const uint8_t value = *pTarget;
                const uint8_t newValue = value << 1 | ((value & (1<<7)) >> 7);
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = (value >> 7) & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            else
            {
                //RRC
                const uint8_t value = *pTarget;
                const uint8_t newValue = value >> 1 | ((value & 0x1) << 7);
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = value & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            break;
        }
        case 0x10:
        {
            if( target <= 0x07 )
            {
                //RL
                const uint8_t value = *pTarget;
                const uint8_t newValue = value << 1 | pCpuState->registers.F.C;
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = (value >> 7) & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            else
            {
                //RR
                const uint8_t value = *pTarget;
                const uint8_t newValue = value >> 1 | (pCpuState->registers.F.C << 7);
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = value & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            break;
        }
        case 0x20:
        {
            if( target <= 0x07 )
            {
                //SLA
                const uint8_t value = *pTarget;
                const uint8_t newValue = value << 1;
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = (value >> 7) & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            else
            {
                //SRA
                const uint8_t value = *pTarget;
                const uint8_t newValue = value >> 1 | (value & (1<<7) << 7);
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = value & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            break;
        }
        case 0x30:
        {
            if( target <= 0x07 )
            {
                //SWAP
                const uint8_t value = *pTarget;
                const uint8_t highNibble = value & 0xF0;
                const uint8_t lowNibble  = value & 0x0F;
                const uint8_t newValue = lowNibble << 4 | highNibble << 0;

                *pTarget = newValue;

                pCpuState->registers.F.Z = (value == 0);
                pCpuState->registers.F.C = 0;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            else
            {
                //SRL
                const uint8_t value = *pTarget;
                const uint8_t newValue = value >> 1;
                *pTarget = newValue;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = value & 0x1;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            break;
        }
        //BIT
        case 0x40:
        case 0x50:
        case 0x60:
        case 0x70:
        {
            const uint8_t value = *pTarget;
            const uint8_t bitValue = (value >> bitIndex) & 0x1;
            pCpuState->registers.F.Z = bitValue == 0;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = 1;
            break;
        }
        //RES
        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        {
            const uint8_t value = *pTarget;
            const uint8_t newValue = value & ~(1 << bitIndex);
            *pTarget = newValue;
            break;
        }
        //SET
        case 0xC0:
        case 0xD0:
        case 0xE0:
        case 0xF0:
        {
            const uint8_t value = *pTarget;
            const uint8_t newValue = value | (1 << bitIndex);
            *pTarget = newValue;
            break;
        }
    }
}

uint8_t executeOpcode( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode )
{
    uint8_t opcodeCondition = 0;
    const GBOpcode* pOpcode = opcode == 0xCB ? cbPrefixedOpcodes + opcode : unprefixedOpcodes + opcode;

    switch( opcode )
    {
        //NOP
        case 0x00:
            break;

        //CALL nn
        case 0xCD:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC + 2);
            pCpuState->registers.PC = address;
            break;
        }

        case 0xC4:
        case 0xCC:
        case 0xD4:
        case 0xDC:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2;

            uint8_t condition = 0;
            switch( opcode )
            {
                //CALL nz, nn
                case 0xC4:
                    condition = pCpuState->registers.F.Z == 0;
                    break;
                //CALL nc, nn
                case 0xCC:
                    condition = pCpuState->registers.F.C == 0;
                    break;
                //CALL z, nn
                case 0xD4:
                    condition = pCpuState->registers.F.Z == 1;
                    break;
                //CALL c, nn
                case 0xDC:
                    condition = pCpuState->registers.F.C == 1;
                    break;
            }

            if( condition )
            {
                push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
                pCpuState->registers.PC = address;
            }

            opcodeCondition = condition;
            break;
        }
  

        //LD A,(BC)
        case 0x0A:
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.BC);
            break;

        //LD A,(DE)
        case 0x1A:
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.DE);
            break;

        //LD (BC), A
        case 0x02:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->registers.BC, pCpuState->registers.A);
            break;

        //LD (DE), A
        case 0x12:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->registers.DE, pCpuState->registers.A);
            break;

        //LD (HL), A
        case 0x77:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->registers.HL, pCpuState->registers.A);
            break;

        //LD A,(nn)
        case 0xFA:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, address);
            pCpuState->registers.PC += 2;
            break;     
        }

        //LD (nn), A
        case 0xEA:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            pCpuState->registers.PC += 2;
            break;
        }

        //LD A,n
        case 0x3E:
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;

        //LD B,n
        case 0x06:  
            pCpuState->registers.B = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;

        //LD C,n
        case 0x0E:
            pCpuState->registers.C = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;

        //LD D,n
        case 0x16: 
            pCpuState->registers.D = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;
            
        //LD E,n
        case 0x1E: 
            pCpuState->registers.E = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;
        
        //LD H,n
        case 0x26:
            pCpuState->registers.H = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;

        //LD L,n
        case 0x2E:
            pCpuState->registers.L = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            break;
        
        //LD a,(HL++)
        case 0x2A:
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL++);
            break;

        //LD a,(HL++)
        case 0x3A:
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL--);
            break;

        //LD (HL++),a
        case 0x22:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->registers.HL++, pCpuState->registers.A);
            break;

        //LD (HL--),a
        case 0x32:
            write8BitValueToMappedMemory(pMemoryMapper, pCpuState->registers.HL--, pCpuState->registers.A);
            break;

        //LD (C),a
        case 0xE2:
        {
            const uint16_t address = 0xFF00 + pCpuState->registers.C;
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            break;
        }

        //LD b,a
        case 0x47:
        {
            pCpuState->registers.B = pCpuState->registers.A;
            break;
        }

        //LD c, a
        case 0x4F:
        {
            pCpuState->registers.C = pCpuState->registers.A;
            break;
        }

        //LD E, A
        case 0x5F:
        {
            pCpuState->registers.E = pCpuState->registers.A;
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
        case 0x7D:
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
        case 0x57:
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
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
        {
            uint8_t* pTarget            = nullptr;
            uint8_t value               = 0;
            switch(opcode)
            {
                //LD A, A
                case 0x7F:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.A;
                    break;
                //LD A, B
                case 0x78:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.B;
                    break;
                //LD A, C
                case 0x79:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.C;
                    break;
                //LD A, D
                case 0x7A:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.D;
                    break;
                //LD A, E
                case 0x7B:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.E;
                    break;
                //LD A, H
                case 0x7C:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.H;
                    break;
                //LD A, L
                case 0x7D:
                    pTarget         = &pCpuState->registers.A;
                    value           = pCpuState->registers.L;
                    break;
                //LD A, (HL)
                case 0x7E:
                    pTarget         = &pCpuState->registers.A;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;

                //LD B, B
                case 0x40:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.B;
                    break;
                //LD B, C
                case 0x41:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.C;
                    break;
                //LD B, D
                case 0x42:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.D;
                    break;
                //LD B, E
                case 0x43:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.E;
                    break;
                //LD B, H
                case 0x44:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.H;
                    break;
                //LD B, L
                case 0x45:
                    pTarget         = &pCpuState->registers.B;
                    value           = pCpuState->registers.L;
                    break;
                //LD B, (HL)
                case 0x46:
                    pTarget         = &pCpuState->registers.B;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;

                //LD C, B
                case 0x48:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.B;
                    break;
                //LD C, C
                case 0x49:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.C;
                    break;
                //LD C, D
                case 0x4A:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.D;
                    break;
                //LD C, E
                case 0x4B:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.E;
                    break;
                //LD C, H
                case 0x4C:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.H;
                    break;
                //LD C, L
                case 0x4D:
                    pTarget         = &pCpuState->registers.C;
                    value           = pCpuState->registers.L;
                    break;
                //LD C, (HL)
                case 0x4E:
                    pTarget         = &pCpuState->registers.C;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;

                //LD D, B
                case 0x50:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.B;
                    break;
                //LD D, C
                case 0x51:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.C;
                    break;
                //LD D, D
                case 0x52:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.D;
                    break;
                //LD D, E
                case 0x53:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.E;
                    break;
                //LD D, H
                case 0x54:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.H;
                    break;
                //LD D, L
                case 0x55:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.L;
                    break;
                //LD D, A
                case 0x57:
                    pTarget         = &pCpuState->registers.D;
                    value           = pCpuState->registers.A;
                    break;
                //LD B, (HL)
                case 0x56:
                    pTarget         = &pCpuState->registers.D;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;

                //LD E, B
                case 0x58:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.B;
                    break;
                //LD E, C
                case 0x59:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.C;
                    break;
                //LD E, D
                case 0x5A:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.D;
                    break;
                //LD E, E
                case 0x5B:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.E;
                    break;
                //LD E, H
                case 0x5C:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.H;
                    break;
                //LD E, L
                case 0x5D:
                    pTarget         = &pCpuState->registers.E;
                    value           = pCpuState->registers.L;
                    break;
                //LD E, (HL)
                case 0x5E:
                    pTarget         = &pCpuState->registers.E;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;

                //LD H, B
                case 0x60:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.B;
                    break;
                //LD H, C
                case 0x61:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.C;
                    break;
                //LD H, D
                case 0x62:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.D;
                    break;
                //LD H, E
                case 0x63:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.E;
                    break;
                //LD H, H
                case 0x64:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.H;
                    break;
                //LD H, L
                case 0x65:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.L;
                    break;
                //LD H, (HL)
                case 0x66:
                    pTarget         = &pCpuState->registers.H;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                case 0x67:
                    pTarget         = &pCpuState->registers.H;
                    value           = pCpuState->registers.A;
                    break;

                //LD L, B
                case 0x68:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.B;
                    break;
                //LD L, C
                case 0x69:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.C;
                    break;
                //LD L, D
                case 0x6A:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.D;
                    break;
                //LD L, E
                case 0x6B:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.E;
                    break;
                //LD L, H
                case 0x6C:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.H;
                    break;
                //LD L, L
                case 0x6D:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.L;
                    break;
                //LD L, (HL)
                case 0x6E:
                    pTarget         = &pCpuState->registers.L;
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //LD L, A
                case 0x6F:
                    pTarget         = &pCpuState->registers.L;
                    value           = pCpuState->registers.A;
                    break;
                //LD (HL), B
                case 0x70:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.B;
                    break;
                //LD (HL), C
                case 0x71:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.C;
                    break;
                //LD (HL), D
                case 0x72:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.D;
                    break;
                //LD (HL), E
                case 0x73:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.E;
                    break;
                //LD (HL), H
                case 0x74:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.H;
                    break;
                //LD (HL), L
                case 0x75:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = pCpuState->registers.L;
                    break;
                //LD (HL), n
                case 0x36:
                    pTarget         = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
                    value           = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            *pTarget = value;
            break;
        }

        //LD SP, HL
        case 0xF9:
        {
            pCpuState->registers.SP = pCpuState->registers.HL;
            break;
        }

        //LD HL, SP + n
        case 0xF8:
        {
            const int8_t offset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.HL = pCpuState->registers.SP + offset;
            pCpuState->registers.F.Z = 0;
            pCpuState->registers.F.N = 0;
            break;
        }

        //LD (nn), SP
        case 0x08:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            write16BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.SP);
            pCpuState->registers.PC += 2;
        }

        //JP nn
        case 0xC3:
        {
            const uint16_t addressToJumpTo = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC = addressToJumpTo;
            break;
        }

        //JP HL
        case 0xE9:
        {
            pCpuState->registers.PC = pCpuState->registers.HL;
            break;
        }

        //JP conditional
        case 0xC2:
        case 0xCA:
        case 0xD2:
        case 0xDA:
        {
            const uint16_t address = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2;

            uint8_t condition = 0;
            switch( opcode )
            {
                //JP nz nn
                case 0xC2:
                    condition = pCpuState->registers.F.Z == 0;
                    break;
                //JP nc nn
                case 0xD2:
                    condition = pCpuState->registers.F.C == 0;
                    break;
                //JP z nn
                case 0xCA:
                    condition = pCpuState->registers.F.Z == 1;
                    break;
                //JP c nn
                    condition = pCpuState->registers.F.C == 1;
                    break;
            }

            if( condition )
            {
                pCpuState->registers.PC = address;
            }

            opcodeCondition = condition;

            break;
        }


        //JR n
        case 0x18:
        {
            const int8_t addressOffset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.PC += addressOffset;
            break;
        }

        //JR
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38:
        {
            const int8_t addressOffset = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            uint8_t condition = 0;
            switch(opcode)
            {
                //JR nz
                case 0x20:
                    condition = pCpuState->registers.F.Z == 0;
                    break;
                //JR nc
                case 0x30:
                    condition = pCpuState->registers.F.C == 0;
                    break;
                //JR z
                case 0x28:
                    condition = pCpuState->registers.F.Z == 1;
                    break;
                //JR c
                case 0x38:
                    condition = pCpuState->registers.F.C == 1;
                    break;
            }

            if( condition )
            {
                pCpuState->registers.PC += addressOffset;
            }

            opcodeCondition = condition;
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
                    operand = pCpuState->registers.A;
                    break;
                case 0xA8:
                    operand = pCpuState->registers.B;
                    break;
                case 0xA9:
                    operand = pCpuState->registers.C;
                    break;
                case 0xAA:
                    operand = pCpuState->registers.D;
                    break;
                case 0xAB:
                    operand = pCpuState->registers.E;
                    break;
                case 0xAC:
                    operand = pCpuState->registers.H;
                    break;
                case 0xAD:
                    operand = pCpuState->registers.L;
                    break;
                case 0xAE:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                case 0xEE:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }
            
            pCpuState->registers.A        = pCpuState->registers.A ^ operand;
            pCpuState->registers.F.value  = 0x0;
            pCpuState->registers.F.Z      = (pCpuState->registers.A == 0);
            
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
                    operand = pCpuState->registers.A;
                    break;
                //OR B
                case 0xB0:
                    operand = pCpuState->registers.B;
                    break;
                //OR C
                case 0xB1:
                    operand = pCpuState->registers.C;
                    break;
                //OR D
                case 0xB2:
                    operand = pCpuState->registers.D;
                    break;
                //OR E
                case 0xB3:
                    operand = pCpuState->registers.E;
                    break;
                //OR H
                case 0xB4:
                    operand = pCpuState->registers.H;
                    break;
                //OR L
                case 0xB5:
                    operand = pCpuState->registers.L;
                    break;
                //OR (HL)
                case 0xB6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //OR #
                case 0xF6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            pCpuState->registers.A        = pCpuState->registers.A | operand;
            pCpuState->registers.F.value  = 0x0;
            pCpuState->registers.F.Z      = (pCpuState->registers.A == 0);
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
                    operand = pCpuState->registers.A;
                    break;
                //AND B
                case 0xA0:
                    operand = pCpuState->registers.B;
                    break;
                //AND C
                case 0xA1:
                    operand = pCpuState->registers.C;
                    break;
                //AND D
                case 0xA2:
                    operand = pCpuState->registers.D;
                    break;
                //AND E
                case 0xA3:
                    operand = pCpuState->registers.E;
                    break;
                //AND H
                case 0xA4:
                    operand = pCpuState->registers.H;
                    break;
                //AND L
                case 0xA5:
                    operand = pCpuState->registers.L;
                    break;
                //AND (HL)
                case 0xA6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //AND #
                case 0xE6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            pCpuState->registers.A = pCpuState->registers.A & operand;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = 1;
            pCpuState->registers.F.C = 0;
            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            break;
        }

        //LD n,nn (16bit loads)
        case 0x01:
            pCpuState->registers.BC = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2u;
            break;
        case 0x11:
            pCpuState->registers.DE = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2u;
            break;
        case 0x21:
            pCpuState->registers.HL = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2u;
            break;
        case 0x31:
            pCpuState->registers.SP = read16BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2u;
            break;

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
                    pRegister = &pCpuState->registers.A;
                    break;
                //INC B
                case 0x04:
                    pRegister = &pCpuState->registers.B;
                    break;
                //INC C
                case 0x0C:
                    pRegister = &pCpuState->registers.C;
                    break;
                //INC D
                case 0x14:
                    pRegister = &pCpuState->registers.D;
                    break;
                //INC E
                case 0x1C:
                    pRegister = &pCpuState->registers.E;
                    break;
                //INC H
                case 0x24:
                    pRegister = &pCpuState->registers.H;
                    break;
                //INC L
                case 0x2C:
                    pRegister = &pCpuState->registers.L;
                    break;
            }

            const uint8_t oldRegisterValue = *pRegister;
            const uint8_t newRegisterValue = oldRegisterValue + 1;

            *pRegister = newRegisterValue;
            pCpuState->registers.F.Z = (newRegisterValue == 0);
            pCpuState->registers.F.H = (oldRegisterValue & 0xF0) != (newRegisterValue & 0xF0);
            pCpuState->registers.F.N = 0;
            
            break;
        }

        //INC BC
        case 0x03:
        {
            pCpuState->registers.BC++;
            break;
        }
        //INC DE
        case 0x13:
        {
            pCpuState->registers.DE++;
            break;
        }
        //INC HL
        case 0x23:
        {
            pCpuState->registers.HL++;
            break;
        }
        //INC SP
        case 0x33:
        {
            pCpuState->registers.SP++;
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
                    pRegister = &pCpuState->registers.A;
                    break;
                //DEC B
                case 0x05:
                    pRegister = &pCpuState->registers.B;
                    break;
                //DEC C
                case 0x0D:
                    pRegister = &pCpuState->registers.C;
                    break;
                //DEC D
                case 0x15:
                    pRegister = &pCpuState->registers.D;
                    break;
                //DEC E
                case 0x1D:
                    pRegister = &pCpuState->registers.E;
                    break;
                //DEC H
                case 0x25:
                    pRegister = &pCpuState->registers.H;
                    break;
                //DEC L
                case 0x2D:
                    pRegister = &pCpuState->registers.L;
                    break;
            }

            const uint8_t oldRegisterValue = *pRegister;
            const uint8_t newRegisterValue = oldRegisterValue - 1;

            *pRegister = newRegisterValue;

            pCpuState->registers.F.Z = (newRegisterValue == 0);
            pCpuState->registers.F.H = ((newRegisterValue & 0x0F) == 0x0F);
            pCpuState->registers.F.N = 1;
            break;
        }

        //DEC BC
        case 0x0B:
        { 
            pCpuState->registers.BC--;
            break;
        }
        //DEC DE
        case 0x1B:
        {
            pCpuState->registers.DE--;
            break;
        }
        //DEC HL
        case 0x2B:
        {
            pCpuState->registers.HL--;
            break;
        }
        //DEC SP
        case 0x3B:
        {
            pCpuState->registers.SP--;
            break;
        }
        //HALT
        case 0x76:
            pCpuState->flags.halt = 1;
            break;

        //LDH (n),A
        case 0xE0:
        {
            const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            break;
        }

        //LDH A,(n)
        case 0xF0:
        {
            const uint16_t address = 0xFF00 + read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.A = read8BitValueFromAddress(pMemoryMapper, address);
            break;
        }

        //DI
        case 0xF3:
        {
            pCpuState->flags.IME = 0;
            break;
        }

        //EI
        case 0xFB:
        {   
            pCpuState->flags.IME = 1;
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
                    operand = pCpuState->registers.BC;
                    break;
                //ADD HL, DE
                case 0x19:
                    operand = pCpuState->registers.DE;
                    break;
                //ADD HL, HL
                case 0x29:
                    operand = pCpuState->registers.HL;
                    break;
                //ADD HL, SP
                case 0x39:
                    operand = pCpuState->registers.SP;
                    break;
            }

            const uint32_t new32BitRegisterValue = pCpuState->registers.HL + operand;

            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = (new32BitRegisterValue > 0xFFF0);
            pCpuState->registers.F.C = (new32BitRegisterValue > 0xFFFF);

            pCpuState->registers.HL = (uint16_t)new32BitRegisterValue;
            break;
        }

        //ADD SP, n
        case 0xE8:
        {
            const int8_t value = (int8_t)read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.SP += value;

            pCpuState->registers.F.Z = 0;
            pCpuState->registers.F.N = 0;
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
                    operand = pCpuState->registers.A;
                    break;
                //ADD A, B
                case 0x80:
                    operand = pCpuState->registers.B;
                    break;
                //ADD A, C
                case 0x81:
                    operand = pCpuState->registers.C;
                    break;
                //ADD A, D
                case 0x82:
                    operand = pCpuState->registers.D;
                    break;
                //ADD A, E
                case 0x83:
                    operand = pCpuState->registers.E;
                    break;
                //ADD A, H
                case 0x84:
                    operand = pCpuState->registers.H;
                    break;
                //ADD A, L
                case 0x85:
                    operand = pCpuState->registers.L;
                    break;
                //ADD A, (HL)
                case 0x86:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //ADD A, #
                case 0xC6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            //FK: promoting to 16bit to check for potential carry or half carry...Is there a better way?
            const uint16_t accumulator16BitValue = pCpuState->registers.A + operand;
            pCpuState->registers.A += operand;

            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            pCpuState->registers.F.C = (accumulator16BitValue > 0xFF);
            pCpuState->registers.F.H = (accumulator16BitValue > 0x0F);
            pCpuState->registers.F.N = 0;
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
                    operand = pCpuState->registers.A;
                    break;
                //SUB A, B
                case 0x80:
                    operand = pCpuState->registers.B;
                    break;
                //SUB A, C
                case 0x81:
                    operand = pCpuState->registers.C;
                    break;
                //SUB A, D
                case 0x82:
                    operand = pCpuState->registers.D;
                    break;
                //SUB A, E
                case 0x83:
                    operand = pCpuState->registers.E;
                    break;
                //SUB A, H
                case 0x84:
                    operand = pCpuState->registers.H;
                    break;
                //SUB A, L
                case 0x85:
                    operand = pCpuState->registers.L;
                    break;
                //SUB A, (HL)
                case 0x86:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //SUB A, #
                case 0xD6:
                    operand = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            const uint8_t accumulatorValue = pCpuState->registers.A;
            pCpuState->registers.A -= operand;

            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            pCpuState->registers.F.C = (accumulatorValue < operand);
            pCpuState->registers.F.H = ((accumulatorValue & 0x0F) < (operand & 0x0F));
            pCpuState->registers.F.N = 1;

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
                    condition = pCpuState->registers.F.Z == 0;
                    break;

                //RET NC
                case 0xD0:
                    condition = pCpuState->registers.F.C == 0;
                    break;

                //RET NZ
                case 0xC8:
                    condition = pCpuState->registers.F.Z == 1;
                    break;

                //RET NC
                case 0xD8:
                    condition = pCpuState->registers.F.C == 1;
                    break;
            }

            if( condition )
            {
                pCpuState->registers.PC = pop16BitValueFromStack(pCpuState, pMemoryMapper);
            }

            opcodeCondition = condition;
            break;
        }

        //RETI
        case 0xD9:
        {
            pCpuState->flags.IME = 1;
            pCpuState->registers.PC = pop16BitValueFromStack(pCpuState, pMemoryMapper);
            break;
        }

        //RET
        case 0xC9:
        {
            pCpuState->registers.PC = pop16BitValueFromStack(pCpuState, pMemoryMapper);
            break;
        }

        //INC (HL)
        case 0x34:
        {
            uint8_t* pValue = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
            const uint8_t newValue = *pValue + 1;
            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = (newValue > 0x0F);
            pCpuState->registers.F.N = 0;

            *pValue = newValue;
            break;
        }

        //DEC (HL)
        case 0x35:
        {
            uint8_t* pValue = getMemoryAddress(pMemoryMapper, pCpuState->registers.HL);
            const uint8_t newValue = *pValue - 1;
            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = ((newValue & 0x0F) == 0x0F);
            pCpuState->registers.F.N = 0;

            *pValue = newValue;
            break;
        }

        // CPL
        case 0x2F:
        {
            pCpuState->registers.A = ~pCpuState->registers.A;
            pCpuState->registers.F.N = 1;
            pCpuState->registers.F.H = 1;
            break;
        }

        //CCF
        case 0x3F:
        {
            pCpuState->registers.F.C = !pCpuState->registers.F.C;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = 0;
            break;
        }

        //SCF
        case 0x37:
        {
            pCpuState->registers.F.C = 1;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = 0;
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
                    value = pCpuState->registers.AF;
                    break;
                //PUSH BC
                case 0xC5:
                    value = pCpuState->registers.BC;
                    break;
                //PUSH DE
                case 0xD5:
                    value = pCpuState->registers.DE;
                    break;
                //PUSH HL
                case 0xE5:
                    value = pCpuState->registers.HL;
                    break;
            }
            
            push16BitValueToStack( pCpuState, pMemoryMapper, value );
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
                    pTarget = &pCpuState->registers.AF;
                    break;
                //POP BC
                case 0xC1:
                    pTarget = &pCpuState->registers.BC;
                    break;
                //POP DE
                case 0xD1:
                    pTarget = &pCpuState->registers.DE;
                    break;
                //POP HL
                case 0xE1:
                    pTarget = &pCpuState->registers.HL;
                    break;
            }
            *pTarget = pop16BitValueFromStack( pCpuState, pMemoryMapper );
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
            const uint8_t cbOpcode = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            handleCbOpcode(pCpuState, pMemoryMapper, cbOpcode);
            break;
        }

        //ADC
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0x8E:
        case 0x8F:
        {
            uint8_t value = 0;
            switch(opcode)
            {
                //ADC B
                case 0x88:
                    value = pCpuState->registers.B;
                    break;
                //ADC C
                case 0x89:
                    value = pCpuState->registers.C;
                    break;
                //ADC D
                case 0x8A:
                    value = pCpuState->registers.D;
                    break;
                //ADC E
                case 0x8B:
                    value = pCpuState->registers.E;
                    break;
                //ADC H
                case 0x8C:
                    value = pCpuState->registers.H;
                    break;
                //ADC L
                case 0x8D:
                    value = pCpuState->registers.L;
                    break;
                //ADC (HL)
                case 0x8E:
                    value = read8BitValueFromAddress( pMemoryMapper, pCpuState->registers.HL );
                    break;
                //ADC A
                case 0x8F:
                    value = pCpuState->registers.A;
                    break;
            }

            const uint16_t accumulator16BitValue = pCpuState->registers.A + pCpuState->registers.F.C + value;
            pCpuState->registers.A = ( uint8_t )accumulator16BitValue;
            
            pCpuState->registers.F.Z = (accumulator16BitValue == 0);
            pCpuState->registers.F.C = (accumulator16BitValue > 0xFF);
            pCpuState->registers.F.H = (accumulator16BitValue > 0x0F);
            pCpuState->registers.F.N = 0;
            break;
        }

        //ADC n
        case 0xCE:
        {
            const uint8_t value = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
            const uint16_t accumulator16BitValue = pCpuState->registers.A + pCpuState->registers.F.C + value;

            pCpuState->registers.A = ( uint8_t )accumulator16BitValue;
            
            pCpuState->registers.F.Z = (accumulator16BitValue == 0);
            pCpuState->registers.F.C = (accumulator16BitValue > 0xFF);
            pCpuState->registers.F.H = (accumulator16BitValue > 0x0F);
            pCpuState->registers.F.N = 0;
            break;
        }

        //SBC
        case 0x98:
        case 0x99:
        case 0x9A:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
        {
            uint8_t value = 0;
            switch(opcode)
            {
                //SBC B
                case 0x98:
                    value = pCpuState->registers.B;
                    break;
                //SBC C
                case 0x99:
                    value = pCpuState->registers.B;
                    break;
                //SBC D
                case 0x9A:
                    value = pCpuState->registers.B;
                    break;
                //SBC E
                case 0x9B:
                    value = pCpuState->registers.B;
                    break;
                //SBC H
                case 0x9C:
                    value = pCpuState->registers.B;
                    break;
                //SBC L
                case 0x9D:
                    value = pCpuState->registers.B;
                    break;
                //SBC (HL)
                case 0x9E:
                    value = read8BitValueFromAddress( pMemoryMapper, pCpuState->registers.HL );
                    break;
                //SBC A
                case 0x9F:
                    value = pCpuState->registers.A;
                    break;
            }

            const uint8_t accumulatorValue = pCpuState->registers.A;
            value += pCpuState->registers.F.C;

            pCpuState->registers.A = accumulatorValue - value;
            
            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            pCpuState->registers.F.C = (accumulatorValue < value);
            pCpuState->registers.F.H = ((accumulatorValue & 0x0F) < (value & 0x0F));
            pCpuState->registers.F.N = 1;
            break;
        }

        //DAA
        case 0x27:
        {
            uint16_t accumulator = pCpuState->registers.A;

            if( !pCpuState->registers.F.N )
            {
                if( pCpuState->registers.F.H || ( accumulator & 0xF ) > 9 )
                {
                    accumulator += 0x06;
                }

                if( pCpuState->registers.F.C || ( accumulator > 0x9F ) )
                {
                    accumulator += 0x60;
                }
            }
            else
            {
                if( pCpuState->registers.F.H )
                {
                    accumulator = ( accumulator - 6 ) & 0xFF;
                }

                if( pCpuState->registers.F.C )
                {
                    accumulator -= 0x60;
                }
            }

            pCpuState->registers.F.H = 0;
            pCpuState->registers.F.C = ( accumulator & 0x100 ) == 0x100;

            pCpuState->registers.A = accumulator & 0xFF;
            pCpuState->registers.F.Z = ( pCpuState->registers.A == 0 );

            break;
        }

        //RLCA
        case 0x07:
        //RLA
        case 0x17:
        //RRCA
        case 0x0F:
        //RRA
        case 0x1F:
        {
            handleCbOpcode(pCpuState, pMemoryMapper, opcode);
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
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC = address;
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
                    value = pCpuState->registers.A;
                    break;
                //CP B
                case 0xB8:
                    value = pCpuState->registers.B;
                    break;
                //CP C
                case 0xB9:
                    value = pCpuState->registers.C;
                    break;
                //CP D
                case 0xBA:
                    value = pCpuState->registers.D;
                    break;
                //CP E
                case 0xBB:
                    value = pCpuState->registers.E;
                    break;
                //CP H
                case 0xBC:
                    value = pCpuState->registers.H;
                    break;
                //CP L
                case 0xBD:
                    value = pCpuState->registers.L;
                    break;
                //CP (HL)
                case 0xBE:
                    value = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.HL);
                    break;
                //CP #
                case 0xFE:
                    value = read8BitValueFromAddress(pMemoryMapper, pCpuState->registers.PC++);
                    break;
            }

            pCpuState->registers.F.Z = (pCpuState->registers.A == value);
            pCpuState->registers.F.H = ((pCpuState->registers.A & 0x0F) < (value & 0x0F));
            pCpuState->registers.F.C = (pCpuState->registers.A < value);
            pCpuState->registers.F.N = 1;

            break;
        }

        case 0xD3:
        case 0xE3:
        case 0xE4:
        case 0xF4:
        case 0xDB:
        case 0xDD:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xFC:
        case 0xFD:
        {
            printf( "Illegal opcode '%.2hhX\n", opcode );
            debugBreak();
            break;
        }
        default:
            printf( "not implemented opcode '$%.2hhX' at pc '$%.4hX'\n", opcode, ( pCpuState->registers.PC - pOpcode->byteCount ) );
#if K15_BREAK_ON_UNKNOWN_INSTRUCTION == 1
            debugBreak();
#endif
    }

    return pOpcode->cycleCosts[ opcodeCondition ];
}

void checkDMAState( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t cycleCostOfLastOpCode )
{
    if( pCpuState->flags.dma )
    {
        pCpuState->dmaCycleCounter += cycleCostOfLastOpCode;
        if( pCpuState->dmaCycleCounter >= gbDMACycleCount )
        {
            pCpuState->flags.dma = 0;

            //FK: Copy sprite attributes from dma address to OAM h
            memcpy( pMemoryMapper->pSpriteAttributes, pMemoryMapper->pBaseAddress + pCpuState->dmaAddress, gbOAMSizeInBytes );
        }
    }

    const uint8_t startDMATransfer = ( pMemoryMapper->lastAddressWrittenTo == 0xFF46 );
    if( startDMATransfer )
    {
        //FK: Cpu can only write to HRAM during DMA transfer
        pCpuState->flags.dma = 1;

        pCpuState->dmaAddress = ( pMemoryMapper->lastValueWritten << 8 );

        //FK: Count up to 160 cycles for the dma flag to be reset
        pCpuState->dmaCycleCounter = 0;

        //FK: hack to not run into an endless DMA loop
        pMemoryMapper->lastAddressWrittenTo = 0x0000;
    }
}

uint8_t fixJoypadState( uint8_t joypadState )
{
    //FK: disallow simultaneous input of opposite dpad values
    const uint8_t leftAndRight = joypadState & (1<<0) | joypadState & (1<<1);
    const uint8_t upAndDown = ( joypadState & (1<<2) | joypadState & (1<<3) ) << 2;

    if( leftAndRight == 0 )
    {
        joypadState |= 0x3; //FK: ignore both left and right
    }

    if( upAndDown == 0 )
    {
        joypadState |= 0xC; //FK: ignore both up and down
    }

    return joypadState;
}

bool handleInput( GBMemoryMapper* pMemoryMapper, GBEmulatorJoypadState joypadState )
{
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF00 )
    {
        //FK: bit 7&6 aren't used so we'll set them (mimicing bgb)
        const uint8_t queryJoypadValue          = pMemoryMapper->pBaseAddress[0xFF00];
        const uint8_t selectActionButtons       = ( queryJoypadValue & (1 << 5) ) == 0;
        const uint8_t selectDirectionButtons    = ( queryJoypadValue & (1 << 4) ) == 0;
        const uint8_t prevJoypadState           = ( queryJoypadValue & 0x0F ); 
        uint8_t newJoypadState                  = 0;
        //FK: Write joypad values to 0xFF00
        if( selectActionButtons )
        {
            //FK: Flip button mask since in our world bit set = input pressed. It's reversed in the gameboy world, though
            newJoypadState = ~joypadState.actionButtonMask & 0x0F;
        }
        else if( selectDirectionButtons )
        {
            //FK: Flip button mask since in our world bit set = input pressed. It's reversed in the gameboy world, though
            newJoypadState = ~joypadState.dpadButtonMask & 0x0F;
        }

        newJoypadState = fixJoypadState( newJoypadState );
        pMemoryMapper->pBaseAddress[0xFF00] = 0xC0 | ( queryJoypadValue & 0x30 ) | newJoypadState;

        return prevJoypadState != newJoypadState;
    }

    return 0;
}

void setGBEmulatorInstanceJoypadState( GBEmulatorInstance* pEmulatorInstance, GBEmulatorJoypadState joypadState )
{
    pEmulatorInstance->joypadState = joypadState;
}

uint8_t runSingleInstruction( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper   = pEmulatorInstance->pMemoryMapper;
    GBCpuState* pCpuState           = pEmulatorInstance->pCpuState;

    triggerPendingInterrupts( pCpuState, pMemoryMapper );

    if( pCpuState->flags.halt )
    {
        //FK: TODO: Check if this accurate
        return 2;
    }

    GBPpuState* pPpuState           = pEmulatorInstance->pPpuState;
    GBTimerState* pTimerState       = pEmulatorInstance->pTimerState;

    const uint16_t opcodeAddress = pCpuState->registers.PC++;
    const uint8_t opcode         = read8BitValueFromAddress( pMemoryMapper, opcodeAddress );

    const uint8_t cycleCost = executeOpcode( pCpuState, pMemoryMapper, opcode );
    updateTimerRegisters( pMemoryMapper, pTimerState );
    
    checkDMAState( pCpuState, pMemoryMapper, cycleCost ); 
    updatePPU( pCpuState, pPpuState, cycleCost );
    updateTimer( pCpuState, pTimerState, cycleCost );
    if( handleInput( pMemoryMapper, pEmulatorInstance->joypadState ) )
    {
        triggerInterrupt( pCpuState, JoypadInterrupt );
    }

    //FK: LCD Control
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF40 )
    {
        GBLcdControl lcdControlValue;
        memcpy(&lcdControlValue, &pMemoryMapper->lastValueWritten, sizeof(GBLcdControl) );
        updatePPULcdControl( pPpuState, lcdControlValue );
    }

    pMemoryMapper->lcdStatus = *pPpuState->lcdRegisters.pStatus;
    pMemoryMapper->lcdEnabled = pPpuState->pLcdControl->enable;

    return cycleCost;
}

void convertGBFrameBufferToRGB8Buffer( uint8_t* pRGBFrameBuffer, const uint8_t* pGBFrameBuffer)
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
			const uint8_t pixels	 			= pGBFrameBuffer[frameBufferPixelIndex];
			
			for( uint8_t pixelIndex = 0; pixelIndex < 4; ++pixelIndex )
			{
				const uint8_t pixelValue = pixels >> ( ( 3 - pixelIndex ) * 2 ) & 0x3;

				//FK: Map gameboy pixels to rgb pixels
				const float intensity = pixelIntensity[ pixelValue ];
				const float r = intensity * gbRGBMapping[0];
				const float g = intensity * gbRGBMapping[1];
				const float b = intensity * gbRGBMapping[2];

				const size_t videoBufferPixelIndex = ( x + pixelIndex + y * gbHorizontalResolutionInPixels ) * 3;
				pRGBFrameBuffer[videoBufferPixelIndex + 0] = (uint8_t)r;
				pRGBFrameBuffer[videoBufferPixelIndex + 1] = (uint8_t)g;
				pRGBFrameBuffer[videoBufferPixelIndex + 2] = (uint8_t)b;
			}
		}
	}
}

const uint8_t* getGBEmulatorInstanceFrameBuffer( GBEmulatorInstance* pInstance )
{
    return pInstance->pPpuState->pGBFrameBuffer;
}

bool hasGBEmulatorInstanceHitVBlank( GBEmulatorInstance* pInstance )
{
    return pInstance->flags.vblank;
}

void runGBEmulatorInstance( GBEmulatorInstance* pInstance )
{
    GBCpuState* pCpuState = pInstance->pCpuState;
    GBPpuState* pPpuState = pInstance->pPpuState;
    
    //FK: reset flags
    pInstance->flags.value = 0;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
    uint8_t runFrame = 0;
    if( pInstance->debug.continueExecution )
    {
        pInstance->debug.continueExecution    = 0;
        pInstance->debug.pauseExecution       = 0;
    }

	if( pInstance->debug.runForOneInstruction )
	{
        const uint8_t cycleCount = runSingleInstruction( pInstance );
        
        if( pPpuState->cycleCounter >= gbCyclesPerFrame )
        {
            pInstance->flags.vblank = 1;
            pPpuState->cycleCounter -= gbCyclesPerFrame;
        }

        pInstance->debug.runForOneInstruction = 0;
	}
    else
    {
        runFrame = pInstance->debug.runSingleFrame || !pInstance->debug.pauseExecution;
        pInstance->debug.runSingleFrame = 0;
    }

    if( !runFrame )
    {
        return;
    }

#endif

    while( pCpuState->cycleCounter < pCpuState->targetCycleCountPerUpdate )
    {
        const uint8_t cycleCount = runSingleInstruction( pInstance );
        pCpuState->cycleCounter += cycleCount;

        if( pInstance->debug.pauseAtBreakpoint && pInstance->debug.breakpointAddress == pInstance->pCpuState->registers.PC )
        {
            pInstance->debug.pauseExecution = 1;
            return;
        }
    }

    if( pPpuState->cycleCounter >= gbCyclesPerFrame )
    {
        pInstance->flags.vblank = 1;
        pPpuState->cycleCounter -= gbCyclesPerFrame;
    }

    pCpuState->cycleCounter -= pCpuState->targetCycleCountPerUpdate;
}