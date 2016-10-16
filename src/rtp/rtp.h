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
#ifndef __RTP_H__
#define __RTP_H__

#include <stdint.h>
#include <stdarg.h>

#define RTP_HEADER_LENGTH       12

typedef enum 
{
    RTP_OK,
    RTP_ERROR_INTERNAL,
    RTP_ERROR_API,
    RTP_ERROR_CB,
    RTP_ERROR_OS,

    RTP_ERROR_LAST
} rtpStatus;

typedef enum 
{
    RTP_MSG_TYPE_DATA,
    RTP_MSG_TYPE_CONTROL
} rtpType;

typedef struct {
    void *userData;
    /* Should ideally be set to a random value */
    uint32_t ssrc;
    /* Value to increment the period time stamp by per RTP packet 
     * transmitted */
    uint32_t periodicTimestampIncr;
    uint8_t payloadType;

    struct {
        char text[32];
        uint8_t length;
    } cname;

} rtpSession;

typedef rtpStatus (*rtpTransmit)(const rtpSession *sessionHandle, 
                                 rtpType type,
                                 void *networkBuffer,
                                 uint8_t *data,
                                 uint32_t length);

/* Buffer is expected to be on a word boundary - % 4 */
typedef rtpStatus (*rtpControlBufAlloc)(const rtpSession *sessionHandle, 
                                        void **networkBuffer,
                                        uint8_t **data,
                                        uint32_t length);

/* Free a previously allocated buffer (rtpControlBufFree).
 * This will be called after a sucessful transmission and after an error. */
typedef rtpStatus (*rtpControlBufFree)(const rtpSession *sessionHandle, 
                                       void *networkBuffer,
                                       uint8_t *data);

typedef rtpStatus (*rtpGetNtpTimestamp)(uint32_t *seconds,
                                        uint32_t *fraction);

typedef rtpStatus (*rtpGetRandom)(uint32_t *random);

typedef rtpStatus (*rtpLog)(const char *fmt, va_list ap);

typedef struct {
    rtpTransmit txCb;
    rtpControlBufAlloc bufAllocCb;
    rtpControlBufFree bufFreeCb;
    rtpGetNtpTimestamp getNtpCb;
    rtpGetRandom getRandomCb;
    rtpLog logCb;
} rtpConfig;

/******************************************************************************/

rtpStatus rtpInit(const rtpConfig *config);
rtpStatus rtpShutdown(void);
rtpStatus rtpSessionInit(const rtpSession *config, rtpSession **sessionHandle);
rtpStatus rtpSessionShutdown(const rtpSession *sessionHandle);
rtpStatus rtpRxData(const rtpSession *sessionHandle, uint8_t *data, uint32_t length);
rtpStatus rtpRxControl(const rtpSession *sessionHandle, uint8_t *data, uint32_t length);
rtpStatus rtpGetTxDataOffset(const rtpSession *sessionHandle, uint32_t *offset);
rtpStatus rtpTxData(const rtpSession *sessionHandle,
                    void *networkBuffer,
                    uint8_t *data,
                    uint32_t length);

#endif /* Header Guard */
