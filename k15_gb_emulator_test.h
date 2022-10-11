#ifndef K15_GB_EMULATOR
#   error "Include this file *after* 'k15_gb_emulator.h'"
#endif

#define K15_TEST_ROM_PATH_MAX 255
#define K15_TEST_ROM_DOWNLOAD_URL_MAX 512

bool8_t downloadTestRomArchive( const char* pTestRomZipUrl, const char* pTargetRootPath )
{
    return 0;
}