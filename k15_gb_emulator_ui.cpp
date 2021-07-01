#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"

constexpr ImGuiInputTextFlags hexTextInputFlags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;

constexpr uint8_t vramVerticalResolutionInTiles   = 24;
constexpr uint8_t vramHorizontalResolutionInTiles = 16;

constexpr uint8_t vramVerticalResolutionInPixels    = vramVerticalResolutionInTiles * gbTileResolutionInPixels;
constexpr uint8_t vramHorizontalResolutionInPixels  = vramHorizontalResolutionInTiles * gbTileResolutionInPixels;

constexpr uint8_t gridCountX = vramHorizontalResolutionInTiles;
constexpr uint8_t gridCountY = vramVerticalResolutionInTiles;

constexpr uint8_t vramTextureVerticalResolution   = vramVerticalResolutionInPixels + gridCountY;
constexpr uint8_t vramTextureHorizontalResolution = vramHorizontalResolutionInPixels + gridCountX;
constexpr uint32_t vramTextureSizeInBytes         = vramTextureVerticalResolution * vramTextureHorizontalResolution * 3;

constexpr uint32_t backgroundHorizontalResolution = gbBackgroundTileCount * gbTileResolutionInPixels;
constexpr uint32_t backgroundVerticalResolution   = gbBackgroundTileCount * gbTileResolutionInPixels;
constexpr uint32_t backgroundTextureSizeInBytes   = backgroundHorizontalResolution * backgroundVerticalResolution * 3;

static struct debugViewState
{
    struct
    {
        char memoryReadAddressHexInput[5]   = { '0', '0', '0', '0', 0 };
        char memoryWriteAddressHexInput[5]  = { '0', '0', '0', '0', 0 };
        char programCounterHexInput[5]      = { '0', '0', '0', '0', 0 };
        char opcodeHexInput[3]              = { '0', '0', 0 };
        bool enableBreakAtProgramCounter    = false;
        bool enableBreakAtOpcode            = false;
        bool enableBreakAtMemoryWrite       = false;
        bool enableBreakAtMemoryRead        = false;
    } hostCpu;

    struct
    {
        char programCounterHexInput[5]      = { '0', '0', '0', '0', 0 };
    } gbCpu;

    struct
    {
        bool drawObjects    = true;
        bool drawWindow     = true;
        bool drawBackground = true;
    } gbPpu;

    struct
    {
        uint8_t windowTextureMemory[ backgroundTextureSizeInBytes ];
        uint8_t backgroundTextureMemory[ backgroundTextureSizeInBytes ];
        uint8_t vramTextureMemory[ vramTextureSizeInBytes ];
    } textureMemory;

    GLuint  vramTextureHandle       = 0;
    GLuint  backgroundTextureHandle = 0;
    GLuint  windowTextureHandle     = 0;
} debugViewState;


uint8_t parseStringToHexNibble( const char hexChar )
{
    switch( hexChar )
    {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return 0xA + hexChar - 'A';
    }

    return (hexChar - 0x30);
}

uint8_t parseStringToHex8Bit( const char* pString )
{
    uint8_t value = 0;
    for( size_t charIndex = 0; charIndex < 2; ++charIndex )
    {
        const uint8_t nibble = parseStringToHexNibble(*pString++);
        value |= nibble << ( 4 - charIndex * 4 );

    }
    return value;
}

uint16_t parseStringToHex16Bit( const char* pString )
{
    uint16_t value = 0;
    for( size_t charIndex = 0; charIndex < 4; ++charIndex )
    {
        const uint8_t nibble = parseStringToHexNibble(*pString++);
        value |= nibble << ( 12 - charIndex * 4 );

    }
    return value;
}

