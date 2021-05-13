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

struct GBUiData
{
    uint8_t resetEmulator                   : 1;
    uint8_t pauseEmulator                   : 1;
    uint8_t continueEmulator                : 1;
    uint8_t executeOneInstruction           : 1;
    uint8_t runSingleFrame                  : 1;
    uint8_t breakAtProgramCounterAddress    : 1;
    uint16_t breakpointProgramCounterAddress;
};

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

void doHostCpuDebugView(GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData)
{
    if(!ImGui::Begin("Host CPU Debug View"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Enable breaking the Host CPU to verify opcode integration");
    ImGui::Separator();
    ImGui::Checkbox( "Enable break at program counter", &debugViewState.hostCpu.enableBreakAtProgramCounter );
    ImGui::Checkbox( "Enable break at specific opcode", &debugViewState.hostCpu.enableBreakAtOpcode );
    ImGui::Checkbox( "Enable break at memory write to address", &debugViewState.hostCpu.enableBreakAtMemoryWrite );
    ImGui::Checkbox( "Enable break at memory read from address", &debugViewState.hostCpu.enableBreakAtMemoryRead );
    const bool breakAtProgramCounterSet = ImGui::InputText( "Break at program counter (hex)", debugViewState.hostCpu.programCounterHexInput, sizeof( debugViewState.hostCpu.programCounterHexInput ), hexTextInputFlags );
    const bool breakAtOpcodeSet         = ImGui::InputText( "Break at specific opcode (hex)", debugViewState.hostCpu.opcodeHexInput, sizeof( debugViewState.hostCpu.opcodeHexInput ), hexTextInputFlags );
    const bool breakAtMemoryWriteSet    = ImGui::InputText( "Break at memory write to address (hex)", debugViewState.hostCpu.memoryWriteAddressHexInput, sizeof( debugViewState.hostCpu.memoryWriteAddressHexInput ), hexTextInputFlags );
    const bool breakAtMemoryReadSet     = ImGui::InputText( "Break at memory read from address (hex)", debugViewState.hostCpu.memoryReadAddressHexInput, sizeof( debugViewState.hostCpu.memoryReadAddressHexInput ), hexTextInputFlags );
    
    ImGui::End();

#if K15_ENABLE_EMULATOR_DEBUG_FEATURES == 1
    if( breakAtProgramCounterSet )
    {
        pEmulatorInstance->hostDebug.breakAtProgramCounter = parseStringToHex16Bit( debugViewState.hostCpu.programCounterHexInput );
    }

    if( breakAtOpcodeSet )
    {
        pEmulatorInstance->hostDebug.breakAtOpcode = parseStringToHex8Bit( debugViewState.hostCpu.opcodeHexInput );
    }

    if( breakAtMemoryWriteSet )
    {
        pEmulatorInstance->hostDebug.breakAtMemoryWriteToAddress = parseStringToHex16Bit( debugViewState.hostCpu.memoryWriteAddressHexInput );
    }

    if( breakAtMemoryReadSet )
    {
        pEmulatorInstance->hostDebug.breakAtMemoryReadFromAddress = parseStringToHex16Bit( debugViewState.hostCpu.memoryReadAddressHexInput );
    }

    pEmulatorInstance->hostDebug.settings.enableBreakAtProgramCounter           = debugViewState.hostCpu.enableBreakAtProgramCounter;
    pEmulatorInstance->hostDebug.settings.enableBreakAtOpcode                   = debugViewState.hostCpu.enableBreakAtOpcode;
    pEmulatorInstance->hostDebug.settings.enableBreakAtMemoryReadFromAddress    = debugViewState.hostCpu.enableBreakAtMemoryRead;
    pEmulatorInstance->hostDebug.settings.enableBreakAtMemoryWriteToAddress     = debugViewState.hostCpu.enableBreakAtMemoryWrite;
#endif
}

void doGbCpuDebugView(GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData)
{
    if( !ImGui::Begin( "GB Cpu Debug View" ) )
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Enable breaking the GB CPU");
    ImGui::Separator();

    if( ImGui::Button("Reset emulator") )
    {
        pUiData->resetEmulator = 1;
    }

    if( ImGui::Button("Pause execution") )
    {
        pUiData->pauseEmulator = 1;
    }

    if( ImGui::Button("continue execution") )
    {
        pUiData->continueEmulator = 1;
    }

    if( ImGui::Button("execute next instruction") )
    {
        pUiData->executeOneInstruction = 1;
    }

    if( ImGui::Button("execute single frame") )
    {
        pUiData->runSingleFrame = 1;
    }

    if( ImGui::Button("break at program counter address") )
    {
        pUiData->breakAtProgramCounterAddress    = 1;
        pUiData->breakpointProgramCounterAddress = parseStringToHex16Bit( debugViewState.gbCpu.programCounterHexInput )  ;   
    }

    ImGui::InputText( "Break at program counter (hex)", debugViewState.gbCpu.programCounterHexInput, sizeof( debugViewState.gbCpu.programCounterHexInput ), hexTextInputFlags );

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

void doInstructionView(GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData)
{
    static size_t opcodeCount = 0;
    const uint8_t* pBytes = pEmulatorInstance->pMemoryMapper->pBaseAddress;

    if( !ImGui::Begin( "Instruction View" ) )
    {
        ImGui::End();
        return;
    }

    if( !ImGui::BeginTable("Opcode Table", 6, ImGuiTableFlags_BordersH ) )
    {
        ImGui::EndTable();
        return;
    }

    ImGui::TableSetupColumn("Executed",     ImGuiTableColumnFlags_WidthFixed, 8.0f);
    ImGui::TableSetupColumn("Breakpoint",   ImGuiTableColumnFlags_WidthFixed, 8.0f);
    ImGui::TableSetupColumn("Address",      ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Opcode",       ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Bytes",        ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("Cycle Count",  ImGuiTableColumnFlags_WidthFixed, 40.0f);

    GBCpuState* pCpuState = pEmulatorInstance->pCpuState;

    ImGuiListClipper clipper;
    clipper.Begin( opcodeCount == 0 ? 0xFFFF : opcodeCount );

    while( clipper.Step() )
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            const uint8_t opcode = pBytes[row];
            const GBOpcode* pOpcode = opcode == 0xCB ? cbPrefixedOpcodes + opcode : unprefixedOpcodes + opcode;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if( row == pCpuState->programCounter )
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

            ImGui::Text("$%04hx", row);
            ImGui::TableNextColumn();

            ImGui::Text(pOpcode->pMnemonic);
            ImGui::TableNextColumn();

            for( size_t byteIndex = 0; byteIndex < pOpcode->byteCount; ++byteIndex )
            {
                ImGui::Text("$%02hhx", pBytes[row + byteIndex]);
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

void doInstructionView()
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

void doUiFrame( GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData )
{
    //ImGui::ShowDemoWindow();
	doHostCpuDebugView(pEmulatorInstance, pUiData);
    doGbCpuDebugView(pEmulatorInstance, pUiData);
    doInstructionView(pEmulatorInstance, pUiData);
    doCpuStateView(pEmulatorInstance);
    doPpuStateView(pEmulatorInstance);
    doInstructionView();
}