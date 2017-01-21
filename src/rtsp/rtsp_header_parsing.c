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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "rtsp.h"
#include "rtsp_internal.h"
#include "rtsp_buffer.h"

/* STR *MUST* be NULL terminated. */
#define STR_MIN(STR, NUM)       ((strlen(STR) < (NUM)) ? strlen((STR)) : (NUM))

/* Header string with colon and space */
const char * const rtspHeaderStrings[RTSP_HEADER_LAST] = {
#define RTSP_HEADER(STR, ENUM) #STR ": ",
    RTSP_HEADER_LIST
#undef RTSP_HEADER
};

static RtspStatus parseHeaderAccept(RtspRxBuffer *workBuf,
                                    RtspRequest *request)
{
    const char * const acceptAppSdp = "application/sdp";
    const uint16_t stringLen = strlen(acceptAppSdp);
    uint32_t idx = 0;

    for (idx = strlen("Accept: ");
         idx < workBuf->lineLength;
         idx++)
    {
        if (strncmp(acceptAppSdp,
                    &workBuf->buffer[idx],
                    MIN(stringLen, workBuf->lineLength)) == 0)
        {
            request->header.accept.appSdp = 1;
            break;
        }
    }
    return RTSP_OK;
}

/* Just store the location of the CSEQ header. */
static RtspStatus parseHeaderCseq(RtspRxBuffer *workBuf,
                                  RtspRequest *request)
{
    request->header.cseq.buffer = workBuf->buffer;
    request->header.cseq.length = workBuf->lineLength;
    return RTSP_OK;
}

/* workBuf expected to be string in form "ddd-ddd" or "dddd-dddd" where d is
 * integer */
static RtspStatus parseHeaderTransportPortRange(const RtspRxBuffer *workBuf,
                                                uint16_t *rangeLower,
                                                uint16_t *rangeUpper)
{
    uint32_t startIndex = 0;
    uint32_t endIndex = 0;
    char strnum[6];

    memset(strnum,0,sizeof(strnum));

    while (isdigit(workBuf->buffer[endIndex]) && endIndex < workBuf->lineLength)
    {
        endIndex++;
    }

    if (endIndex >= sizeof(strnum))
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    strncpy(strnum, workBuf->buffer, endIndex+1);
    *rangeLower = atoi(strnum);

    /* End Index should be sitting on "-" */
    if (endIndex >= workBuf->lineLength)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }
    if (workBuf->buffer[endIndex] != '-')
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    /* Move to expected end range */
    if (++endIndex >= workBuf->lineLength)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    startIndex = endIndex;

    while (isdigit(workBuf->buffer[endIndex]) && endIndex < workBuf->lineLength)
    {
        endIndex++;
    }

    if (endIndex-startIndex >= sizeof(strnum))
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    strncpy(strnum, &workBuf->buffer[startIndex], endIndex-startIndex);

    *rangeUpper = atoi(strnum);

    return RTSP_OK;
}

