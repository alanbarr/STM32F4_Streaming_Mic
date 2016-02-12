/*******************************************************************************
* Copyright (c) 2016, Alan Barr
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef _DEBUG_h_
#define _DEBUG_h_

#include "ch.h"
#include "hal.h"

void debugInit(void);
void debugShutdown(void);
void debugSerialPrint(const char * fmt, ...);

#if 1
#define PRINT(FMT, ...)                                                     \
        debugSerialPrint("(%s:%d) " FMT "\n\r", __FILE__, __LINE__, __VA_ARGS__)  
#else
#define PRINT(FMT, ...)
#endif

#define PRINT_ERROR(FMT, ...)\
        PRINT("ERROR:" FMT, __VA_ARGS__);                   \
                                                            \
        while (1)                                           \
        {                                                   \
            LED_RED_SET();                                  \
            chThdSleepMilliseconds(500);                    \
            LED_RED_CLEAR();                                \
            chThdSleepMilliseconds(500);                    \
            LED_RED_SET();                                  \
            chSysHalt("ERROR");                             \
        }

#define LED_ORANGE_SET()    palSetPad(GPIOD, GPIOD_LED3);
#define LED_ORANGE_CLEAR()  palClearPad(GPIOD, GPIOD_LED3);
#define LED_ORANGE_TOGGLE() palTogglePad(GPIOD, GPIOD_LED3);

#define LED_GREEN_SET()     palSetPad(GPIOD, GPIOD_LED4);
#define LED_GREEN_CLEAR()   palClearPad(GPIOD, GPIOD_LED4);
#define LED_GREEN_TOGGLE()  palTogglePad(GPIOD, GPIOD_LED4);

#define LED_RED_SET()       palSetPad(GPIOD, GPIOD_LED5);
#define LED_RED_CLEAR()     palClearPad(GPIOD, GPIOD_LED5);
#define LED_RED_TOGGLE()    palTogglePad(GPIOD, GPIOD_LED5);

#define LED_BLUE_SET()      palSetPad(GPIOD, GPIOD_LED6);
#define LED_BLUE_CLEAR()    palClearPad(GPIOD, GPIOD_LED6);
#define LED_BLUE_TOGGLE()   palTogglePad(GPIOD, GPIOD_LED6);

#endif /* Header Guard*/
