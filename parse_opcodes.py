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
    uint8_t byteCount;
    uint8_t cycleCosts[2];
};
""")

opcodesUnprefixed = "static constexpr GBOpcode unprefixedOpcodes[] = {\n"
opcodesCBPrefixed = "static constexpr GBOpcode cbPrefixedOpcodes[] = {\n"

for opcodeTable in opcodesTableIndex.items():
    opcodeTableName = opcodeTable[0]
    counter = 0

    for opcodeTableEntry in opcodeTable[1].items():
        opcodeData      = opcodeTableEntry[1]
        mnemonic        = opcodeData["mnemonic"]
        byteCount       = opcodeData["bytes"]
        operands        = opcodeData["operands"]
        cycles          = opcodeData["cycles"]

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

        cycleCostValues = [cycles[0] if len(cycles) == 1 else cycles[1],0 if len(cycles) == 1 else cycles[0]]
        cycleCost = ""
        for cycle in cycleCostValues:
            cycleCost += str(cycle)
            cycleCost += ", "

        cycleCost = cycleCost[:-2] #FK: Remove last ,

        counter += 1
        sourceCodeLine += "\t{{ \"{mnemonic}\", {bytes}, {{ {cycleCost} }} }}".format(mnemonic=fullMnemonic, bytes=byteCount, cycleCost=cycleCost)
        
        if opcodeTableName == "unprefixed":
            opcodesUnprefixed += sourceCodeLine
        else:
            opcodesCBPrefixed += sourceCodeLine

opcodesCSourceFileHandle.writelines(opcodesUnprefixed)
opcodesCSourceFileHandle.write("};\n\n")

opcodesCSourceFileHandle.writelines(opcodesCBPrefixed)
opcodesCSourceFileHandle.write("};")

