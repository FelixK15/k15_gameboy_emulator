#include "stdint.h"

#define K15_ENABLE_EMULATOR_DEBUG_FEATURES      0
#define K15_BREAK_ON_UNKNOWN_INSTRUCTION        1
#define K15_BREAK_ON_ILLEGAL_INSTRUCTION        1

#define K15_UNUSED_VAR(x) (void)x

#include "k15_gb_opcodes.h"
#include "k15_gb_font.h"

//FK: Compiler specific functions
#ifdef _MSC_VER
#   include <intrin.h>
#   define BreakPointHook() __nop()
#   define DebugBreak       __debugbreak
#else
#   define BreakPointHook()
#   define DebugBreak
#endif

#define Kbyte(x) ((x)*1024)
#define Mbyte(x) (Kbyte(x)*1024)

#define Kbit(x) (Kbyte(x)/8)
#define Mbit(x) (Mbyte(x)/8)

#define FourCC(a, b, c, d)  ((uint32_t)((a) << 0) | (uint32_t)((b) << 8) | (uint32_t)((c) << 16) | (uint32_t)((d) << 24))
#define ArrayCount(arr)     (sizeof(arr)/sizeof(arr[0]))

#define IllegalCodePath()       DebugBreak()
#define CompiletimeAssert(x)    typedef char compile_time_assertion_##_LINE_[(x)?1:-1]

#define GetMin(a, b) ((a)>(b)?(b):(a))
#define GetMax(a, b) ((a)>(b)?(a):(b))

#ifdef K15_RELEASE_BUILD
    #define RuntimeAssert(x)
#else
    #define RuntimeAssert(x)        if(!(x)) DebugBreak()
#endif

static constexpr uint8_t    gbStateVersion = 6;
static constexpr uint32_t   gbStateFourCC  = FourCC( 'K', 'G', 'B', 'C' ); //FK: FourCC of state files

static constexpr uint8_t    gbNintendoLogo[]                        = { 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E };
static constexpr char       gbRamFileExtension[]                    = ".k15_gb_ram";
static constexpr char       gbStateFileExtension[]                  = ".k15_gb_state";
static constexpr uint32_t   gbCyclesPerFrame                        = 70224u;
static constexpr uint32_t   gbSerialClockCyclesPerBitTransfer       = 512u;
static constexpr uint32_t   gbEmulatorFrameRate                     = 60u;
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
static constexpr uint8_t    gbFrameBufferCount                      = 2;
static constexpr size_t     gbMappedMemorySizeInBytes               = 0x10000;
static constexpr size_t     gbCompressionTokenSizeInBytes           = 1;
static constexpr size_t     gbRamBankSizeInBytes                    = Kbyte( 8 );
static constexpr size_t     gbRomBankSizeInBytes                    = Kbyte( 16 );
static constexpr size_t     gbMaxRomSizeInBytes                     = Mbyte( 8 );
static constexpr size_t     gbMinRomSizeInBytes                     = Kbyte( 32 );

typedef uint32_t GBEmulatorInstanceEventMask;

enum
{
    K15_GB_NO_EVENT_FLAG            = 0x00,
    K15_GB_VBLANK_EVENT_FLAG        = 0x01,
    K15_GB_STATE_SAVED_EVENT_FLAG   = 0x02,
    K15_GB_STATE_LOADED_EVENT_FLAG  = 0x04,
};

enum 
{
    K15_GB_COUNTER_FREQUENCY_BIT_00   = 9,
    K15_GB_COUNTER_FREQUENCY_BIT_01   = 3,
    K15_GB_COUNTER_FREQUENCY_BIT_10   = 5,
    K15_GB_COUNTER_FREQUENCY_BIT_11   = 7,
};

enum GBStateLoadResult
{
    K15_GB_STATE_LOAD_SUCCESS = 0,
    K15_GB_STATE_LOAD_FAILED_OLD_VERSION,
    K15_GB_STATE_LOAD_FAILED_INCOMPATIBLE_DATA,
    K15_GB_STATE_LOAD_FAILED_WRONG_ROM
};

enum GBMapCartridgeResult
{
    K15_GB_CARTRIDGE_MAPPED_SUCCESSFULLY = 0,
    K15_GB_CARTRIDGE_TYPE_UNSUPPORTED
};

enum GBMappedIOAdresses
{
    K15_GB_MAPPED_IO_ADDRESS_JOYP   = 0xFF00,
    K15_GB_MAPPED_IO_ADDRESS_SC     = 0xFF02,
    K15_GB_MAPPED_IO_ADDRESS_DIV    = 0xFF04,
    K15_GB_MAPPED_IO_ADDRESS_TIMA   = 0xFF05,
    K15_GB_MAPPED_IO_ADDRESS_TMA    = 0xFF06,
    K15_GB_MAPPED_IO_ADDRESS_TAC    = 0xFF07,

    K15_GB_MAPPED_IO_ADDRESS_NR10   = 0xFF10,
    K15_GB_MAPPED_IO_ADDRESS_NR11   = 0xFF11,
    K15_GB_MAPPED_IO_ADDRESS_NR12   = 0xFF12,
    K15_GB_MAPPED_IO_ADDRESS_NR13   = 0xFF13,
    K15_GB_MAPPED_IO_ADDRESS_NR14   = 0xFF14,
    
    K15_GB_MAPPED_IO_ADDRESS_NR21   = 0xFF16,
    K15_GB_MAPPED_IO_ADDRESS_NR22   = 0xFF17,
    K15_GB_MAPPED_IO_ADDRESS_NR23   = 0xFF18,
    K15_GB_MAPPED_IO_ADDRESS_NR24   = 0xFF19,
    
    K15_GB_MAPPED_IO_ADDRESS_NR30   = 0xFF1A,
    K15_GB_MAPPED_IO_ADDRESS_NR31   = 0xFF1B,
    K15_GB_MAPPED_IO_ADDRESS_NR32   = 0xFF1C,
    K15_GB_MAPPED_IO_ADDRESS_NR33   = 0xFF1D,
    K15_GB_MAPPED_IO_ADDRESS_NR34   = 0xFF1E,

    K15_GB_MAPPED_IO_ADDRESS_NR41   = 0xFF20,
    K15_GB_MAPPED_IO_ADDRESS_NR42   = 0xFF21,
    K15_GB_MAPPED_IO_ADDRESS_NR43   = 0xFF22,
    K15_GB_MAPPED_IO_ADDRESS_NR44   = 0xFF23,

    K15_GB_MAPPED_IO_ADDRESS_NR50   = 0xFF24,
    K15_GB_MAPPED_IO_ADDRESS_NR51   = 0xFF25,
    K15_GB_MAPPED_IO_ADDRESS_NR52   = 0xFF26,

    K15_GB_MAPPED_IO_ADDRESS_LCDC   = 0xFF40,
    K15_GB_MAPPED_IO_ADDRESS_STAT   = 0xFF41,

    K15_GB_MAPPED_IO_ADDRESS_DMA    = 0xFF46,

    K15_GB_MAPPED_IO_ADDRESS_BGP    = 0xFF47,
    K15_GB_MAPPED_IO_ADDRESS_OBP0   = 0xFF48,
    K15_GB_MAPPED_IO_ADDRESS_OBP1   = 0xFF49,

    K15_GB_MAPPED_IO_ADDRESS_IF     = 0xFF0F,
    K15_GB_MAPPED_IO_ADDRESS_IE     = 0xFFFF
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
    uint16_t        dmaAddress;
    uint8_t         dmaCycleCounter;
    GBCpuStateFlags flags;          //FK: not to be confused with GBCpuRegisters::F
};

struct GBSerialState
{
    uint8_t* pSerialTransferData    = nullptr;
    uint8_t* pSerialTransferControl = nullptr;
    uint8_t shiftIndex              = 0u;
    uint8_t inByte                  = 0xFFu;
    uint8_t transferInProgress      = 0u;
    uint32_t cycleCounter           = 0u;
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

enum GBCpuInterrupt : uint8_t
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
    uint8_t*            pGBFrameBuffers[ gbFrameBufferCount ];

    uint32_t            cycleCounter;
    uint32_t            dotCounter;

    GBObjectAttributes  scanlineSprites[ gbSpritesPerScanline ];
    
    uint8_t             objectMonochromePlatte[ 8 ];
    uint8_t             backgroundMonochromePalette[ 4 ];

    uint8_t             scanlineSpriteCounter;
    uint8_t             activeFrameBufferIndex;
};

enum GBMemoryAccess
{
    GBMemoryAccess_None     = 0,
    GBMemoryAccess_Written  = 1,
    GBMemoryAccess_Read     = 2
};

struct GBMemoryMapper
{
    uint8_t*            pBaseAddress;
    uint8_t*            pRom0Bank;
    uint8_t*            pRom1Bank;
    uint8_t*            pVideoRAM;
    uint8_t*            pRamBankSwitch;
    uint8_t*            pSpriteAttributes;
    uint16_t            lastAddressWrittenTo;
    uint16_t            lastAddressReadFrom;
    uint8_t             lastValueWritten;

    GBMemoryAccess      memoryAccess;
    GBLcdStatus         lcdStatus;  //FK: Mirror lcd status to check whether we can read from VRAM and/or OAM 
    uint8_t             lcdEnabled; //FK: Mirror lcd enabled flag to check whether we can read from VRAM and/or OAM
    uint8_t             dmaActive;  //FK: Mirror dma flag to check whether we can read only from HRAM
    uint8_t             ramEnabled; //FK: Mirror ram enabled flag to check whether we can write to external RAM
};

struct GBRomHeader
{
    uint8_t         reserved[4];                //FK: Entry point     
    uint8_t         nintendoLogo[48];

    union
    {
        uint8_t         gameTitle[16];
        struct 
        {
            uint8_t     padding[11];
            uint8_t     manufactures[4];
            uint8_t     colorCompatibility;
        };
    };

    uint8_t         licenseHigh;
    uint8_t         licenseLow;
    uint8_t         superGameBoyCompatibility;
    GBCartridgeType cartridgeType;
    uint8_t         romSize;
    uint8_t         ramSize;
    uint8_t         destinationCode;
    uint8_t         licenseCode;
    uint8_t         maskRomVersion;
    uint8_t         headerChecksum;
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
            uint8_t vblank      : 1;
            uint8_t ramAccessed : 1;
        };

        uint8_t value;
    };
};

struct GBTimerState
{
    uint8_t*    pDivider;
    uint8_t*    pCounter;
    uint8_t*    pModulo;
    uint8_t*    pControl;

    uint16_t    internalDivCounter;

    uint8_t     counterFrequencyBit     : 4;
    uint8_t     enableCounter           : 1;
    uint8_t     timerOverflow           : 1;
    uint8_t     timerLoading            : 1;
};

struct GBCartridge
{
    const uint8_t*      pRomBaseAddress;
    uint8_t*            pRamBaseAddress;

    uint32_t            romSizeInBytes;
    uint32_t            ramSizeInBytes;
    uint16_t            mappedRom0BankNumber;
    uint16_t            mappedRom1BankNumber;
    uint16_t            romBankCount;
    uint8_t             highBankValue;
    uint8_t             lowBankValue;

    GBRomHeader   header;
    uint8_t             ramBankCount;
    uint8_t             mappedRamBankNumber;
    uint8_t             ramEnabled                  : 1;
    uint8_t             bankingMode                 : 1;
};

struct GBApuSquareWaveChannel1
{
    uint8_t lengthTimer;
};

struct GBApuSquareWaveChannel2
{
    uint8_t lengthTimer;
};

struct GBApuWaveChannel
{
    uint8_t* pWavePattern;
    uint8_t lengthTimer;
    uint8_t currentSample;
    uint16_t samplePosition;
    uint16_t cycleCount;
    uint16_t frequencyCycleCountTarget;

    uint8_t channelEnabled  : 1;
    uint8_t volumeShift     : 3;
};

struct GBApuNoiseChannel
{
    uint8_t lengthTimer;
};

struct GBApuFrameSequencer
{
    uint16_t cycleCounter;
    uint8_t clockCounter;
};

struct GBApuState
{
    GBApuSquareWaveChannel1 squareWaveChannel1;
    GBApuSquareWaveChannel2 squareWaveChannel2;
    GBApuWaveChannel        waveChannel;
    GBApuNoiseChannel       noiseChannel;
    GBApuFrameSequencer     frameSequencer;
};

