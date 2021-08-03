import argparse
import pathlib
import os
import struct
import types

def checkArguments(args):
    inputFilePath = args.input[0]
    if not os.path.exists(inputFilePath):
        print("Input bmp file '{}' does not exist.".format(inputFilePath))
        return False
        
    return True

def isValidBitmapFile(bitmapFileHandle):
    #FK: Only support windows bmp for now
    bitmapHeaderField = struct.unpack('h', bitmapFileHandle.read(2))[0]
    if bitmapHeaderField != 0x4d42:
        print("ERROR: Not a valid bmp file (must be Microsoft Windows bitmap file format)")
        return False

    #FK: Move to DIB header
    bitmapFileHandle.seek(14, os.SEEK_SET)
    dibHeaderSizeInBytes = struct.unpack('i', bitmapFileHandle.read(4))[0]

    #FK: Only support windows bmp for now
    if dibHeaderSizeInBytes != 40:
        print("ERROR: Not a valid bmp file (must be Microsoft Windows bitmap file format)")
        return False

    #FK: Move to bpp
    bitmapFileHandle.seek(28, os.SEEK_SET)
    bitsPerPixel = struct.unpack('i', bitmapFileHandle.read(4))[0]

    #FK: Only support RGB for now
    if bitsPerPixel != 24:
        print("ERROR: Not a valid bmp file (currently only bit-depth of 24bpp is supported)")
        return False

    compressionMethod = struct.unpack('i', bitmapFileHandle.read(4))[0]

    BI_RGB = 0
    if compressionMethod != BI_RGB:
        print("ERROR: Not a valid bmp file (currently only compression method RGB is supported)")
        return False

    return True

def extractBitmapInformations(bitmapFileHandle):
    #FK: offset to bitmap pixel data
    bitmapFileHandle.seek(10, os.SEEK_SET)
    bitmapPixelDataOffsetInBytes = struct.unpack('i', bitmapFileHandle.read(4))[0]

    #FK: bitmap dimension
    bitmapFileHandle.seek(18, os.SEEK_SET)
    bitmapWidthInPixels = struct.unpack('i', bitmapFileHandle.read(4))[0]
    bitmapHeightInPixels = struct.unpack('i', bitmapFileHandle.read(4))[0]

    return types.SimpleNamespace(pixelDataOffsetInBytes=bitmapPixelDataOffsetInBytes, width=bitmapWidthInPixels, height=bitmapHeightInPixels)

def writeArrayBitmapFile(arrayFileHandle, glyphWidth, glyphHeight, glyphRowCount, bitmapInformations, bitmapFileHandle):
    arrayFileHandle.writelines("static constexpr uint32_t glyphWidthInPixels = {}u;\n".format(glyphWidth))
    arrayFileHandle.writelines("static constexpr uint32_t glyphHeightInPixels = {}u;\n".format(glyphHeight))
    arrayFileHandle.writelines("static constexpr uint32_t glyphRowCount = {}u;\n".format(glyphRowCount))
    arrayFileHandle.writelines("static constexpr uint32_t fontPixelDataWidthInPixels = {}u;\n".format(bitmapInformations.width))
    arrayFileHandle.writelines("static constexpr uint32_t fontPixelDataHeightInPixels = {}u;\n".format(bitmapInformations.height))

    arrayFileHandle.writelines("static constexpr uint8_t fontPixelData[] = {\n")
    arrayFileHandle.write("\t")

    #FK: seek to beginning of pixel data
    bitmapFileHandle.seek(bitmapInformations.pixelDataOffsetInBytes, os.SEEK_SET)

    pixelCount = 0
    totalPixelCount = bitmapInformations.height * bitmapInformations.width
    while pixelCount < totalPixelCount:
        pixelValues = (bitmapFileHandle.read(1), bitmapFileHandle.read(1), bitmapFileHandle.read(1))
        arrayFileHandle.write("0x" + pixelValues[0].hex() + ", ")
        arrayFileHandle.write("0x" + pixelValues[1].hex() + ", ")
        arrayFileHandle.write("0x" + pixelValues[2].hex())
        pixelCount += 1

        if pixelCount != totalPixelCount:
            if pixelCount % 10 == 0:
                arrayFileHandle.write(",\n\t")
            else:
                arrayFileHandle.write(", ")

    arrayFileHandle.write("\n};\n")
    arrayFileHandle.flush()

argumentParser = argparse.ArgumentParser(prog="bitmap_font_to_c_array", description="Converts a given *.bmp font image to a C array (8-bit RGB encoded) for direct use in a C codebase." )
argumentParser.add_argument("-gw", "--glyph_width", nargs=1, type=int, required=True, help="width of glyph in unscaled pixels")
argumentParser.add_argument("-gh", "--glyph_height", nargs=1, type=int, required=True, help="height of glyph in unscaled pixels")
argumentParser.add_argument("-gr", "--glyph_row_count", nargs=1, type=int, required=True, help="amount of glyphs per row")
argumentParser.add_argument("-o", "--output", nargs=1, type=str, required=True, help="Output *.c file")
argumentParser.add_argument("-i", "--input", nargs=1, type=str, required=True, help="Input *.bmp file")

args = argumentParser.parse_args()

if not checkArguments(args):
    exit(-1)

inputFilePath = str(args.input[0])
bitmapFileHandle = open(inputFilePath, "rb")

outputFilePath = str(args.output[0])
arrayFileHandle = open(outputFilePath, "w")
1
if not isValidBitmapFile(bitmapFileHandle):
    exit(-1)

bitmapInformations = extractBitmapInformations(bitmapFileHandle)
writeArrayBitmapFile(arrayFileHandle, args.glyph_width[0], args.glyph_height[0], args.glyph_row_count[0], bitmapInformations, bitmapFileHandle) 