void doGbCpuDebugView( GBEmulatorInstance* pEmulatorInstance )
{
    if( !ImGui::Begin( "GB Cpu Debug View" ) )
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Enable breaking the GB CPU");
    ImGui::Separator();
    ImGui::Text("GB CPU currently");
    ImGui::SameLine();
    if( pEmulatorInstance->debug.pauseExecution )
    {
        ImGui::TextColored(ImVec4(0.7f, 0.0f, 0.2f, 1.0f), "halted");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.01f, 0.7f, 0.1f, 1.0f), "running");
    }

    if( ImGui::Button("Reset emulator") )
    {
        resetGBEmulatorInstance(pEmulatorInstance);
    }

    if( ImGui::Button("Pause execution") )
    {
        pauseGBEmulatorInstanceExecution( pEmulatorInstance );
    }

    if( ImGui::Button("Continue execution") )
    {
        continueGBEmulatorInstanceExecution( pEmulatorInstance );
    }

    if( ImGui::Button("Execute next instruction") )
    {
        runGBEmulatorInstanceForOneInstruction( pEmulatorInstance );
    }

    if( ImGui::Button("Execute single frame") )
    {
        runGBEmulatorInstanceForOneFrame( pEmulatorInstance );
    }

    static bool breakAtPCAddress = false;
    ImGui::Checkbox("Break at program counter address", &breakAtPCAddress);
    const uint16_t breakpointAddress = parseStringToHex16Bit( debugViewState.gbCpu.programCounterHexInput );
    setGBEmulatorInstanceBreakpoint( pEmulatorInstance, breakAtPCAddress, breakpointAddress );
    
    ImGui::SameLine();
    ImGui::InputText( "", debugViewState.gbCpu.programCounterHexInput, sizeof( debugViewState.gbCpu.programCounterHexInput ), hexTextInputFlags );

#if 0
    //FK: TODO
    ImGui::Text("Keybindings");
    ImGui::Text("F5         - Continue execution");
    ImGui::Text("SHIFT + F5 - Restart emulation (break on first instruction)");
    ImGui::Text("F10        - Step into");
    ImGui::Separator();
#endif

    ImGui::End();
}

void doCpuStateView(GBEmulatorInstance* pEmulatorInstance)
{
    if( !ImGui::Begin( "Cpu State View" ) )
    {
        ImGui::End();
        return;
    }

    if( !ImGui::BeginTable("Register Table", 2, ImGuiTableFlags_BordersV ) )
    {
        ImGui::EndTable();
        return;
    }

    const GBCpuState* pCpuState = pEmulatorInstance->pCpuState;
    
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::Text("AF");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.AF);      ImGui::TableNextColumn();
    ImGui::TableNextRow();  
    ImGui::TableNextColumn();
    
    ImGui::Text("BC");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.BC);      ImGui::TableNextColumn();
    ImGui::TableNextRow();  
    ImGui::TableNextColumn();

    ImGui::Text("DE");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.DE);      ImGui::TableNextColumn();
    ImGui::TableNextRow();  
    ImGui::TableNextColumn();

    ImGui::Text("HL");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.HL);      ImGui::TableNextColumn();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::Text("SP");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.SP);      ImGui::TableNextColumn();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::Text("PC");                                  ImGui::TableNextColumn();
    ImGui::Text("$%04hx", pCpuState->registers.PC);    ImGui::TableNextColumn();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::EndTable();

    ImGui::Separator();
    ImGui::Text("CPU Flags");
    ImGui::RadioButton("Z", pCpuState->registers.F.Z);
    ImGui::RadioButton("C", pCpuState->registers.F.C);
    ImGui::RadioButton("H", pCpuState->registers.F.H);
    ImGui::RadioButton("N", pCpuState->registers.F.N);

    ImGui::Separator();
    ImGui::Text("CPU Internals");
    ImGui::Text("Cycle Counter");  ImGui::SameLine();
    ImGui::Text("%04x", pCpuState->cycleCounter );

    ImGui::Text("DMA Counter");  ImGui::SameLine();
    ImGui::Text("%04x", pCpuState->dmaCycleCounter );
    ImGui::End();
}

void doPpuStateView(GBEmulatorInstance* pEmulatorInstance)
{
    if( !ImGui::Begin( "Ppu State View" ) )
    {
        ImGui::End();
        return;
    }

    const GBPpuState* pPpuState = pEmulatorInstance->pPpuState;

    if( ImGui::BeginTable("LCD Status Register Table", 2, ImGuiTableFlags_BordersV ) )
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text("Mode");                                                                ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->mode );                          ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();
        
        ImGui::Text("lyc flag");                                                            ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->LycEqLyFlag);                    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LCD Stat HBlank Interrupt");                                           ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->enableMode0HBlankInterrupt);     ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LCD Stat VBlank Interrupt");                                           ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->enableMode1VBlankInterrupt);     ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LCD Stat OAM Interrupt");                                              ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->enableMode2OAMInterrupt);        ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LCD lyc==ly");                                                         ImGui::TableNextColumn();
        ImGui::Text("%d", pPpuState->lcdRegisters.pStatus->enableLycEqLyInterrupt);         ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();
    }

    ImGui::EndTable();
    ImGui::Separator();

    if( ImGui::BeginTable("LCD Register Table", 2, ImGuiTableFlags_BordersV ) )
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text("SCY");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pScy);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("SCX");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pScx);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LY");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pLy);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("LYC");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pLyc);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("WY");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pWy);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();

        ImGui::Text("WX");                                     ImGui::TableNextColumn();
        ImGui::Text("$%02hx", *pPpuState->lcdRegisters.pWx);    ImGui::TableNextColumn();
        ImGui::TableNextRow();  
        ImGui::TableNextColumn();
    }

    ImGui::EndTable();
    ImGui::End();
}

