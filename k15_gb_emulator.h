#include "stdint.h"

#define K15_ENABLE_EMULATOR_DEBUG_FEATURES      1
#define K15_BREAK_ON_UNKNOWN_INSTRUCTION        1
#define K15_BREAK_ON_ILLEGAL_INSTRUCTION        1

#define K15_UNUSED_VAR(x) (void)x

#include "k15_gb_opcodes.h"
#include "k15_bitmap_font_data.cpp"

#define Kbyte(x) ((x)*1024)
#define Mbyte(x) (Kbyte(x)*1024)

#define Kbit(x) (Kbyte(x)/8)
#define Mbit(x) (Mbyte(x)/8)

#define FourCC(a, b, c, d) ((uint32_t)((a) << 0) | (uint32_t)((b) << 8) | (uint32_t)((c) << 16) | (uint32_t)((d) << 24))
#define ArrayCount(arr) (sizeof(arr)/sizeof(arr[0]))
//FK: Compiler specific functions
#ifdef _MSC_VER
#   include <intrin.h>
#   define breakPointHook() __nop()
#   define debugBreak __debugbreak
#else
#   define breakPointHook()
#   define debugBreak
#endif

#define illegalCodePath() debugBreak()

#define K15_RUNTIME_ASSERT(x) if(!(x)) debugBreak()

static constexpr uint8_t    gbNintendoLogo[]                        = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };
static constexpr char       gbRamFileExtension[]                    = ".k15_gb_ram";
static constexpr char       gbStateFileExtension[]                  = ".k15_gb_state";
static constexpr uint32_t   gbStateFourCC                           = FourCC( 'K', 'G', 'B', 'C' ); //FK: FourCC of state files
static constexpr uint32_t   gbCpuFrequency                          = 4194304u;
static constexpr uint32_t   gbPPUCyclesPerFrame                     = 70224u;
static constexpr uint8_t    gbStateVersion                          = 2;
static constexpr uint8_t    gbOAMSizeInBytes                        = 0x9Fu;
static constexpr uint8_t    gbDMACycleCount                         = 160u;
static constexpr uint8_t    gbSpriteHeight                          = 16u;
static constexpr uint8_t    gbTileResolutionInPixels                = 8u;  //FK: Tiles are 8x8 pixels
static constexpr uint8_t    gbHalfTileResolutionInPixels            = 4u;
static constexpr uint8_t    gbHorizontalResolutionInPixels          = 160u;
static constexpr uint8_t    gbVerticalResolutionInPixels            = 144u;
static constexpr uint8_t    gbHorizontalResolutionInTiles           = gbHorizontalResolutionInPixels / gbTileResolutionInPixels;
static constexpr uint8_t    gbVerticalResolutionInTiles             = gbVerticalResolutionInPixels / gbTileResolutionInPixels;
static constexpr uint8_t    gbBackgroundTileCount                   = 32u; //FK: BG maps are 32x32 tiles
static constexpr uint8_t    gbTileSizeInBytes                       = 16u; //FK: 8x8 pixels with 2bpp
static constexpr uint8_t    gbOpcodeHistoryBufferCapacity           = 255u;
static constexpr uint8_t    gbObjectAttributeCapacity               = 40u;
static constexpr uint8_t    gbSpritesPerScanline                    = 10u; //FK: the hardware allowed no more than 10 sprites per scanline
static constexpr uint8_t    gbFrameBufferScanlineSizeInBytes        = gbHorizontalResolutionInPixels / 4;
static constexpr size_t     gbFrameBufferSizeInBytes                = gbFrameBufferScanlineSizeInBytes * gbVerticalResolutionInPixels;
static constexpr size_t     gbMappedMemorySizeInBytes               = 0x10000;
static constexpr size_t     gbCompressionTokenSizeInBytes           = 1;

typedef uint32_t GBEmulatorInstanceEventMask;

enum
{
    K15_GB_NO_EVENT_FLAG            = 0x00,
    K15_GB_VBLANK_EVENT_FLAG        = 0x01,
    K15_GB_STATE_SAVED_EVENT_FLAG   = 0x02,
    K15_GB_STATE_LOADED_EVENT_FLAG  = 0x04,
};

struct GBEmulatorJoypadState
{
    union 
    {
        struct
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

                uint8_t actionButtonMask;
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

                uint8_t dpadButtonMask;
            };
        };

        uint16_t value = 0;
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
    uint8_t IME                 : 1;
    uint8_t halt                : 1;
    uint8_t stop                : 1;
    uint8_t dma                 : 1;
    uint8_t haltBug             : 1;
};

struct GBCpuState
{
    uint8_t*        pIE;
    uint8_t*        pIF;

    GBCpuRegisters  registers;

    uint32_t        cycleCounter;
    uint32_t        targetCycleCountPerUpdate;
    uint16_t        dmaAddress;
    uint8_t         dmaCycleCounter;
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
    uint8_t bgEnable                    : 1;
    uint8_t objEnable                   : 1;
    uint8_t objSize                     : 1;
    uint8_t bgTileMapArea               : 1;
    uint8_t bgAndWindowTileDataArea     : 1;
    uint8_t windowEnable                : 1;
    uint8_t windowTileMapArea           : 1;
    uint8_t enable                      : 1;
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

struct GBPpuFlags
{
    uint8_t drawObjects     : 1;
    uint8_t drawBackground  : 1;
    uint8_t drawWindow      : 1;
};

struct GBPpuState
{
    GBPpuFlags          flags;
    GBLcdRegisters      lcdRegisters;
    GBObjectAttributes* pOAM;
    GBLcdControl*       pLcdControl;
    GBPalette*          pPalettes;
    uint8_t*            pBackgroundOrWindowTileIds[ 2 ];
    uint8_t*            pTileBlocks[ 3 ];
    uint8_t*            pGBFrameBuffer;

    GBObjectAttributes  scanlineSprites[ gbSpritesPerScanline ];
    uint8_t             scanlineSpriteCounter;
    uint8_t             objectMonochromePlatte[ 8 ];
    uint32_t            dotCounter;
    uint32_t            cycleCounter;
    uint8_t             backgroundMonochromePalette[ 4 ];
};

struct GBMemoryMapper
{
    uint8_t*            pBaseAddress;
    uint8_t*            pRomBankSwitch;
    uint8_t*            pVideoRAM;
    uint8_t*            pRamBankSwitch;
    uint8_t*            pSpriteAttributes;
    uint16_t            lastAddressWrittenTo;
    uint16_t            lastAddressReadFrom;
    uint8_t             lastValueWritten;

    GBLcdStatus         lcdStatus;  //FK: Mirror lcd status to check wether we can read from VRAM and/or OAM 
    uint8_t             lcdEnabled; //FK: Mirror lcd enabled flag to check wether we can read from VRAM and/or OAM
    uint8_t             dmaActive;  //FK: Mirror dma flag to check wether we can read only from HRAM
};

struct GBCartridgeHeader
{
    uint8_t         reserved[4];                //FK: Entry point     
    uint8_t         nintendoLogo[48];
    uint8_t         gameTitle[15];
    uint8_t         colorCompatibility;
    uint8_t         licenseHigh;
    uint8_t         licenseLow;
    uint8_t         superGameBoyCompatibility;
    GBCartridgeType cartridgeType;
    uint8_t         romSize;
    uint8_t         ramSize;
    uint8_t         destinationCode;
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
    uint16_t    counterValue;       //FK: how many cpu cycles until TIMA is increased?
    uint16_t    counterTarget;
    uint8_t     enableCounter;
    uint8_t*    pDivider;
    uint8_t*    pCounter;
    uint8_t*    pModulo;
    uint8_t*    pControl;
};

struct GBCartridge
{
    const uint8_t*      pRomBaseAddress;
    uint8_t*            pRamBaseAddress;

    GBCartridgeHeader   header;
    uint8_t             romBankCount;
    uint8_t             mappedRomBankNumber;
    uint8_t             ramEnabled              : 1;
};

struct GBEmulatorInstance
{
    GBCpuState*             pCpuState;
    GBPpuState*             pPpuState;
    GBTimerState*           pTimerState;
    GBMemoryMapper*         pMemoryMapper;
    GBCartridge*            pCartridge;

    GBEmulatorJoypadState   joypadState;
    GBEmulatorInstanceFlags flags;

    uint16_t                monitorRefreshRate;
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
    GBEmulatorDebug         debug;
#endif
};

struct GBEmulatorState
{
    GBCpuState    cpuState;
    GBPpuState    ppuState;
    GBTimerState  timerState;
    GBCartridge   cartridge;
    uint8_t       mappedRomBankNumber;
};

uint8_t isInVRAMAddressRange( const uint16_t address )
{
    return address >= 0x8000 && address < 0xA000;
}

uint8_t isInOAMAddressRange( const uint16_t address )
{
    return address >= 0xFE00 && address < 0xFEA0;
}

uint8_t isInCartridgeRomAddressRange( const uint16_t address )
{
    return address >= 0x0000 && address < 0x8000;
}

uint8_t isInMBC1BankingModeSelectRegisterRange( const uint16_t address )
{
    return address >= 0x6000 && address < 0x8000;
}

uint8_t isInMBC1RamBankNumberRegisterRange( const uint16_t address )
{
    return address >= 0x4000 && address < 0x6000;
}

uint8_t isInMBC1RomBankNumberRegisterRange( const uint16_t address )
{
    return address >= 0x2000 && address < 0x4000;
}

uint8_t isInMBC1RamEnableRegisterRange( const uint16_t address )
{
    return address >= 0x0000 && address <= 0x2000;
}

uint8_t isInExternalRamRange( const uint16_t address )
{
    return address >= 0x8000 && address < 0xA000;
}

uint8_t isInHRAMAddressRange( const uint16_t address )
{
    return address >= 0xFF80 && address < 0xFFFF;
}

void patchIOCartridgeMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBCartridge* pCartridge )
{
    pCartridge->pRomBaseAddress = pMemoryMapper->pBaseAddress;
}

void patchIOCpuMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBCpuState* pState )
{
    pState->pIE = pMemoryMapper->pBaseAddress + 0xFFFF;
    pState->pIF = pMemoryMapper->pBaseAddress + 0xFF0F;
}

void patchIOTimerMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimerState )
{
    pTimerState->pDivider       = pMemoryMapper->pBaseAddress + 0xFF04;
    pTimerState->pCounter       = pMemoryMapper->pBaseAddress + 0xFF05;
    pTimerState->pModulo        = pMemoryMapper->pBaseAddress + 0xFF06;
    pTimerState->pControl       = pMemoryMapper->pBaseAddress + 0xFF07;
}

void patchIOPpuMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBPpuState* pPpuState )
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
    pPpuState->cycleCounter                     = 0;

    pPpuState->lcdRegisters.pStatus = (GBLcdStatus*)(pMemoryMapper->pBaseAddress + 0xFF41);
    pPpuState->lcdRegisters.pScy    = pMemoryMapper->pBaseAddress + 0xFF42;
    pPpuState->lcdRegisters.pScx    = pMemoryMapper->pBaseAddress + 0xFF43;
    pPpuState->lcdRegisters.pLy     = pMemoryMapper->pBaseAddress + 0xFF44;
    pPpuState->lcdRegisters.pLyc    = pMemoryMapper->pBaseAddress + 0xFF45;
    pPpuState->lcdRegisters.pWy     = pMemoryMapper->pBaseAddress + 0xFF4A;
    pPpuState->lcdRegisters.pWx     = pMemoryMapper->pBaseAddress + 0xFF4B;
}

GBCartridgeHeader getGBCartridgeHeader(const uint8_t* pRomData)
{
    GBCartridgeHeader header;
    memcpy(&header, pRomData + 0x0100, sizeof(GBCartridgeHeader));
    return header;
}

uint8_t isValidGBRomData( const uint8_t* pRomData )
{
    const GBCartridgeHeader header = getGBCartridgeHeader( pRomData );
    return memcmp( header.nintendoLogo, gbNintendoLogo, sizeof( gbNintendoLogo ) ) == 0;
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

    //FK: Unsupported romSize identifier
    illegalCodePath();
    return 0;
}

size_t mapRamSizeToByteSize(uint8_t ramSize)
{
    switch( ramSize )
    {
        case 0x00:
            return 0;
        case 0x02:
            return Kbit(8);
        case 0x03:
            return Kbit(32);
        case 0x04:
            return Kbit(128);
        case 0x05:
            return Kbit(64);
    }

    //FK: Unsupported ramSize identifier
    illegalCodePath();
    return 0;
}

void mapCartridgeMemoryBank( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, uint8_t romBankNumber )
{
    if( pCartridge->mappedRomBankNumber == romBankNumber )
    {
        return;
    }

    pCartridge->mappedRomBankNumber = romBankNumber;

    const uint8_t* pRomBank = pCartridge->pRomBaseAddress + romBankNumber * Kbyte( 16 );
    memcpy( pMemoryMapper->pRomBankSwitch, pRomBank, Kbyte( 16 ) );
}

uint8_t isGBEmulatorRomMapped( const GBEmulatorInstance* pEmulatorInstance )
{
    return pEmulatorInstance->pCartridge->pRomBaseAddress != nullptr;
}

GBCartridgeHeader getGBEmulatorInstanceCurrentCartridgeHeader( const GBEmulatorInstance* pEmulatorInstance )
{
    K15_RUNTIME_ASSERT( pEmulatorInstance->pCartridge->pRomBaseAddress != nullptr );
    return getGBCartridgeHeader( pEmulatorInstance->pCartridge->pRomBaseAddress );
}

void setGBEmulatorRamData( GBEmulatorInstance* pEmulatorInstance, uint8_t* pRamData )
{
    pEmulatorInstance->pCartridge->pRamBaseAddress = pRamData;
}

void setGBEmulatorInstanceMonitorRefreshRate( GBEmulatorInstance* pEmulatorInstance, uint16_t monitorRefreshRate )
{
    K15_RUNTIME_ASSERT( monitorRefreshRate > 0 );

    pEmulatorInstance->monitorRefreshRate                   = monitorRefreshRate;
    pEmulatorInstance->pCpuState->targetCycleCountPerUpdate = gbCpuFrequency / monitorRefreshRate;
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
    constexpr size_t checksumSizeInBytes   = 2;
    const size_t compressedRAMSizeInBytes  = calculateCompressedMemorySizeRLE( pEmulatorInstance->pMemoryMapper->pBaseAddress + 0x8000, 0x8000 );
    const size_t stateSizeInBytes = sizeof( GBEmulatorState ) + sizeof( gbStateFourCC ) + checksumSizeInBytes + sizeof(gbStateVersion) + compressedRAMSizeInBytes;
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
    const GBCartridge* pCartridge       = pEmulatorInstance->pCartridge;

    const GBCartridgeHeader header = getGBEmulatorInstanceCurrentCartridgeHeader( pEmulatorInstance );
    const uint16_t cartridgeChecksum = header.checksumHigher << 8 | header.checksumLower;

    GBEmulatorState state;
    state.cpuState                  = *pCpuState;
    state.ppuState                  = *pPpuState;
    state.timerState                = *pTimerState;
    state.cartridge                 = *pCartridge;
    state.mappedRomBankNumber       = pCartridge->mappedRomBankNumber;

    memcpy( pStateMemory, &gbStateFourCC, sizeof( gbStateFourCC ) );
    pStateMemory += sizeof( gbStateFourCC );

    memcpy( pStateMemory, &cartridgeChecksum, sizeof( cartridgeChecksum ) );
    pStateMemory += sizeof( cartridgeChecksum );

    *pStateMemory = gbStateVersion;
    ++pStateMemory;

    memcpy( pStateMemory, &state, sizeof( GBEmulatorState ) );
    pStateMemory += sizeof( GBEmulatorState );

    uint8_t* pCompressedMemory = pStateMemory;
    
    //FK: Store cartridge data as well...?
    compressMemoryBlockRLE( pCompressedMemory, pMemoryMapper->pBaseAddress + 0x8000, 0x8000 );
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
    pStateMemory += sizeof( gbStateFourCC );

    if( fourCC != gbStateFourCC )
    {
        return false;
    }

    const uint16_t stateCartridgeChecksum = *(uint16_t*)pStateMemory;
    pStateMemory += sizeof( stateCartridgeChecksum );

    const GBCartridgeHeader cartridgeHeader = getGBEmulatorInstanceCurrentCartridgeHeader( pEmulatorInstance );

    const uint16_t cartridgeChecksum = cartridgeHeader.checksumHigher << 8 | cartridgeHeader.checksumLower;
    if( cartridgeChecksum != stateCartridgeChecksum )
    {
        return false;
    }

    const uint8_t stateVersion = *pStateMemory;
    ++pStateMemory;

    if( stateVersion != gbStateVersion )
    {
        return false;
    }

    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    uint8_t* pGBFrameBuffer = pEmulatorInstance->pPpuState->pGBFrameBuffer;

    GBEmulatorState state;
    memcpy( &state, pStateMemory, sizeof( GBEmulatorState ) );
    pStateMemory += sizeof( GBEmulatorState );

    const uint8_t* pRomBaseAddress = pEmulatorInstance->pCartridge->pRomBaseAddress;

    *pEmulatorInstance->pCpuState   = state.cpuState;
    *pEmulatorInstance->pPpuState   = state.ppuState;
    *pEmulatorInstance->pTimerState = state.timerState;
    *pEmulatorInstance->pCartridge  = state.cartridge;

    GBCartridge* pCartridge = pEmulatorInstance->pCartridge;
    pCartridge->pRomBaseAddress     = pRomBaseAddress;
    pCartridge->mappedRomBankNumber = 0;
    pCartridge->header              = getGBCartridgeHeader( pRomBaseAddress );

    const size_t romSizeInBytes = mapRomSizeToByteSize( pCartridge->header.romSize );
    pCartridge->romBankCount    = ( uint8_t )( romSizeInBytes / Kbyte( 16 ) );
    
    mapCartridgeMemoryBank( pCartridge, pEmulatorInstance->pMemoryMapper, state.mappedRomBankNumber );

    patchIOTimerMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pTimerState );
    patchIOPpuMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pPpuState );
    patchIOCpuMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pCpuState );

    pEmulatorInstance->pPpuState->pGBFrameBuffer = pGBFrameBuffer;

    pMemoryMapper->lcdStatus  = *pEmulatorInstance->pPpuState->lcdRegisters.pStatus;
    pMemoryMapper->dmaActive  = pEmulatorInstance->pCpuState->flags.dma;
    pMemoryMapper->lcdEnabled = pEmulatorInstance->pPpuState->pLcdControl->enable;

    setGBEmulatorInstanceMonitorRefreshRate( pEmulatorInstance, pEmulatorInstance->monitorRefreshRate );

    const uint8_t* pCompressedMemory = pStateMemory;
    uncompressMemoryBlockRLE( pMemoryMapper->pBaseAddress + 0x8000, pCompressedMemory );
    return true;
}

uint8_t allowReadFromMemoryAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    //FK: Don't allow VRAM and OAM access while ppu is in mode 3 or 2 (drawing and oam search respectively)
    if( pMemoryMapper->lcdEnabled )
    {
        if ( pMemoryMapper->lcdStatus.mode == 3 )
        {
            if( isInOAMAddressRange( addressOffset ) ||
                isInVRAMAddressRange( addressOffset ) )
            {
                return 0;
            }
        }

        else if( pMemoryMapper->lcdStatus.mode == 2 )
        {
            if( isInOAMAddressRange( addressOffset ) )
            {
                return 0;
            }
        }
    }
    
    //FK: Only allow access to HRAM while DMA is active
    if( pMemoryMapper->dmaActive && !isInHRAMAddressRange( addressOffset ) )
    {
        return 0;
    }

    return 1;
}

uint8_t read8BitValueFromMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    if( !allowReadFromMemoryAddress( pMemoryMapper, addressOffset ) )
    {
        return 0xFF;
    }

    pMemoryMapper->lastAddressReadFrom = addressOffset;
    return pMemoryMapper->pBaseAddress[addressOffset];
}

uint16_t read16BitValueFromMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    pMemoryMapper->lastAddressReadFrom = addressOffset;
    const uint8_t ls = read8BitValueFromMappedMemory( pMemoryMapper, addressOffset + 0);
    const uint8_t hs = read8BitValueFromMappedMemory( pMemoryMapper, addressOffset + 1);
    return (hs << 8u) | (ls << 0u);
}

uint8_t* getMappedMemoryAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset)
{
    return pMemoryMapper->pBaseAddress + addressOffset;
}

uint8_t allowWriteToMemoryAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    if( isInCartridgeRomAddressRange( addressOffset ) )
    {
        return 0;
    }

    if( pMemoryMapper->lcdStatus.mode == 3 && pMemoryMapper->lcdEnabled )
    {
        if( isInVRAMAddressRange( addressOffset ) || 
            isInOAMAddressRange( addressOffset ) )
        {
            //FK: can't read from VRAM and/or OAM during lcd mode 3 
            return 0;
        }
    }

    if( pMemoryMapper->dmaActive )
    {
        if( !isInHRAMAddressRange( addressOffset ) )
        {
            //FK: can only access HRAM during dma
            return 0;
        }
    }

    return 1;
}

