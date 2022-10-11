#ifndef K15_TYPES_HEADER
#define K15_TYPES_HEADER

#include <stdint.h>

//FK: Compiler specific functions
#ifdef _MSC_VER
#   include <intrin.h>
#   define BreakPointHook()         __nop()
#   define DebugBreak               __debugbreak
#else
#   define BreakPointHook()
#   define DebugBreak
#endif

#define K15_UNUSED_VAR(x) (void)x

#define Kbyte(x) ((x)*1024)
#define Mbyte(x) (Kbyte(x)*1024)

#define Kbit(x) (Kbyte(x)/8)
#define Mbit(x) (Mbyte(x)/8)

#define FourCC(a, b, c, d)  ((uint32_t)((a) << 0) | (uint32_t)((b) << 8) | (uint32_t)((c) << 16) | (uint32_t)((d) << 24))
#define ArrayCount(arr)     (sizeof(arr)/sizeof(arr[0]))

#define IllegalCodePath()       DebugBreak()
#define CompiletimeAssert(x)    typedef char compile_time_assertion_##_LINE_[(x)?1:-1]

#define GetMin(a, b) ((a)>(b)?(b):(a))
#define GetMax(a, b) ((a)>(b)?(a):(b))

#ifdef K15_RELEASE_BUILD
    #define RuntimeAssert(x)
#else
    #define RuntimeAssert(x)        if(!(x)) DebugBreak()
#endif

typedef uint8_t bool8_t;

bool8_t areStringsEqual( const char* pStringA, const char* pStringB, const uint32_t stringLength )
{
    for( uint32_t charIndex = 0u; charIndex < stringLength; ++charIndex )
    {
        if( pStringA[ charIndex ] != pStringB[ charIndex ] )
        {
            return 0u;
        }
    }

    return 1u;
}

const char* findLastInString( const char* pString, uint32_t stringLength, const char needle )
{
    const char* pStringStart = pString;
    const char* pStringEnd = pString + stringLength;
    while( pStringStart != pStringEnd )
    {
        if( *pStringEnd == needle )
        {
            return pStringEnd;
        }

        --pStringEnd;
    }

    return nullptr;
}

#endif //K15_TYPES_HEADER