void doMemoryStateView( GBEmulatorInstance* pEmulatorInstance )
{
    static MemoryEditor memoryEditor;
    memoryEditor.ReadOnly           = true;
    memoryEditor.OptGreyOutZeroes   = false;
    if( !ImGui::Begin( "Memory View" ) )
    {
        ImGui::End();
        return;
    }

    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    memoryEditor.DrawContents( pMemoryMapper->pBaseAddress, gbMappedMemorySizeInBytes );
    ImGui::End();
}

void doEmulatorStateSaveLoadView( GBEmulatorInstance* pEmulatorInstance )
{
    if( !ImGui::Begin( "Emulator State Load/Save View" ) )
    {
        ImGui::End();
        return;
    }

    {
        uint32_t saveIndex = ~0;
        ImGui::Text("Save State");
        ImGui::SameLine();
        if( ImGui::Button("1##0") )
        {
            saveIndex = 1;
        }
        ImGui::SameLine();
        if( ImGui::Button("2##0") )
        {
            saveIndex = 2;
        }
        ImGui::SameLine();
        if( ImGui::Button("3##0") )
        {
            saveIndex = 3;
        }

        if( saveIndex != ~0 )
        {
            const size_t stateSaveMemorySizeInBytes = calculateGBEmulatorStateSizeInBytes( pEmulatorInstance );
            uint8_t* pSaveStateMemory = ( uint8_t* )malloc( stateSaveMemorySizeInBytes );
            if( pSaveStateMemory != nullptr )
            {
                storeGBEmulatorInstanceState( pEmulatorInstance, pSaveStateMemory, stateSaveMemorySizeInBytes );

                const GBCartridgeHeader cartridgeHeader = getGBEmulatorInstanceCurrentCartridgeHeader( pEmulatorInstance );
                char gameTitle[16] = {0};
                memcpy(gameTitle, cartridgeHeader.gameTitle, sizeof( cartridgeHeader.gameTitle ) );

                char fileNameBuffer[64];
                sprintf( fileNameBuffer, "%s_%d.k15_gb_state.bin", gameTitle, saveIndex );

                FILE* pStateFileHandle = fopen(fileNameBuffer, "wb");
                if( pStateFileHandle != nullptr )
                {
                    fwrite( pSaveStateMemory, 1, stateSaveMemorySizeInBytes, pStateFileHandle );
                    fclose( pStateFileHandle );
                }

                free( pSaveStateMemory );
            }
        }
    }

    {
        uint32_t loadIndex = ~0;
        ImGui::Text("Load State");
        ImGui::SameLine();
        if( ImGui::Button("1##1") )
        {
            loadIndex = 1;
        }
        ImGui::SameLine();
        if( ImGui::Button("2##1") )
        {
            loadIndex = 2;
        }
        ImGui::SameLine();
        if( ImGui::Button("3##1") )
        {
            loadIndex = 3;
        }

        if( loadIndex != ~0 )
        {
            const GBCartridgeHeader cartridgeHeader = getGBEmulatorInstanceCurrentCartridgeHeader( pEmulatorInstance );

            char gameTitle[16] = {0};
            memcpy(gameTitle, cartridgeHeader.gameTitle, sizeof( cartridgeHeader.gameTitle ) );

            char fileNameBuffer[64];
            sprintf( fileNameBuffer, "%s_%d.k15_gb_state.bin", gameTitle, loadIndex );

            size_t fileSizeInBytes = 0;
            FILE* pStateFileHandle = fopen(fileNameBuffer, "rb");
            if( pStateFileHandle != nullptr )
            {
                //FK: File map would be better...
                fseek( pStateFileHandle, 0, SEEK_END );
                fileSizeInBytes = ftell( pStateFileHandle );
                fseek( pStateFileHandle, 0, SEEK_SET );

                uint8_t* pStateContent = (uint8_t*)malloc( fileSizeInBytes );
                if( pStateContent != nullptr )
                {
                    fread( pStateContent, 1, fileSizeInBytes, pStateFileHandle );
                    fclose( pStateFileHandle );

                    loadGBEmulatorInstanceState( pEmulatorInstance, pStateContent );
                    free( pStateContent );
                }
            }
        }
    }

    ImGui::End();
}