struct GBEmulatorInstance
{
    GBCpuState*             pCpuState;
    GBPpuState*             pPpuState;
    GBApuState*             pApuState;
    GBTimerState*           pTimerState;
    GBMemoryMapper*         pMemoryMapper;
    GBSerialState*          pSerialState;
    GBCartridge*            pCartridge;

    GBEmulatorJoypadState   joypadState;
    GBEmulatorInstanceFlags flags;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
    GBEmulatorDebug         debug;
#endif
};

struct GBEmulatorState
{
    GBCpuState    cpuState;
    GBPpuState    ppuState;
    GBApuState    apuState;
    GBTimerState  timerState;
    GBSerialState serialState;
    GBCartridge   cartridge;
    uint16_t      mappedRom0BankNumber;
    uint16_t      mappedRom1BankNumber;
    uint8_t       mappedRamBankNumber;
};

const uint8_t* getFontGlyphPixel( char glyph )
{
    //FK: bitmap font starts with space
    RuntimeAssert( glyph >= 32 );

    const char glyphIndex = glyph - 32;
    const uint8_t rowIndex = glyphIndex / glyphRowCount;
    const uint8_t columnIndex = glyphIndex % glyphRowCount;
    const uint8_t y = ( fontPixelDataHeightInPixels - 1 ) - ( rowIndex * glyphHeightInPixels );
    const uint8_t x = columnIndex * glyphWidthInPixels;
    return fontPixelData + (x + y * fontPixelDataWidthInPixels) * 3;
}

static inline uint32_t castSizeToUint32( const size_t value )
{
    RuntimeAssert( value < 0xFFFFFFFF );
    return ( uint32_t )value;
}

static inline int32_t castSizeToInt32( const size_t value )
{
    RuntimeAssert( value < INT32_MAX && value > INT32_MIN );
    return (int32_t)value;
}

uint8_t isInVideoRamAddressRange( const uint16_t address )
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

uint8_t isInMBC5LowRomBankNumberRegisterRange( const uint16_t address )
{
    return address >= 0x2000 && address < 0x3000;
}

uint8_t isInMBC5HighRomBankNumberRegisterRange( const uint16_t address )
{
    return address >= 0x3000 && address < 0x4000;
}

uint8_t isInMBC5RamBankNumberRegisterRange( const uint16_t address )
{
    return address >= 0x4000 && address < 0x6000;
}

uint8_t isInRamEnableRegisterRange( const uint16_t address )
{
    return address >= 0x0000 && address < 0x2000;
}

uint8_t isInExternalRamRange( const uint16_t address )
{
    return address >= 0xA000 && address < 0xC000;
}

uint8_t isInIORegisterRange( const uint16_t address )
{
    return address >= 0xFF00 && address < 0xFF80;
}

uint8_t isInHighRamAddressRange( const uint16_t address )
{
    return address >= 0xFF80 && address < 0xFFFF;
}

uint8_t isInWorkRamRange( const uint16_t address )
{
    return address >= 0xC000 && address < 0xCFFF;
}

