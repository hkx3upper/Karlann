
#include "kbd.h"

UCHAR gKeyString[340][20] =
{
    "?", "[ESC]", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[BackSpace]", "[Tab]",                               //Normal
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "[Enter]", "[Left Ctrl]", "a", "s",
    "d", "f", "g", "h", "j", "k", "l", ";", "\'", "~", "[Left Shift]", "\\", "z", "x", "c", "v",
    "b", "n", "m", ",", ".", "/", "[Right Shift]", "[Keypad *]", "[Left Alt]", "[Space]", "[Caps Lock]", "F1", "F2", "F3", "F4", "F5",
    "F6", "F7", "F8", "F9", "F10", "[Num Lock]", "[Scroll Lock]", "[Keypad 7]", "[Keypad 8]", "[Keypad 9]", "[Keypad -]", "[Keypad 4]", "[Keypad 5]", "[Keypad 6]", "[Keypad +]", "[Keypad 1]",
    "[Keypad 2]", "[Keypad 3]", "[Keypad 0]", "[Keypad .]",

    "?", "[ESC]", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[BackSpace]", "[Tab]",                               //CapsLock
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "[Enter]", "[Left Ctrl]", "A", "S",
    "D", "F", "G", "H", "J", "K", "L", ";", "\'", "~", "[Left Shift]", "\\", "Z", "X", "C", "V",
    "B", "N", "M", ",", ".", "/", "[Right Shift]", "[Keypad *]", "[Left Alt]", "[Space]", "[Caps Lock]", "F1", "F2", "F3", "F4", "F5",
    "F6", "F7", "F8", "F9", "F10", "[Num Lock]", "[Scroll Lock]", "[Keypad 7]", "[Keypad 8]", "[Keypad 9]", "[Keypad -]", "[Keypad 4]", "[Keypad 5]", "[Keypad 6]", "[Keypad +]", "[Keypad 1]",
    "[Keypad 2]", "[Keypad 3]", "[Keypad 0]", "[Keypad .]",

    "?", "[ESC]", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "[BackSpace]", "[Tab]",                               //Shift
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "[Enter]", "[Left Ctrl]", "A", "S",
    "D", "F", "G", "H", "J", "K", "L", ":", "\"", "~", "[Left Shift]", "|", "Z", "X", "C", "V",
    "B", "N", "M", "<", ">", "?", "[Right Shift]", "[Keypad *]", "[Left Alt]", "[Space]", "[Caps Lock]", "F1", "F2", "F3", "F4", "F5",
    "F6", "F7", "F8", "F9", "F10", "[Num Lock]", "[Scroll Lock]", "[Keypad 7]", "[Keypad 8]", "[Keypad 9]", "[Keypad -]", "[Keypad 4]", "[Keypad 5]", "[Keypad 6]", "[Keypad +]", "[Keypad 1]",
    "[Keypad 2]", "[Keypad 3]", "[Keypad 0]", "[Keypad .]",

    "?", "[ESC]", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "[BackSpace]", "[Tab]",                               //CapsLock + Shift
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "{", "}", "[Enter]", "[Left Ctrl]", "a", "s",
    "d", "f", "g", "h", "j", "k", "l", ":", "\"", "~", "[Left Shift]", "|", "z", "x", "c", "v",
    "b", "n", "m", "<", ">", "?", "[Right Shift]", "[Keypad *]", "[Left Alt]", "[Space]", "[Caps Lock]", "F1", "F2", "F3", "F4", "F5",
    "F6", "F7", "F8", "F9", "F10", "[Num Lock]", "[Scroll Lock]", "[Keypad 7]", "[Keypad 8]", "[Keypad 9]", "[Keypad -]", "[Keypad 4]", "[Keypad 5]", "[Keypad 6]", "[Keypad +]", "[Keypad 1]",
    "[Keypad 2]", "[Keypad 3]", "[Keypad 0]", "[Keypad .]"
};

ULONG gKbdStatus = 0;


VOID
PocPrintScanCode(
    IN PKEYBOARD_INPUT_DATA InputData
)
/*
* ´òÓ¡Scancode
*/
{
    ASSERT(NULL != InputData);

    UCHAR MakeCode = 0;
    ULONG Index = 0;

    CHAR Buffer[20] = { 0 };

    MakeCode = (UCHAR)InputData->MakeCode;

    if (FlagOn(InputData->Flags, KEY_E0))
    {
        switch (MakeCode) {
        case 0x1D: strcpy(Buffer, "[Right Ctrl]"); break;
        case 0x35: strcpy(Buffer, "[Keypad /]"); break;
        case 0x37: strcpy(Buffer, "[Prt Scr SysRq]"); break;
        case 0x38: strcpy(Buffer, "[Right Alt]"); break;
        case 0x47: strcpy(Buffer, "[Home]"); break;
        case 0x48: strcpy(Buffer, "[Up]"); break;
        case 0x49: strcpy(Buffer, "[PageUp]"); break;
        case 0x4B: strcpy(Buffer, "[Left]"); break;
        case 0x4D: strcpy(Buffer, "[Right]"); break;
        case 0x4F: strcpy(Buffer, "[End]"); break;
        case 0x50: strcpy(Buffer, "[Down]"); break;
        case 0x51: strcpy(Buffer, "[PageDown]"); break;
        case 0x52: strcpy(Buffer, "[Insert]"); break;
        case 0x53: strcpy(Buffer, "[Delete]"); break;
        case 0x5B: strcpy(Buffer, "[Left Windows]"); break;
        case 0x5C: strcpy(Buffer, "[Right Windows]"); break;
        case 0x5D: strcpy(Buffer, "[Menu]"); break;
        default: strcpy(Buffer, "?"); break;
        }

        if (MakeCode > 0x5D)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("Unknow makecode = 0x%x\n", MakeCode));
            return;
        }

        if (FlagOn(InputData->Flags, KEY_BREAK))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("KeyUp   = %s\n", Buffer));
        }
        else
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("KeyDown = %s\n", Buffer));
        }

    }
    else
    {
        if (MakeCode > 0x54 * 4)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("Unknow makecode = 0x%x\n", MakeCode));
            return;
        }

        Index = MakeCode;

        if (gKbdStatus & KEY_CAPS)
        {
            Index += 0x54;
        }

        if (gKbdStatus & KEY_SHIFT)
        {
            Index += 0x54 * 2;
        }

        if (FlagOn(InputData->Flags, KEY_BREAK))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("KeyUp   = %s\n", gKeyString[Index]));
        }
        else
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES, ("KeyDown = %s\n", gKeyString[Index]));
        }


        if (FlagOn(InputData->Flags, KEY_MAKE))
        {
            switch (MakeCode)
            {
            case 0x3A:
                gKbdStatus ^= KEY_CAPS;
                break;

            case 0x2A:
            case 0x36:
                gKbdStatus |= KEY_SHIFT;
                break;

            case 0x45:
                gKbdStatus ^= KEY_NUM;
            }
        }
        else if (FlagOn(InputData->Flags, KEY_BREAK))
        {
            switch (MakeCode)
            {
            case 0x2A:
            case 0x36:
                gKbdStatus &= ~KEY_SHIFT;
                break;
            }
        }
    }
}