void doJoystickView( GBEmulatorInstance* pEmulatorInstance )
{
    if( !ImGui::Begin( "Joypad View" ) )
    {
        ImGui::End();
        return;
    }

    ImGui::End();
}

void doInstructionView( GBEmulatorInstance* pEmulatorInstance )
{
    const uint8_t* pBytes = pEmulatorInstance->pMemoryMapper->pBaseAddress;

    if( !ImGui::Begin( "Instruction View" ) )
    {
        ImGui::End();
        return;
    }

    if( !ImGui::BeginTable( "Opcode Table", 6, ImGuiTableFlags_BordersH ) )
    {
        ImGui::EndTable();
        return;
    }

    ImGui::TableSetupColumn( "Executed",     ImGuiTableColumnFlags_WidthFixed, 8.0f );
    ImGui::TableSetupColumn( "Breakpoint",   ImGuiTableColumnFlags_WidthFixed, 8.0f );
    ImGui::TableSetupColumn( "Address",      ImGuiTableColumnFlags_WidthFixed, 40.0f );
    ImGui::TableSetupColumn( "Opcode",       ImGuiTableColumnFlags_WidthFixed, 100.0f );
    ImGui::TableSetupColumn( "Bytes",        ImGuiTableColumnFlags_WidthFixed, 100.0f );
    ImGui::TableSetupColumn( "Cycle Count",  ImGuiTableColumnFlags_WidthFixed, 40.0f );

    GBCpuState* pCpuState = pEmulatorInstance->pCpuState;

    ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + pCpuState->registers.PC * ImGui::GetTextLineHeight());

    ImGuiListClipper clipper;
    clipper.Begin( 0xFFFF );

    while( clipper.Step() )
    {
        for (int byteIndex = clipper.DisplayStart; byteIndex < clipper.DisplayEnd; )
        {
            const uint8_t opcode = pBytes[byteIndex];
            const GBOpcode* pOpcode = opcode == 0xCB ? cbPrefixedOpcodes + opcode : unprefixedOpcodes + opcode;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if( byteIndex == pCpuState->registers.PC )
            {
                ImGui::Text(">");
            }
            ImGui::TableNextColumn();

#if 0
//FK: TODO
            if( debugViewState.breakpointSet )
            {
                if( debugViewState.breakpointAddress == row )
                {
                    ImGui::Text("x");
                }
            }
#endif
            ImGui::TableNextColumn();

            ImGui::Text("$%04hx", byteIndex);
            ImGui::TableNextColumn();

            ImGui::Text(pOpcode->pMnemonic);
            ImGui::TableNextColumn();

            for( size_t opcodeByteIndex = 0; opcodeByteIndex < pOpcode->byteCount; ++opcodeByteIndex )
            {
                ImGui::Text("$%02hhx", pBytes[byteIndex++]);
                ImGui::SameLine();
            }
            ImGui::TableNextColumn();

            if( pOpcode->cycleCosts[1] == 0)
            {
                ImGui::Text("%d", pOpcode->cycleCosts[0]);
            }
            else
            {
                ImGui::Text("%d/%d", pOpcode->cycleCosts[0], pOpcode->cycleCosts[1]);
            }
            ImGui::TableNextColumn();
        }
    }
    ImGui::EndTable();
    ImGui::End();
}

