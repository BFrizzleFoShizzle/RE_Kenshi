// Taken from https://blog.talosintelligence.com/2014/06/exceptional-behavior-windows-81-x64-seh.html

#include <Windows.h>

#include <stdint.h>

// Unwind info flags
#define UNW_FLAG_EHANDLER 0x01
#define UNW_FLAG_UHANDLER 0x02
#define UNW_FLAG_CHAININFO 0x04

// UNWIND_CODE 3 bytes structure
typedef union _UNWIND_CODE {
    struct {
        uint8_t CodeOffset;
        uint8_t UnwindOp : 4;
        uint8_t OpInfo : 4;
    };
    USHORT FrameOffset;
} UNWIND_CODE, * PUNWIND_CODE;

typedef struct _UNWIND_INFO {
    uint8_t Version : 3;          // + 0x00 - Unwind info structure version
    uint8_t Flags : 5;            // + 0x00 - Flags (see above)
    uint8_t SizeOfProlog;         // + 0x01
    uint8_t CountOfCodes;         // + 0x02 - Count of unwind codes
    uint8_t FrameRegister : 4;    // + 0x03
    uint8_t FrameOffset : 4;      // + 0x03
    UNWIND_CODE UnwindCodes[1];  // + 0x04 - Unwind code array
    /*
    UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
    union {
        OPTIONAL ULONG ExceptionHandler;    // Exception handler routine
        OPTIONAL ULONG FunctionEntry;
    };
    OPTIONAL ULONG ExceptionData[];       // C++ Scope table structure
    */
} UNWIND_INFO, * PUNWIND_INFO;