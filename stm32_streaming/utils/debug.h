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
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "ch.h"
#include "hal.h"
#include <stdarg.h>

#if 1
#define PRINT(FMT, ...)                                                     \
        debugSerialPrint("(%s:%d) " FMT "\n\r", __FILE__, __LINE__, __VA_ARGS__)  

#else
#define PRINT(FMT, ...)
#endif

#define PRINT_CRITICAL(FMT, ...)\
        PRINT("ERROR: " FMT, __VA_ARGS__);                  \
                                                            \
        while (1)                                           \
        {                                                   \
            BOARD_LED_RED_SET();                            \
            chThdSleepMilliseconds(500);                    \
            BOARD_LED_RED_CLEAR();                          \
            chThdSleepMilliseconds(500);                    \
            BOARD_LED_RED_SET();                            \
            chSysHalt("ERROR");                             \
        }

#define SC_ASSERT(FUNC_CALL)                                \
    {                                                       \
        StatusCode _sc = FUNC_CALL;                         \
        if (_sc != STATUS_OK)                               \
        {                                                   \
            PRINT("Error: %s", statusCodeToString(_sc));    \
            return _sc;                                     \
        }                                                   \
    }

#define SC_ASSERT_MSG(FUNC_CALL, FMT, ...)                  \
    {                                                       \
        StatusCode _sc = FUNC_CALL;                         \
        if (_sc != STATUS_OK)                               \
        {                                                   \
            PRINT("Error: " FMT, __VA_ARGS__);              \
            PRINT("Error: %s.", statusCodeToString(_sc));   \
            return _sc;                                     \
        }                                                   \
    }

extern mutex_t serialPrintMtx;

typedef enum 
{
    /* Success */
    STATUS_OK,
    /* Argument provided to API was bad */
    STATUS_ERROR_API,
    /* Callback failed */
    STATUS_ERROR_CALLBACK,
    /* Internal error */
    STATUS_ERROR_INTERNAL,
    /* Some form of system input was out of expected range. */
    STATUS_ERROR_EXTERNAL_INPUT,
    /* OS returned Error */
    STATUS_ERROR_OS,
    /* Library returned error */
    STATUS_ERROR_LIBRARY,
    /* LWIP Library returned error */
    STATUS_ERROR_LIBRARY_LWIP,
    /* Error interfacing with HW */
    STATUS_ERROR_HW,
    /* Unexpected timeout */
    STATUS_ERROR_TIMEOUT,
    /* Placeholder for bounds checking */
    STATUS_CODE_ENUM_MAX
} StatusCode;


void debugInit(void);
void debugShutdown(void);
void debugSerialPrint(const char * fmt, ...);
void debugSerialPrintVa(const char *fmt, va_list argList);
const char* statusCodeToString(StatusCode code);

#endif /* Header Guard*/
