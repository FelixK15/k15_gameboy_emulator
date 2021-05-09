static struct GBDebugViewState
{
    char memoryReadAddressHexInput[5]   = { '0', '0', '0', '0', 0 };
    char memoryWriteAddressHexInput[5]  = { '0', '0', '0', '0', 0 };
    char programCounterHexInput[5]      = { '0', '0', '0', '0', 0 };
    char opcodeHexInput[3]              = { '0', '0', 0 };
    bool enableBreakAtProgramCounter    = false;
    bool enableBreakAtOpcode            = false;
    bool enableBreakAtMemoryWrite       = false;
    bool enableBreakAtMemoryRead        = false;
} debugViewState;

struct GBUiData
{
    uint8_t resetEmulator : 1;
};

uint8_t parseStringToHexNibble( const char hexChar )
{
    switch( hexChar )
    {
        case 'A':
            return 0xA;
            break;
        case 'B':
            return 0xB;
            break;
        case 'C':
            return 0xC;
            break;
        case 'D':
            return 0xD;
            break;
        case 'E':
            return 0xE;
            break;
        case 'F':
            return 0xF;
            break;
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

void doDebugView(GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData)
{
    constexpr ImGuiInputTextFlags hexTextInputFlags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase;
    ImGui::Begin("Debug View");
    ImGui::SetWindowSize(ImVec2(600.0f, 300.0f));
        const bool resetEmulator = ImGui::Button("Reset emulator");
        ImGui::Checkbox( "Enable break at program counter", &debugViewState.enableBreakAtProgramCounter );
        ImGui::Checkbox( "Enable break at specific opcode", &debugViewState.enableBreakAtOpcode );
        ImGui::Checkbox( "Enable break at memory write to address", &debugViewState.enableBreakAtMemoryWrite );
        ImGui::Checkbox( "Enable break at memory read from address", &debugViewState.enableBreakAtMemoryRead );
        const bool breakAtProgramCounterSet = ImGui::InputText( "Break at program counter (hex)", debugViewState.programCounterHexInput, sizeof( debugViewState.programCounterHexInput ), hexTextInputFlags );
        const bool breakAtOpcodeSet         = ImGui::InputText( "Break at specific opcode (hex)", debugViewState.opcodeHexInput, sizeof( debugViewState.opcodeHexInput ), hexTextInputFlags );
        const bool breakAtMemoryWriteSet    = ImGui::InputText( "Break at memory write to address (hex)", debugViewState.memoryWriteAddressHexInput, sizeof( debugViewState.memoryWriteAddressHexInput ), hexTextInputFlags );
        const bool breakAtMemoryReadSet     = ImGui::InputText( "Break at memory read from address (hex)", debugViewState.memoryReadAddressHexInput, sizeof( debugViewState.memoryReadAddressHexInput ), hexTextInputFlags );
    ImGui::End();

    if( resetEmulator )
    {
        pUiData->resetEmulator = 1;
    }

    if( breakAtProgramCounterSet )
    {
        pEmulatorInstance->debug.breakAtProgramCounter = parseStringToHex16Bit( debugViewState.programCounterHexInput );
    }

    if( breakAtOpcodeSet )
    {
        pEmulatorInstance->debug.breakAtOpcode = parseStringToHex8Bit( debugViewState.opcodeHexInput );
    }

    if( breakAtMemoryWriteSet )
    {
        pEmulatorInstance->debug.breakAtMemoryWriteToAddress = parseStringToHex16Bit( debugViewState.memoryWriteAddressHexInput );
    }

    if( breakAtMemoryReadSet )
    {
        pEmulatorInstance->debug.breakAtMemoryReadFromAddress = parseStringToHex16Bit( debugViewState.memoryReadAddressHexInput );
    }

    pEmulatorInstance->debug.settings.enableBreakAtProgramCounter           = debugViewState.enableBreakAtProgramCounter;
    pEmulatorInstance->debug.settings.enableBreakAtOpcode                   = debugViewState.enableBreakAtOpcode;
    pEmulatorInstance->debug.settings.enableBreakAtMemoryReadFromAddress    = debugViewState.enableBreakAtMemoryRead;
    pEmulatorInstance->debug.settings.enableBreakAtMemoryWriteToAddress     = debugViewState.enableBreakAtMemoryWrite;
}

void doMemoryView(GBEmulatorInstance* pEmulatorInstance)
{
    const uint8_t* pBytes = pEmulatorInstance->pMemoryMapper->pBaseAddress;

    ImGui::Begin("Memory View" );
    while( ( pBytes - pEmulatorInstance->pMemoryMapper->pBaseAddress ) < 0x10000)
    {
        const uint16_t address = (uint16_t)( pBytes - pEmulatorInstance->pMemoryMapper->pBaseAddress );
        ImGui::Text("%.4hx: ", address);

        const GBOpcode* pOpcode = opcodeDisassembly + *pBytes;

        ImGui::SameLine();
        ImGui::Text("%.2hx", *pBytes++);

        for( size_t opcodeArgs = 1; opcodeArgs < pOpcode->arguments; ++opcodeArgs )
        {
            ImGui::SameLine();
            ImGui::Text(" %.2hx", *pBytes++);
        }

        ImGui::SameLine();
        ImGui::Text(pOpcode->pMnemonic);
    }
    ImGui::End();
}

void doUiFrame( GBEmulatorInstance* pEmulatorInstance, GBUiData* pUiData )
{
#if 1
    ImGui::NewFrame();
	doDebugView(pEmulatorInstance, pUiData);
	ImGui::EndFrame();
#else
    //FK: WIP
	ImGui::NewFrame();
	doMemoryView(pEmulatorInstance);
	ImGui::EndFrame();
#endif

}