uint8_t isInEchoRamRange( const uint16_t address )
{
    return address >= 0xE000 && address < 0xFDFF;
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

void patchIOSerialMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBSerialState* pSerialState )
{
    pSerialState->pSerialTransferData       = pMemoryMapper->pBaseAddress + 0xFF01;
    pSerialState->pSerialTransferControl    = pMemoryMapper->pBaseAddress + 0xFF02;
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

void patchIOApuMappedMemoryPointer( GBMemoryMapper* pMemoryMapper, GBApuState* pApuState )
{
    pApuState->waveChannel.pWavePattern = pMemoryMapper->pBaseAddress + 0xFF30;
}

uint8_t calculateGBRomHeaderChecksum( const uint8_t* pRomData )
{
    const uint8_t* pData        = pRomData + 0x0134;
    const uint8_t* pDataEnd     = pRomData + 0x014C;

    uint8_t checksum = 0u;
    while( pData <= pDataEnd )
    {
        checksum = checksum - *pData - 1;
        ++pData;
    }

    return checksum;
}

uint8_t isGBRomData( const uint8_t* pRomData, const size_t romDataSizeInBytes )
{
    if( romDataSizeInBytes < gbMinRomSizeInBytes || romDataSizeInBytes > gbMaxRomSizeInBytes )
    {
        return 0u;
    }

    GBRomHeader header;
    memcpy(&header, pRomData + 0x0100, sizeof(GBRomHeader));

    //FK: TODO calculate complete rom checksum eventually...
    const uint8_t headerChecksum = calculateGBRomHeaderChecksum( pRomData );
    return headerChecksum == header.headerChecksum;
}

GBRomHeader getGBRomHeader(const uint8_t* pRomData)
{
    GBRomHeader header;
    memcpy(&header, pRomData + 0x0100, sizeof(GBRomHeader));
    return header;
}

struct ZipEndOfCentralDirectory
{
    uint32_t signature;
    uint16_t diskNumber;
    uint16_t centralDirDisk;
    uint16_t centralDirRecordCount;
    uint32_t centralDirectorySizeInBytes;
    uint32_t centralDirectoryAbsoluteOffset;
};

struct ZipArchive
{
    const uint8_t*                   restrict_modifier      pData;
    const ZipEndOfCentralDirectory*  restrict_modifier      pEocd;
    uint32_t                                                dataSizeInBytes;
};

struct crc32
{
    uint32_t value = (uint32_t)~0;
};

crc32 calculateCrc32Checksum( crc32 crc, const uint8_t* pData, const uint32_t dataSizeInBytes )
{
    static uint32_t constexpr crc32Table[] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
        0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

	for(size_t byteIndex = 0; byteIndex < dataSizeInBytes; byteIndex++) {
		const uint32_t t = (*pData ^ crc.value) & 0xFF;
		crc.value=( crc.value >> 8 ) ^ crc32Table[t];
	}
	
    crc.value = ~crc.value;
	return crc;
}

struct GZipArchiveFlags
{
    union
    {
        struct 
        {
            uint8_t FTEXT       : 1;
            uint8_t FHCRC       : 1;
            uint8_t FEXTRA      : 1;
            uint8_t FNAME       : 1;
            uint8_t FCOMMENT    : 1;
            uint8_t reserved    : 3;
        };

        uint8_t value;
    };
};

struct GZipFooter
{
    uint32_t crc32Checksum;
    uint32_t uncompressedSizeInBytes;
};

struct GZipArchive
{
    const uint8_t*          pData;
    const GZipFooter*       pFooter;
    uint32_t                dataSizeInBytes;
    uint8_t                 compressionMethod;
    GZipArchiveFlags        flags;
};

enum class GZipUncompressResult : uint8_t
{
    Success,
    UnsupportedCompressionMethod,
    ChecksumError,
    TargetBufferTooSmall
};

struct BitStream
{
    const uint8_t* pDataStart;
    const uint8_t* pDataEnd;
    const uint8_t* pDataCurrent;
    uint8_t bitMask;
};

struct MemoryWriteStream
{
    uint8_t* pTargetData;
    const uint8_t* pTargetDataEnd;
};

struct DeflateBlockHeader
{
    uint8_t         isFinal             : 1;
    uint8_t         compressionMethod   : 2;
};

MemoryWriteStream createMemoryWriteStream( uint8_t* pTargetBuffer, const uint32_t targetBufferCapacityInBytes )
{
    MemoryWriteStream writeStream;
    writeStream.pTargetData     = pTargetBuffer;
    writeStream.pTargetDataEnd  = pTargetBuffer + targetBufferCapacityInBytes;

    return writeStream;
}

uint8_t pushData( MemoryWriteStream* pWriteStream, const uint8_t* pData, const uint32_t dataSizeInBytes )
{
    if( pWriteStream->pTargetData + dataSizeInBytes < pWriteStream->pTargetDataEnd )
    {
        return 0u;
    }

    memcpy( pWriteStream->pTargetData, pData, dataSizeInBytes );
    pWriteStream->pTargetData += dataSizeInBytes;

    return 1u;
}

BitStream createBitStream( const uint8_t* pData, uint32_t dataSizeInBytes )
{
    BitStream bitStream;
    bitStream.pDataStart    = pData;
    bitStream.pDataCurrent  = pData;
    bitStream.pDataEnd      = pData + dataSizeInBytes;
    bitStream.bitMask       = 1;
    return bitStream;
}

void advanceToNextByte( BitStream* pBitStream )
{
    RuntimeAssert( ( pBitStream->pDataCurrent + 1 ) < pBitStream->pDataEnd );
    ++pBitStream->pDataCurrent;
    pBitStream->bitMask = 1;
}

uint8_t reverseBitsInByte( uint8_t value )
{
    //FK: https://developer.squareup.com/blog/reversing-bits-in-c/
    return (uint8_t)(((value * 0x0802LU & 0x22110LU) |
            (value * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16);
}

uint8_t readBit( BitStream* pBitStream )
{
    const uint8_t bit = ( *pBitStream->pDataCurrent & pBitStream->bitMask ) > 0;
    pBitStream->bitMask <<= 1;

    if( pBitStream->bitMask == 0u )
    {
        advanceToNextByte( pBitStream );
    }

    return bit;
}

uint8_t readBits( BitStream* pBitStream, uint8_t bitsToRead )
{
    RuntimeAssert( bitsToRead <= 8 );

    uint8_t value = 0u;
    while( true )
    {
        value |= readBit( pBitStream );

        if( ( --bitsToRead ) == 0u )
        {
            break;
        }
        value <<= 1u;
    }

    return value;
}

uint8_t readBitsInverse( BitStream* pBitStream, uint8_t bitsToRead )
{
    RuntimeAssert( bitsToRead <= 8 );

    uint8_t value = 0u;
    for( uint8_t bitIndex = 0u; bitIndex < bitsToRead; ++bitIndex )
    {
        value |= ( readBit( pBitStream ) << bitIndex );
    }
    
    return value;
}

void moveToNextByteBoundary( BitStream* pBitStream )
{
    if( pBitStream->bitMask == 1u )
    {
        return;
    }

    advanceToNextByte( pBitStream );
}

void skipBytes( BitStream* pBitStream, const uint32_t bytesToSkip )
{
    RuntimeAssert( pBitStream->bitMask == 1u );
    RuntimeAssert( pBitStream->pDataCurrent + bytesToSkip < pBitStream->pDataEnd );

    pBitStream->pDataCurrent += bytesToSkip;
}

const ZipEndOfCentralDirectory* findZipArchiveEndOfCentralDirectory( const uint8_t* pZipData, const uint32_t zipDataSizeInBytes )
{
    uint32_t eocdPos = zipDataSizeInBytes - 20;
	while( eocdPos != 0 )
	{
		if( pZipData[ eocdPos + 3 ] == 0x06 &&
			pZipData[ eocdPos + 2 ] == 0x05 &&
			pZipData[ eocdPos + 1 ] == 0x4b &&
			pZipData[ eocdPos + 0 ] == 0x50 )
		{
			break;
		}

		--eocdPos;
	}

    if( eocdPos == 0u )
    {
        return nullptr;
    }

    return (const ZipEndOfCentralDirectory*)( pZipData + eocdPos );
}

uint8_t isGZipArchiveData( const uint8_t* pGZipData, const uint32_t gzipDataSizeInBytes )
{
    //FK: Check for file header size
    if( gzipDataSizeInBytes <= 18 )
    {
        return 0u;
    }

    const uint8_t isGzipHeader = ( pGZipData[0] == 0x1F && pGZipData[1] == 0x8b );
    return isGzipHeader;
}

uint8_t openGZipArchive( GZipArchive* pOutArchive, const uint8_t* pGzipData, const uint32_t gzipDataSizeInBytes )
{
    if( !isGZipArchiveData( pGzipData, gzipDataSizeInBytes) )
    {
        return 0u;
    }

    GZipArchive archive;
    archive.dataSizeInBytes     = gzipDataSizeInBytes;
    archive.pData               = pGzipData;
    archive.compressionMethod   = *(pGzipData + 2u);
    archive.flags.value         = *(pGzipData + 3u);
    archive.pFooter             = (GZipFooter*)( pGzipData + gzipDataSizeInBytes - sizeof( GZipFooter ) );

    *pOutArchive = archive;

    return 1u;
}

const char* getGZipFileNameMember( const GZipArchive* pArchive )
{
    uint32_t dataOffset = 10u; //FK: Gzip header
    if( pArchive->flags.FEXTRA )
    {
        const uint8_t extraSizeInBytesHigh  = *( pArchive->pData + dataOffset + 0u );
        const uint8_t extraSizeInBytesLow   = *( pArchive->pData + dataOffset + 1u );
        const uint16_t extraSizeInBytes    = ( uint16_t )extraSizeInBytesHigh << 8u | extraSizeInBytesLow;
        dataOffset += extraSizeInBytes + 2u;
    }

    return (const char*)( pArchive->pData + dataOffset );
}

struct GZipCompressedData
{
    const uint8_t* pCompressedData;
    uint32_t compressedDataSizeInBytes;
};

GZipCompressedData getGZipCompressedData( const GZipArchive* pArchive )
{
    uint32_t dataOffset = 10u; //FK: Gzip header
    if( pArchive->flags.FEXTRA )
    {
        const uint8_t extraSizeInBytesHigh  = *( pArchive->pData + dataOffset + 0u );
        const uint8_t extraSizeInBytesLow   = *( pArchive->pData + dataOffset + 1u );
        const uint16_t extraSizeInBytes    = ( uint16_t )extraSizeInBytesHigh << 8u | extraSizeInBytesLow;
        dataOffset += extraSizeInBytes + 2u;
    }

    if( pArchive->flags.FNAME )
    {
        dataOffset += ( uint32_t )strlen( (const char*)pArchive->pData + dataOffset ) + 1;
    }

    if( pArchive->flags.FCOMMENT )
    {
        dataOffset += ( uint32_t )strlen( (const char*)pArchive->pData + dataOffset ) + 1;
    }

    if( pArchive->flags.FHCRC )
    {
        dataOffset += 2u;
    }

    const uint8_t* pCompressedDataStart = ( const uint8_t* )( pArchive->pData + dataOffset );
    const uint8_t* pCompressedDataEnd   = ( const uint8_t* )( pArchive->pFooter );

    GZipCompressedData compressedData;
    compressedData.pCompressedData              = pCompressedDataStart;
    compressedData.compressedDataSizeInBytes    = ( uint32_t )( pCompressedDataEnd - pCompressedDataStart );

    return compressedData;
}

const char* getGZipCompressedFileName( const GZipArchive* pArchive )
{
    if( pArchive->flags.FNAME == 0 )
    {
        return nullptr;
    }

    return getGZipFileNameMember( pArchive );
}

BitStream createGZipBitStream( const GZipArchive* pArchive )
{
    const GZipCompressedData compressedData = getGZipCompressedData( pArchive );
    return createBitStream( compressedData.pCompressedData, compressedData.compressedDataSizeInBytes );
}

uint8_t uncompressDeflateStream( BitStream* pDeflateStream, MemoryWriteStream* pTargetStream )
{
    //FK: Reference: https://github.com/madler/zlib/blob/master/contrib/puff/puff.c
    //FK: Reference: https://www.w3.org/Graphics/PNG/RFC-1951
    //FK: Reference: https://www.infinitepartitions.com/art001.html
#if 0
    while( true )
    {
        const uint8_t lastBlock         = readBit( pDeflateStream );
        const uint8_t compressionMethod = readBitsInverse( pDeflateStream, 2u );

        switch( compressionMethod )
        {
            case 0:
            {
                //FK: Block is not compressed
                moveToNextByteBoundary( pDeflateStream );
                const uint8_t dataLengthLow         = readBits( pDeflateStream, 8u );
                const uint8_t dataLengthHigh        = readBits( pDeflateStream, 8u );
                const uint16_t dataLengthInBytes    = ( ( uint16_t )dataLengthLow << 8u | dataLengthHigh );

                //FK: Skip nlen
                skipBytes( pDeflateStream, 2u );
                
                const uint8_t* pBlockData = pDeflateStream->pDataCurrent;
                if( !pushData( pTargetStream, pBlockData, dataLengthInBytes ) )
                {
                    return 0u;
                }

                skipBytes( pDeflateStream, dataLengthInBytes );
                break;
            }

            case 1:
            {
                //FK: Block compressed with static huffman
                break;
            }

            case 2:
            {
                static constexpr uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 
                    11, 4, 12, 3, 13, 2, 14, 1, 15 };

#define MAXLCODES 286           /* maximum number of literal/length codes */
#define MAXDCODES 30            /* maximum number of distance codes */
#define MAXCODES (MAXLCODES+MAXDCODES)  /* maximum codes lengths to read */
                uint16_t lengths[MAXCODES] = {0};
                //FK: Block compressed with dynamic huffman
                uint16_t nlit   = readBitsInverse( pDeflateStream, 5u ) + 257;
                uint16_t ndist  = readBitsInverse( pDeflateStream, 5u ) + 1;
                uint16_t nlen   = readBitsInverse( pDeflateStream, 4u ) + 4;

                uint8_t index = 0u;
                for( index = 0u; index < nlen; ++index )
                {
                    lengths[ order[ index ] ] = readBitsInverse( pDeflateStream, 3u );
                }

                uint8_t constructHuffmanTable( HuffmanTable* pOutHuffmanTable, const uint16_t* pCodes, const uint16_t codeLengths )
                {

                }

                HuffmanTable lenCode;
                if( !constructHuffmanTable( &lenCode, lenghts, 19 ) )
                {
                    return 0u;
                }



                BreakPointHook();

                break;
            }
        }

        if( lastBlock )
        {
            break;
        }
    }
#endif
    return 0u;
}

GZipUncompressResult uncompressGZipArchive( const GZipArchive* pArchive, uint8_t* pTargetBuffer, const uint32_t targetBufferCapacityInBytes )
{
    if( pArchive->compressionMethod != 8u )
    {
        return GZipUncompressResult::UnsupportedCompressionMethod;
    }

    if( pArchive->pFooter->uncompressedSizeInBytes > targetBufferCapacityInBytes )
    {
        return GZipUncompressResult::TargetBufferTooSmall;
    }

    BitStream decodeStream          = createGZipBitStream( pArchive );
    MemoryWriteStream writeStream   = createMemoryWriteStream( pTargetBuffer, targetBufferCapacityInBytes );
    if( !uncompressDeflateStream( &decodeStream, &writeStream ) )
    {
        return GZipUncompressResult::TargetBufferTooSmall;
    }

    return GZipUncompressResult::ChecksumError;
}

uint8_t isValidZipArchive( const uint8_t* pZipArchiveData, const uint32_t zipArchiveDataSizeInBytes )
{
    //FK: Check for file header size
    if( zipArchiveDataSizeInBytes <= 30 )
    {
        return 0u;
    }

    return ( pZipArchiveData[0] == 'P' && pZipArchiveData[1] == 'K' && pZipArchiveData[2] == '\3' && pZipArchiveData[3] == '\4' );
}

char* trimTrailingWhitespaces( char* pString )
{
    const size_t stringLength = strlen( pString );
    size_t charIndex = stringLength - 1;
    while( isspace( pString[ charIndex ] ) && charIndex > 0u )
    {
        pString[ charIndex ] = 0;
        --charIndex;
    }

    return pString;
}

#pragma pack(push, 1)
struct ZipCentralDirectoryFileHeader
{
    uint32_t signature;
    uint16_t versionMadeBy;
    uint16_t versionNeededToExtract;
    uint16_t generalPurposeBitFlag;
    uint16_t compressionMethod;
    uint16_t lastModificationTime;
    uint16_t lastModificationDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumberWhereFileStarts;
    uint16_t internalFileAttributes;
    uint32_t externalFileAttributes;
    int32_t localFileHeaderRelativeOffset;
};

struct ZipLocalFileHeader
{
    uint32_t signature;
    uint16_t minVersion;
    uint16_t generalPurposeFlag;
    uint16_t compressionMethod;
    uint16_t fileLastModificationTime;
    uint16_t fileLastModificationDate;
    uint32_t crc32;
    uint32_t compressedSizeInBytes;
    uint32_t uncompressedSizeInBytes;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};
#pragma pack(pop)

struct ZipArchiveEntry
{
    const char* pFileName;
    const char* pFileComment;
    int32_t absoluteOffsetToFileHeader;
    uint32_t absoluteOffsetToNextEntry;
    uint32_t uncompressedSizeInBytes;
    uint32_t compressedSizeInBytes;
    uint16_t fileNameLength;
    uint16_t recordIndex;
    uint16_t compressionMethod;
};

ZipArchiveEntry createZipArchiveEntry( const ZipArchive* pZipArchive, const uint32_t absoluteOffsetToCentralDirectoryFileHeader, uint16_t recordIndex )
{
    ZipCentralDirectoryFileHeader centralDirectoryFileHeader;
    memcpy( &centralDirectoryFileHeader, pZipArchive->pData + absoluteOffsetToCentralDirectoryFileHeader, sizeof( ZipCentralDirectoryFileHeader ) );

    uint32_t offset;
    memcpy( &offset, pZipArchive->pData + absoluteOffsetToCentralDirectoryFileHeader + 42, sizeof( offset ) );

    const uint32_t variableByteLength = 46 + centralDirectoryFileHeader.fileNameLength + centralDirectoryFileHeader.extraFieldLength + centralDirectoryFileHeader.fileCommentLength;
    const uint16_t newRecordIndex = recordIndex + 1u;

    ZipArchiveEntry archiveEntry;
    archiveEntry.compressedSizeInBytes      = centralDirectoryFileHeader.compressedSize;
    archiveEntry.uncompressedSizeInBytes    = centralDirectoryFileHeader.uncompressedSize;
    archiveEntry.compressionMethod          = centralDirectoryFileHeader.compressionMethod;
    archiveEntry.recordIndex                = newRecordIndex;
    archiveEntry.absoluteOffsetToFileHeader = centralDirectoryFileHeader.localFileHeaderRelativeOffset;
    archiveEntry.absoluteOffsetToNextEntry  = pZipArchive->pEocd->centralDirRecordCount == newRecordIndex ? 0u : absoluteOffsetToCentralDirectoryFileHeader + variableByteLength;
    archiveEntry.fileNameLength             = centralDirectoryFileHeader.fileNameLength;
    archiveEntry.pFileName                  = (const char*)pZipArchive->pData + absoluteOffsetToCentralDirectoryFileHeader + 46;
    archiveEntry.pFileComment               = (const char*)pZipArchive->pData + absoluteOffsetToCentralDirectoryFileHeader + centralDirectoryFileHeader.fileNameLength + 46;

    return archiveEntry;
}

ZipArchiveEntry findFirstZipArchiveEntry( const ZipArchive* pZipArchive )
{
    const ZipEndOfCentralDirectory* pZipDirectory = pZipArchive->pEocd;
    return createZipArchiveEntry( pZipArchive, pZipDirectory->centralDirectoryAbsoluteOffset, 0u );
}

ZipArchiveEntry findNextZipArchiveEntry( const ZipArchive* pZipArchive, const ZipArchiveEntry* pPrevEntry )
{
    RuntimeAssert( pPrevEntry->absoluteOffsetToNextEntry > 0u );
    return createZipArchiveEntry( pZipArchive, pPrevEntry->absoluteOffsetToNextEntry, pPrevEntry->recordIndex );
}

uint8_t isLastZipArchiveEntry( const ZipArchiveEntry* pEntry )
{
    return pEntry->absoluteOffsetToNextEntry == 0u;
}

uint8_t areStringsEqual( const char* pStringA, const char* pStringB, const uint32_t stringLength )
{
    for( uint32_t charIndex = 0u; charIndex < stringLength; ++charIndex )
    {
        if( pStringA[ charIndex ] != pStringB[ charIndex ] )
        {
            return 0u;
        }
    }

    return 1u;
}

const char* findLastInString( const char* pString, uint32_t stringLength, const char needle )
{
    const char* pStringStart = pString;
    const char* pStringEnd = pString + stringLength;
    while( pStringStart != pStringEnd )
    {
        if( *pStringEnd == needle )
        {
            return pStringEnd;
        }

        --pStringEnd;
    }

    return nullptr;
}

uint8_t filePathHasRomFileExtension( const char* pFilePath, const uint16_t filePathLength )
{
    const char* pFileExtension = findLastInString( pFilePath, filePathLength, '.' );
    if( pFileExtension != nullptr )
    {
        const uint32_t fileExtensionLength = filePathLength - (uint32_t)( pFileExtension - pFilePath );
        return areStringsEqual( pFileExtension, ".gb", fileExtensionLength ) || areStringsEqual( pFileExtension, ".gbc", fileExtensionLength );
    }

    return 0;
}  

ZipArchiveEntry findFirstRomEntryInZipArchive( const ZipArchive* pZipArchive )
{
    ZipArchiveEntry zipArchiveEntry = findFirstZipArchiveEntry( pZipArchive );
    while( true )
    {
        if( filePathHasRomFileExtension( zipArchiveEntry.pFileName, zipArchiveEntry.fileNameLength ) )
        {
            return zipArchiveEntry;
        }

        if( isLastZipArchiveEntry( &zipArchiveEntry ) )
        {
            break;
        }

        zipArchiveEntry = findNextZipArchiveEntry( pZipArchive, &zipArchiveEntry );
    }

    //FK: The caller should check if there's at least one zip file in the archive before calling this function.
    IllegalCodePath();
    return zipArchiveEntry;
}

uint32_t countRomsInZipArchive( const ZipArchive* pZipArchive )
{
    uint32_t romCount = 0u;
    const ZipEndOfCentralDirectory* pEocd = pZipArchive->pEocd;

    ZipArchiveEntry zipArchiveEntry = findFirstZipArchiveEntry( pZipArchive );
    while( true )
    {
        if( filePathHasRomFileExtension( zipArchiveEntry.pFileName, zipArchiveEntry.fileNameLength ) )
        {
            ++romCount;
        }

        if( isLastZipArchiveEntry( &zipArchiveEntry ) )
        {
            break;
        }

        zipArchiveEntry = findNextZipArchiveEntry( pZipArchive, &zipArchiveEntry );
    }

    return romCount;
}

uint8_t openZipArchive( ZipArchive* pOutZipArchive, const uint8_t* pZipData, const uint32_t zipDataSizeInBytes )
{
    if( !isValidZipArchive( pZipData, zipDataSizeInBytes ) )
	{
		return 0u;
	}

    const ZipEndOfCentralDirectory* pZipEOCD = findZipArchiveEndOfCentralDirectory( pZipData, zipDataSizeInBytes );
    ZipArchive zipArchive;
    zipArchive.pData                = pZipData;
    zipArchive.dataSizeInBytes      = zipDataSizeInBytes;
    zipArchive.pEocd                = pZipEOCD;

    *pOutZipArchive = zipArchive;
    return 1u;
}

enum class ZipDecompressionResult : uint8_t
{
    Success,
    NotSupported
};

struct ZipLocalFileEntry
{
    const uint8_t* pDataStart;
    uint32_t    dataSizeInBytes;
};

ZipLocalFileEntry getZipArchiveEntryFile( const ZipArchive* pZipArchive, const ZipArchiveEntry* pZipArchiveEntry )
{
    ZipLocalFileHeader localFileHeader;
    memcpy( &localFileHeader, pZipArchive->pData + pZipArchiveEntry->absoluteOffsetToFileHeader, sizeof( localFileHeader ) );

    ZipLocalFileEntry localFileEntry;
    localFileEntry.pDataStart       = pZipArchive->pData + pZipArchiveEntry->absoluteOffsetToFileHeader + sizeof( localFileHeader ) + localFileHeader.fileNameLength + localFileHeader.extraFieldLength;
    localFileEntry.dataSizeInBytes  = localFileHeader.compressedSizeInBytes;
    return localFileEntry;
}

#if 0
void readDeflateBlockHeader( ZipDeflateBlock* pZipDeflateBlock )
{
    const uint8_t* pZipBlockData = (const uint8_t*)pZipDeflateBlock;
    pZipDeflateBlock->header.isFinal            = readBits( &pZipDeflateBlock->bitReader, 1u );
    pZipDeflateBlock->header.compressionMethod  = readBits( &pZipDeflateBlock->bitReader, 2u );
}

ZipDeflateBlock readFirstZipBlockFromLocalFileEntry( const ZipLocalFileEntry* pZipLocalFileEntry )
{
    ZipDeflateBlock deflateBlock;
    deflateBlock.bitReader = initBitReader( pZipLocalFileEntry->pDataStart, pZipLocalFileEntry->dataSizeInBytes );
    readDeflateBlockHeader( &deflateBlock );

    return deflateBlock;
}

const uint8_t* readUncompressedDeflateBlock( uint16_t* pOutSizeInBytes, ZipDeflateBlock* pZipDeflateBlock )
{
    RuntimeAssert( pZipDeflateBlock->header.compressionMethod == 0b00 );

    const uint8_t* pBlockData = pZipDeflateBlock->pBlockStart;
    if( pZipDeflateBlock->blockShift != 0u )
    {
        ++pBlockData;
    }

    const uint16_t uncompressedDataSizeInBytes = *(const uint16_t*)pBlockData;
    *pOutSizeInBytes = uncompressedDataSizeInBytes;

    return ( pBlockData += 4u );
}
#endif

uint8_t isValidGBRomData( const uint8_t* pRomData, const uint32_t romSizeInBytes )
{
    if( romSizeInBytes < Kbyte( 32u ) )
    {
        return 0;
    }

    const GBRomHeader header = getGBRomHeader( pRomData );
    return memcmp( header.nintendoLogo, gbNintendoLogo, sizeof( gbNintendoLogo ) ) == 0;
}

uint32_t mapRomSizeToByteSize( uint8_t romSize )
{
    switch(romSize)
    {
        case 0x00:
            return Kbyte(32u);
        case 0x01:
            return Kbyte(64u);
        case 0x02:
            return Kbyte(128u);
        case 0x03:
            return Kbyte(256u);
        case 0x04:
            return Kbyte(512u);
        case 0x05:
            return Mbyte(1u);
        case 0x06:
            return Mbyte(2u);
        case 0x07:
            return Mbyte(4u);
        case 0x08:
            return Mbyte(8u);
    }

    //FK: Unsupported romSize identifier
    IllegalCodePath();
    return 0;
}

uint32_t mapRamSizeToByteSize( uint8_t ramSize )
{
    switch( ramSize )
    {
        case 0x00:
            return 0;
        case 0x02:
            return Kbyte(8);
        case 0x03:
            return Kbyte(32);
        case 0x04:
            return Kbyte(128);
        case 0x05:
            return Kbyte(64);
    }

    //FK: Unsupported ramSize identifier
    IllegalCodePath();
    return 0;
}

void mapCartridgeRom0Bank( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, uint16_t romBankNumber )
{
    RuntimeAssert( romBankNumber < pCartridge->romBankCount );
    if( pCartridge->mappedRom0BankNumber == romBankNumber )
    {
        return;
    }

    pCartridge->mappedRom0BankNumber = romBankNumber;

    const uint8_t* pRomBank = pCartridge->pRomBaseAddress + romBankNumber * gbRomBankSizeInBytes;
    memcpy( pMemoryMapper->pRom0Bank, pRomBank, gbRomBankSizeInBytes );
}

void mapCartridgeRom1Bank( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, uint16_t romBankNumber )
{
    RuntimeAssert( romBankNumber < pCartridge->romBankCount );
    if( pCartridge->mappedRom1BankNumber == romBankNumber )
    {
        return;
    }

    pCartridge->mappedRom1BankNumber = romBankNumber;

    const uint8_t* pRomBank = pCartridge->pRomBaseAddress + romBankNumber * gbRomBankSizeInBytes;
    memcpy( pMemoryMapper->pRom1Bank, pRomBank, gbRomBankSizeInBytes );
}

void mapCartridgeRamBank( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, uint8_t ramBankNumber )
{
    RuntimeAssert( ramBankNumber < pCartridge->ramBankCount );
    if( pCartridge->mappedRamBankNumber == ramBankNumber )
    {
        return;
    }

    pCartridge->mappedRamBankNumber = ramBankNumber;

    const uint8_t* pRamBank = pCartridge->pRamBaseAddress + ramBankNumber * gbRamBankSizeInBytes;
    memcpy( pMemoryMapper->pRamBankSwitch, pRamBank, gbRamBankSizeInBytes );
}

uint8_t isGBEmulatorRomMapped( const GBEmulatorInstance* pEmulatorInstance )
{
    return pEmulatorInstance->pCartridge->pRomBaseAddress != nullptr;
}

GBRomHeader getGBEmulatorCurrentCartridgeHeader( const GBEmulatorInstance* pEmulatorInstance )
{
    RuntimeAssert( pEmulatorInstance->pCartridge->pRomBaseAddress != nullptr );
    return getGBRomHeader( pEmulatorInstance->pCartridge->pRomBaseAddress );
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
    memcpy( pDestination, &tokenCounter, sizeof( uint16_t ) );
    pDestination[2] = token;

    //FK: write compressed memory size in bytes at the beginning of the memory block
    memcpy( pDestinationBaseAddress, &compressedMemorySizeInBytes, sizeof( uint32_t ) );

    return compressedMemorySizeInBytes;
}

void storeGBEmulatorState( const GBEmulatorInstance* pEmulatorInstance, uint8_t* pStateMemory, size_t stateMemorySizeInBytes )
{
    const GBCpuState* pCpuState         = pEmulatorInstance->pCpuState;
    const GBPpuState* pPpuState         = pEmulatorInstance->pPpuState;
    const GBApuState* pApuState         = pEmulatorInstance->pApuState;
    const GBTimerState* pTimerState     = pEmulatorInstance->pTimerState;
    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    const GBSerialState* pSerialState   = pEmulatorInstance->pSerialState;
    const GBCartridge* pCartridge       = pEmulatorInstance->pCartridge;

    const GBRomHeader header = getGBEmulatorCurrentCartridgeHeader( pEmulatorInstance );
    const uint16_t cartridgeChecksum = header.checksumHigher << 8 | header.checksumLower;

    GBEmulatorState state;
    state.cpuState                  = *pCpuState;
    state.ppuState                  = *pPpuState;
    state.apuState                  = *pApuState;
    state.timerState                = *pTimerState;
    state.serialState               = *pSerialState;
    state.cartridge                 = *pCartridge;
    state.mappedRom0BankNumber      = pCartridge->mappedRom0BankNumber;
    state.mappedRom1BankNumber      = pCartridge->mappedRom1BankNumber;
    state.mappedRamBankNumber       = pCartridge->mappedRamBankNumber;

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

GBStateLoadResult loadGBEmulatorState( GBEmulatorInstance* pEmulatorInstance, const uint8_t* pStateMemory )
{
    uint32_t fourCC;
    memcpy( &fourCC, pStateMemory, sizeof( gbStateFourCC ) );
    pStateMemory += sizeof( gbStateFourCC );

    if( fourCC != gbStateFourCC )
    {
        return K15_GB_STATE_LOAD_FAILED_INCOMPATIBLE_DATA;
    }

    const uint16_t stateCartridgeChecksum = *(uint16_t*)pStateMemory;
    pStateMemory += sizeof( stateCartridgeChecksum );

    const GBRomHeader cartridgeHeader = getGBEmulatorCurrentCartridgeHeader( pEmulatorInstance );

    const uint16_t cartridgeChecksum = cartridgeHeader.checksumHigher << 8 | cartridgeHeader.checksumLower;
    if( cartridgeChecksum != stateCartridgeChecksum )
    {
        return K15_GB_STATE_LOAD_FAILED_WRONG_ROM;
    }

    const uint8_t stateVersion = *pStateMemory;
    ++pStateMemory;

    if( stateVersion != gbStateVersion )
    {
        return K15_GB_STATE_LOAD_FAILED_OLD_VERSION;
    }

    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;

    uint8_t* pGBFrameBuffers[ gbFrameBufferCount ] = { 
        pEmulatorInstance->pPpuState->pGBFrameBuffers[ 0 ],
        pEmulatorInstance->pPpuState->pGBFrameBuffers[ 1 ]
    };

    GBEmulatorState state;
    memcpy( &state, pStateMemory, sizeof( GBEmulatorState ) );
    pStateMemory += sizeof( GBEmulatorState );

    const uint8_t* pRomBaseAddress  = pEmulatorInstance->pCartridge->pRomBaseAddress;
    uint8_t* pRamBaseAddress        = pEmulatorInstance->pCartridge->pRamBaseAddress;

    *pEmulatorInstance->pCpuState       = state.cpuState;
    *pEmulatorInstance->pPpuState       = state.ppuState;
    *pEmulatorInstance->pApuState       = state.apuState;
    *pEmulatorInstance->pTimerState     = state.timerState;
    *pEmulatorInstance->pSerialState    = state.serialState;
    *pEmulatorInstance->pCartridge      = state.cartridge;

    GBCartridge* pCartridge = pEmulatorInstance->pCartridge;
    pCartridge->pRomBaseAddress     = pRomBaseAddress;
    pCartridge->pRamBaseAddress     = pRamBaseAddress;
    pCartridge->header              = getGBRomHeader( pRomBaseAddress );

    const size_t romSizeInBytes = mapRomSizeToByteSize( pCartridge->header.romSize );
    pCartridge->mappedRom1BankNumber = 0xFF;
    pCartridge->mappedRom0BankNumber = 0xFF;
    pCartridge->romBankCount         = ( uint16_t )( romSizeInBytes / gbRomBankSizeInBytes );
    mapCartridgeRom0Bank( pCartridge, pEmulatorInstance->pMemoryMapper, state.mappedRom0BankNumber );
    mapCartridgeRom1Bank( pCartridge, pEmulatorInstance->pMemoryMapper, state.mappedRom1BankNumber );

    const size_t ramSizeInBytes = mapRamSizeToByteSize( pCartridge->header.ramSize );
    if( ramSizeInBytes > 0u )
    {
        pCartridge->mappedRamBankNumber = 0xFF;
        pCartridge->ramBankCount = ( uint8_t )( ramSizeInBytes / gbRamBankSizeInBytes );
        mapCartridgeRamBank( pCartridge, pEmulatorInstance->pMemoryMapper, 0 );
    }

    patchIOTimerMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pTimerState );
    patchIOPpuMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pPpuState );
    patchIOCpuMappedMemoryPointer( pMemoryMapper, pEmulatorInstance->pCpuState );

    pEmulatorInstance->pPpuState->pGBFrameBuffers[ 0 ] = pGBFrameBuffers[ 0 ];
    pEmulatorInstance->pPpuState->pGBFrameBuffers[ 1 ] = pGBFrameBuffers[ 1 ];

    pMemoryMapper->lcdStatus  = *pEmulatorInstance->pPpuState->lcdRegisters.pStatus;
    pMemoryMapper->dmaActive  = pEmulatorInstance->pCpuState->flags.dma;
    pMemoryMapper->lcdEnabled = pEmulatorInstance->pPpuState->pLcdControl->enable;
    pMemoryMapper->ramEnabled = pEmulatorInstance->pCartridge->ramEnabled;

    const uint8_t* pCompressedMemory = pStateMemory;
    uncompressMemoryBlockRLE( pMemoryMapper->pBaseAddress + 0x8000, pCompressedMemory );
    return K15_GB_STATE_LOAD_SUCCESS;
}

uint8_t allowReadFromMemoryAddress( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    //FK: Don't allow VRAM and OAM access while ppu is in mode 3 or 2 (drawing and oam search respectively)
    if( pMemoryMapper->lcdEnabled )
    {
        if ( pMemoryMapper->lcdStatus.mode == 3 )
        {
            if( isInOAMAddressRange( addressOffset ) ||
                isInVideoRamAddressRange( addressOffset ) )
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
    if( pMemoryMapper->dmaActive && !isInHighRamAddressRange( addressOffset ) )
    {
        return 0;
    }

    if( !pMemoryMapper->ramEnabled && isInExternalRamRange( addressOffset ) )
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

    pMemoryMapper->memoryAccess = GBMemoryAccess_Read;
    pMemoryMapper->lastAddressReadFrom = addressOffset;
    return pMemoryMapper->pBaseAddress[addressOffset];
}

uint16_t read16BitValueFromMappedMemory( GBMemoryMapper* pMemoryMapper, uint16_t addressOffset )
{
    pMemoryMapper->memoryAccess = GBMemoryAccess_Read;
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
    if( isInIORegisterRange( addressOffset ) )
    {
         //FK: Check if the address is part of the I/O registers
        //    these will be written later in `handleMappedIORegisterWrite()`
        return 0;
    }

    if( isInCartridgeRomAddressRange( addressOffset ) )
    {
        return 0;
    }

    if( isInExternalRamRange( addressOffset ) && !pMemoryMapper->ramEnabled )
    {
        return 0;
    }

    if( pMemoryMapper->lcdStatus.mode == 3 && pMemoryMapper->lcdEnabled )
    {
        if( isInVideoRamAddressRange( addressOffset ) || 
            isInOAMAddressRange( addressOffset ) )
        {
            //FK: can't read from VRAM and/or OAM during lcd mode 3 
            return 0;
        }
    }

    if( pMemoryMapper->dmaActive )
    {
        if( !isInHighRamAddressRange( addressOffset ) )
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
    pMemoryMapper->memoryAccess         = GBMemoryAccess_Written;

    if( !allowWriteToMemoryAddress( pMemoryMapper, addressOffset ) )
    {
        return;
    }
   
    //FK: Save to write immediately to memory
    pMemoryMapper->pBaseAddress[addressOffset] = value;

    //FK: Echo 8kB internal Ram
    if( isInWorkRamRange( addressOffset ) )
    {
        addressOffset += 0x2000;
        pMemoryMapper->pBaseAddress[addressOffset] = value;
    }
    else if( isInEchoRamRange( addressOffset ) )
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

uint8_t isCartridgeTypeSupported( const GBCartridgeType cartridgeType )
{
    switch( cartridgeType )
    {
        case ROM_ONLY:
        case ROM_RAM:
        case ROM_RAM_BATT:

        case ROM_MBC1:
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:

        case ROM_MBC5:
        case ROM_MBC5_RAM:
        case ROM_MBC5_RAM_BATT:
        case ROM_MBC5_RUMBLE:
        case ROM_MBC5_RUMBLE_SRAM:
        case ROM_MBC5_RUMBLE_SRAM_BATT:
            return 1;

        default:
            return 0;
    }

    IllegalCodePath();
    return 0;
}

GBMapCartridgeResult mapCartridgeMemory( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper, const uint8_t* pRomMemory, uint8_t* pRamMemory )
{
    const GBRomHeader header = getGBRomHeader( pRomMemory );
    if( !isCartridgeTypeSupported( header.cartridgeType ) )
    {
        return K15_GB_CARTRIDGE_TYPE_UNSUPPORTED;
    }

    const uint32_t romSizeInBytes = mapRomSizeToByteSize( header.romSize );
    const uint32_t ramSizeInBytes = mapRamSizeToByteSize( header.ramSize );
    pCartridge->ramEnabled              = 0;
    pCartridge->header                  = header;
    pCartridge->pRomBaseAddress         = pRomMemory;
    pCartridge->pRamBaseAddress         = pRamMemory;
    pCartridge->romBankCount            = ( uint16_t )( romSizeInBytes / gbRomBankSizeInBytes );
    pCartridge->ramBankCount            = ( uint8_t )( ramSizeInBytes / gbRamBankSizeInBytes );
    pCartridge->romSizeInBytes          = romSizeInBytes;
    pCartridge->ramSizeInBytes          = ramSizeInBytes;
    pCartridge->lowBankValue            = 1;
    pCartridge->highBankValue           = 0;

    mapCartridgeRom0Bank( pCartridge, pMemoryMapper, 0 );
    mapCartridgeRom1Bank( pCartridge, pMemoryMapper, 1 );

    if( pCartridge->ramBankCount > 0 )
    {
        mapCartridgeRamBank( pCartridge, pMemoryMapper, 0 );
    }

    return K15_GB_CARTRIDGE_MAPPED_SUCCESSFULLY;
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
    pState->registers.AF            = 0x0100;
    pState->registers.BC            = 0xFF13;
    pState->registers.DE            = 0x00C1;
    pState->registers.HL            = 0x8403;

    pState->dmaCycleCounter         = 0;
    pState->cycleCounter            = 0;

    *pState->pIE = 0xF0;
    *pState->pIF = 0xE1;

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
    pMapper->ramEnabled = 0;
}

void initMemoryMapper( GBMemoryMapper* pMapper, uint8_t* pMemory )
{
    pMapper->pBaseAddress           = pMemory;
    pMapper->pRom0Bank              = pMemory;
    pMapper->pRom1Bank              = pMemory + 0x4000;
    pMapper->pVideoRAM              = pMemory + 0x8000;
    pMapper->pRamBankSwitch         = pMemory + 0xA000;
    pMapper->pSpriteAttributes      = pMemory + 0xFE00;

    resetMemoryMapper( pMapper );
}

void initTimerState( GBMemoryMapper* pMemoryMapper, GBTimerState* pTimerState )
{
    patchIOTimerMappedMemoryPointer( pMemoryMapper, pTimerState );
    
    pTimerState->counterFrequencyBit    = K15_GB_COUNTER_FREQUENCY_BIT_00;
    pTimerState->internalDivCounter     = 0u;
    pTimerState->enableCounter          = 0u;
    pTimerState->timerOverflow          = 0u;

    *pTimerState->pDivider      = 0xAB;
    *pTimerState->pCounter      = 0;
    *pTimerState->pModulo       = 0;
    *pTimerState->pControl      = 0xF8;
}

void initSerialState( GBMemoryMapper* pMemoryMapper, GBSerialState* pSerialState )
{
    patchIOSerialMappedMemoryPointer( pMemoryMapper, pSerialState );

    pSerialState->cycleCounter          = 0u;
    pSerialState->inByte                = 0xFFu;
    pSerialState->shiftIndex            = 0u;
    pSerialState->transferInProgress    = 0u;

    *pSerialState->pSerialTransferControl   = 0x7E;
    *pSerialState->pSerialTransferData      = 0x00;
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

void initPpuFrameBuffers( GBPpuState* pPpuState, uint8_t* pMemory )
{
    pPpuState->pGBFrameBuffers[ 0 ] = pMemory + 0;
    pPpuState->pGBFrameBuffers[ 1 ] = pMemory + gbFrameBufferSizeInBytes;
    clearGBFrameBuffer( pPpuState->pGBFrameBuffers[ 0 ] );
    clearGBFrameBuffer( pPpuState->pGBFrameBuffers[ 1 ] );
}

void initPpuState( GBMemoryMapper* pMemoryMapper, GBPpuState* pPpuState )
{
    patchIOPpuMappedMemoryPointer( pMemoryMapper, pPpuState );

    //FK: set default state of LCDC (taken from bgb)
    pMemoryMapper->pBaseAddress[ 0xFF40 ] = 0x91;
    pMemoryMapper->pBaseAddress[ 0xFF41 ] = 0x80;

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

    pPpuState->activeFrameBufferIndex = 0;

    clearGBFrameBuffer( pPpuState->pGBFrameBuffers[ pPpuState->activeFrameBufferIndex ] );
}

void initApuState( GBMemoryMapper* pMemoryMapper, GBApuState* pApuState )
{
    patchIOApuMappedMemoryPointer( pMemoryMapper, pApuState );
    pApuState->frameSequencer.clockCounter = 0u;
    pApuState->frameSequencer.cycleCounter = 0u;

    pApuState->squareWaveChannel1.lengthTimer = 0u;
    pApuState->squareWaveChannel2.lengthTimer = 0u;
    pApuState->noiseChannel.lengthTimer = 0u;

    pApuState->waveChannel.cycleCount                   = 0u;
    pApuState->waveChannel.frequencyCycleCountTarget    = 0u;
    pApuState->waveChannel.channelEnabled               = 0u;
    pApuState->waveChannel.currentSample                = 0u;
    pApuState->waveChannel.samplePosition               = 0u;
    pApuState->waveChannel.lengthTimer                  = 0u;

    pMemoryMapper->pBaseAddress[ 0xFF10 ] = 0x80;
    pMemoryMapper->pBaseAddress[ 0xFF11 ] = 0xBF;
    pMemoryMapper->pBaseAddress[ 0xFF12 ] = 0xF3;
    pMemoryMapper->pBaseAddress[ 0xFF14 ] = 0xBF;
    pMemoryMapper->pBaseAddress[ 0xFF16 ] = 0x3F;
    pMemoryMapper->pBaseAddress[ 0xFF17 ] = 0x00;
    pMemoryMapper->pBaseAddress[ 0xFF19 ] = 0xBF;
    pMemoryMapper->pBaseAddress[ 0xFF1A ] = 0x7F;
    pMemoryMapper->pBaseAddress[ 0xFF1C ] = 0x9F;
    pMemoryMapper->pBaseAddress[ 0xFF1E ] = 0xBF;
    pMemoryMapper->pBaseAddress[ 0xFF21 ] = 0x00;
    pMemoryMapper->pBaseAddress[ 0xFF22 ] = 0x00;
    pMemoryMapper->pBaseAddress[ 0xFF23 ] = 0xBF;
    pMemoryMapper->pBaseAddress[ 0xFF24 ] = 0x77;
    pMemoryMapper->pBaseAddress[ 0xFF25 ] = 0xF3;
    pMemoryMapper->pBaseAddress[ 0xFF26 ] = 0xF1;
}

size_t calculateGBEmulatorMemoryRequirementsInBytes()
{
    const size_t memoryRequirementsInBytes = sizeof(GBEmulatorInstance) + sizeof(GBCpuState) + 
        sizeof(GBMemoryMapper) + sizeof(GBPpuState) + sizeof(GBTimerState) + sizeof(GBCartridge) + 
        sizeof(GBSerialState) + gbMappedMemorySizeInBytes + ( gbFrameBufferSizeInBytes * gbFrameBufferCount );

    return memoryRequirementsInBytes;
}

void resetGBEmulator( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    GBCartridge* pCartridge = pEmulatorInstance->pCartridge;

    resetMemoryMapper(pEmulatorInstance->pMemoryMapper );

    uint8_t* pRamBaseAddress = pCartridge->pRamBaseAddress;
    if( pCartridge->pRomBaseAddress != nullptr )
    {
        pCartridge->mappedRom0BankNumber    = 0;
        pCartridge->mappedRom1BankNumber    = 0;
        pCartridge->mappedRamBankNumber     = 0;
        mapCartridgeMemory(pCartridge, pMemoryMapper, pCartridge->pRomBaseAddress, pCartridge->pRamBaseAddress );
    }

    initCpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pCpuState);
    initPpuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pPpuState);
    initApuState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pApuState);
    initTimerState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pTimerState);
    initSerialState(pEmulatorInstance->pMemoryMapper, pEmulatorInstance->pSerialState);

    pEmulatorInstance->joypadState.actionButtonMask  = 0;
    pEmulatorInstance->joypadState.dpadButtonMask    = 0;

    //FK: Reset joypad value
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF00] = 0xCF;
    pEmulatorInstance->pMemoryMapper->pBaseAddress[0xFF04] = 0x19;
}

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES
void setGBEmulatorBreakpoint( GBEmulatorInstance* pEmulatorInstance, uint8_t pauseAtBreakpoint, uint16_t breakpointAddress )
{
    pEmulatorInstance->debug.pauseAtBreakpoint = pauseAtBreakpoint;
    pEmulatorInstance->debug.breakpointAddress = breakpointAddress;
}

void continueGBEmulatorExecution( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.continueExecution = 1;
}

void pauseGBEmulatorExecution( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.pauseExecution = 1;
}

void runGBEmulatorForOneInstruction( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.runForOneInstruction = 1;
}

void runGBEmulatorForOneFrame( GBEmulatorInstance* pEmulatorInstance )
{
    pEmulatorInstance->debug.runSingleFrame = 1;
}
#endif

GBEmulatorInstance* createGBEmulatorInstance( uint8_t* pEmulatorInstanceMemory )
{
    GBEmulatorInstance* pEmulatorInstance = (GBEmulatorInstance*)pEmulatorInstanceMemory;
    pEmulatorInstance->pCpuState        = (GBCpuState*)(pEmulatorInstance + 1);
    pEmulatorInstance->pApuState        = (GBApuState*)(pEmulatorInstance->pCpuState + 1);
    pEmulatorInstance->pMemoryMapper    = (GBMemoryMapper*)(pEmulatorInstance->pApuState + 1);
    pEmulatorInstance->pPpuState        = (GBPpuState*)(pEmulatorInstance->pMemoryMapper + 1);
    pEmulatorInstance->pTimerState      = (GBTimerState*)(pEmulatorInstance->pPpuState + 1);
    pEmulatorInstance->pSerialState     = (GBSerialState*)(pEmulatorInstance->pTimerState + 1);
    pEmulatorInstance->pCartridge       = (GBCartridge*)(pEmulatorInstance->pSerialState + 1);

    uint8_t* pGBMemory = (uint8_t*)(pEmulatorInstance->pCartridge + 1);
    initMemoryMapper( pEmulatorInstance->pMemoryMapper, pGBMemory );

    uint8_t* pFramebufferMemory = (uint8_t*)(pGBMemory + gbMappedMemorySizeInBytes);
    initPpuFrameBuffers( pEmulatorInstance->pPpuState, pFramebufferMemory );

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

    resetGBEmulator( pEmulatorInstance );
    return pEmulatorInstance;
}

GBMapCartridgeResult loadGBEmulatorRom( GBEmulatorInstance* pEmulator, const uint8_t* pRomMemory, uint8_t* pRamMemory )
{
    pEmulator->pCartridge->pRomBaseAddress = nullptr;
    pEmulator->pCartridge->mappedRom0BankNumber = 0xFFu;
    pEmulator->pCartridge->mappedRom1BankNumber = 0xFFu;
    pEmulator->pCartridge->mappedRamBankNumber  = 0xFFu;

    resetGBEmulator( pEmulator );
    return mapCartridgeMemory( pEmulator->pCartridge, pEmulator->pMemoryMapper, pRomMemory, pRamMemory );
}

uint8_t* getActiveFrameBuffer( GBPpuState* pPpuState )
{
    return pPpuState->pGBFrameBuffers[ pPpuState->activeFrameBufferIndex ];
}

void pushSpritePixelsToScanline( GBPpuState* pPpuState, uint8_t scanlineYCoordinate )
{
    if( pPpuState->scanlineSpriteCounter == 0 )
    {
        return;
    }

    uint8_t* pActiveFrameBuffer = getActiveFrameBuffer( pPpuState );
    uint8_t* pFrameBufferPixelData = pActiveFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
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

        uint8_t* pFrameBufferPixelData = pActiveFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
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

    uint8_t* pActiveFrameBuffer = getActiveFrameBuffer( pPpuState );
    uint8_t* pFrameBufferPixelData = pActiveFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
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

    uint8_t* pActiveFrameBuffer = getActiveFrameBuffer( pPpuState );
    uint8_t* pFrameBufferPixelData = pActiveFrameBuffer + ( gbFrameBufferScanlineSizeInBytes * scanlineYCoordinate );
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
    uint8_t* pActiveFrameBuffer = getActiveFrameBuffer( pPpuState );
    clearGBFrameBufferScanline( pActiveFrameBuffer, scanlineYCoordinate );

    if( pPpuState->pLcdControl->bgEnable )
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

    if( pPpuState->pLcdControl->windowEnable )
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

    if( pPpuState->pLcdControl->objEnable )
    {
        pushSpritePixelsToScanline( pPpuState, scanlineYCoordinate );
    }
}

void triggerInterrupt( GBCpuState* pCpuState, GBCpuInterrupt interruptFlag )
{
    *pCpuState->pIF |= (uint8_t)interruptFlag;
}

void updatePPULcdControl( GBPpuState* pPpuState, GBLcdControl lcdControlValue )
{
    if( lcdControlValue.enable != pPpuState->pLcdControl->enable )
    {
        if( !lcdControlValue.enable )
        {
            clearGBFrameBuffer( pPpuState->pGBFrameBuffers[ pPpuState->activeFrameBufferIndex ] );
            *pPpuState->lcdRegisters.pLy = 0;
            pPpuState->lcdRegisters.pStatus->mode = 0;
            pPpuState->dotCounter = 0;
        }
    }

    *pPpuState->pLcdControl = lcdControlValue;
}

uint8_t convertTimerControlFrequencyBit( const uint8_t timerControlValue )
{
    const uint8_t frequency = timerControlValue & 0x3;
    switch( frequency )
    {
        case 0b00:
            return K15_GB_COUNTER_FREQUENCY_BIT_00;
        case 0b01:
            return K15_GB_COUNTER_FREQUENCY_BIT_01;
        case 0b10:
            return K15_GB_COUNTER_FREQUENCY_BIT_10;
        case 0b11:
            return K15_GB_COUNTER_FREQUENCY_BIT_11;
    }

    IllegalCodePath();
    return 0;
}

void updateSerial( GBCpuState* pCpuState, GBSerialState* pSerial, const uint8_t cycleCount )
{
    if( !pSerial->transferInProgress )
    {
        return;
    }

    pSerial->cycleCounter += cycleCount;
    while( pSerial->cycleCounter >= gbSerialClockCyclesPerBitTransfer )
    {
        const uint8_t transferData = *pSerial->pSerialTransferData;
        pSerial->cycleCounter -= gbSerialClockCyclesPerBitTransfer;

        const uint8_t outBits = ( transferData << 1 );
        const uint8_t inBits  = pSerial->inByte >> ( 7 - pSerial->shiftIndex );
        *pSerial->pSerialTransferData = outBits | inBits;

        ++pSerial->shiftIndex;
        if( pSerial->shiftIndex == 8 )
        {
            *pSerial->pSerialTransferControl &= ~0x80;
            pSerial->transferInProgress = 0;
            pSerial->shiftIndex = 0u;
            triggerInterrupt( pCpuState, SerialInterrupt );
        }
    }
}

void incrementTimerCounter( GBTimerState* pTimer )
{
    *pTimer->pCounter += 1;
    if( *pTimer->pCounter == 0 )
    {
        //FK: Delay interrupt triggering due to obscure timer behavior
        //    https://gbdev.gg8.se/wiki/articles/Timer_Obscure_Behaviour
        pTimer->timerOverflow  = 1;
    }
}

void updateTimerInternalDivCounterValue( GBTimerState* pTimer, const uint16_t internalDivCounter )
{
    const uint16_t newInternalDivCounter = internalDivCounter;
    const uint16_t oldInternalDivCounter = pTimer->internalDivCounter;
    pTimer->internalDivCounter = newInternalDivCounter;
    *pTimer->pDivider = pTimer->internalDivCounter >> 8;

    if( !pTimer->enableCounter )
    {
        return;
    }

    const uint16_t counterFrequencyMask = 1u << pTimer->counterFrequencyBit;
    const uint8_t fallingEdge = ( ( counterFrequencyMask & oldInternalDivCounter ) > 0 ) && 
                                ( ( counterFrequencyMask & newInternalDivCounter ) == 0 );

    if( fallingEdge )
    {
        incrementTimerCounter( pTimer );
    }
}

void tickTimerInternalDivCounterForCycles( GBTimerState* pTimerState, uint8_t cycleCount )
{
    const uint16_t oldInternalDivCounter = pTimerState->internalDivCounter;
    const uint16_t newInternalDivCounter = oldInternalDivCounter + cycleCount;
    pTimerState->internalDivCounter = newInternalDivCounter;
    *pTimerState->pDivider = newInternalDivCounter >> 8;

    if( !pTimerState->enableCounter )
    {
        return;
    }

    const uint16_t counterFrequencyMask = 1u << pTimerState->counterFrequencyBit;

    //FK: check for falling edge for each cycle since the timer can be incremented multiple times per instruction
    uint16_t currentInternalDivCounter  = oldInternalDivCounter;
    uint16_t nextInternalDivCounter     = currentInternalDivCounter + 1;
    while( cycleCount > 0 )
    {
        const uint8_t fallingEdge = ( ( counterFrequencyMask & currentInternalDivCounter ) > 0 ) && 
                                    ( ( counterFrequencyMask & nextInternalDivCounter ) == 0 );

        currentInternalDivCounter = nextInternalDivCounter++;

        if( fallingEdge )
        {
            incrementTimerCounter( pTimerState );
        }

        --cycleCount;
    }
}

void updateTimer( GBCpuState* pCpuState, GBTimerState* pTimer, const uint8_t cycleCount )
{
    pTimer->timerLoading = 0;
    if( pTimer->timerOverflow )
    {
        triggerInterrupt( pCpuState, TimerInterrupt );
        pTimer->timerOverflow = 0;

        *pTimer->pCounter = *pTimer->pModulo;
        pTimer->timerLoading = 1;
    }

    if( pCpuState->flags.stop )
    {
        return;
    }

    tickTimerInternalDivCounterForCycles( pTimer, cycleCount );
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
    if( !pPpuState->pLcdControl->enable )
    {
        return;
    }

    GBLcdStatus* pLcdStatus = pPpuState->lcdRegisters.pStatus;
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

            //FK: change between index 0 and 1
            pPpuState->activeFrameBufferIndex = !pPpuState->activeFrameBufferIndex;
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

uint8_t convertOutputLevelToVolumeShift( const uint8_t outputLevel )
{
    switch( outputLevel )
    {
        case 0b00:
            return 4u;
        case 0b01:
            return 0;
        case 0b10:
            return 1;
        case 0b11:
            return 2;
    }

    IllegalCodePath();
    return 0;
}

void updateAPU( GBApuState* pApuState, const uint8_t cycleCost )
{
    pApuState->frameSequencer.cycleCounter += cycleCost;
    if( pApuState->frameSequencer.cycleCounter >= 512 )
    {
        pApuState->frameSequencer.cycleCounter -= 512;

        const uint8_t clockLengthCounter    = ( pApuState->frameSequencer.clockCounter % 2 );
        const uint8_t clockVolumeEnvelop    = ( pApuState->frameSequencer.clockCounter == 7 );
        const uint8_t clockSweep            = ( pApuState->frameSequencer.clockCounter == 2 || pApuState->frameSequencer.clockCounter == 6 );

        const uint8_t clockCounter = pApuState->frameSequencer.clockCounter + 1;
        pApuState->frameSequencer.clockCounter = clockCounter % 8;

        if( clockLengthCounter )
        {
            if( pApuState->squareWaveChannel1.lengthTimer > 0 )
            {
                --pApuState->squareWaveChannel1.lengthTimer;
            }

            if( pApuState->squareWaveChannel2.lengthTimer > 0 )
            {
                --pApuState->squareWaveChannel2.lengthTimer;
            }

            if( pApuState->waveChannel.lengthTimer > 0 )
            {
                --pApuState->waveChannel.lengthTimer;
            }

            if( pApuState->noiseChannel.lengthTimer > 0 )
            {
                --pApuState->noiseChannel.lengthTimer;
            }
        }
    }

    const uint8_t waveChannelLenghtTime = pApuState->waveChannel.lengthTimer;
    if( waveChannelLenghtTime > 0u )
    {
        const uint8_t waveSampleVolumeShift     = pApuState->waveChannel.volumeShift;
        const uint16_t cycleCountTarget         = pApuState->waveChannel.frequencyCycleCountTarget;
        uint16_t samplePosition                 = pApuState->waveChannel.samplePosition;
        uint16_t cycleCount                     = pApuState->waveChannel.cycleCount;

        cycleCount += cycleCost;
        if( cycleCount > cycleCountTarget )
        {
            cycleCount -= cycleCountTarget;
            ++samplePosition %= 64u;

            const uint8_t* pWaveSample = pApuState->waveChannel.pWavePattern + ( samplePosition << 2 );
            uint8_t nextWaveSample = *pWaveSample;
            nextWaveSample >>= ( samplePosition % 2 ) * 4 + waveSampleVolumeShift;
            nextWaveSample &= 0xF;

            pApuState->waveChannel.currentSample  = nextWaveSample;
            pApuState->waveChannel.samplePosition = samplePosition;
        }
    }
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
                    if( interruptFlag == TimerInterrupt )
                    {
                        BreakPointHook();
                    }
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

    IllegalCodePath();
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

    IllegalCodePath();
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

    IllegalCodePath();
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

    IllegalCodePath();
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
            DebugBreak();
#endif
            break;
        }
        default:
            //FK: opcode not implemented
#if K15_BREAK_ON_UNKNOWN_INSTRUCTION == 1
            DebugBreak();
#endif
    }

    return pOpcode->cycleCosts[ opcodeCondition ];
}

