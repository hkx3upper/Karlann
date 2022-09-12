#pragma once

#include "global.h"

#define	KEY_SHIFT				        1
#define	KEY_CAPS				        2
#define	KEY_NUM				            4

VOID
PocPrintScanCode(
    IN PKEYBOARD_INPUT_DATA InputData
);

VOID
PocConfigureKeyMapping(
    IN PKEYBOARD_INPUT_DATA InputData
);
