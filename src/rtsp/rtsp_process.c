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

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "rtsp.h"
#include "rtsp_internal.h"
#include "rtsp_buffer.h"

static RtspStatus rtspRxProcessDescribeSendResponse(RtspRequest *request,
                                                    char *responseBuf,
                                                    uint16_t responseBufLen)
{
    int stringLength = 0;

    if (request->header.accept.appSdp == 0 ||
        request->resource->sdp.length == 0)
    {
        /* TODO what to return here. */
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    stringLength = snprintf(responseBuf, 
                            responseBufLen,
                            "RTSP/1.0 200 OK"               RTSP_EOL_STR
                            RTSP_SERVER_STRING
                            "Content-Type: application/sdp" RTSP_EOL_STR
                            "Content-Length: %u"            RTSP_EOL_STR,
                            request->resource->sdp.length);

    if (stringLength >= responseBufLen || stringLength < 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    /* Add in CSEQ string */
    if ((stringLength + request->header.cseq.length + RTSP_EOL_LEN) >= responseBufLen)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    memcpy(&responseBuf[stringLength], 
           request->header.cseq.buffer,
           request->header.cseq.length + RTSP_EOL_LEN);

    stringLength += request->header.cseq.length + RTSP_EOL_LEN;

    /* Add in extra EOL */
    stringLength += snprintf(&responseBuf[stringLength],
                             responseBufLen,
                             RTSP_EOL_STR);

    if (stringLength >= responseBufLen || stringLength < 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    /* Add in SDP data */
    if ((stringLength + request->resource->sdp.length) >= responseBufLen)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    memcpy(&responseBuf[stringLength], 
           request->resource->sdp.data,
           request->resource->sdp.length);

    stringLength += request->resource->sdp.length;

    /* Transmit */
    RTSP_TRY_RTN(rtspTcpTransmitCallback(request->client,
                                         responseBuf,
                                         stringLength));

    return RTSP_OK;
}

static RtspStatus rtspRxProcessDescribe(RtspRequest *request)
{
    char *responseBuf = NULL;
    uint16_t responseBufLen = 0;
    RtspStatus status = RTSP_ERROR_INTERNAL;

    RTSP_TRY_RTN(rtspWriteBufferGet(&responseBuf, &responseBufLen));

    status = rtspRxProcessDescribeSendResponse(request,
                                               responseBuf, 
                                               responseBufLen);

    RTSP_TRY_RTN(rtspWriteBufferFree(responseBuf));

    return status;
}


static RtspStatus rtspRxProcessSetupSendResponse(RtspRequest *request,
                                                 RtspSession *session,
                                                 char *responseBuf,
                                                 uint16_t responseBufLen)
{
    int stringLength = 0;

    if (request->header.transport.unicast != 1 ||
        request->header.transport.rtp != 1)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INVALID_PARAMETER;
        return RTSP_ERROR_INVALID;
    }

    if (request->header.transport.rtpData.clientPortRange[0] == 0 ||
        request->header.transport.rtpData.clientPortRange[1] == 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INVALID_RANGE;
        return RTSP_ERROR_INVALID;
    }

    stringLength = snprintf(responseBuf,
                            responseBufLen,
                            "RTSP/1.0 200 OK" RTSP_EOL_STR
                            RTSP_SERVER_STRING);

    if (stringLength >= responseBufLen || stringLength < 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    /* Add in CSEQ string */
    if ((stringLength + request->header.cseq.length + RTSP_EOL_LEN) >= responseBufLen)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    memcpy(&responseBuf[stringLength], 
           request->header.cseq.buffer,
           request->header.cseq.length + RTSP_EOL_LEN);

    stringLength += request->header.cseq.length + RTSP_EOL_LEN;

    /* Add in Session ID */
    if ((stringLength +
         session->client.id.length + 
         RTSP_EOL_LEN +
         strlen(rtspHeaderStrings[RTSP_HEADER_SESSION])) >= responseBufLen)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    stringLength += snprintf(&responseBuf[stringLength],
                             responseBufLen-stringLength,
                             "%s",
                             rtspHeaderStrings[RTSP_HEADER_SESSION]);

    memcpy(&responseBuf[stringLength], 
           session->client.id.data,
           session->client.id.length);
    stringLength += session->client.id.length;

    memcpy(&responseBuf[stringLength], RTSP_EOL_STR, RTSP_EOL_LEN);
    stringLength += RTSP_EOL_LEN;

    /* Add in Other headers */
    stringLength += snprintf(&responseBuf[stringLength],
                             responseBufLen - stringLength,
                             "Transport: RTP/AVP;unicast;client_port=%u-%u;"
                             "server_port=%u-%u" 
                             RTSP_EOL_STR
                             RTSP_EOL_STR,
                             request->header.transport.rtpData.clientPortRange[0],
                             request->header.transport.rtpData.clientPortRange[1],
                             session->resource->serverPortRange[0],
                             session->resource->serverPortRange[1]);

    if (stringLength >= responseBufLen || stringLength < 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    RTSP_TRY_RTN(rtspTcpTransmitCallback(request->client,
                                         responseBuf,
                                         stringLength));

    return RTSP_OK;

}

static RtspStatus rtspRxProcessSetup(RtspRequest *request)
{
    char *responseBuf = NULL;
    uint16_t responseBufLen = 0;
    RtspStatus status = RTSP_ERROR_INTERNAL;
    RtspSession *session = NULL;

    /* Errors and response */
    RTSP_TRY_RTN(rtspSessionCreate(request, &session));

    RTSP_TRY_RTN(rtspSessionControlCallback(session, RTSP_SESSION_CREATE));

    RTSP_TRY_RTN(rtspWriteBufferGet(&responseBuf, &responseBufLen));

    status = rtspRxProcessSetupSendResponse(request, 
                                            session,
                                            responseBuf,
                                            responseBufLen);

    RTSP_TRY_RTN(rtspWriteBufferFree(responseBuf));

    return status;
}


static RtspStatus rtspRxProcessPlay(RtspRequest *request)
{

    RtspSession *session = NULL;
    RtspStatus status = RTSP_ERROR_INTERNAL;

    RTSP_TRY_RTN(rtspSessionFind(request, &session));

    RTSP_TRY_RTN(rtspSessionControlCallback(session, RTSP_SESSION_PLAY));

    return RTSP_OK;
}

static RtspStatus rtspRxProcessPause(RtspRequest *request)
{
    RtspSession *session = NULL;
    RtspStatus status = RTSP_ERROR_INTERNAL;

    RTSP_TRY_RTN(rtspSessionControlCallback(session, RTSP_SESSION_PAUSE));

    return RTSP_OK;
}

static RtspStatus rtspRxProcessTeardown(RtspRequest *request)
{
    RtspSession *session = NULL;

    RTSP_TRY_RTN(rtspSessionFind(request, &session));

    return rtspSessionDestroy(session);
}


static RtspStatus rtspRxProcessOptions(RtspRequest *request)
{
    char *replyBuf = NULL;
    uint16_t replyBufLen = 0;
    RtspStatus status = RTSP_OK;
    int strLen = 0;
    char *reply = 
        "RTSP/1.0 200 OK"                                RTSP_EOL_STR
        RTSP_SERVER_STRING
        "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE" RTSP_EOL_STR;

    uint16_t replyLen = strlen(reply);

    rtspWriteBufferGet(&replyBuf, &replyBufLen);

    memcpy(&replyBuf[0], reply, replyLen);

    memcpy(&replyBuf[replyLen],
           request->header.cseq.buffer, 
           request->header.cseq.length + RTSP_EOL_LEN);

    replyLen += request->header.cseq.length + RTSP_EOL_LEN;

    memcpy(&replyBuf[replyLen],
           RTSP_EOL_STR,
           RTSP_EOL_LEN);

    replyLen += RTSP_EOL_LEN;

    status = rtspTcpTransmitCallback(request->client,
                                     replyBuf,
                                     replyLen);

    /* TODO XXX */
    rtspWriteBufferFree(replyBuf);

    return status;
}

RtspStatus rtspRxProcess(RtspRequest *request)
{
    RtspStatus status = RTSP_ERROR_INTERNAL;

    if (RTSP_OK != (status = rtspFindResourceByPath(request->path.buffer, 
                                                    request->path.length,
                                                    &request->resource)))
    {
        request->response.error = RTSP_PROTOCOL_ERROR_NOT_FOUND;
        RTSP_LOG_AND_RTN(status);
    }

    RTSP_LOG("Processing method: %s", rtspMethodString[request->method]);

    switch (request->method)
    {
        case RTSP_METHOD_OPTIONS:        
            RTSP_TRY_RTN(rtspRxProcessOptions(request));
            break;
        case RTSP_METHOD_DESCRIBE:
            RTSP_TRY_RTN(rtspRxProcessDescribe(request));
            break;
        case RTSP_METHOD_SETUP:          
            RTSP_TRY_RTN(rtspRxProcessSetup(request));
            break;
        case RTSP_METHOD_PLAY:           
            RTSP_TRY_RTN(rtspRxProcessPlay(request));
            break;
        case RTSP_METHOD_PAUSE:          
            RTSP_TRY_RTN(rtspRxProcessPause(request));
            break;
        case RTSP_METHOD_TEARDOWN:       
            RTSP_TRY_RTN(rtspRxProcessTeardown(request));
            break;

        case RTSP_METHOD_ANNOUNCE:   
        case RTSP_METHOD_GET_PARAMETER:  
        case RTSP_METHOD_RECORD:         
        case RTSP_METHOD_REDIRECT:       
        case RTSP_METHOD_SET_PARAMETER:  
        default:
            request->response.error = RTSP_PROTOCOL_ERROR_METHOD_NOT_ALLOWED;
            RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }
    return RTSP_OK;
}