void updateDmaState( GBCpuState* pCpuState, GBMemoryMapper* pMemoryMapper, uint8_t cycleCostOfLastOpCode )
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

uint8_t handleInput( const uint8_t queryJoypadValue, GBEmulatorJoypadState joyPadState )
{
    joyPadState = fixJoypadState( joyPadState );

    //FK: bit 7&6 aren't used so we'll set them (mimicing bgb)
    const uint8_t selectActionButtons       = ( queryJoypadValue & (1 << 5) ) == 0;
    const uint8_t selectDirectionButtons    = ( queryJoypadValue & (1 << 4) ) == 0;
    const uint8_t prevJoypadState           = ( queryJoypadValue & 0x0F ); 
    uint8_t newJoypadState                  = 0;
    //FK: Write joypad values to 0xFF00
    if( selectActionButtons )
    {
        //FK: Flip button mask since in our world bit set = input pressed. It's reversed in the gameboy world, though
        newJoypadState = ~joyPadState.actionButtonMask & 0x0F;
    }
    else if( selectDirectionButtons )
    {
        //FK: Flip button mask since in our world bit set = input pressed. It's reversed in the gameboy world, though
        newJoypadState = ~joyPadState.dpadButtonMask & 0x0F;
    }

    const uint8_t registerValue = 0xC0 | ( queryJoypadValue & 0x30 ) | newJoypadState;
    return registerValue;
}