void doInstructionHistoryView( GBEmulatorInstance* pEmulatorInstance )
{
    if( !ImGui::Begin("Instruction History View") )
    {
        ImGui::End();
        return;
    }

    if( !ImGui::BeginTable("Opcode Table", 5, ImGuiTableFlags_BordersH ) )
    {
        ImGui::EndTable();
        return;
    }

    ImGui::TableSetupColumn("Address",              ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Opcode",               ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Bytes",                ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Cycle Count",          ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("CPU Register State",   ImGuiTableColumnFlags_WidthFixed, 400.0f);

    ImGuiListClipper clipper;
    clipper.Begin( pEmulatorInstance->debug.opcodeHistorySize );

    while( clipper.Step() )
    {
        for ( int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex )
        {
            const GBOpcodeHistoryElement* pOpcodeHistoryElement = pEmulatorInstance->debug.opcodeHistory + rowIndex;
            const GBOpcode* pOpcode = pOpcodeHistoryElement->opcode == 0xCB ? cbPrefixedOpcodes + pOpcodeHistoryElement->opcode : unprefixedOpcodes + pOpcodeHistoryElement->opcode;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Text("$%04hx", pOpcodeHistoryElement->address);
            ImGui::TableNextColumn();

            ImGui::Text(pOpcode->pMnemonic);
            ImGui::TableNextColumn();

            const uint8_t* pOpcodeBytes = pEmulatorInstance->pMemoryMapper->pBaseAddress + pOpcodeHistoryElement->address;
            for( size_t opcodeByteIndex = 0; opcodeByteIndex < pOpcode->byteCount; ++opcodeByteIndex )
            {
                ImGui::Text("$%02hhx", pOpcodeBytes[opcodeByteIndex++]);
                ImGui::SameLine();
            }
            ImGui::TableNextColumn();

            if( pOpcode->cycleCosts[1] == 0)
            {
                ImGui::Text("%d", pOpcode->cycleCosts[0]);
            }
            else
            {
                ImGui::Text("%d/%d", pOpcode->cycleCosts[0], pOpcode->cycleCosts[1]);
            }
            ImGui::TableNextColumn();

            ImGui::Text("AF: %04hx BC: %04hx DE: %04hx HL: %04hx SP: %04hx", 
                pOpcodeHistoryElement->registers.AF,
                pOpcodeHistoryElement->registers.BC,
                pOpcodeHistoryElement->registers.DE,
                pOpcodeHistoryElement->registers.HL,
                pOpcodeHistoryElement->registers.SP);

            ImGui::TableNextColumn();
        }
    }
    ImGui::EndTable();
    ImGui::End();
}

void doEmulatorInstructionView()
{
    if( !ImGui::Begin("Emulator UX Instruction") )
    {
        ImGui::End();
        return;
    }

    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("Toggle overlay UI with F1");
    ImGui::Separator();
    ImGui::Text("K15 GameBoy Emulator by FelixK15");
    ImGui::Text("This program tries to emulate the hardware of a Nintendo GameBoy/Nintendo GameBoy Color.");
    ImGui::Text("Current state of the project can be seen over at https://github.com/FelixK15/k15_gameboy_emulator");

    ImGui::End();
}

#if 0
void updateWindowTexture( GBEmulatorInstance* pEmulatorInstance )
{
    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    const GBPpuState* pPpuState = pEmulatorInstance->pPpuState;

    //FK: clear texture with white color
    memset( debugViewState.textureMemory.windowTextureMemory, 0xFF, windowTextureSizeInBytes );

    if( pPpuState->pLcdControl->windowEnable )
    {
        const uint8_t wx = *pPpuState->lcdRegisters.pWx;
        const uint8_t wy = *pPpuState->lcdRegisters.pWy;

        for( uint32_t y = 0; y < backgroundVerticalResolution; ++y )
        {
            if( pPpuState->pLcdControl->windowTileMapArea )
            {   
                const uint8_t* pTileData = pPpuState->pTileBlocks[2];
                pushDebugUiBackgroundTilePixels< int8_t >( pPpuState, pTileData, y );
            }
            else
            {
                const uint8_t* pTileData = pPpuState->pTileBlocks[0];
                pushDebugUiBackgroundTilePixels< uint8_t >( pPpuState, pTileData, y );
            }
        }

        for( uint8_t cy = 0; cy < gbVerticalResolutionInPixels; ++cy )
        {
            const uint8_t y = scy + cy;
            for( uint8_t cx = 0; cx < gbHorizontalResolutionInPixels; ++cx )
            {
                const uint8_t x = scx + cx;
                const uint32_t backgroundPixelIndex = ( x + y * backgroundHorizontalResolution ) * 3;

                if( y == scy || ( y + 1 ) == scyEnd || x == scx || ( x + 1 ) == scxEnd )
                {
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 0 ] = 0xFF;
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 1 ] = 0x00;
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 2 ] = 0x00;
                }
            }
        }
    }

    GLuint backgroundTextureHandle;
    glGenTextures( 1, &backgroundTextureHandle );
    glBindTexture(GL_TEXTURE_2D, backgroundTextureHandle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, backgroundHorizontalResolution, backgroundVerticalResolution, 0, GL_RGB, GL_UNSIGNED_BYTE, debugViewState.textureMemory.backgroundTextureMemory);

    glDeleteTextures( 1, &debugViewState.backgroundTextureHandle );
    debugViewState.backgroundTextureHandle = backgroundTextureHandle;
}
#endif

