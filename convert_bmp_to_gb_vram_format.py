import os
import struct
import types
import hashlib
import sys

tileSizeInPixels = 8

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

def convertToVramPixelValue(brightness):
    if brightness == 0:
        return 3
    elif brightness == 64:
        return 2
    elif brightness == 128:
        return 1
    
    return 0

def writeBitmapDataToCArray(vramDataFileHandle, bitmapInformations, bitmapFileHandle):
    #FK: seek to beginning of pixel data
    bitmapFileHandle.seek(bitmapInformations.pixelDataOffsetInBytes, os.SEEK_SET)

    tileIndex = 0
    tileCountHorizontal = bitmapInformations.width  / tileSizeInPixels
    tileCountVertical   = bitmapInformations.height / tileSizeInPixels

    totalTileCount = tileCountHorizontal * tileCountVertical
    vramPixel = 0
    shiftIndex = 0
    tileHashes  = []
    tilePixels  = []

    while tileIndex < totalTileCount:
        sha1Hash = hashlib.new("sha1")
        startX = ( int( tileIndex % tileCountHorizontal ) * tileSizeInPixels )
        startY = ( int( tileIndex / tileCountHorizontal ) * tileSizeInPixels )
        endY = startY + 8
        y = startY

        while y < endY:
            fixedY = ( bitmapInformations.height - 1 ) - y #FK: Bitmap data has it's origin in the lower left corner, we assume upper left

            x = startX
            endX = startX + 8
            while x < endX:
                pixelDataPosition = ( x + fixedY * bitmapInformations.width ) * 3
                bitmapFileHandle.seek(bitmapInformations.pixelDataOffsetInBytes + pixelDataPosition)
                pixelValue = int.from_bytes( bitmapFileHandle.read(1), "little" ) #FK: It's enough to only read the first value
                vramPixelValue = convertToVramPixelValue( int( pixelValue ) )
                lb = ( vramPixelValue >> 0 ) & 1
                hb = ( vramPixelValue >> 1 ) & 1
                vramPixel |= lb
                vramPixel |= hb << 8
                shiftIndex += 1

                if shiftIndex == 8:
                    vramPixelInBytes = vramPixel.to_bytes(2, "little", signed=False)
                    sha1Hash.update( vramPixelInBytes )
                    tilePixels.append( vramPixel )
                    shiftIndex = 0
                    vramPixel = 0
                else:
                    vramPixel <<= 1


                x += 1 
            y += 1
        tileIndex += 1
        newTileHash = int.from_bytes( sha1Hash.digest(), byteorder="little" )

        tileHashes.index()
        tileAlreadyInCache = False
        for tileHash in tileHashes:
            if tileHash == newTileHash:
                tileAlreadyInCache = True
                break

        if tileAlreadyInCache == False:
            tileHashes.append( newTileHash )
            for tilePixel in tilePixels:
                pixelInBytes = tilePixel.to_bytes(2, "little")
                vramDataFileHandle.write( pixelInBytes )
                vramDataFileHandle.flush()

        tilePixels.clear()

    print( "Unique tiles: {}".format( len( tileHashes ) ) )

if len(sys.argv) < 3:
    print("Too few arguments. Usage: convert_bmp_to_gb_vram_format input.bmp output.bin")
    exit(-1)


bitmapFileHandle = open(sys.argv[1], "rb")
if not isValidBitmapFile(bitmapFileHandle):
    exit(-1)

bitmapInformations = extractBitmapInformations(bitmapFileHandle)
if bitmapInformations.width % tileSizeInPixels != 0 or bitmapInformations.height % tileSizeInPixels != 0:
    print( "BMP dimension has to be multiple of 8. Currently is {}x{}".format( bitmapInformations.width, bitmapInformations.height ) )
    exit( -1 )

vramDataFileHandle = open(sys.argv[2], "wb")
writeBitmapDataToCArray(vramDataFileHandle, bitmapInformations, bitmapFileHandle)