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

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "project.h"

#define SERIAL_PRINT_DRIVER SD1

static mutex_t serialPrintMtx;

static THD_WORKING_AREA(waThread1, 128);

static THD_FUNCTION(Thread1, arg) 
{
    (void)arg;
    chRegSetThreadName("blinker");
    while (TRUE) 
    {
        LED_GREEN_TOGGLE();
        chThdSleepMilliseconds(500);
    }
}

void debugInit(void)
{
    /* Configure Debug Serial */
    sdStart(&SERIAL_PRINT_DRIVER, NULL);
    palSetPadMode(GPIOB, 6, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOB, 7, PAL_MODE_ALTERNATE(7));
    chMtxObjectInit(&serialPrintMtx);
}

void debugShutdown(void)
{
    sdStop(&SERIAL_PRINT_DRIVER);
}

void debugSerialPrint(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    chMtxLock(&serialPrintMtx);
    chvprintf((BaseSequentialStream*)&SERIAL_PRINT_DRIVER, fmt, ap);
    chMtxUnlock(&serialPrintMtx);
    va_end(ap);
}

int main(void) 
{
    halInit();
    chSysInit();
    debugInit();

    PRINT("Main is running",0);

    lan8720Init();
    
    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

    lan8720TestUDP();

    while(1)
    {
        chThdSleep(S2ST(1));
    }
}