void updateVRamTexture( GBEmulatorInstance* pEmulatorInstance )
{
    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    const uint8_t* pVRam = pMemoryMapper->pVideoRAM;

    //FK: Fill texture with grid color
    memset( debugViewState.textureMemory.vramTextureMemory, 0xCC, vramTextureSizeInBytes );
    
    for( uint8_t tileY = 0; tileY < vramVerticalResolutionInTiles; ++tileY )
    {
        for( uint8_t tileX = 0; tileX < vramHorizontalResolutionInTiles; ++tileX )
        {
            const uint16_t tileIndex = tileX + tileY * gbTileSizeInBytes;
            const uint8_t* pTileData = pVRam + tileIndex * gbTileSizeInBytes;

            const uint32_t vramPixelX = tileX * gbTileResolutionInPixels + ( tileX + 1 );
            const uint32_t vramPixelY = tileY * gbTileResolutionInPixels + ( tileY + 1 );

            for( uint8_t tilePixelLine = 0; tilePixelLine < gbTileResolutionInPixels; ++tilePixelLine )
            {
                uint8_t pixelBytes[2] = {
                    *pTileData++,
                    *pTileData++
                };

                const uint32_t vramPixelindex = 3 * ( vramPixelX + ( vramPixelY + tilePixelLine ) * vramTextureHorizontalResolution );

                uint8_t* pVramTextureContentRGBR = debugViewState.textureMemory.vramTextureMemory + vramPixelindex;
                for( uint8_t pixelIndex = 0; pixelIndex < 8; ++pixelIndex )
                {
                    const uint8_t bitIndex = (7 - pixelIndex);
                    const uint8_t pixelLSB = ( pixelBytes[0] >> bitIndex ) & 0x1;
                    const uint8_t pixelMSB = ( pixelBytes[1] >> bitIndex ) & 0x1;
                    const uint8_t pixelValue = pixelMSB << 1 | pixelLSB;

                    const float intensity = 1.0f - (float)pixelValue * (1.0f/3.0f);
                    
                    *pVramTextureContentRGBR++ = (uint8_t)(intensity * 255.f);
                    *pVramTextureContentRGBR++ = (uint8_t)(intensity * 255.f);
                    *pVramTextureContentRGBR++ = (uint8_t)(intensity * 255.f);
                }
            }
        }
    }

    GLuint vramTextureHandle;
    glGenTextures( 1, &vramTextureHandle );
    glBindTexture(GL_TEXTURE_2D, vramTextureHandle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, vramTextureHorizontalResolution, vramTextureVerticalResolution, 0, GL_RGB, GL_UNSIGNED_BYTE, debugViewState.textureMemory.vramTextureMemory);

    glDeleteTextures( 1, &debugViewState.vramTextureHandle );
    debugViewState.vramTextureHandle = vramTextureHandle;
}

