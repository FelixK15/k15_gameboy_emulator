#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"

constexpr ImGuiInputTextFlags hexTextInputFlags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;

static struct GBDebugViewState
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
    if( pEmulatorInstance->gbDebug.pauseExecution )
    {
        ImGui::TextColored(ImVec4(0.7f, 0.0f, 0.2f, 1.0f), "halted");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.01f, 0.7f, 0.1f, 1.0f), "running");
    }

    if( ImGui::Button("Reset emulator") )
    {
        resetEmulatorInstance(pEmulatorInstance);
    }

    if( ImGui::Button("Pause execution") )
    {
        pauseEmulatorExecution( pEmulatorInstance );
    }

    if( ImGui::Button("Continue execution") )
    {
        continueEmulatorExecution( pEmulatorInstance );
    }

    if( ImGui::Button("Execute next instruction") )
    {
        runEmulatorForOneInstruction( pEmulatorInstance );
    }

    if( ImGui::Button("Execute single frame") )
    {
        runEmulatorForOneFrame( pEmulatorInstance );
    }

    static bool breakAtPCAddress = false;
    ImGui::Checkbox("Break at program counter address", &breakAtPCAddress);
    const uint16_t breakpointAddress = parseStringToHex16Bit( debugViewState.gbCpu.programCounterHexInput );
    setEmulatorBreakpoint( pEmulatorInstance, breakAtPCAddress, breakpointAddress );
    
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
    ImGui::Text("$%04hx", pCpuState->programCounter);    ImGui::TableNextColumn();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::EndTable();

    ImGui::Separator();

    ImGui::Text("CPU Flags");
    ImGui::RadioButton("Z", pCpuState->registers.F.Z);
    ImGui::SameLine();
    ImGui::RadioButton("C", pCpuState->registers.F.C);
    
    ImGui::RadioButton("H", pCpuState->registers.F.H);
    ImGui::SameLine();
    ImGui::RadioButton("N", pCpuState->registers.F.N);

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
    }

    const GBMemoryMapper* pMemoryMapper = pEmulatorInstance->pMemoryMapper;
    memoryEditor.DrawContents( pMemoryMapper->pBaseAddress, 0x10000 );
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
            const size_t stateSaveMemorySizeInBytes = calculateEmulatorStateSizeInBytes( pEmulatorInstance );
            uint8_t* pSaveStateMemory = ( uint8_t* )malloc( stateSaveMemorySizeInBytes );
            if( pSaveStateMemory != nullptr )
            {
                storeEmulatorState( pEmulatorInstance, pSaveStateMemory, stateSaveMemorySizeInBytes );

                char fileNameBuffer[64];
                sprintf( fileNameBuffer, "state_%d.k15_gb_state.bin", saveIndex );

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
            char fileNameBuffer[64];
            sprintf( fileNameBuffer, "state_%d.k15_gb_state.bin", loadIndex );

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

                    loadEmulatorState( pEmulatorInstance, pStateContent );
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
    static size_t opcodeCount = 0;
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

    ImGuiListClipper clipper;
    clipper.Begin( opcodeCount == 0 ? 0xFFFF : opcodeCount );

    while( clipper.Step() )
    {
        for (int byteIndex = clipper.DisplayStart; byteIndex < clipper.DisplayEnd; )
        {
            const uint8_t opcode = pBytes[byteIndex];
            const GBOpcode* pOpcode = opcode == 0xCB ? cbPrefixedOpcodes + opcode : unprefixedOpcodes + opcode;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if( byteIndex == pCpuState->programCounter )
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
    clipper.Begin( pEmulatorInstance->gbDebug.opcodeHistorySize );

    while( clipper.Step() )
    {
        for ( int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex )
        {
            const GBOpcodeHistoryElement* pOpcodeHistoryElement = pEmulatorInstance->gbDebug.opcodeHistory + rowIndex;
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

void doUiFrame( GBEmulatorInstance* pEmulatorInstance )
{
    //ImGui::ShowDemoWindow();
    doGbCpuDebugView( pEmulatorInstance );
    doInstructionView( pEmulatorInstance );
    doInstructionHistoryView( pEmulatorInstance );
    doCpuStateView( pEmulatorInstance );
    doPpuStateView( pEmulatorInstance );
    doMemoryStateView( pEmulatorInstance );
    doEmulatorStateSaveLoadView( pEmulatorInstance );
    doEmulatorInstructionView();

    //FK: TODO
    //doJoystickView( pEmulatorInstance );
}