import json
import os

opcodesTablePath = "gb_opcodes.json"
if not os.path.exists(opcodesTablePath):
    print("'gb_opcodes.json' not found. File needs to be present for this script to run." )
    os.system("pause")
    exit(-1)

    

opcodesTableFileHandle = open(opcodesTablePath, "r")
opcodesTableIndex = json.load(opcodesTableFileHandle)

opcodesCSourceFileHandle = open("k15_gb_opcodes.h", "w")
opcodesCSourceFileHandle.write("""struct GBOpcode
{
    const char* pMnemonic;
    uint8_t arguments;
};

static constexpr GBOpcode opcodeDisassembly[] = {
""")

counter = 0
for opcodeTable in opcodesTableIndex.items():
    for opcodeTableEntry in opcodeTable[1].items():
        opcodeData = opcodeTableEntry[1]
        mnemonic  = opcodeData["mnemonic"]
        byteCount = opcodeData["bytes"]
        operands  = opcodeData["operands"]

        fullMnemonic = mnemonic
        for operand in operands:
            if operand["immediate"]:
                fullMnemonic += " {}".format(operand["name"])
            else:
                fullMnemonic += " ({})".format(operand["name"])

        sourceCodeLine = ""
        if counter != 0:
            if ( counter % 0xF ) == 0:
                sourceCodeLine += ",\n"
            else:
                sourceCodeLine += ", "

        counter += 1
        sourceCodeLine += "{{ \"{mnemonic}\", {bytes} }}".format(mnemonic=fullMnemonic, bytes=byteCount)
        
        opcodesCSourceFileHandle.writelines(sourceCodeLine)

opcodesCSourceFileHandle.write("};")

