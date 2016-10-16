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

#include <string.h>
#include "rtp.h"
#include "rtp_internal.h"
#include "rtp_protocol.h"

static struct 
{
    rtpConfig config; 

    /* AB TODO I think we should assume there will only be 1 session and 1
     * remote */
    rtpSessionInternal session;
} rtpDataStore;

const char *rtpStatusToString(rtpStatus status)
{
    static const char * const rtpErrorString[] = 
    {
        "RTP_OK",
        "RTP_ERROR_INTERNAL",
        "RTP_ERROR_API",
        "RTP_ERROR_CB",
        "RTP_ERROR_OS",
        "INVALID RTP ERROR CODE" /* RTP ERROR LAST */
    };

    if (status < RTP_ERROR_LAST)
    {
        return rtpErrorString[status];
    }

    return rtpErrorString[RTP_ERROR_LAST];
}



rtpStatus rtpBufferGet(const rtpSession *sessionHandle, 
                       void **networkBuffer,
                       uint8_t **data,
                       uint32_t length)
{
    return rtpDataStore.config.bufAllocCb(sessionHandle, 
                                          networkBuffer,
                                          data,
                                          length);
}

rtpStatus rtpBufferFree(const rtpSession *sessionHandle, 
                        void *networkBuffer,
                        uint8_t *data)
{
    return rtpDataStore.config.bufFreeCb(sessionHandle, 
                                         networkBuffer,
                                         data);
}

rtpStatus rtpTx(const rtpSession *sessionHandle, 
                rtpType type,
                void *networkBuffer,
                uint8_t *data,
                uint32_t length)

{
    return rtpDataStore.config.txCb(sessionHandle,
                                    type,
                                    networkBuffer,
                                    data,
                                    length);
}

rtpStatus rtpGetNtpTime(uint32_t *seconds,
                        uint32_t *fraction)

{
    return rtpDataStore.config.getNtpCb(seconds,
                                        fraction);
}

rtpStatus rtpGetRand(uint32_t *random)
{
    return rtpDataStore.config.getRandomCb(random);
}

rtpStatus rtpLogger(const char *fmt, ...)
{
    rtpStatus status = RTP_ERROR_INTERNAL;
    va_list argList;

    va_start(argList, fmt);
    status = rtpDataStore.config.logCb(fmt, argList);
    va_end(argList);

    return status;
}

rtpStatus rtpInit(const rtpConfig *config)
{
    if (config->txCb == NULL)
    {
        return RTP_ERROR_API;
    }

    if (config->bufAllocCb == NULL)
    {
        return RTP_ERROR_API;
    }

    if (config->bufFreeCb == NULL)
    {
        return RTP_ERROR_API;
    }

    if (config->getNtpCb == NULL)
    {
        return RTP_ERROR_API;
    }

    if (config->getRandomCb == NULL)
    {
        return RTP_ERROR_API;
    }

    if (config->logCb == NULL)
    {
        return RTP_ERROR_API;
    }

    memcpy(&rtpDataStore.config, config, sizeof(rtpDataStore.config));

    return RTP_OK;
}

rtpStatus rtpShutdown(void)
{
    rtpStatus status = RTP_ERROR_INTERNAL;

    if (RTP_OK != (status = rtcpTaskShutdown()))
    {
        return status;
    }

    return RTP_OK;
}

rtpStatus rtpSessionInit(const rtpSession *config, rtpSession **sessionHandle)
{
    rtpSessionInternal *session  = &rtpDataStore.session;
    rtpStatus status = RTP_ERROR_INTERNAL;
    uint32_t sequenceNumber = 0;

    memcpy((&session->config), config, sizeof(session->config));

    if (RTP_OK != (status = rtpGetRand(&sequenceNumber)))
    {
        return status;
    }
    session->sequenceNumber = sequenceNumber;

    if (RTP_OK != (status = rtpGetRand(&session->periodicTimestamp)))
    {
        return status;
    }

    *sessionHandle = (rtpSession*)session;

    if (RTP_OK != (status = rtcpTaskInit(session)))
    {
        return status;
    }

    return RTP_OK;
}

rtpStatus rtpSessionShutdown(const rtpSession *sessionHandle)
{
    (void)sessionHandle;
    if (sessionHandle == NULL)
    {
        return RTP_ERROR_API;
    }
    return RTP_OK;
}

rtpStatus rtpRxData(const rtpSession *sessionHandle, 
                    uint8_t *data,
                    uint32_t length)
{
    (void)sessionHandle;
    (void)data;
    (void)length;

    if (sessionHandle == NULL)
    {
        return RTP_ERROR_API;
    }
    return RTP_OK;
}

rtpStatus rtpGetTxDataOffset(const rtpSession *sessionHandle,
                             uint32_t *offset)
{
    (void)sessionHandle;

    if (sessionHandle == NULL)
    {
        return RTP_ERROR_API;
    }
    *offset = RTP_HEADER_LENGTH;
    return RTP_OK;
}

static rtpStatus rtpUpdateRtpTxNtpDelta(rtpSessionInternal *session)
{
    rtpStatus status = RTP_ERROR_INTERNAL;
    uint32_t seconds = 0;
    uint32_t fraction = 0;
    uint64_t combined = 0;
    uint64_t delta = 0;

    if (RTP_OK != (status = rtpGetNtpTime(&seconds, &fraction)))
    {
        return status;
    }

    combined = fraction | (uint64_t)seconds << 32;
    delta    = combined - session->ntpAtRtpTx.lastTx;

    session->ntpAtRtpTx.lastTx = combined;
    session->ntpAtRtpTx.delta  = delta;

    return RTP_OK;
}

/* data should be a buffer with payload already in the correct place. */
rtpStatus rtpTxData(const rtpSession *sessionHandle,
                    void *networkBuffer,
                    uint8_t *data,
                    uint32_t length)
{
    rtpStatus status;
    rtpDataHeader *header = (rtpDataHeader*)data;

    rtpSessionInternal *session = (rtpSessionInternal*)sessionHandle;

    if (session == NULL)
    {
        return RTP_ERROR_API;
    }
    if (data == NULL)
    {
        return RTP_ERROR_API;
    }

    session->periodicTimestamp += session->config.periodicTimestampIncr;
    session->sequenceNumber++;

    header->verPadExCC          = RTP_VERSION << 6;
    header->markerPayloadType   = session->config.payloadType;
    header->timestamp           = HTON32(session->periodicTimestamp);
    header->sequenceNumber      = HTON16(session->sequenceNumber);
    header->ssrc                = HTON32(session->config.ssrc);

    if ((status = rtpTx(sessionHandle,
                        RTP_MSG_TYPE_DATA,
                        networkBuffer,
                        data,
                        length) != RTP_OK))
    {
        session->stats.rtp.txFailedPackets += 1;
        return status;
    }

    session->stats.rtp.txOctets  += length;
    session->stats.rtp.txPackets += 1;
    
    return RTP_OK;
}