uint8_t isLargeRamCartridge( uint32_t ramSizeInBytes )
{
    return ramSizeInBytes > Kbyte( 8 );
}

uint8_t isLargeRomCartridge( uint32_t romSizeInBytes )
{
    return romSizeInBytes >= Mbyte( 1 );
}

uint8_t fixMBC1Rom0BankNumber( const uint16_t romBankCount, const uint8_t romBankNumber )
{
    return romBankNumber % romBankCount;
}

uint8_t fixMBC1RamBankNumber( const uint8_t ramBankCount, const uint8_t ramBankNumber )
{
    return ramBankNumber % ramBankCount;
}

uint8_t fixMBC1Rom1BankNumber( const uint16_t romBankCount, const uint8_t lowBankNumber, const uint8_t highBankNumber )
{
    uint8_t fixedRomBankNumber = ( highBankNumber << 5 );
    fixedRomBankNumber += lowBankNumber;

    if( ( fixedRomBankNumber % 0x20 ) == 0 )
    {
        ++fixedRomBankNumber;
    }

    fixedRomBankNumber %= romBankCount; 
    return fixedRomBankNumber;
}

uint16_t fixMBC5RomBankNumber( const uint16_t romBankCount, const uint8_t lowBankNumber, const uint8_t highBankNumber )
{
    uint16_t fixedRomBankNumber = lowBankNumber | ( highBankNumber << 8 );
    fixedRomBankNumber %= romBankCount;
    return fixedRomBankNumber;
}