void write8BitValueToMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset, uint8_t value )
{
    pMemoryMapper->lastValueWritten     = value;
    pMemoryMapper->lastAddressWrittenTo = addressOffset;

    if( !allowWriteToMemoryAddress( pMemoryMapper, addressOffset ) )
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
    const uint8_t lsn = (uint8_t)( ( value >> 0 ) & 0xFF );
    const uint8_t msn = (uint8_t)( ( value >> 8 ) & 0xFF );
    write8BitValueToMappedMemory( pMemoryMapper, addressOffset + 0, lsn );
    write8BitValueToMappedMemory( pMemoryMapper, addressOffset + 1, msn );
}

void mapCartridgeMemory( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, const uint8_t* pRomMemory )
{
    const GBCartridgeHeader header = getGBCartridgeHeader( pRomMemory );
    const size_t romSizeInBytes = mapRomSizeToByteSize( header.romSize );
    pCartridge->ramEnabled      = 0;
    pCartridge->header          = header;
    pCartridge->pRomBaseAddress = pRomMemory;
    pCartridge->pRamBaseAddress = nullptr;
    pCartridge->romBankCount    = ( uint8_t )( romSizeInBytes / Kbyte( 16 ) );

    //FK: Map first rom bank to 0x0000-0x4000
    memcpy( pMemoryMapper->pBaseAddress, pRomMemory, Kbyte( 16 ) );

    //FK: Selectively enable different cartridge types after testing
    switch(pCartridge->header.cartridgeType)
    {
        case ROM_ONLY:
        case ROM_MBC1:
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:
        {
            mapCartridgeMemoryBank( pCartridge, pMemoryMapper, 1 );
            break;
        }
        default:
        {
            //FK: Cartridge type not supported
            debugBreak();
        }
    }
}

void initCpuState( GBMemoryMapper* pMemoryMapper, GBCpuState* pState )
{
    patchIOCpuMappedMemoryPointer( pMemoryMapper, pState );

    //FK: (Gameboy cpu manual) The GameBoy stack pointer 
    //is initialized to $FFFE on power up but a programmer 
    //should not rely on this setting and rather should 
    //explicitly set its value.
    pState->registers.SP = 0xFFFE;

    //FK: TODO: Make the following values user defined
    

    //FK: examine Bit 0 of the CPUs B-Register to separate between CGB (bit cleared) and GBA (bit set)
    //pState->registers.B = 0x1;

    //FK: Start fetching instructions at address $1000
    pState->registers.PC = 0x0100;

    //FK: defualt values from BGB
    //FK: A value of 11h indicates CGB (or GBA) hardware
    //pState->registers.A             = 0x11B0;
    pState->registers.AF            = 0x01B0;
    pState->registers.BC            = 0x0013;
    pState->registers.DE            = 0x00D8;
    pState->registers.HL            = 0x014D;

    pState->dmaCycleCounter         = 0;
    pState->cycleCounter            = 0;

    pState->flags.dma               = 0;
    pState->flags.IME               = 1;
    pState->flags.stop              = 0;
    pState->flags.halt              = 0;
    pState->flags.haltBug           = 0;
}

void resetMemoryMapper( GBMemoryMapper* pMapper )
{
    memset(pMapper->pBaseAddress, 0, gbMappedMemorySizeInBytes);
    memset(pMapper->pBaseAddress + 0xFF00, 0xFF, 0x80); //FK: reset IO ports

    pMapper->dmaActive  = 0;
    pMapper->lcdEnabled = 0;
}

void initMemoryMapper( GBMemoryMapper* pMapper, uint8_t* pMemory )
{
    pMapper->pBaseAddress           = pMemory;
    pMapper->pRomBankSwitch         = pMemory + 0x4000;
    pMapper->pVideoRAM              = pMemory + 0x8000;
    pMapper->pRamBankSwitch         = pMemory + 0xA000;
    pMapper->pSpriteAttributes      = pMemory + 0xFE00;

    resetMemoryMapper( pMapper );
}

void initTimerState( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimerState )
{
    patchIOTimerMappedMemoryPointer( pMemoryMapper, pTimerState );

    pTimerState->dividerCounter = 0xAB;
    pTimerState->counterValue   = 0;
    pTimerState->counterTarget  = 1024u;
    pTimerState->enableCounter  = 0;

    *pTimerState->pDivider      = 0xAB;
    *pTimerState->pCounter      = 0;
    *pTimerState->pModulo       = 0;
    *pTimerState->pControl      = 0xFB;
}

void clearGBFrameBuffer( uint8_t* pGBFrameBuffer )
{
    memset( pGBFrameBuffer, 0, gbFrameBufferSizeInBytes );
}

void extractMonochromePaletteFrom8BitValue( uint8_t* pMonochromePalette, uint8_t value )
{
    pMonochromePalette[0] = ( value >> 0 ) & 0x3;
    pMonochromePalette[1] = ( value >> 2 ) & 0x3;
    pMonochromePalette[2] = ( value >> 4 ) & 0x3;
    pMonochromePalette[3] = ( value >> 6 ) & 0x3;
}

void initPpuFrameBuffer( GBPpuState* pPpuState, uint8_t* pMemory )
{
    pPpuState->pGBFrameBuffer = pMemory;
    clearGBFrameBuffer( pPpuState->pGBFrameBuffer );
}

void initPpuState( GBMemoryMapper* pMemoryMapper, GBPpuState* pPpuState )
{
    patchIOPpuMappedMemoryPointer( pMemoryMapper, pPpuState );

    //FK: set default state of LCDC (taken from bgb)
    pMemoryMapper->pBaseAddress[ 0xFF40 ] = 0x91;

    //FK: set default state of STAT (taken from bgb)
    pMemoryMapper->pBaseAddress[ 0xFF41 ] = 0x85;

    *pPpuState->lcdRegisters.pLy  = 0;
    *pPpuState->lcdRegisters.pLyc = 0;
    *pPpuState->lcdRegisters.pWx  = 0;
    *pPpuState->lcdRegisters.pWy  = 0;
    *pPpuState->lcdRegisters.pScx = 0;
    *pPpuState->lcdRegisters.pScy = 0;
    
    //FK: set default state of palettes (taken from bgb)
    extractMonochromePaletteFrom8BitValue( pPpuState->backgroundMonochromePalette, 0b11100100 );
    extractMonochromePaletteFrom8BitValue( pPpuState->objectMonochromePlatte + 0,  0b11100100 );
    extractMonochromePaletteFrom8BitValue( pPpuState->objectMonochromePlatte + 4,  0b11100100 );

    pPpuState->dotCounter = 0;
    pPpuState->scanlineSpriteCounter = 0;

    pPpuState->flags.drawBackground = 1;
    pPpuState->flags.drawWindow     = 1;
    pPpuState->flags.drawObjects    = 1;

    clearGBFrameBuffer( pPpuState->pGBFrameBuffer );
}

size_t calculateGBEmulatorInstanceMemoryRequirementsInBytes()
{
    const size_t memoryRequirementsInBytes = sizeof(GBEmulatorInstance) + sizeof(GBCpuState) + 
        sizeof(GBMemoryMapper) + sizeof(GBPpuState) + sizeof(GBTimerState) + sizeof(GBCartridge) + 
        gbMappedMemorySizeInBytes + gbFrameBufferSizeInBytes;

    return memoryRequirementsInBytes;
}

void resetGBEmulatorInstance( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    GBCartridge* pCartridge = pEmulatorInstance->pCartridge;

    resetMemoryMapper(pEmulatorInstance->pMemoryMapper );

    if( pCartridge->pRomBaseAddress != nullptr )
    {
        pCartridge->mappedRomBankNumber = 0;
        mapCartridgeMemory(pCartridge, pMemoryMapper, pCartridge->pRomBaseAddress );
    }

    initCpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pCpuState);
    initPpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pPpuState);
    initTimerState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pTimerState);

    pEmulatorInstance->joypadState.actionButtonMask  = 0;
    pEmulatorInstance->joypadState.dpadButtonMask    = 0;

    //FK: Reset joypad value
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF00] = 0x0F;

    //FK: Reset serial
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF01] = 0x00;
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF02] = 0x7E;

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
    pEmulatorInstance->pCpuState        = (GBCpuState*)(pEmulatorInstance + 1);
    pEmulatorInstance->pMemoryMapper    = (GBMemoryMapper*)(pEmulatorInstance->pCpuState + 1);
    pEmulatorInstance->pPpuState        = (GBPpuState*)(pEmulatorInstance->pMemoryMapper + 1);
    pEmulatorInstance->pTimerState      = (GBTimerState*)(pEmulatorInstance->pPpuState + 1);
    pEmulatorInstance->pCartridge       = (GBCartridge*)(pEmulatorInstance->pTimerState + 1);

    uint8_t* pGBMemory = (uint8_t*)(pEmulatorInstance->pCartridge + 1);
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

    //FK: no cartridge loaded yet
    memset( pEmulatorInstance->pCartridge, 0, sizeof( GBCartridge ) );

    resetGBEmulatorInstance( pEmulatorInstance );

    //FK: Assume 60hz output as default
    setGBEmulatorInstanceMonitorRefreshRate( pEmulatorInstance, 60 );

    return pEmulatorInstance;
}

void loadGBEmulatorInstanceRom( GBEmulatorInstance* pEmulator, const uint8_t* pRomMemory )
{
    pEmulator->pCartridge->pRomBaseAddress = nullptr;

    resetGBEmulatorInstance( pEmulator );
    mapCartridgeMemory( pEmulator->pCartridge, pEmulator->pMemoryMapper, pRomMemory );
}

