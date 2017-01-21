/*******************************************************************************
* Copyright (c) 2017, Alan Barr
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

#ifndef  __RTSP_BUFFER_H__
#define  __RTSP_BUFFER_H__

#include <stdint.h>

#define RTSP_EOL_STR            "\r\n"
#define RTSP_EOL_LEN            2

typedef struct {
    /* Pointer into the received message */
    char *buffer;
    /* Remaining length in buffer. (All characters) */
    uint16_t totalLength;
    /* Length of line not including EOL characters */
    uint16_t lineLength;
} RtspRxBuffer;

typedef enum {
    /* Move along current line. Length to increment must be provided*/
    BUFM_LINE_INCR,
    /* Move along current line to after next occurance of semi colon */
    BUFM_LINE_INCR_SEMICOLON,
    /* Move along current line to after next occurance of comma*/
    BUFM_LINE_INCR_COMMA,
    /* Move to next line, and set length */
    BUFM_LINE_NEXT
} BufferManipulaton;


RtspStatus rtspRxBufferInit(RtspRxBuffer *buf,
                            uint8_t *data,
                            uint16_t length);

RtspStatus rtspRxBufferIncr(RtspRxBuffer *buf,
                            BufferManipulaton manipulation,
                            uint16_t incr);

void rtspRxBufferDump(RtspRxBuffer *buf);


RtspStatus rtspWriteBufferGet(char **buffer, uint16_t *length);
RtspStatus rtspWriteBufferFree(char *buffer);

#endif