uint8_t isMBC1CartridgeType( GBCartridgeType cartridgeType )
{
    switch( cartridgeType )
    {
        case ROM_MBC1:
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:
            return true;

        default:
            return false;
    }

    IllegalCodePath();
    return false;
}

uint8_t isMBC5CartridgeType( GBCartridgeType cartridgeType )
{
    switch( cartridgeType )
    {
        case ROM_MBC5:
        case ROM_MBC5_RAM:
        case ROM_MBC5_RAM_BATT:
        case ROM_MBC5_RUMBLE:     
        case ROM_MBC5_RUMBLE_SRAM:
        case ROM_MBC5_RUMBLE_SRAM_BATT:
            return true;

        default:
            return false;
    }

    IllegalCodePath();
    return false;
}

void handleMBC1Write( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    const uint16_t address  = pMemoryMapper->lastAddressWrittenTo;
    const uint8_t value     = pMemoryMapper->lastValueWritten;
    
    if( isInMBC1RomBankNumberRegisterRange( address ) )
    {
        pCartridge->lowBankValue = value & 0x1F;
        const uint8_t romBankNumber = fixMBC1Rom1BankNumber( pCartridge->romBankCount, pCartridge->lowBankValue, pCartridge->highBankValue );
        mapCartridgeRom1Bank( pCartridge, pMemoryMapper, romBankNumber );
    }
    else if( isInMBC1RamBankNumberRegisterRange( address ) )
    {
        pCartridge->highBankValue = value & 0x3;
        
        const uint8_t rom1BankNumber = fixMBC1Rom1BankNumber( pCartridge->romBankCount, pCartridge->lowBankValue, pCartridge->highBankValue );
        mapCartridgeRom1Bank( pCartridge, pMemoryMapper, rom1BankNumber );

        if( pCartridge->bankingMode == 1 )
        {
            if( isLargeRomCartridge( pCartridge->romSizeInBytes ) )
            {
                const uint8_t fixedRom0BankNumber = fixMBC1Rom0BankNumber( pCartridge->romBankCount, pCartridge->highBankValue << 5 );
                mapCartridgeRom0Bank( pCartridge, pMemoryMapper, fixedRom0BankNumber );
            }
            else if( isLargeRamCartridge( pCartridge->ramSizeInBytes ) )
            {
                const uint8_t ramBankNumber = fixMBC1RamBankNumber( pCartridge->ramBankCount, pCartridge->highBankValue );
                mapCartridgeRamBank( pCartridge, pMemoryMapper, ramBankNumber );
            }
        }
    }
    else if( isInMBC1BankingModeSelectRegisterRange( address ) )
    {
        const uint8_t bankingMode = ( value & 0x1 );
        //FK: according to Pandocs, ram bank should be mapped once 
        //    bankingmode 1 is selected and the cartridge is a 
        //    large ram cartridge. 
        //  TODO: Check if it is enough to only switch ram bank 
        //        when writing to isInMBC1RamBankNumberRegisterRange

        pCartridge->bankingMode = bankingMode;

        if( bankingMode == 0 )
        {
            mapCartridgeRom0Bank( pCartridge, pMemoryMapper, 0u );

            if( pCartridge->ramSizeInBytes > 0u )
            {
                mapCartridgeRamBank( pCartridge, pMemoryMapper, 0u );
            }
        }
        else
        {
            if( isLargeRomCartridge( pCartridge->romSizeInBytes ) )
            {
                const uint16_t romBankNumber = fixMBC1Rom0BankNumber( pCartridge->romBankCount, pCartridge->highBankValue << 5 );
                mapCartridgeRom0Bank( pCartridge, pMemoryMapper, romBankNumber );
            }
            else if( pCartridge->ramEnabled && pCartridge->ramBankCount > 0u )
            {
                const uint8_t ramBankNumber = fixMBC1RamBankNumber( pCartridge->ramBankCount, pCartridge->highBankValue );
                mapCartridgeRamBank( pCartridge, pMemoryMapper, ramBankNumber );
            }
        }
    }
}