uint8_t reverseBitsInByte( uint8_t value )
{
    //FK: https://developer.squareup.com/blog/reversing-bits-in-c/
    return (uint8_t)(((value * 0x0802LU & 0x22110LU) |
            (value * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16);
}

void pushSpritePixelsToScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    if( pPpuState->scanlineSpriteCounter == 0 )
    {
        return;
    }

    uint8_t* pFrameBufferPixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
    for( size_t spriteIndex = 0; spriteIndex < pPpuState->scanlineSpriteCounter; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = pPpuState->scanlineSprites + spriteIndex;
        if( pSprite->x >= 168 )
        {
            //FK: Sprite is off screen
            continue;
        }

        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pPpuState->pTileBlocks[0] + pSprite->tileIndex * gbTileSizeInBytes;
        uint32_t tileScanlineOffset = ( scanlineYCoordinate - pSprite->y + gbSpriteHeight );
        if( pSprite->flags.yflip )
        {
            const uint8_t objHeight = pPpuState->pLcdControl->objSize == 0 ? 8 : 16;
            tileScanlineOffset = ( objHeight - 1 ) - tileScanlineOffset;
        }

        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + tileScanlineOffset * 2;
        
        //FK: Get pixel data of this tile for the current scanline
        const uint8_t tilePixelDataLSB = pSprite->flags.xflip ? reverseBitsInByte(pTileScanlinePixelData[0]) : pTileScanlinePixelData[0];
        const uint8_t tilePixelDataMSB = pSprite->flags.xflip ? reverseBitsInByte(pTileScanlinePixelData[1]) : pTileScanlinePixelData[1];

        //FK: Tile pixel will be written interleaved here
        uint16_t interleavedTilePixelData = 0;
        uint16_t tilePixelMask = 0;

        uint8_t* pFrameBufferPixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
        const uint8_t spriteOffset = 8u;                    //FK: sprites are offset by 8 pixels according to pandocs
        const uint8_t spriteByteOffset = spriteOffset / 4;  //FK: 4 pixel per byte
        uint8_t scanlineByteIndex = (pSprite->x/4) - spriteByteOffset;
        uint8_t scanlinePixelDataShift = 6 - (pSprite->x%4) * 2;
        for( uint8_t pixelIndex = 0u; pixelIndex < gbTileResolutionInPixels; ++pixelIndex )
        {
            const uint8_t scanlinePixelShift             = 14 - pixelIndex * 2;
            const uint8_t colorIdShift                   = 7 - pixelIndex;
            const uint8_t colorIdMSB                     = ( tilePixelDataMSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdLSB                     = ( tilePixelDataLSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdData                    = colorIdLSB << 0 | colorIdMSB << 1;
            
            if( colorIdData != 0 )
            {
                //FK: only write non-transparent sprite pixels
                const uint8_t paletteOffset = pSprite->flags.paletteNumber * 4;
                const uint8_t pixelData = pPpuState->objectMonochromePlatte[ colorIdData + paletteOffset ];
            
                //FK: TODO: I'm sure there's a better way to do this but it's 2am and I'm tired...
                //FK: Clear existing background pixel
                pFrameBufferPixelData[ scanlineByteIndex ] &= ~( 0x3 << scanlinePixelDataShift );

                //FK: Write sprite pixel
                pFrameBufferPixelData[ scanlineByteIndex ] |= ( pixelData << scanlinePixelDataShift );
            }

            if( scanlinePixelDataShift == 0 )
            {
                scanlinePixelDataShift = 8;
                ++scanlineByteIndex;
            }
            
            scanlinePixelDataShift -= 2;
        }
    }
}

template<typename IndexType>
void pushWindowPixelsToScanline( GBPpuState* pPpuState, const uint8_t* pTileData, uint8_t scanlineYCoordinate, const uint8_t wx, const uint8_t wy )
{
    const IndexType* pBackgroundTileIds  = (IndexType*)pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->windowTileMapArea ];

    //FK: Calculate the tile row that intersects with the current scanline
    const uint8_t y = scanlineYCoordinate - wy;
    const uint8_t yInTileSpace = y / gbTileResolutionInPixels;
    const uint16_t startTileIndex = yInTileSpace * gbBackgroundTileCount;

    uint8_t scanlineBackgroundTilesIds[ gbBackgroundTileCount ] = { 0 };
    uint8_t tileCounter = 0;

    //FK: push background tiles
    while( tileCounter < gbBackgroundTileCount )
    {
        //FK: get tile from background tile map
        const uint16_t tileIndex = startTileIndex + tileCounter;
        scanlineBackgroundTilesIds[ tileCounter ] = pBackgroundTileIds[ tileIndex ];
        ++tileCounter;
    }

    //FK: Get the y position of the top most pixel inside the tile row that the current scanline is in
    const uint8_t tileRowStartYPos  = startTileIndex / gbBackgroundTileCount * gbTileResolutionInPixels;

    //FK: Get the y offset inside the tile row to where the scanline currently is.
    const uint8_t tileRowYOffset = y - tileRowStartYPos;
    uint8_t interleavedScanlinePixelData[ gbBackgroundTileCount * 2 ] = { 0 };

    //FK: rasterize the pixels of the background/window tiles at the scanline position
    //    Note: the whole 256 pixels scanline will be rasterized eventhough only 144 pixels 
    //          will be shown in the end - this is done to simplify the implementation
    for( uint8_t tileIdIndex = 0; tileIdIndex < gbBackgroundTileCount; ++tileIdIndex )
    {
        const IndexType tileIndex = scanlineBackgroundTilesIds[ tileIdIndex ];
        
        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pTileData + tileIndex * gbTileSizeInBytes;
        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + ( y - yInTileSpace * 8 ) * 2;

        //FK: Get pixel data of this tile for the current scanline
        const uint8_t tilePixelDataMSB = pTileScanlinePixelData[0];
        const uint8_t tilePixelDataLSB = pTileScanlinePixelData[1];

        //FK: Pixel will be written interleaved here
        uint16_t interleavedTilePixelData = 0;
        for( uint8_t pixelIndex = 0; pixelIndex < gbTileResolutionInPixels; ++pixelIndex )
        {
            const uint8_t scanlinePixelShift          = 14 - pixelIndex*2;
            const uint8_t colorIdShift                = 7 - pixelIndex;
            const uint8_t colorIdMSB                  = ( tilePixelDataMSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdLSB                  = ( tilePixelDataLSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdData                 = colorIdMSB << 1 | colorIdLSB << 0;
            const uint8_t pixelData                   = pPpuState->backgroundMonochromePalette[ colorIdData ];
            interleavedTilePixelData |= ( pixelData << scanlinePixelShift );
        }

        const uint8_t scanlinePixelIndex = tileIdIndex * 2;
        interleavedScanlinePixelData[ scanlinePixelIndex + 0 ] = ( interleavedTilePixelData >> 8 ) & 0xFF;
        interleavedScanlinePixelData[ scanlinePixelIndex + 1 ] = ( interleavedTilePixelData >> 0 ) & 0xFF;
    }

    //FK: scanlinePixelData now contains a fully rastered 256 pixel wide scanline
    //    now we have to evaluate sx to copy the correct 144 pixel range to the framebuffer.
    //      Note: scanlinePixelData is now 2bpp

    const uint8_t wxpos = wx < 7 ? 0 : wx-7;
    uint8_t scanlinePixelDataShift = 6 - (wxpos%4) * 2;
    uint8_t scanlinePixelDataIndex = wxpos/4;

    uint8_t* pFrameBufferPixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
    for( uint8_t scanlineByteIndex = scanlinePixelDataIndex; scanlineByteIndex < gbFrameBufferScanlineSizeInBytes; ++scanlineByteIndex )
    {
        uint8_t frameBufferByte = 0; //FK: 1 framebuffer byte = 4 pixels with 2bpp

        for( uint8_t pixelIndex = 0; pixelIndex < 4; ++pixelIndex )
        {
            const uint8_t pixelValue = ( interleavedScanlinePixelData[ scanlinePixelDataIndex - (wxpos/4) ] >> scanlinePixelDataShift ) & 0x3;
            frameBufferByte |= pixelValue << ( 6 - ( pixelIndex * 2 ) );

            if( scanlinePixelDataShift == 0 )
            {
                scanlinePixelDataShift = 6;
                scanlinePixelDataIndex = ++scanlinePixelDataIndex % (gbBackgroundTileCount * 2);
            }
            else
            {
                scanlinePixelDataShift -= 2;
            }
        }
        
        pFrameBufferPixelData[scanlineByteIndex] = frameBufferByte;
    }
}

template<typename IndexType>
void pushBackgroundPixelsToScanline( GBPpuState* pPpuState, const uint8_t* pTileData, uint8_t scanlineYCoordinate )
{
    const IndexType* pBackgroundTileIds  = (IndexType*)pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->bgTileMapArea ];
    const uint8_t sx = *pPpuState->lcdRegisters.pScx;
    const uint8_t sy = *pPpuState->lcdRegisters.pScy;

    //FK: Calculate the tile row that intersects with the current scanline
    const uint8_t y = sy + scanlineYCoordinate;
    const uint8_t yInTileSpace = y / gbTileResolutionInPixels;
    const uint16_t startTileIndex = yInTileSpace * gbBackgroundTileCount;

    uint8_t scanlineBackgroundTilesIds[ gbBackgroundTileCount ] = { 0 };
    uint8_t tileCounter = 0;

    //FK: push background tiles
    while( tileCounter < gbBackgroundTileCount )
    {
        //FK: get tile from background tile map
        const uint16_t tileIndex = startTileIndex + tileCounter;
        scanlineBackgroundTilesIds[ tileCounter ] = pBackgroundTileIds[ tileIndex ];
        ++tileCounter;
    }

    //FK: Get the y position of the top most pixel inside the tile row that the current scanline is in
    const uint8_t tileRowStartYPos  = startTileIndex / gbBackgroundTileCount * gbTileResolutionInPixels;

    //FK: Get the y offset inside the tile row to where the scanline currently is.
    const uint8_t tileRowYOffset = y - tileRowStartYPos;
    uint8_t interleavedScanlinePixelData[ gbBackgroundTileCount * 2 ] = { 0 };

    //FK: rasterize the pixels of the background/window tiles at the scanline position
    //    Note: the whole 256 pixels scanline will be rasterized eventhough only 144 pixels 
    //          will be shown in the end - this is done to simplify the implementation
    for( uint8_t tileIdIndex = 0; tileIdIndex < gbBackgroundTileCount; ++tileIdIndex )
    {
        const IndexType tileIndex = scanlineBackgroundTilesIds[ tileIdIndex ];
        
        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pTileData + tileIndex * gbTileSizeInBytes;
        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + ( y - yInTileSpace * 8 ) * 2;

        //FK: Get pixel data of this tile for the current scanline
        const uint8_t tilePixelDataLSB = pTileScanlinePixelData[0];
        const uint8_t tilePixelDataMSB = pTileScanlinePixelData[1];

        //FK: Pixel will be written interleaved here
        uint16_t interleavedTilePixelData = 0;
        for( uint8_t pixelIndex = 0; pixelIndex < gbTileResolutionInPixels; ++pixelIndex )
        {
            const uint8_t scanlinePixelShift          = 14 - pixelIndex*2;
            const uint8_t colorIdShift                = 7 - pixelIndex;
            const uint8_t colorIdMSB                  = ( tilePixelDataMSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdLSB                  = ( tilePixelDataLSB >> colorIdShift ) & 0x1;
            const uint8_t colorIdData                 = colorIdMSB << 1 | colorIdLSB << 0;
            const uint8_t pixelData                   = pPpuState->backgroundMonochromePalette[ colorIdData ];
            interleavedTilePixelData |= ( pixelData << scanlinePixelShift );
        }

        const uint8_t scanlinePixelIndex = tileIdIndex * 2;
        interleavedScanlinePixelData[ scanlinePixelIndex + 0 ] = ( interleavedTilePixelData >> 8 ) & 0xFF;
        interleavedScanlinePixelData[ scanlinePixelIndex + 1 ] = ( interleavedTilePixelData >> 0 ) & 0xFF;
    }

    //FK: scanlinePixelData now contains a fully rastered 256 pixel wide scanline
    //    now we have to evaluate sx to copy the correct 144 pixel range to the framebuffer.
    //      Note: scanlinePixelData is now 2bpp

    uint8_t scanlinePixelDataShift = 6 - (sx%4) * 2;
    uint8_t scanlinePixelDataIndex = sx/4;

    uint8_t* pFrameBufferPixelData = pPpuState->pGBFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
    for( uint8_t scanlineByteIndex = 0; scanlineByteIndex < gbFrameBufferScanlineSizeInBytes; ++scanlineByteIndex )
    {
        uint8_t frameBufferByte = 0; //FK: 1 framebuffer byte = 4 pixels with 2bpp

        for( uint8_t pixelIndex = 0; pixelIndex < 4; ++pixelIndex )
        {
            const uint8_t pixelValue = ( interleavedScanlinePixelData[ scanlinePixelDataIndex ] >> scanlinePixelDataShift ) & 0x3;
            frameBufferByte |= pixelValue << ( 6 - ( pixelIndex * 2 ) );

            if( scanlinePixelDataShift == 0 )
            {
                scanlinePixelDataShift = 6;
                scanlinePixelDataIndex = ++scanlinePixelDataIndex % (gbBackgroundTileCount * 2);
            }
            else
            {
                scanlinePixelDataShift -= 2;
            }
        }
        
        pFrameBufferPixelData[scanlineByteIndex] = frameBufferByte;
    }
}

void clearGBFrameBufferScanline( uint8_t* pGBFrameBuffer, uint8_t scanlineYCoordinate )
{
    memset( pGBFrameBuffer + scanlineYCoordinate * gbFrameBufferScanlineSizeInBytes, 0, gbFrameBufferScanlineSizeInBytes );
}

void collectScanlineSprites( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    const uint8_t objHeight = pPpuState->pLcdControl->objSize == 0 ? 8 : 16;

    uint8_t spriteCounter = 0;
    for( size_t spriteIndex = 0u; spriteIndex < gbObjectAttributeCapacity; ++spriteIndex )
    {
        const GBObjectAttributes* pSprite = pPpuState->pOAM + spriteIndex;
        if( pSprite->x == 0 || pSprite->y < gbSpriteHeight || pSprite->y > gbVerticalResolutionInPixels + gbSpriteHeight )
        {
            //FK: Sprite is invisible
            continue;
        }

        const uint8_t spriteTopPosition = pSprite->y - gbSpriteHeight;
        if( spriteTopPosition <= scanlineYCoordinate && spriteTopPosition + objHeight > scanlineYCoordinate )
        {
            pPpuState->scanlineSprites[spriteCounter++] = *pSprite;
            
            if( spriteCounter == gbSpritesPerScanline )
            {
                break;
            }
        }
    }

    pPpuState->scanlineSpriteCounter = spriteCounter;

#if 0
    //FK: TODO: This shouldn't happen in CGB mode
    sortSpritesByXCoordinate( scanlineSprites, spriteCounter );
#endif
}

void drawScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    clearGBFrameBufferScanline( pPpuState->pGBFrameBuffer, scanlineYCoordinate );

    if( pPpuState->pLcdControl->bgEnable && pPpuState->flags.drawBackground )
    {
        //FK: Determine tile addressing mode
        if( !pPpuState->pLcdControl->bgAndWindowTileDataArea )
        {
            const uint8_t* pTileData = pPpuState->pTileBlocks[2];
            pushBackgroundPixelsToScanline< int8_t >( pPpuState, pTileData, scanlineYCoordinate );
        }
        else
        {
            const uint8_t* pTileData = pPpuState->pTileBlocks[0];
            pushBackgroundPixelsToScanline< uint8_t >( pPpuState, pTileData, scanlineYCoordinate );
        }
    }

    if( pPpuState->pLcdControl->windowEnable && pPpuState->flags.drawWindow )
    {
        const uint8_t wy = *pPpuState->lcdRegisters.pWy;
        const uint8_t wx = *pPpuState->lcdRegisters.pWx;

        if( wx <= 166 && wy <= 143 && scanlineYCoordinate >= wy )
        {
            //FK: Determine tile addressing mode
            if( !pPpuState->pLcdControl->bgAndWindowTileDataArea )
            {
                const uint8_t* pTileData = pPpuState->pTileBlocks[2];
                pushWindowPixelsToScanline< int8_t >( pPpuState, pTileData, scanlineYCoordinate, wx, wy );
            }
            else
            {
                const uint8_t* pTileData = pPpuState->pTileBlocks[0];
                pushWindowPixelsToScanline< uint8_t >( pPpuState, pTileData, scanlineYCoordinate, wx, wy );
            }
        }
    }

    if( pPpuState->pLcdControl->objEnable && pPpuState->flags.drawObjects )
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
            *pPpuState->lcdRegisters.pLy = 0;
            pPpuState->lcdRegisters.pStatus->mode = 0;
            pPpuState->dotCounter = 0;
        }
    }

    *pPpuState->pLcdControl = lcdControlValue;
}

uint16_t readTimerControlFrequency( const uint8_t timerControlValue )
{
    const uint8_t frequency = timerControlValue & 0x3;
    switch( frequency )
    {
        case 0x0:
            return 1024u;
        case 0x1:
            return 16u;
        case 0x2:
            return 64u;
        case 0x3:
            return 256u;
    }

    illegalCodePath();
    return 0;
}

void updateTimerRegisters( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimer )
{
    //FK: Timer Divider
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF04 )
    {
        pMemoryMapper->pBaseAddress[0xFF04] = 0x00;
        pTimer->dividerCounter = 0;
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
    if( pCpuState->flags.stop )
    {
        return;
    }

    //FK: timer divier runs at 16384hz (update counter every 256 cpu cycles to be accurate)
    pTimer->dividerCounter += cycleCount;
    while( pTimer->dividerCounter >= 256 )
    {
        pTimer->dividerCounter -= 256;
        *pTimer->pDivider += 1;
    }

    if( !pTimer->enableCounter )
    {
        return;
    }

    pTimer->counterValue += cycleCount;
    while( pTimer->counterValue > pTimer->counterTarget )
    {
        pTimer->counterValue -= pTimer->counterTarget;
        if( *pTimer->pCounter == 0xFF )
        {
            *pTimer->pCounter = *pTimer->pModulo;
            triggerInterrupt( pCpuState, TimerInterrupt );
        }

        *pTimer->pCounter += 1;
    }
}

void incrementLy( GBLcdRegisters* pLcdRegisters, uint8_t* pLy )
{
    GBLcdStatus* pLcdStatus = pLcdRegisters->pStatus;
    const uint8_t lyc       = *pLcdRegisters->pLyc;

    *pLy = *pLy + 1;
    pLcdStatus->LycEqLyFlag = ( *pLy == lyc );
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
    uint8_t* pLy            = pPpuState->lcdRegisters.pLy;
    uint16_t lcdDotCounter  = pPpuState->dotCounter;

    uint8_t triggerLCDStatInterrupt = 0;
    lcdDotCounter += cycleCount;

    if( lcdMode == 2 && lcdDotCounter >= 80 )
    {
        collectScanlineSprites( pPpuState, *pLy );
        lcdDotCounter -= 80;
        lcdMode = 3;
    }
    else if( lcdMode == 3 && lcdDotCounter >= 172 )
    {
        drawScanline( pPpuState, *pLy );
        lcdDotCounter -= 172;
        lcdMode = 0;
        triggerLCDStatInterrupt = pLcdStatus->enableMode0HBlankInterrupt;
    }
    else if( lcdMode == 0 && lcdDotCounter >= 204 )
    {
        incrementLy( &pPpuState->lcdRegisters, pLy );
		if( pLcdStatus->enableLycEqLyInterrupt == 1 && pLcdStatus->LycEqLyFlag )
		{
			triggerLCDStatInterrupt = 1;
		}

        lcdDotCounter -= 204;

        if( *pLy == 144 )
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
            incrementLy( &pPpuState->lcdRegisters, pLy );
            if( *pLy == 154 )
            {
                *pLy = 0;
                lcdMode = 2;
                triggerLCDStatInterrupt = pLcdStatus->enableMode2OAMInterrupt;
            }
        }
    }

    if( triggerLCDStatInterrupt )
    {
        triggerInterrupt( pCpuState, LCDStatInterrupt );
    }

    //FK: update ppu state
    pPpuState->dotCounter = lcdDotCounter;
    pLcdStatus->mode = lcdMode;
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

void executePendingInterrupts( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper )
{
    const uint8_t interruptEnable       = *pCpuState->pIE;
    const uint8_t interruptFlags        = *pCpuState->pIF;
    const uint8_t interruptHandleMask   = ( interruptEnable & interruptFlags );
    if( interruptHandleMask > 0u )
    {
        if( pCpuState->flags.IME )
        {
            for( uint8_t interruptIndex = 0u; interruptIndex < 5u; ++interruptIndex )
            {
                const uint8_t interruptFlag = ( 1 << interruptIndex );
                if( interruptHandleMask & interruptFlag )
                {
                    push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
                    pCpuState->registers.PC = 0x40 + 0x08 * interruptIndex;

                    *pCpuState->pIF &= ~interruptFlag;

                    pCpuState->flags.IME = 0;
                    pCpuState->cycleCounter += 20;
                    if( pCpuState->flags.halt )
                    {
                        pCpuState->cycleCounter += 4;
                    }

                    break;
                }
            }
        }

        pCpuState->flags.halt = 0;
    }
}

uint8_t getOpcode8BitOperandRHS( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode )
{
    const uint8_t msn = opcode & 0xF0;
    const uint8_t lsn = opcode & 0x0F;
    const uint8_t targetId = lsn > 0x07 ? lsn - 0x08 : lsn;

    const bool readFromProgramCounter = ( msn == 0x00 || msn == 0x10 || msn == 0x20 || msn == 0x30 || 
                                          msn == 0xC0 || msn == 0xD0 || msn == 0xE0 || msn == 0xF0 );

    switch( targetId )
    {
        case 0x00:
            return pCpuState->registers.B;
        case 0x01:
            return pCpuState->registers.C;
        case 0x02:
            return pCpuState->registers.D;
        case 0x03:
            return pCpuState->registers.E;
        case 0x04:
            return pCpuState->registers.H;
        case 0x05:
            return pCpuState->registers.L;
        case 0x06:
            return readFromProgramCounter ? read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ ) : read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.HL );
        case 0x07:
            return pCpuState->registers.A;
    }

    illegalCodePath();
    return 0;
}

