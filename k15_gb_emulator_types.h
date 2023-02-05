#ifndef K15_GB_EMULATOR_TYPES
#define K15_GB_EMULATOR_TYPES

#include "k15_types.h"

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
    K15_GB_NO_EVENT_FLAG                    = 0x00,
    K15_GB_VBLANK_EVENT_FLAG                = 0x01,
    K15_GB_STATE_SAVED_EVENT_FLAG           = 0x02,
    K15_GB_STATE_LOADED_EVENT_FLAG          = 0x04,
    K15_GB_DEBUGGER_CONNECTED_EVENT_FLAG    = 0x08,
    K15_GB_DEBUGGER_DISCONNECTED_EVENT_FLAG = 0x10
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

#define K15_MAX_MNEMONIC_NAME_LENGTH 64
struct GBEmulatorCpuInstruction
{
    char mnemonic[ K15_MAX_MNEMONIC_NAME_LENGTH ];
    uint16_t address;
    uint8_t opcode;
    uint8_t argumentByteCount;
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
    uint8_t initiateTransfer        = 0u;
    uint8_t useInternalClock        = 0u;
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
    bool8_t             lcdEnabled; //FK: Mirror lcd enabled flag to check whether we can read from VRAM and/or OAM
    bool8_t             dmaActive;  //FK: Mirror dma flag to check whether we can read only from HRAM
    bool8_t             ramEnabled; //FK: Mirror ram enabled flag to check whether we can write to external RAM
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

struct GBEmulatorInstanceFlags
{
    union
    {
        struct 
        {
            uint8_t vblank                  : 1;
            uint8_t ramAccessed             : 1;
            uint8_t debuggerDisconnected    : 1;
            uint8_t debuggerConnected       : 1;
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

    GBRomHeader         header;
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

    intptr_t                debuggerSocket;
    IN_ADDR                 debuggerIpAddress;
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

#endif //K15_GB_EMULATOR_TYPES