void handleMBC5Write( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    const uint16_t address  = pMemoryMapper->lastAddressWrittenTo;
    const uint8_t value     = pMemoryMapper->lastValueWritten;

    uint8_t mapRomBank = 0;
    if( isInMBC5LowRomBankNumberRegisterRange( address ) )
    {
        pCartridge->lowBankValue = value;
        mapRomBank = 1;
    }
    else if( isInMBC5HighRomBankNumberRegisterRange( address ) )
    {
        pCartridge->highBankValue = value & 0x1;
        mapRomBank = 1;
    }
    else if( isInMBC5RamBankNumberRegisterRange( address ) )
    {
        const uint8_t ramBankNumber = value % pCartridge->ramBankCount;
        mapCartridgeRamBank( pCartridge, pMemoryMapper, ramBankNumber );
    }

    if( mapRomBank )
    {
        const uint16_t rom1BankNumber = fixMBC5RomBankNumber( pCartridge->romBankCount, pCartridge->lowBankValue, pCartridge->highBankValue );
        mapCartridgeRom1Bank( pCartridge, pMemoryMapper, rom1BankNumber );
    }
}

void handleMemoryControllerWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    const uint16_t address  = pMemoryMapper->lastAddressWrittenTo;
    const uint8_t value     = pMemoryMapper->lastValueWritten;

    if( isInRamEnableRegisterRange( address ) )
    {
        pCartridge->ramEnabled = ( value & 0xF ) == 0xA;
    }
    else
    {
        if( isMBC1CartridgeType( pCartridge->header.cartridgeType ) )
        {
            handleMBC1Write( pCartridge, pMemoryMapper );
        }
        else if( isMBC5CartridgeType( pCartridge->header.cartridgeType ) )
        {
            handleMBC5Write( pCartridge, pMemoryMapper );
        }
    }
}

void handleCartridgeRomWrite( GBCartridge* pCartridge, GBMemoryMapper* pMemoryMapper )
{
    switch( pCartridge->header.cartridgeType )
    {
        case ROM_MBC1:
        case ROM_MBC1_RAM:
        case ROM_MBC1_RAM_BATT:
        case ROM_MBC5:
        case ROM_MBC5_RAM:
        case ROM_MBC5_RAM_BATT:
        case ROM_MBC5_RUMBLE:     
        case ROM_MBC5_RUMBLE_SRAM:
        case ROM_MBC5_RUMBLE_SRAM_BATT:
            handleMemoryControllerWrite( pCartridge, pMemoryMapper );
            return;
        
        default:
            return;
    }

    IllegalCodePath();
    return;
}