uint8_t* getOpcode8BitOperandLHS( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode )
{
    const uint8_t lsn = opcode & 0x0F;
    const uint8_t msn = opcode & 0xF0;
    switch( msn )
    {
        case 0x00: case 0x40:
            return lsn > 0x07 ? &pCpuState->registers.C : &pCpuState->registers.B;
        case 0x10: case 0x50:
            return lsn > 0x07 ? &pCpuState->registers.E : &pCpuState->registers.D;
        case 0x20: case 0x60:
            return lsn > 0x07 ? &pCpuState->registers.L : &pCpuState->registers.H;
        case 0x30: case 0x70:
            if( lsn > 0x07 )
            {
                return &pCpuState->registers.A;
            }
            break;
    }

    illegalCodePath();
    return nullptr;
}

uint16_t* getOpcode16BitOperand( GBCpuState* pCpuState, uint8_t opcode )
{
    const uint8_t lsn = opcode & 0x0F;
    const uint8_t msn = opcode & 0xF0;

    switch( msn )
    {
        case 0x00: case 0xC0:
            return &pCpuState->registers.BC;

        case 0x10: case 0xD0:
            return &pCpuState->registers.DE;

        case 0x20: 
            return &pCpuState->registers.HL;

        case 0x30: 
        {
            const uint8_t useStackPointer = ( lsn == 0x01 || lsn == 0x03 || lsn == 0x09 || lsn == 0x0B );
            return useStackPointer ? &pCpuState->registers.SP : &pCpuState->registers.HL;
        }

        case 0xE0:
        {
            const uint8_t useStackPointer = ( lsn == 0x08  );
            return useStackPointer ? &pCpuState->registers.SP : &pCpuState->registers.HL;
        }

        case 0xF0:
            return &pCpuState->registers.AF;
    }

    illegalCodePath();
    return nullptr;
}

