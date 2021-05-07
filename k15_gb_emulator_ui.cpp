struct UiState
{
    bool memoryViewOpen = true;
};

static UiState uiState;

void doMemoryView(GBEmulatorInstance* pEmulatorInstance)
{
    const uint8_t* pBytes = pEmulatorInstance->pMemoryMapper->pBaseAddress;

    ImGui::Begin("Memory View", &uiState.memoryViewOpen, 0);
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

void doUiFrame(GBEmulatorInstance* pEmulatorInstance)
{
	ImGui::NewFrame();
	doMemoryView(pEmulatorInstance);
	ImGui::EndFrame();
}