void setGBEmulatorJoypadState( GBEmulatorInstance* pEmulatorInstance, GBEmulatorJoypadState joypadState )
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

void handleCartridgeWrites( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper   = pEmulatorInstance->pMemoryMapper;
    GBCartridge* pCartridge         = pEmulatorInstance->pCartridge;

    if( isInCartridgeRomAddressRange( pMemoryMapper->lastAddressWrittenTo ) )
    {
        handleCartridgeRomWrite( pCartridge, pMemoryMapper );
    }

    if( isInExternalRamRange( pMemoryMapper->lastAddressWrittenTo ) && pCartridge->ramEnabled )
    {
        //FK: mirror writes to ram area to emulator ram memory
        const uint16_t baseAddressOffset = ( pMemoryMapper->lastAddressWrittenTo - 0xA000 ) + pCartridge->mappedRamBankNumber * gbRamBankSizeInBytes;
        if( baseAddressOffset < pCartridge->ramSizeInBytes )
        {
            pCartridge->pRamBaseAddress[ baseAddressOffset ] = pMemoryMapper->lastValueWritten;
            pEmulatorInstance->flags.ramAccessed = 1;
        }
    }
}

uint8_t isUnmappedIORegisterAddress( const uint16_t address )
{
    switch( address )
    {
        case 0xFF03:
        case 0xFF08:
        case 0xFF09:
        case 0xFF0A:
        case 0xFF0B:
        case 0xFF0C:
        case 0xFF0D:
        case 0xFF0E:
        case 0xFF15:
        case 0xFF1F:
        case 0xFF27:
        case 0xFF28:
        case 0xFF29:
        {
            return 1;
            break;
        }
    }

    return address >= 0xFF4C && address < 0xFF80;
}

void handleMappedIORegisterWrite( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper   = pEmulatorInstance->pMemoryMapper;
    GBCpuState* pCpuState           = pEmulatorInstance->pCpuState;
    GBPpuState* pPpuState           = pEmulatorInstance->pPpuState;
    GBApuState* pApuState           = pEmulatorInstance->pApuState;
    GBTimerState* pTimerState       = pEmulatorInstance->pTimerState;
    GBSerialState* pSerialState     = pEmulatorInstance->pSerialState;
    GBCartridge* pCartridge         = pEmulatorInstance->pCartridge;

    const uint16_t address = pMemoryMapper->lastAddressWrittenTo;
    if( isUnmappedIORegisterAddress( address ) )
    {
        //FK: Don't allow writes to unmapped IO registers
        return;
    }

    if( pTimerState->timerLoading && address == K15_GB_MAPPED_IO_ADDRESS_TIMA )
    {
        //FK: Don't allow writes to TIMA during timer loading
        return;
    }

    uint8_t memoryValueBitMask      = 0xFF;
    uint8_t newMemoryValue          = pMemoryMapper->lastValueWritten;
    const uint8_t oldMemoryValue    = pMemoryMapper->pBaseAddress[ address ];

    switch( address )
    {
        case K15_GB_MAPPED_IO_ADDRESS_JOYP:
        {
            newMemoryValue = handleInput( newMemoryValue, pEmulatorInstance->joypadState );
            triggerInterrupt( pCpuState, JoypadInterrupt );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_SC:
        {
            memoryValueBitMask = 0b10000001;
            pSerialState->transferInProgress = ( newMemoryValue & 0x80 ) > 0u;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_DIV:
        {
            newMemoryValue = 0x00;
            updateTimerInternalDivCounterValue( pTimerState, 0u );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_TIMA:
        {
            if( pTimerState->timerLoading )
            {
                return;
            }

            //FK: Reset overflow flag if TIMA gets written directly after an overflow
            //    This will effectively prevent the timer interrupt flag from being set
            pTimerState->timerOverflow = 0;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_TMA:
        {
            if( pTimerState->timerLoading )
            {
                //FK: If you write to TIMA during the cycle that TMA is being loaded to it, 
                //    the write will be ignored and TMA value will be written to TIMA instead.
                *pTimerState->pCounter = newMemoryValue;
                return;
            }
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_TAC:
        {
            memoryValueBitMask = 0b00000111;

            const uint8_t timerControlValue = newMemoryValue & memoryValueBitMask;
            pTimerState->enableCounter = ( timerControlValue & 0x4 ) > 0;
            
            const uint16_t oldCounterFrequencyBit = pTimerState->counterFrequencyBit;
            const uint16_t newCounterFrequencyBit = convertTimerControlFrequencyBit( timerControlValue );
            pTimerState->counterFrequencyBit = newCounterFrequencyBit;

            if( !pTimerState->enableCounter )
            {
                break;
            }

            //FK: Check for falling edge with new frequency - a falling edge results in a timer increment!
            const uint8_t fallingEdge = ( ( ( 1 << oldCounterFrequencyBit ) & pTimerState->internalDivCounter ) > 0 ) && 
                                        ( ( ( 1 << oldCounterFrequencyBit ) & pTimerState->internalDivCounter ) == 0 );

            if( fallingEdge )
            {
                incrementTimerCounter( pTimerState );
            }
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR10:
        {
            memoryValueBitMask = 0b01111111;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR14:
        case K15_GB_MAPPED_IO_ADDRESS_NR24:
        {
            memoryValueBitMask = 0b11000111;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR30:
        {
            memoryValueBitMask = 0b10000000;
            pApuState->waveChannel.channelEnabled = newMemoryValue & 0x80;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR31:
        {
            pApuState->waveChannel.lengthTimer = newMemoryValue & 0x1F;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR32:
        {
            memoryValueBitMask = 0b01100000;
            const uint8_t outputLevel = ( newMemoryValue >> 4 ) & 0x3;
            pApuState->waveChannel.volumeShift = convertOutputLevelToVolumeShift( outputLevel );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR33:
        {
            //FK: Clear and set lower 8 bits of frequency
            pApuState->waveChannel.frequencyCycleCountTarget &= 0xFF;
            pApuState->waveChannel.frequencyCycleCountTarget |= newMemoryValue;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR34:
        {
            memoryValueBitMask = 0b1100111;

            //FK: clear an set higher 3 bits of frequency
            pApuState->waveChannel.frequencyCycleCountTarget &= 0x700;
            pApuState->waveChannel.frequencyCycleCountTarget |= ( newMemoryValue & 0x3 ) << 8;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR41:
        {
            memoryValueBitMask = 0b00111111;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR42:
        {
            memoryValueBitMask = 0b11000000;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_NR52:
        {
            memoryValueBitMask = 0b10001111;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_LCDC:
        {
            GBLcdControl lcdControlValue;
            memcpy(&lcdControlValue, &newMemoryValue, sizeof(GBLcdControl) );
            updatePPULcdControl( pPpuState, lcdControlValue );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_STAT:
        {
            //FK: LCD Status - only bit 3:6 are writeable
            memoryValueBitMask = 0x78;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_DMA:
        {
            //FK: Cpu can only write to HRAM during DMA transfer
            pCpuState->flags.dma = 1;
            pCpuState->dmaAddress = ( newMemoryValue << 8 );
            //FK: Count up to 160 cycles for the dma flag to be reset
            pCpuState->dmaCycleCounter = 0;
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_BGP:
        {
            extractMonochromePaletteFrom8BitValue( pPpuState->backgroundMonochromePalette, newMemoryValue );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_OBP0:
        case K15_GB_MAPPED_IO_ADDRESS_OBP1:
        {
            const uint8_t paletteOffset = ( address - K15_GB_MAPPED_IO_ADDRESS_OBP0 ) * 4;
            extractMonochromePaletteFrom8BitValue( pPpuState->objectMonochromePlatte + paletteOffset, newMemoryValue );
            break;
        }
        case K15_GB_MAPPED_IO_ADDRESS_IF:
        case K15_GB_MAPPED_IO_ADDRESS_IE:
        {
            memoryValueBitMask = 0b00011111;
            break;
        }
    }

    pMemoryMapper->pBaseAddress[ address ] = ( newMemoryValue & memoryValueBitMask ) | ( oldMemoryValue & ~memoryValueBitMask );
}

uint8_t runSingleInstruction( GBEmulatorInstance* pEmulatorInstance )
{
    GBMemoryMapper* pMemoryMapper   = pEmulatorInstance->pMemoryMapper;
    GBCpuState* pCpuState           = pEmulatorInstance->pCpuState;
    GBPpuState* pPpuState           = pEmulatorInstance->pPpuState;
    GBApuState* pApuState           = pEmulatorInstance->pApuState;
    GBTimerState* pTimerState       = pEmulatorInstance->pTimerState;
    GBSerialState* pSerialState     = pEmulatorInstance->pSerialState;
    GBCartridge* pCartridge         = pEmulatorInstance->pCartridge;

    executePendingInterrupts( pCpuState, pMemoryMapper );

    uint8_t cycleCost = 4u; //FK: Default cycle cost when CPU is halted

    if( !pCpuState->flags.halt )
    {
        const uint8_t haltBug = pCpuState->flags.haltBug;
        pCpuState->flags.haltBug = 0;

        //FK: Don't increment PC if entered halt bug state as part of the halt bug emulation
        const uint16_t opcodeAddress    = haltBug ? pCpuState->registers.PC : pCpuState->registers.PC++;
        const uint8_t opcode            = read8BitValueFromMappedMemory( pMemoryMapper, opcodeAddress );
        const uint8_t opcodeByteCount   = opcode == 0xCB ? cbPrefixedOpcodes[opcode].byteCount : unprefixedOpcodes[opcode].byteCount;

        addOpcodeToOpcodeHistory( pEmulatorInstance, opcodeAddress, opcode );
        cycleCost = executeInstruction( pCpuState, pMemoryMapper, opcode );
    }

    if( pMemoryMapper->memoryAccess == GBMemoryAccess_Written )
    {
        if( isInIORegisterRange( pMemoryMapper->lastAddressWrittenTo ) )
        {
            handleMappedIORegisterWrite( pEmulatorInstance );
        }
        else
        {
            handleCartridgeWrites( pEmulatorInstance );
        }
    }

    updateDmaState( pCpuState, pMemoryMapper, cycleCost ); 
    updatePPU( pCpuState, pPpuState, cycleCost );
    updateAPU( pApuState, cycleCost );
    updateTimer( pCpuState, pTimerState, cycleCost );
    updateSerial( pCpuState, pSerialState, cycleCost );

    pMemoryMapper->memoryAccess         = GBMemoryAccess_None;
    pMemoryMapper->lcdStatus            = *pPpuState->lcdRegisters.pStatus;
    pMemoryMapper->lcdEnabled           = pPpuState->pLcdControl->enable;
    pMemoryMapper->dmaActive            = pCpuState->flags.dma;
    pMemoryMapper->ramEnabled           = pEmulatorInstance->pCartridge->ramEnabled;

    return cycleCost;
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

const uint8_t* getGBEmulatorFrameBuffer( GBEmulatorInstance* pInstance )
{
    const uint8_t backBufferIndex = !pInstance->pPpuState->activeFrameBufferIndex;
    return pInstance->pPpuState->pGBFrameBuffers[ backBufferIndex ];
}

GBEmulatorInstanceEventMask runGBEmulatorForCycles( GBEmulatorInstance* pInstance, uint32_t cycleCountToRunFor )
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
        return K15_GB_NO_EVENT_FLAG;
    }
#endif

    uint32_t localCycleCounter = 0u;
    while( localCycleCounter < cycleCountToRunFor )
    {
        const uint8_t cycleCount = runSingleInstruction( pInstance );
        localCycleCounter += cycleCount;

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
        if( pInstance->debug.pauseAtBreakpoint && pInstance->debug.breakpointAddress == pInstance->pCpuState->registers.PC )
        {
            pInstance->debug.pauseExecution = 1;
            return K15_GB_NO_EVENT_FLAG;
        }
#endif
    }

    pCpuState->cycleCounter += localCycleCounter;
    if( pPpuState->cycleCounter >= gbCyclesPerFrame )
    {
        pInstance->flags.vblank = 1;
        pPpuState->cycleCounter -= gbCyclesPerFrame;
        pCpuState->cycleCounter -= gbCyclesPerFrame;
    }
    else
    {
        BreakPointHook();
    }

    return pInstance->flags.vblank == 1 ? K15_GB_VBLANK_EVENT_FLAG : K15_GB_NO_EVENT_FLAG;
}