uint8_t getOpcodeCondition( GBCpuState* pCpuState, uint8_t opcode )
{
    const uint8_t msn = opcode & 0xF0;
    const uint8_t lsn = opcode & 0x0F;
    const uint8_t conditionValue = lsn < 0x07 ? 0 : 1;

    if( msn == 0x20 || msn == 0xC0 )
    {
        return pCpuState->registers.F.Z == conditionValue;
    }
    else if( msn == 0x30 || msn == 0xD0 )
    {
        return pCpuState->registers.F.C == conditionValue;
    }

    illegalCodePath();
    return 0;
}

void handleCbOpcode(GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode)
{
    const uint8_t lsn = (opcode & 0x0F);
    const uint8_t msn = (opcode & 0xF0);

    const uint8_t registerIndex = lsn > 0x07 ? lsn - 0x08 : lsn;
    const uint8_t bitIndex = ( ( msn%0x40 ) / 0x10 ) * 2 + (lsn > 0x07);

    uint8_t* pRegister = nullptr;
    switch( registerIndex )
    {
        case 0x00:
        {
            pRegister = &pCpuState->registers.B;
            break;
        }
        case 0x01:
        {
            pRegister = &pCpuState->registers.C;
            break;
        }
        case 0x02:
        {
            pRegister = &pCpuState->registers.D;
            break;
        }
        case 0x03:
        {
            pRegister = &pCpuState->registers.E;
            break;
        }
        case 0x04:
        {
            pRegister = &pCpuState->registers.H;
            break;
        }
        case 0x05:
        {
            pRegister = &pCpuState->registers.L;
            break;
        }
        case 0x07:
        {
            pRegister = &pCpuState->registers.A;
            break;
        }
    }

    uint8_t value = ( pRegister == nullptr ) ? read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.HL ) : *pRegister;
    switch( msn )
    {
        case 0x00:
        {
            if( lsn <= 0x07 )
            {
                //RLC
                const uint8_t msb = (value >> 7) & 0x1;
                const uint8_t newValue = value << 1 | msb;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = msb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            else
            {
                //RRC
                const uint8_t lsb       = value & 0x1;
                const uint8_t newValue  = value >> 1 | (lsb << 7);

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = lsb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            break;
        }
        case 0x10:
        {
            if( lsn <= 0x07 )
            {
                //RL
                const uint8_t msb = (value >> 7) & 0x1;
                const uint8_t newValue = value << 1 | pCpuState->registers.F.C;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = msb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            else
            {
                //RR
                const uint8_t lsb = (value & 0x1);
                const uint8_t newValue = value >> 1 | (pCpuState->registers.F.C << 7);

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = lsb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            break;
        }
        case 0x20:
        {
            if( lsn <= 0x07 )
            {
                //SLA
                const uint8_t msb = ( value >> 7 ) & 0x1;
                const uint8_t newValue = value << 1;

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = msb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            else
            {
                //SRA
                const uint8_t lsb = ( value & 0x1 );
                const uint8_t msb = ( ( value >> 7 ) & 0x1 );
                const uint8_t newValue = value >> 1 | ( msb << 7 );

                pCpuState->registers.F.Z = (newValue == 0);
                pCpuState->registers.F.C = lsb;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;

                value = newValue;
            }
            break;
        }
        case 0x30:
        {
            if( lsn <= 0x07 )
            {
                //SWAP
                const uint8_t highNibble = ( value & 0xF0 ) >> 4;
                const uint8_t lowNibble  = value & 0x0F;
                const uint8_t newValue = lowNibble << 4 | highNibble << 0;
                value = newValue;

                pCpuState->registers.F.Z = (value == 0);
                pCpuState->registers.F.C = 0;
                pCpuState->registers.F.N = 0;
                pCpuState->registers.F.H = 0;
            }
            else
            {
                //SRL
                const uint8_t lsb = value & 0x1;
                const uint8_t newValue = value >> 1;
                value = newValue;

                pCpuState->registers.F.Z = (value == 0);
                pCpuState->registers.F.C = lsb;
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
            const uint8_t newValue = value & ~(1 << bitIndex);
            value = newValue;
            break;
        }
        //SET
        case 0xC0:
        case 0xD0:
        case 0xE0:
        case 0xF0:
        {
            const uint8_t newValue = value | (1 << bitIndex);
            value = newValue;
            break;
        }
    }

    if( pRegister != nullptr )
    {
        *pRegister = value;
        return;
    }

    write8BitValueToMappedMemory( pMemoryMapper, pCpuState->registers.HL, value );
}

uint8_t executeInstruction( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t opcode )
{
    uint8_t opcodeCondition = 0;
    const GBOpcode* pOpcode = unprefixedOpcodes + opcode;

    switch( opcode )
    {
        //NOP
        case 0x00:
            break;

        //CALL nn
        case 0xCD:
        {
            const uint16_t address = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC + 2);
            pCpuState->registers.PC = address;
            break;
        }

        //CALL condition
        case 0xC4: case 0xCC: case 0xD4: case 0xDC:
        {
            const uint16_t address = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2;

            const uint8_t condition = getOpcodeCondition( pCpuState, opcode );
            if( condition )
            {
                push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
                pCpuState->registers.PC = address;
            }

            opcodeCondition = condition;
            break;
        }
  

        //LD (nn), A
        case 0x02: case 0x12: case 0x22: case 0x32:
        {
            uint16_t* pTarget = getOpcode16BitOperand( pCpuState, opcode );
            write8BitValueToMappedMemory( pMemoryMapper, *pTarget, pCpuState->registers.A );

            if( opcode == 0x22 )
            {
                *pTarget += 1;
            }
            else if ( opcode == 0x32 )
            {
                *pTarget -= 1;
            }
            break;
        }

        //LD r, n
        case 0x06: case 0x16: case 0x26:
        case 0x0E: case 0x1E: case 0x2E: case 0x3E:
        {
            uint8_t* pDestination = getOpcode8BitOperandLHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t value = read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ );

            *pDestination = value;
            break;
        }

        //LD (HL), n
        case 0x36:
        {
            const uint8_t value = read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ );
            write8BitValueToMappedMemory( pMemoryMapper, pCpuState->registers.HL, value );
            break;
        }
            
        //LD A,(rr)
        case 0x0A: case 0x1A: case 0x2A: case 0x3A:
        {
            uint16_t* pSource = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t sourceAddress = *pSource;

            pCpuState->registers.A = read8BitValueFromMappedMemory( pMemoryMapper, sourceAddress );

            if( opcode == 0x2A )
            {
                *pSource += 1;
            }
            else if( opcode == 0x3A )
            {
                *pSource -= 1;
            }
            break;
        }

        //LD A, (nn)
        case 0xFA:
        {
            const uint16_t address = read16BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC );
            pCpuState->registers.A = read8BitValueFromMappedMemory( pMemoryMapper, address );
            pCpuState->registers.PC += 2;
            break;
        }
        
        //LD (nn), A
        case 0xEA:
        {
            const uint16_t address = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            pCpuState->registers.PC += 2;
            break;
        }

        //LD (c),a
        case 0xE2:
        {
            const uint16_t address = 0xFF00 + pCpuState->registers.C;
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            break;
        }

        //LD a, (c)
        case 0xF2:
        {
            const uint16_t address = 0xFF00 + pCpuState->registers.C;
            pCpuState->registers.A = read8BitValueFromMappedMemory(pMemoryMapper, address);
            break;
        }

        //LD r, r
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: 
        case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: 
        case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: 
        case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:

        case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        {
            uint8_t* pDestination   = getOpcode8BitOperandLHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t value     = getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );

            *pDestination = value;
            break;
        }

        //LD (HL), r
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x77:
        {
            const uint8_t value = getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            write8BitValueToMappedMemory( pMemoryMapper, pCpuState->registers.HL, value );
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
            const int8_t offset = (int8_t)read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.HL = pCpuState->registers.SP + offset;
            pCpuState->registers.F.Z = 0;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = ((pCpuState->registers.SP & 0x0F) + (offset & 0x0F)) > 0x0F;
            pCpuState->registers.F.C = ((pCpuState->registers.SP & 0xFF) + (offset & 0xFF)) > 0xFF;
            break;
        }

        //LD (nn), SP
        case 0x08:
        {
            const uint16_t address = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            write16BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.SP);
            pCpuState->registers.PC += 2;
            break;
        }

        //JP nn
        case 0xC3:
        {
            const uint16_t addressToJumpTo = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
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
        case 0xC2: case 0xCA: case 0xD2: case 0xDA:
        {
            const uint16_t address = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC += 2;

            const uint8_t condition = getOpcodeCondition( pCpuState, opcode );
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
            const int8_t addressOffset = (int8_t)read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.PC += addressOffset;
            break;
        }

        //JR conditional
        case 0x20: case 0x28: case 0x30: case 0x38:
        {
            const int8_t addressOffset = (int8_t)read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            const uint8_t condition = getOpcodeCondition( pCpuState, opcode );
            if( condition )
            {
                pCpuState->registers.PC += addressOffset;
            }

            opcodeCondition = condition;
            break;
        }

        //XOR n
        case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF: 
        case 0xEE:
        {
            const uint8_t operand = ( opcode == 0xEE ) ? read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++) : 
                                                         getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            
            pCpuState->registers.A        = pCpuState->registers.A ^ operand;
            pCpuState->registers.F.value  = 0x0;
            pCpuState->registers.F.Z      = (pCpuState->registers.A == 0);
            break;
        }

        //OR n
        case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: 
        case 0xF6:
        {
            const uint8_t operand = ( opcode == 0xF6 ) ? read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++) : 
                                                         getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );

            pCpuState->registers.A        = pCpuState->registers.A | operand;
            pCpuState->registers.F.value  = 0x0;
            pCpuState->registers.F.Z      = (pCpuState->registers.A == 0);
            break;
        }

        //AND n
        case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: 
        case 0xE6:
        {
            const uint8_t operand = ( opcode == 0xE6 ) ? read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++) : 
                                                         getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );

            pCpuState->registers.A = pCpuState->registers.A & operand;
            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.H = 1;
            pCpuState->registers.F.C = 0;
            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            break;
        }

        //LD n,nn (16bit loads)
        case 0x01: case 0x11: case 0x21: case 0x31:
        {
            uint16_t* pDestination = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t value = read16BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC);
            *pDestination = value;
            pCpuState->registers.PC += 2u;
            break;
        }

        //INC n
        case 0x04: case 0x14: case 0x24:
        case 0x0C: case 0x1C: case 0x2C: case 0x3C:
        {
            uint8_t* pDestination = getOpcode8BitOperandLHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t oldValue = *pDestination;
            const uint8_t newValue = oldValue + 1;

            *pDestination = newValue;
            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = (oldValue & 0xF0) != (newValue & 0xF0);
            pCpuState->registers.F.N = 0;
            break;
        }

        //INC (HL)
        case 0x34:
        {
            const uint8_t oldValue = read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.HL );
            const uint8_t newValue = oldValue + 1;

            write8BitValueToMappedMemory( pMemoryMapper, pCpuState->registers.HL, newValue );
            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = (oldValue & 0xF0) != (newValue & 0xF0);
            pCpuState->registers.F.N = 0;
            break;
        }

        //INC nn
        case 0x03: case 0x13: case 0x23: case 0x33:
        {
            uint16_t* pDestination = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t oldValue = *pDestination;
            const uint16_t newValue = oldValue + 1;

            *pDestination = newValue;
            break;
        }

        //DEC n
        case 0x05: case 0x15: case 0x25: 
        case 0x0D: case 0x1D: case 0x2D: case 0x3D:
        {
            uint8_t* pDestination = getOpcode8BitOperandLHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t oldValue = *pDestination;
            const uint8_t newValue = oldValue - 1;

            *pDestination = newValue;

            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = ((newValue & 0x0F) == 0x0F);
            pCpuState->registers.F.N = 1;
            break;
        }

        //DC (HL)
        case 0x35:
        {
            const uint8_t oldValue = read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.HL );
            const uint8_t newValue = oldValue - 1;

            write8BitValueToMappedMemory( pMemoryMapper, pCpuState->registers.HL, newValue );
            pCpuState->registers.F.Z = (newValue == 0);
            pCpuState->registers.F.H = ((newValue & 0x0F) == 0x0F);
            pCpuState->registers.F.N = 1;
            break;
        }

        //DEC nn
        case 0x0B: case 0x1B: case 0x2B: case 0x3B:
        { 
            uint16_t* pDestination = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t oldValue = *pDestination;
            const uint16_t newValue = oldValue - 1;

            *pDestination = newValue;
            break;
        }
        
        //HALT
        case 0x76:
        {
            if( pCpuState->flags.IME )
            {
                pCpuState->flags.halt = 1;
            }
            else
            {
                //FK: If interrupts are disabled, halt doesn't suspend operation but it does
                //    cause the program counter to stop counting for one instruction and thus
                //    execute the next instruction twice
                const uint8_t interruptEnable = *pCpuState->pIE;
                const uint8_t interruptFlags  = *pCpuState->pIF;
                if( interruptEnable & interruptFlags & 0x1F )
                {
                    pCpuState->flags.haltBug = 1;
                }
                else
                {
                    pCpuState->flags.halt = 1;
                }
            }
            break;
        }

        //STOP
        case 0x10:
        {
            //FK: Reset timer div
            write8BitValueToMappedMemory(pMemoryMapper, 0xFF04, 0);
            pCpuState->flags.stop = !pCpuState->flags.stop;
            break;
        }
        
        //LDH (n),A
        case 0xE0:
        {
            const uint16_t address = 0xFF00 + read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            write8BitValueToMappedMemory(pMemoryMapper, address, pCpuState->registers.A);
            break;
        }

        //LDH A,(n)
        case 0xF0:
        {
            const uint8_t value = read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            const uint16_t address = 0xFF00 + value;
            pCpuState->registers.A = read8BitValueFromMappedMemory(pMemoryMapper, address);
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

        //ADD HL, nn
        case 0x09: case 0x19: case 0x29: case 0x39:
        {
            const uint16_t* pOperand    = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t value        = *pOperand;
            const uint16_t hl           = pCpuState->registers.HL;
            const uint32_t newValueHL   = hl + value;

            pCpuState->registers.F.N = 0;
            pCpuState->registers.F.C = ( newValueHL & 0x10000 ) > 0;
            pCpuState->registers.F.H = ( (hl & 0x0FFF) + ( value & 0x0FFF ) & 0x1000 ) > 0;

            pCpuState->registers.HL = (uint16_t)newValueHL;
            break;
        }

        //ADD SP, n
        case 0xE8:
        {
            const int8_t value = (int8_t)read8BitValueFromMappedMemory(pMemoryMapper, pCpuState->registers.PC++);
            pCpuState->registers.F.H = ((pCpuState->registers.SP & 0x0F) + (value & 0x0F)) > 0x0F;
            pCpuState->registers.F.C = ((pCpuState->registers.SP & 0xFF) + (value & 0xFF)) > 0xFF;
            pCpuState->registers.F.Z = 0;
            pCpuState->registers.F.N = 0;

            pCpuState->registers.SP += value;
            break;
        }

        //ADD A, n 
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: 
        case 0xC6:
        {
            const uint8_t operand = getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );

            //FK: promoting to 16bit to check for potential carry or half carry...Is there a better way?
            const uint16_t accumulator16BitValue = pCpuState->registers.A + operand;
            const uint8_t accumulatorLN = ( pCpuState->registers.A & 0x0F ) + ( operand & 0x0F );
            pCpuState->registers.A += operand;

            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            pCpuState->registers.F.C = (accumulator16BitValue > 0xFF);
            pCpuState->registers.F.H = (accumulatorLN > 0x0F);
            pCpuState->registers.F.N = 0;
            break;
        }

        //SUB A, n
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        case 0xD6:
        {
            const uint8_t operand = getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t accumulatorValue = pCpuState->registers.A;
            pCpuState->registers.A -= operand;

            pCpuState->registers.F.Z = (pCpuState->registers.A == 0);
            pCpuState->registers.F.C = (accumulatorValue < operand);
            pCpuState->registers.F.H = ((accumulatorValue & 0x0F) < (operand & 0x0F));
            pCpuState->registers.F.N = 1;
            break;
        }

        //RET Conditional
        case 0xC0: case 0xD0: case 0xC8: case 0xD8:
        {
            const uint8_t condition = getOpcodeCondition( pCpuState, opcode );
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

        //CPL
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
        case 0xC5: case 0xD5: case 0xE5: case 0xF5: 
        {
            const uint16_t* pOperand = getOpcode16BitOperand( pCpuState, opcode );
            push16BitValueToStack( pCpuState, pMemoryMapper, *pOperand );
            break;
        }

        //POP nn
        case 0xC1: case 0xD1: case 0xE1:
        {
            uint16_t* pOperand = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t value = pop16BitValueFromStack( pCpuState, pMemoryMapper );
            *pOperand = value;
            break;
        }

        //POP AF
        case 0xF1:
        {
            uint16_t* pOperand = getOpcode16BitOperand( pCpuState, opcode );
            const uint16_t value = pop16BitValueFromStack( pCpuState, pMemoryMapper );

            //FK: For AF we don't want to set the last 4 bits of F
            *pOperand = value & 0xFFF0;
            break;
        }

        case 0xCB: 
            opcode = read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ );
            pOpcode = cbPrefixedOpcodes + opcode;
            handleCbOpcode(pCpuState, pMemoryMapper, opcode);
            break;

        case 0x07: case 0x17: case 0x0F: case 0x1F:
            handleCbOpcode(pCpuState, pMemoryMapper, opcode);

            //FK: Reset Zero flag for these instructions.
            pCpuState->registers.F.Z = 0;
            break;

        //ADC
        case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: case 0xCE:
        {
            const uint8_t value             = opcode == 0xCE ? read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ ) :
                                                               getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t accumulator       = pCpuState->registers.A;
            const uint8_t carry             = pCpuState->registers.F.C;
            const uint16_t newValue         = accumulator + value + carry;
            const uint8_t  newValueNibble   = ( accumulator & 0x0F ) + ( value & 0x0F ) + carry;

            pCpuState->registers.A = ( uint8_t )newValue;

            pCpuState->registers.F.Z = pCpuState->registers.A == 0;
            pCpuState->registers.F.C = newValue > 0xFF;
            pCpuState->registers.F.H = newValueNibble > 0x0F;
            pCpuState->registers.F.N = 0;
            break;
        }

        //SBC
        case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F: case 0xDE:
        {
            const uint8_t value         = opcode == 0xDE ? read8BitValueFromMappedMemory( pMemoryMapper, pCpuState->registers.PC++ ) : 
                                                           getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            const uint8_t carry         = pCpuState->registers.F.C;
            const uint8_t accumulator   = pCpuState->registers.A;

            const int16_t newValue = accumulator - value - carry;
            const int16_t newValueNibble = ( accumulator & 0x0F ) - ( value & 0x0F ) - carry;

            pCpuState->registers.A = ( uint8_t )newValue;

            pCpuState->registers.F.Z = pCpuState->registers.A == 0;
            pCpuState->registers.F.C = newValue < 0;
            pCpuState->registers.F.H = newValueNibble < 0;
            pCpuState->registers.F.N = 1;
            break;
        }

        //DAA
        case 0x27:
        {
            uint16_t accumulator = pCpuState->registers.A;

            if( pCpuState->registers.F.N )
            {
                if( pCpuState->registers.F.H )
                {
                    accumulator = (accumulator - 0x06) & 0xFF;
                }

                if( pCpuState->registers.F.C )
                {
                    accumulator -= 0x60;
                }
            }
            else
            {
                if( pCpuState->registers.F.H || (accumulator & 0x0F) > 0x09 )
                {
                    accumulator += 0x06;
                }

                if( pCpuState->registers.F.C || accumulator > 0x9F )
                {
                    accumulator += 0x60;
                }
            }

            pCpuState->registers.F.Z = ( ( accumulator & 0xFF ) == 0 );
            pCpuState->registers.F.H = 0;

            if( ( accumulator & 0x100 ) == 0x100 )
            {
                pCpuState->registers.F.C = 1;
            }

            pCpuState->registers.A = (uint8_t)accumulator;
            break;
        }

        //RST n
        case 0xC7: case 0xCF:
        case 0xD7: case 0xDF:
        case 0xE7: case 0xEF:
        case 0xF7: case 0xFF:
        {
            const uint16_t address = (uint16_t)(opcode - 0xC7);
            push16BitValueToStack(pCpuState, pMemoryMapper, pCpuState->registers.PC);
            pCpuState->registers.PC = address;
            break;
        }

        // CP n
        case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: 
        case 0xFE:
        {
            const uint8_t value = getOpcode8BitOperandRHS( pCpuState, pMemoryMapper, opcode );
            pCpuState->registers.F.Z = (pCpuState->registers.A == value);
            pCpuState->registers.F.H = ((pCpuState->registers.A & 0x0F) < (value & 0x0F));
            pCpuState->registers.F.C = (pCpuState->registers.A < value);
            pCpuState->registers.F.N = 1;
            break;
        }

        case 0xD3: case 0xDB: case 0xDD:
        case 0xE3: case 0xE4: case 0xEB: case 0xEC: case 0xED: 
        case 0xF4: case 0xFC: case 0xFD:
        {
            //FK: illegal opcode
#if K15_BREAK_ON_ILLEGAL_INSTRUCTION == 1
            debugBreak();
#endif
            break;
        }
        default:
            //FK: opcode not implemented
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