template< typename IndexType >
void pushDebugUiBackgroundTilePixels( const GBPpuState* pPpuState, const uint8_t* pTileData, uint8_t scanlineYCoordinate )
{
    const IndexType* pWindowTileIds      = (IndexType*)pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->windowTileMapArea ];
    const IndexType* pBackgroundTileIds  = (IndexType*)pPpuState->pBackgroundOrWindowTileIds[ pPpuState->pLcdControl->bgTileMapArea ];

    //FK: Calculate the tile row that intersects with the current scanline
    //FK: TODO: implement sx/sy here
    const uint8_t scanlineYCoordinateInTileSpace = scanlineYCoordinate / gbTileResolutionInPixels;
    const uint16_t startTileIndex = scanlineYCoordinateInTileSpace * gbBackgroundTileCount;
    const uint16_t endTileIndex = startTileIndex + gbBackgroundTileCount;

    int8_t scanlineBackgroundTilesIds[ gbBackgroundTileCount ] = { 0 };

    for( uint16_t tileIndex = startTileIndex; tileIndex < endTileIndex; ++tileIndex )
    {
        //FK: get tile from background tile map
        scanlineBackgroundTilesIds[ tileIndex - startTileIndex ] = pBackgroundTileIds[ tileIndex ];
    }

    //FK: Get the y position of the top most pixel inside the tile row that the current scanline is in
    const uint8_t tileRowStartYPos  = startTileIndex / gbBackgroundTileCount * gbTileResolutionInPixels;

    //FK: Get the y offset inside the tile row to where the scanline currently is.
    const uint8_t tileRowYOffset    = scanlineYCoordinate - tileRowStartYPos;

    for( uint8_t tileIdIndex = 0; tileIdIndex < gbBackgroundTileCount; ++tileIdIndex )
    {
        const IndexType tileIndex = scanlineBackgroundTilesIds[ tileIdIndex ];
        
        //FK: Get pixel data of top most pixel line of current tile
        const uint8_t* pTileTopPixelData = pTileData + tileIndex * gbTileSizeInBytes;
        const uint8_t* pTileScanlinePixelData = pTileTopPixelData + ( scanlineYCoordinate - scanlineYCoordinateInTileSpace * 8 ) * 2;
        //FK: Get pixel data of this tile for the current scanline
        const uint8_t pixelDataLSB = pTileScanlinePixelData[0];
        const uint8_t pixelDataMSB = pTileScanlinePixelData[1];

        //FK: Pixel will be written interleaved here
        uint8_t interleavedScanlinePixelData[2] = {0};
        for( uint8_t scanlinePixelDataIndex = 0; scanlinePixelDataIndex < 2; ++scanlinePixelDataIndex )
        {
            const uint8_t startPixelIndex = scanlinePixelDataIndex * gbHalfTileResolutionInPixels;
            const uint8_t endPixelIndex = startPixelIndex + gbHalfTileResolutionInPixels;
            for( uint8_t pixelIndex = startPixelIndex; pixelIndex < endPixelIndex; ++pixelIndex )
            {
                const uint8_t scanlinePixelShift          = 6 - (pixelIndex-startPixelIndex)*2;
                const uint8_t colorIdShift                = 7 - pixelIndex;
                const uint8_t colorIdMSB                  = ( pixelDataMSB >> colorIdShift ) & 0x1;
                const uint8_t colorIdLSB                  = ( pixelDataLSB >> colorIdShift ) & 0x1;
                const uint8_t colorIdData                 = colorIdMSB << 1 | colorIdLSB << 0;
                const uint8_t pixelData                   = pPpuState->backgroundMonochromePalette[ colorIdData ];
                interleavedScanlinePixelData[ scanlinePixelDataIndex ] |= ( pixelData << scanlinePixelShift );
            }
        }

        const uint32_t backgroundX = tileIdIndex * gbTileResolutionInPixels;
        const uint32_t backgroundY = scanlineYCoordinate;
        const uint32_t backgroundPixelIndex = ( backgroundX + backgroundY * backgroundHorizontalResolution ) * 3;
        uint8_t* pBackgroundPixelData = debugViewState.textureMemory.backgroundTextureMemory + backgroundPixelIndex;

        for( size_t pixelIndex = 0; pixelIndex < 2; ++pixelIndex )
        {
            for( size_t pixelShift = 0; pixelShift < 8; pixelShift += 2)
            {
                const uint8_t pixelMonochromeColor = ( ( interleavedScanlinePixelData[pixelIndex] >> ( 6 - pixelShift ) ) & 0x3 );
                const float intensity = 1.0f - ( float )pixelMonochromeColor * ( 1.0f / 3.0f );
                *pBackgroundPixelData++ = (uint8_t)( 255.f * intensity );
                *pBackgroundPixelData++ = (uint8_t)( 255.f * intensity );
                *pBackgroundPixelData++ = (uint8_t)( 255.f * intensity );
            }
        }
    }
}

