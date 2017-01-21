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
#include "rtp.h"

#define RTP_VERSION         2

#define HTON32(H32)         (__builtin_bswap32(H32))
#define HTON16(H16)         (__builtin_bswap16(H16))

#define COMPILE_ASSERT(EXPR) \
    extern int (*compileAssert(void)) \
                    [sizeof(struct {unsigned int assert : (EXPR) ? 1 : -1;})]

/* RTP data header on the wire */
typedef struct {
    /* 0 1 2 3 4 5 6 7
     *|V  |P|X|  CC   | */
    uint8_t verPadExCC;
    /* 0 1 2 3 4 5 6 7
     *|M|     PT      | */
    uint8_t markerPayloadType;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[0];
} rtpDataHeader;

COMPILE_ASSERT(sizeof(rtpDataHeader) == RTP_HEADER_LENGTH);

static struct 
{
    rtpConfig config; 

    /* Last transmitted sequence number in RTP */
    uint16_t sequenceNumber;

    /* If RTP packets are generated periodically, the nominal sampling instant
     * as determined from the sampling clock is to be used, not a reading of the
     * system clock.  */
    uint32_t periodicTimestamp;

    uint32_t ssrc;

} rtpDataStore;

StatusCode rtpGetRand(uint32_t *random)
{
    return rtpDataStore.config.getRandomCb(random);
}

StatusCode rtpInit(const rtpConfig *config)
{
    StatusCode status = STATUS_ERROR_INTERNAL;
    uint32_t sequenceNumber = 0;

    if (config->getRandomCb == NULL)
    {
        return STATUS_ERROR_API;
    }

    memcpy(&rtpDataStore.config, config, sizeof(rtpDataStore.config));

    if (STATUS_OK != (status = rtpGetRand(&rtpDataStore.ssrc)))
    {
        return status;
    }

    if (STATUS_OK != (status = rtpGetRand(&sequenceNumber)))
    {
        return status;
    }

    rtpDataStore.sequenceNumber = sequenceNumber;

    if (STATUS_OK != (status = rtpGetRand(&rtpDataStore.periodicTimestamp)))
    {
        return status;
    }

    return STATUS_OK;
}

StatusCode rtpShutdown(void)
{
    memset(&rtpDataStore, 0, sizeof(rtpDataStore));

    return STATUS_OK;
}

/* data should be a buffer with payload already in the correct place. */
StatusCode rtpAddHeader(uint8_t *data,
                        uint32_t length)
{
    rtpDataHeader *header = (rtpDataHeader*)data;

    if (data == NULL || length < RTP_HEADER_LENGTH)
    {
        return STATUS_ERROR_API;
    }

    rtpDataStore.periodicTimestamp += rtpDataStore.config.periodicTimestampIncr;
    rtpDataStore.sequenceNumber++;

    header->verPadExCC          = RTP_VERSION << 6;
    header->markerPayloadType   = rtpDataStore.config.payloadType;
    header->timestamp           = HTON32(rtpDataStore.periodicTimestamp);
    header->sequenceNumber      = HTON16(rtpDataStore.sequenceNumber);
    header->ssrc                = HTON32(rtpDataStore.ssrc);

    return STATUS_OK;
}