GBEmulatorJoypadState fixJoypadState( GBEmulatorJoypadState joypadState )
{
    //FK: disallow simultaneous input of opposite dpad values
    if( joypadState.left == 1 && joypadState.right == 1 )
    {
        //FK: ignore both left and right
        joypadState.left = 0;
        joypadState.right = 0;
    }

    if( joypadState.up == 1 && joypadState.down == 1 )
    {
        //FK: ignore both up and down
        joypadState.up = 0;
        joypadState.down = 0;
    }

    return joypadState;
}

bool handleInput( GBMemoryMapper* pMemoryMapper, GBEmulatorJoypadState joypadState )
{
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF00 )
    {
        joypadState = fixJoypadState( joypadState );

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

        pMemoryMapper->pBaseAddress[0xFF00] = 0xC0 | ( queryJoypadValue & 0x30 ) | newJoypadState;

        return prevJoypadState != newJoypadState;
    }

    return 0;
}

void handleMBC1RamWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    const uint16_t address  = pMemoryMapper->lastAddressWrittenTo;
    const uint8_t value     = pMemoryMapper->lastValueWritten;

    if( isInMBC1RamEnableRegisterRange( address ) )
    {
        pCartridge->ramEnabled = ( value & 0xF ) == 0xA;
    }
    else if( isInExternalRamRange( address ) && pCartridge->ramEnabled )
    {
        //FK: mirror writes to ram area to emulator ram memory
        const uint16_t baseAddressOffset = (address - 0xA000);
        pCartridge->pRamBaseAddress[ baseAddressOffset ] = value;
    }
}