void updateBackgroundTexture( GBEmulatorInstance* pEmulatorInstance )
{
    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    const GBPpuState* pPpuState = pEmulatorInstance->pPpuState;

    //FK: clear texture with white color
    memset( debugViewState.textureMemory.backgroundTextureMemory, 0xFF, backgroundTextureSizeInBytes );

    if( pPpuState->pLcdControl->bgEnable )
    {
        const uint8_t scx = *pPpuState->lcdRegisters.pScx;
        const uint8_t scy = *pPpuState->lcdRegisters.pScy;
        const uint8_t scxEnd = scx + gbHorizontalResolutionInPixels;
        const uint8_t scyEnd = scy + gbVerticalResolutionInPixels;

        for( uint32_t y = 0; y < backgroundVerticalResolution; ++y )
        {
            if( !pPpuState->pLcdControl->bgAndWindowTileDataArea )
            {   
                const uint8_t* pTileData = pPpuState->pTileBlocks[2];
                pushDebugUiBackgroundTilePixels< int8_t >( pPpuState, pTileData, y );
            }
            else
            {
                const uint8_t* pTileData = pPpuState->pTileBlocks[0];
                pushDebugUiBackgroundTilePixels< uint8_t >( pPpuState, pTileData, y );
            }
        }

        for( uint8_t cy = 0; cy < gbVerticalResolutionInPixels; ++cy )
        {
            const uint8_t y = scy + cy;
            for( uint8_t cx = 0; cx < gbHorizontalResolutionInPixels; ++cx )
            {
                const uint8_t x = scx + cx;
                const uint32_t backgroundPixelIndex = ( x + y * backgroundHorizontalResolution ) * 3;

                if( y == scy || ( y + 1 ) == scyEnd || x == scx || ( x + 1 ) == scxEnd )
                {
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 0 ] = 0xFF;
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 1 ] = 0x00;
                    debugViewState.textureMemory.backgroundTextureMemory[ backgroundPixelIndex + 2 ] = 0x00;
                }
            }
        }
    }

    GLuint backgroundTextureHandle;
    glGenTextures( 1, &backgroundTextureHandle );
    glBindTexture(GL_TEXTURE_2D, backgroundTextureHandle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, backgroundHorizontalResolution, backgroundVerticalResolution, 0, GL_RGB, GL_UNSIGNED_BYTE, debugViewState.textureMemory.backgroundTextureMemory);

    glDeleteTextures( 1, &debugViewState.backgroundTextureHandle );
    debugViewState.backgroundTextureHandle = backgroundTextureHandle;
}

void doVRamView( GBEmulatorInstance* pEmulatorInstance )
{
    if( !ImGui::BeginTabBar("VRam View") )
    {
        ImGui::EndTabBar();
        return;
    }

    {
        if( ImGui::BeginTabItem( "Background" ) )
        {
            updateBackgroundTexture( pEmulatorInstance );
            const size_t textureHandle = ( size_t )debugViewState.backgroundTextureHandle;
            ImGui::Image( (ImTextureID)textureHandle, ImVec2( backgroundHorizontalResolution, backgroundVerticalResolution ) );
            ImGui::EndTabItem();
        }
    }

    {
        if( ImGui::BeginTabItem( "VRAM" ) )
        {
            updateVRamTexture( pEmulatorInstance );
            const size_t textureHandle = ( size_t )debugViewState.vramTextureHandle;
            ImGui::Image( (ImTextureID)textureHandle, ImVec2( vramTextureHorizontalResolution, vramTextureVerticalResolution) );
            ImGui::EndTabItem();
        }
    }

#if 0
    {
        if( ImGui::BeginTabItem( "Window" ) )
        {
            updateWindowTexture( pEmulatorInstance );
            const size_t textureHandle = ( size_t )debugViewState.windowTextureHandle;
            ImGui::Image( (ImTextureID)textureHandle, ImVec2( gbHorizontalWindowResolutionInPixels, gbVerticalWindowResolutionInPixels ) );
            ImGui::EndTabItem();
        }

        ImGui::EndTabItem();
    }
#endif
   
    ImGui::EndTabBar();

    ImGui::Checkbox("Show Objects",     &debugViewState.gbPpu.drawObjects );
    ImGui::Checkbox("Show Background",  &debugViewState.gbPpu.drawBackground );
    ImGui::Checkbox("Show Window",      &debugViewState.gbPpu.drawWindow );

    pEmulatorInstance->pPpuState->flags.drawBackground  = debugViewState.gbPpu.drawBackground;
    pEmulatorInstance->pPpuState->flags.drawWindow      = debugViewState.gbPpu.drawWindow;
    pEmulatorInstance->pPpuState->flags.drawObjects     = debugViewState.gbPpu.drawObjects;
}

void doUiFrame( GBEmulatorInstance* pEmulatorInstance )
{
    doGbCpuDebugView( pEmulatorInstance );
    doInstructionView( pEmulatorInstance );
    doInstructionHistoryView( pEmulatorInstance );
    doCpuStateView( pEmulatorInstance );
    doPpuStateView( pEmulatorInstance );
    doMemoryStateView( pEmulatorInstance );
    doEmulatorStateSaveLoadView( pEmulatorInstance );
    doVRamView( pEmulatorInstance );
    doEmulatorInstructionView();

    //FK: TODO
    //doJoystickView( pEmulatorInstance );
}