/* Parse the transport header for useful things. */
static RtspStatus parseHeaderTransport(RtspRxBuffer *workBuf,
                                       RtspRequest *request)
{
    bool gotoNext = false;
    const char * const rtpStr = "RTP/AVP";
    const char * const unicastStr = "unicast";
    const char * const clientPortStr = "client_port=";

    RTSP_TRY_RTN(rtspRxBufferIncr(workBuf,
                                  BUFM_LINE_INCR,
                                  strlen(rtspHeaderStrings[RTSP_HEADER_TRANSPORT])));

    /* Transport Spec 
     * e.g. RTP/AVP */
    /* *Could* be multiple types here, seperated by a comma. 
     * Would need comma count and loop */
    if (strncmp(rtpStr,
                &workBuf->buffer[0],
                MIN(strlen(rtpStr), workBuf->lineLength)) == 0)
    {
        request->header.transport.rtp = 1;
    }
    else
    {
        RTSP_LOG("Unexpected transport Type",0);
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    gotoNext = true;

    /* Transport Parameters */
    while (workBuf->lineLength)
    {
        if (gotoNext)
        {
            if (RTSP_OK != rtspRxBufferIncr(workBuf,
                                            BUFM_LINE_INCR_SEMICOLON,
                                            0))
            {
                /* No more colon's on this line. */
                return RTSP_OK;
            }
            gotoNext = false;
        }

        if (strncmp(unicastStr,
                    workBuf->buffer,
                    MIN(strlen(unicastStr), workBuf->lineLength)) == 0)
        {
            request->header.transport.unicast = 1;
            gotoNext = true;
            continue;
        }

        if (strncmp(clientPortStr,
                    workBuf->buffer,
                    MIN(strlen(clientPortStr), workBuf->lineLength)) == 0)
        {
            RTSP_TRY_RTN(rtspRxBufferIncr(workBuf,
                                          BUFM_LINE_INCR,
                                          strlen(clientPortStr)));

            RTSP_TRY_RTN(parseHeaderTransportPortRange(
                            workBuf,
                            &request->header.transport.rtpData.clientPortRange[0],
                            &request->header.transport.rtpData.clientPortRange[1]));

            gotoNext = true;
            continue;
        }

        /* Missed Everything, move on */
        gotoNext = true;
    }

    return RTSP_OK;
}

static RtspStatus parseHeaderSession(RtspRxBuffer *workBuf,
                                     RtspRequest *request)
{
    uint16_t length = 0;
    RtspRxBuffer afterSemicolon = {0};
    bool hasSemiColon = false;

    RTSP_TRY_RTN(rtspRxBufferIncr(workBuf,
                                  BUFM_LINE_INCR,
                                  strlen(rtspHeaderStrings[RTSP_HEADER_SESSION])));

    afterSemicolon = *workBuf;

    /* Attempt to move just after semicolon */
    if (RTSP_OK == rtspRxBufferIncr(&afterSemicolon,
                                    BUFM_LINE_INCR_SEMICOLON,
                                    1))
    {
        hasSemiColon = true;
    }

    /* Hold the length of the session string up to the next semi colon. */
    length = hasSemiColon ?
             workBuf->lineLength - afterSemicolon.lineLength - 2 :
             workBuf->lineLength;

    /* Ensure we don't overflow the buffer */
    length = length < sizeof(request->header.session.data) ?
             length : sizeof(request->header.session.data);

    memcpy(request->header.session.data, workBuf->buffer, length);
    request->header.session.length = length;

    /* TODO Get timeout value after semicolon */

    return RTSP_OK;
}


static RtspStatus parseHeaderRequire(RtspRxBuffer *workBuf,
                                     RtspRequest *request)
{
    uint16_t length = 0;

    RTSP_TRY_RTN(rtspRxBufferIncr(workBuf,
                                  BUFM_LINE_INCR,
                                  strlen(rtspHeaderStrings[RTSP_HEADER_REQUIRE])));

    /* We are not sitting on the feature's option-tag.
     * Currently not interested in option-tags so we'll just note that a require
     * header was present */
    if (request->header.require.count++ > RTSP_REQUIRE_MAX)
    {
        /* No more room to store anything */
        RTSP_LOG("Require count too big",0);
        return RTSP_OK;
    }

    request->header.require.unsupported[request->header.require.count-1].
        buffer = workBuf->buffer;

    request->header.require.unsupported[request->header.require.count-1].
        length = workBuf->lineLength;

    request->header.require.count;

    return RTSP_OK;
}

static RtspStatus parseHeaderConnection(RtspRxBuffer *workBuf,
                                        RtspRequest *request)
{
    RTSP_TRY_RTN(rtspRxBufferIncr(workBuf,
                                  BUFM_LINE_INCR,
                                  strlen(rtspHeaderStrings[RTSP_HEADER_CONNECTION])));

    if (strncmp("close", workBuf->buffer, workBuf->lineLength) == 0)
    {
        request->header.connection = RTSP_CONNECTION_CLOSE;
    }
    else if (strncmp("Keep-Alive", workBuf->buffer, workBuf->lineLength) == 0)
    {
        request->header.connection = RTSP_CONNECTION_KEEPALIVE;
    }

    return RTSP_OK;
}

static RtspStatus parseHeader(RtspRxBuffer *workBuf, RtspRequest *request)
{
    RtspStatus status = RTSP_OK;
    RtspHeader header;

    for (header = 0; header < RTSP_HEADER_LAST; header++)
    {
        if (strncmp(rtspHeaderStrings[header],
                    workBuf->buffer,
                    STR_MIN(rtspHeaderStrings[header],
                            workBuf->lineLength)) == 0)
        {
            break;
        }
    }

    if (header < RTSP_HEADER_LAST)
    {
        RTSP_LOG("HEADER: %s", rtspHeaderStrings[header]);
    }

    switch (header)
    {
        case RTSP_HEADER_ACCEPT:
            status = parseHeaderAccept(workBuf, request);
            break;
        case RTSP_HEADER_CSEQ:
            status = parseHeaderCseq(workBuf, request);
            break;
        case RTSP_HEADER_TRANSPORT:
            status = parseHeaderTransport(workBuf, request);
            break;
        case RTSP_HEADER_CONNECTION:
            status = parseHeaderConnection(workBuf, request);
            break;
        case RTSP_HEADER_SESSION:
            status = parseHeaderSession(workBuf, request);
            break;
        case RTSP_HEADER_REQUIRE:
            status = parseHeaderRequire(workBuf, request);
            break;
        default:
            /* Didn't find a header we cared about - not an error though */
            break;
    }

    return status;
}

RtspStatus parseHeaders(RtspRxBuffer *workBuf, RtspRequest *request)
{
    RtspStatus status = RTSP_ERROR_INTERNAL;

    while (workBuf->lineLength)
    {
        RTSP_TRY_RTN(parseHeader(workBuf, request));
        RTSP_TRY_RTN(rtspRxBufferIncr(workBuf, BUFM_LINE_NEXT, 0));
     }

    return RTSP_OK;
}