void handleMBC1RomWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    const uint16_t address  = pMemoryMapper->lastAddressWrittenTo;
    const uint8_t value     = pMemoryMapper->lastValueWritten;

    if( isInMBC1RomBankNumberRegisterRange( address ) )
    {
        const uint8_t romBankNumberMask = ( pCartridge->romBankCount - 1 );
        const uint8_t romBankNumber = value == 0 ? 1 : ( value & romBankNumberMask );
        mapCartridgeMemoryBank( pCartridge, pMemoryMapper, romBankNumber );
    }
}

void handleCartridgeRamWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    switch( pCartridge->header.cartridgeType )
    {
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:
            handleMBC1RamWrite( pCartridge, pMemoryMapper );
            return;
    }

    return;
}

void handleCartridgeRomWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    switch( pCartridge->header.cartridgeType )
    {
        case ROM_MBC1:
            handleMBC1RomWrite( pCartridge, pMemoryMapper );
            return;
    }

    return;
}

void setGBEmulatorInstanceJoypadState( GBEmulatorInstance* pEmulatorInstance, GBEmulatorJoypadState joypadState )
{
    pEmulatorInstance->joypadState.value = joypadState.value;
}

void addOpcodeToOpcodeHistory( GBEmulatorInstance* pEmulatorInstance, uint16_t address, uint8_t opcode )
{
#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 0
    K15_UNUSED_VAR(pEmulatorInstance);
    K15_UNUSED_VAR(address);
    K15_UNUSED_VAR(opcode);
    return;
#else
    memmove(pEmulatorInstance->debug.opcodeHistory + 1, pEmulatorInstance->debug.opcodeHistory + 0, pEmulatorInstance->debug.opcodeHistorySize * sizeof( GBOpcodeHistoryElement ) );

    pEmulatorInstance->debug.opcodeHistory[0].address     = address; 
    pEmulatorInstance->debug.opcodeHistory[0].opcode      = opcode; 
    pEmulatorInstance->debug.opcodeHistory[0].registers   = pEmulatorInstance->pCpuState->registers; 

    if( pEmulatorInstance->debug.opcodeHistorySize + 1 != gbOpcodeHistoryBufferCapacity )
    {
        pEmulatorInstance->debug.opcodeHistorySize++;
    }
#endif
}

uint8_t runSingleInstruction( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper   = pEmulatorInstance->pMemoryMapper;
    GBCpuState* pCpuState           = pEmulatorInstance->pCpuState;
    GBPpuState* pPpuState           = pEmulatorInstance->pPpuState;
    GBTimerState* pTimerState       = pEmulatorInstance->pTimerState;

    executePendingInterrupts( pCpuState, pMemoryMapper );

    uint8_t cycleCost = 4u; //FK: Default cycle cost when CPU is halted

    if( !pCpuState->flags.halt )
    {
        const uint8_t haltBug = pCpuState->flags.haltBug;
        pCpuState->flags.haltBug = 0;

        const uint16_t opcodeAddress    = pCpuState->registers.PC++;
        const uint8_t opcode            = read8BitValueFromMappedMemory( pMemoryMapper, opcodeAddress );
        const uint8_t opcodeByteCount   = opcode == 0xCB ? cbPrefixedOpcodes[opcode].byteCount : unprefixedOpcodes[opcode].byteCount;

        addOpcodeToOpcodeHistory( pEmulatorInstance, opcodeAddress, opcode );
        cycleCost = executeInstruction( pCpuState, pMemoryMapper, opcode );

        if( haltBug )
        {
            pCpuState->registers.PC -= opcodeByteCount;
        }
    }

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

    //FK: LCD Monochrome palette - Background
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF47 )
    {
        extractMonochromePaletteFrom8BitValue( pPpuState->backgroundMonochromePalette, pMemoryMapper->lastValueWritten );
    }

    //FK: LCD Monochrome palette - Object
    if( pMemoryMapper->lastAddressWrittenTo == 0xFF48 || pMemoryMapper->lastAddressWrittenTo == 0xFF49 )
    {
        const uint8_t paletteOffset = ( pMemoryMapper->lastAddressWrittenTo - 0xFF48 ) * 4;
        //extractMonochromePaletteFrom8BitValue( pPpuState->objectMonochromePlatte + paletteOffset, pMemoryMapper->lastValueWritten );
    }

    //FK: Serial - only used for printf right now
    if( ( pMemoryMapper->pBaseAddress[0xFF02] & (7<<1) ) > 0 )
    {
        const char c = (const char)pMemoryMapper->pBaseAddress[0xFF01];
        printf("%c", c);
        pMemoryMapper->pBaseAddress[0xFF02] &= ~(7<<1);
        triggerInterrupt( pCpuState, SerialInterrupt );
    }

    if( isInCartridgeRomAddressRange( pMemoryMapper->lastAddressWrittenTo ) )
    {
        handleCartridgeRamWrite( pEmulatorInstance->pCartridge, pMemoryMapper );
        handleCartridgeRomWrite( pEmulatorInstance->pCartridge, pMemoryMapper );
    }

    //FK: Only keep these values alive for one instruction tick
    pMemoryMapper->lastAddressWrittenTo = 0x0;
    pMemoryMapper->lastValueWritten     = 0x0;

    pMemoryMapper->lcdStatus    = *pPpuState->lcdRegisters.pStatus;
    pMemoryMapper->lcdEnabled   = pPpuState->pLcdControl->enable;
    pMemoryMapper->dmaActive    = pCpuState->flags.dma;

    return cycleCost;
}

const uint8_t* getGlyphPixel( char glyph )
{
    const char glyphIndex = glyph - 32u;
    const uint8_t rowIndex = glyphIndex / glyphRowCount;
    const uint8_t columnIndex = glyphIndex % glyphRowCount;
    const uint8_t y = ( fontPixelDataHeightInPixels - 1 ) - ( rowIndex * glyphHeightInPixels );
    const uint8_t x = columnIndex * glyphWidthInPixels;
    return fontPixelData + (x + y * fontPixelDataWidthInPixels) * 3;
}

void convertGBFrameBufferToRGB8Buffer( uint8_t* pRGBFrameBuffer, const uint8_t* pGBFrameBuffer)
{
	//FK: greenish hue of gameboy lcd
	constexpr float gbRGBMapping[3] = {
		(float)0x99,
		(float)0xEF,
		(float)0xAC,
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

GBEmulatorInstanceEventMask runGBEmulatorInstance( GBEmulatorInstance* pInstance )
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
        
        if( pPpuState->cycleCounter >= gbPPUCyclesPerFrame )
        {
            pInstance->flags.vblank = 1;
            pPpuState->cycleCounter -= gbPPUCyclesPerFrame;
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
        return K15_GB_NO_EVENT_FLAG;
    }
#endif

    while( pCpuState->cycleCounter < pCpuState->targetCycleCountPerUpdate )
    {
        const uint8_t cycleCount = runSingleInstruction( pInstance );
        pCpuState->cycleCounter += cycleCount;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
        if( pInstance->debug.pauseAtBreakpoint && pInstance->debug.breakpointAddress == pInstance->pCpuState->registers.PC )
        {
            pInstance->debug.pauseExecution = 1;
            return K15_GB_NO_EVENT_FLAG;
        }
#endif
    }

    if( pPpuState->cycleCounter >= gbPPUCyclesPerFrame )
    {
        pInstance->flags.vblank = 1;
        pPpuState->cycleCounter -= gbPPUCyclesPerFrame;
    }

    pCpuState->cycleCounter -= pCpuState->targetCycleCountPerUpdate;

    return pInstance->flags.vblank == 1 ? K15_GB_VBLANK_EVENT_FLAG : K15_GB_NO_EVENT_FLAG;
}
