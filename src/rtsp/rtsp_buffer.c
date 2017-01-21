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

#include <string.h>
#include "rtsp.h"
#include "rtsp_buffer.h"
#include "rtsp_internal.h"

/* Assume single threaded for now. This can be changed later with syncronisation
 * protection if required. */
#define RTSP_WRITE_BUFFER_LENGTH    256
static char rtspWriteBuffer[RTSP_WRITE_BUFFER_LENGTH];

void rtspRxBufferDump(RtspRxBuffer *buf)
{
    uint32_t x = 0;
    RTSP_LOG("=====\nBuffer - Line %u Total: %u", buf->lineLength, buf->totalLength);

    while (x < MIN(buf->lineLength,20))
    {
        if (isspace(buf->buffer[x]))
        {
            RTSP_LOG("%02u : \' \' : 0x%02x", x, buf->buffer[x], buf->buffer[x]);
        }
        else
        {
            RTSP_LOG("%02u : \'%c\' : 0x%02x", x, buf->buffer[x], buf->buffer[x]);
        }
        x++;
    }
}

/* Set the length of the current line. */
static RtspStatus bufSetLineEnding(RtspRxBuffer *buf)
{
    uint16_t index = 0;

    while (index + RTSP_EOL_LEN <= buf->totalLength)
    {
        if (strncmp(&buf->buffer[index], RTSP_EOL_STR, RTSP_EOL_LEN) == 0)
        {
            buf->lineLength = index;
            return RTSP_OK;
        }
        index++;
    }
    RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
}

RtspStatus rtspRxBufferInit(RtspRxBuffer *buf,
                            uint8_t *data,
                            uint16_t length)
{
    buf->totalLength = length;
    buf->buffer      = data;

    RTSP_TRY_RTN(bufSetLineEnding(buf));

    return RTSP_OK;
}

RtspStatus rtspRxBufferIncr(RtspRxBuffer *buf,
                      BufferManipulaton manipulation,
                      uint16_t incr)
{
    if (buf->totalLength == 0)
    {
        return RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    if (manipulation == BUFM_LINE_NEXT)
    {
        incr = buf->lineLength + RTSP_EOL_LEN;

        if (buf->totalLength < incr)
        {
            return RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
        }

        buf->buffer      += incr;
        buf->totalLength -= incr;
        buf->lineLength   = buf->totalLength;

        RTSP_TRY_RTN(bufSetLineEnding(buf));
        return RTSP_OK;
    }

    if (buf->lineLength == 0)
    {
        return RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    if (manipulation == BUFM_LINE_INCR)
    {
        if (buf->lineLength < incr)
        {
            return RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
        }

        buf->buffer      += incr;
        buf->totalLength -= incr;
        buf->lineLength  -= incr;
    }

    if (manipulation == BUFM_LINE_INCR_SEMICOLON)
    {
        uint16_t idx = 0;

        for (idx = 0; idx < buf->lineLength - 1 - incr; idx++)
        {
            if (buf->buffer[idx] == ';')
            {
                buf->buffer      += idx + incr + 1;
                buf->totalLength -= idx + incr + 1;
                buf->lineLength  -= idx + incr + 1;
                return RTSP_OK;
            }
        }

        return RTSP_ERROR_INVALID;
    }

    return RTSP_OK;
}

/* Get a write buffer from the pool. */
RtspStatus rtspWriteBufferGet(char **buffer, uint16_t *length)
{
    *buffer = rtspWriteBuffer;
    *length = RTSP_WRITE_BUFFER_LENGTH;

    memset(*buffer, 0, *length);

    return RTSP_OK;
}

/* Release a write buffer to the pool. */
RtspStatus rtspWriteBufferFree(char *buffer)
{

    return RTSP_OK;
}

