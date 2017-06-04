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
#include <stdbool.h>
#include <ctype.h>
#include "rtsp.h"
#include "rtsp_internal.h"
#include "rtsp_buffer.h"

#define RTSP_SUPPORTED_VERSION  "RTSP/1.0"
#define RTSP_VERSION_STR_LEN    8
#define RTSP_MIN_URI_LEN        3

#ifdef RTSP_TESTING
RtspRequest lastRequest;
#endif

bool rtspInitialised = false;

static struct {
    RtspConfig config;
    RtspResource resource[RTSP_MAX_RESOURCES];
    RtspSession session[RTSP_MAX_SESSIONS];
} rtspDb;

const char * const rtspMethodString[RTSP_METHOD_LAST] =
{
#define RTSP_METHOD(M) #M,
    RTSP_METHOD_LIST
#undef RTSP_METHOD
};

const char * const rtspErrorString[RTSP_ERROR_LAST] =
{
#define RTSP_ERROR(E) #E,
    RTSP_ERROR_LIST
#undef RTSP_ERROR
};

const char * const rtspProtocolErrorString[RTSP_PROTOCOL_ERROR_LAST] =
{
#define RTSP_PROTOCOL_ERROR(VAL, STR, NAME) #VAL " " #STR ,
    "RTSP_PROTOCOL_ERROR_NONE",
    RTSP_PROTOCOL_ERROR_LIST
#undef RTSP_PROTOCOL_ERROR
};

struct {
    uint32_t rtspRxValid;
    uint32_t rtspRxInvalid;
} stats;

RtspStatus rtspLogger(const char *fmt, ...)
{
    RtspStatus status = RTSP_ERROR_INTERNAL;
    va_list argList;

    va_start(argList, fmt);
    status = rtspDb.config.callback.log(fmt, argList);
    va_end(argList);

    return status;
}

int rtspSnprintf(char *dst,
                 uint32_t size,
                 const char *fmt,
                 ...)
{
    int rtn = 0;
    va_list argList;

    va_start(argList, fmt);
    rtn = rtspDb.config.callback.snprint(dst, size, fmt, argList);
    va_end(argList);

    return rtn;
}

RtspStatus rtspGetRandomNumber(uint32_t *random)
{
    *random = 0xDEADBEEF;
    return RTSP_OK;
}

RtspStatus rtspSessionCreate(RtspRequest *request, 
                             RtspSession **session)
{
    uint32_t sessionIdx = 0;
    int snprintfRtn = 0;
    uint32_t rand = 0;

    if (rtspInitialised == false)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    for (sessionIdx = 0; sessionIdx < RTSP_MAX_SESSIONS; sessionIdx++)
    {
        /* Find a free session */
        if (rtspDb.session[sessionIdx].resource == NULL)
        {
            break;
        }
    }

    /* Didn't find a unused resource. */
    if (sessionIdx >= RTSP_MAX_SESSIONS)
    {
        return RTSP_ERROR_NOT_FOUND;
    }

    *session             = &rtspDb.session[sessionIdx];
    (*session)->resource = request->resource;

    RTSP_TRY_RTN(rtspGetRandomNumber(&rand));

    /* TODO XXX Check there is not a session with this same id */

    snprintfRtn = rtspSnprintf((*session)->client.id.data, 
                                sizeof((*session)->client.id.data),
                                "%X", 
                                rand);

    if (snprintfRtn < 0 || snprintfRtn >= RTSP_MAX_SESSION_LENGTH)
    {
        return RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    (*session)->client.id.length = snprintfRtn;

    (*session)->client.portRange[0] =
                        request->header.transport.rtpData.clientPortRange[0];
    (*session)->client.portRange[1] =
                        request->header.transport.rtpData.clientPortRange[1];

    (*session)->client.info = *(request->client);

    return RTSP_OK;

}

RtspStatus rtspSessionFind(RtspRequest *request,
                           RtspSession **session)
{
    uint32_t sessionIdx = 0;

    for (sessionIdx = 0; sessionIdx < RTSP_MAX_SESSIONS; sessionIdx++)
    {

        if (rtspDb.session[sessionIdx].resource == NULL)
        {
            RTSP_LOG("session not used",0);
            continue;
        }
        
        if (rtspDb.session[sessionIdx].client.id.length !=
            request->header.session.length)
        {
            RTSP_LOG("session lengths not the same",0);
            continue;
        }
        
        if (memcmp(rtspDb.session[sessionIdx].client.id.data,
                   request->header.session.data,
                   request->header.session.length))
        {
            RTSP_LOG("session memcmp failed",0); 
            continue;
        }
        *session = &rtspDb.session[sessionIdx];
        return RTSP_OK;
    }

    request->response.error = RTSP_PROTOCOL_ERROR_SESSION_NOT_FOUND;
    return RTSP_ERROR_NOT_FOUND;
}

RtspStatus rtspSessionDestroy(RtspSession *session)
{
    RtspStatus status = RTSP_ERROR_INTERNAL;

    if (RTSP_OK != (status = rtspSessionControlCallback(session,
                                                        RTSP_SESSION_DELETE)))
    {
        RTSP_TRY_RTN(status);
    }

    memset(session, 0, sizeof(*session));

    return RTSP_OK;
}


RtspStatus rtspTcpTransmitCallback(RtspClientRx *client,
                                   const char *data,
                                   uint16_t length)
{
    return rtspDb.config.callback.tx(client, data, length);
}

RtspStatus rtspSessionControlCallback(RtspSession *session,
                                      RtspSessionInstruction instruction)
{
    return rtspDb.config.callback.control(session, instruction);
}


static RtspStatus parseRequestLine(const RtspRxBuffer *bufIn,
                                   RtspRequest *request)
{
    uint32_t index = 0;
    uint32_t methodLength = 0;
    RtspRxBuffer bufWork = *bufIn;

    if (bufWork.lineLength < 4)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    /* Only need 4 bytes to determine the method */
    while (index < RTSP_METHOD_LAST)
    {
        if (strncmp(rtspMethodString[index],
                    bufWork.buffer,
                    4) == 0)
        {
            request->method = index;
            break;
        }

        index++;
    }
    if (index == RTSP_METHOD_LAST)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    methodLength = strlen(rtspMethodString[request->method]);

    if ((methodLength + 1 + RTSP_MIN_URI_LEN +
         RTSP_VERSION_STR_LEN) >= (bufWork.lineLength))
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    if (bufWork.buffer[methodLength] != ' ')
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    /* URI */
    /* Theres probably weird and wonderful variants of URI which break this.
     * Assuming for now:
     *  - the third '/' precedes the path, and is optional
     *  - the first space is the end of the URI
     */
    RTSP_TRY_RTN(rtspRxBufferIncr(&bufWork, BUFM_LINE_INCR, methodLength + 1));

    index = 0;
    uint16_t firstSpace = 0;
    uint16_t thirdSlash = 0;
    uint8_t slashCount = 0;
    while (index < bufWork.lineLength)
    {
        if (bufWork.buffer[index] == ' ')
        {
            firstSpace = index;
            break;
        }
        if (thirdSlash != 3 && bufWork.buffer[index] == '/')
        {
            slashCount++;
            if (slashCount == 3)
            {
                thirdSlash = index;
            }
        }
        index++;
    }

    if (firstSpace == 0)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    request->uri.buffer = bufWork.buffer;
    request->uri.length = index;

    if (thirdSlash)
    {
        request->path.buffer = &bufWork.buffer[thirdSlash];
        request->path.length = firstSpace - thirdSlash;
    }

    RTSP_TRY_RTN(rtspRxBufferIncr(&bufWork,
                                  BUFM_LINE_INCR,
                                  request->uri.length + 1));

    /* Version */
    if (strncmp(RTSP_SUPPORTED_VERSION,
                bufWork.buffer,
                MIN(RTSP_VERSION_STR_LEN, bufWork.lineLength)))
    {
        request->response.error = RTSP_PROTOCOL_ERROR_RTSP_VERSION_NOT_SUPPORTED;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INVALID);
    }

    return RTSP_OK;
}

static RtspStatus rtspSendErrorReply(RtspRequest *request)
{
    char *responseBuf = NULL;
    uint16_t responseBufLen = 0;
    int stringLength = 0;
    RtspStatus status = RTSP_ERROR_INTERNAL;

    RTSP_TRY_RTN(rtspWriteBufferGet(&responseBuf, &responseBufLen));

    stringLength = rtspSnprintf(responseBuf,
                                responseBufLen,
                                "RTSP/1.0 %s %s %s",
                                rtspProtocolErrorString[request->response.error],
                                RTSP_EOL_STR,
                                RTSP_EOL_STR);

    if (stringLength >= responseBufLen || stringLength < 0)
    {
        request->response.error = RTSP_PROTOCOL_ERROR_INTERNAL_SERVER_ERROR;
        RTSP_LOG_AND_RTN(RTSP_ERROR_INTERNAL);
    }

    RTSP_TRY_LOG(status = rtspTcpTransmitCallback(request->client,
                                                  responseBuf,
                                                  stringLength));

    RTSP_TRY_RTN(rtspWriteBufferFree(responseBuf));

    return status;
}

RtspStatus rtspFindResourceByPath(const char *pathData,
                                  uint16_t pathLength,
                                  RtspResource **resource)
{
    uint32_t resourceIdx = 0;

    /* Ignore trailing `/` on path */
    if (pathLength >= 1 && pathData[pathLength-1] == '/')
    {
        pathLength -= 1;
    }

    /* Ignore leading `/` on path */
    if (pathLength >= 1 && pathData[0] == '/')
    {
        pathLength -= 1;
        pathData   +=1;
    }

    for (resourceIdx = 0; resourceIdx < RTSP_MAX_RESOURCES; resourceIdx++)
    {
        if (pathLength != rtspDb.resource[resourceIdx].path.length)
        {
            continue;
        }

        if (memcmp(pathData, rtspDb.resource[resourceIdx].path.data, pathLength))
        {
            RTSP_LOG("pathData<:%s> resource: <%s>", 
                     pathData,
                     rtspDb.resource[resourceIdx].path.data);
            continue;
        }

        *resource = &rtspDb.resource[resourceIdx];

        return RTSP_OK;
    }
    return RTSP_ERROR_NOT_FOUND;
}

RtspStatus rtspRx(RtspClientRx *client, char *buffer, uint16_t length)
{
    RtspStatus status = RTSP_ERROR_INTERNAL;
    RtspRxBuffer workBuf;
    RtspRequest request;

    if (rtspInitialised == false)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    memset(&request, 0, sizeof(request));

    request.client = client;

    if ((status = rtspRxBufferInit(&workBuf, buffer, length)))
    {
        RTSP_LOG("failed %u", status);
        goto update_stats_exit;
    }

    if ((status = parseRequestLine(&workBuf, &request)))
    {
        RTSP_LOG("Failed: %s", rtspErrorString[(status)]);
        goto update_stats_exit;
    }

    if ((status = (rtspRxBufferIncr(&workBuf, BUFM_LINE_INCR, 0))))
    {
        RTSP_LOG("Failed: %s", rtspErrorString[(status)]);
        goto update_stats_exit;
    }

    if ((status = parseHeaders(&workBuf, &request)))
    {
        RTSP_LOG("Failed: %s", rtspErrorString[(status)]);
        goto update_stats_exit;
    }

    if ((status = rtspRxProcess(&request)))
    {
        RTSP_LOG("Failed: %s", rtspErrorString[(status)]);
        goto update_stats_exit;
    }

update_stats_exit:
#ifdef RTSP_TESTING
    lastRequest = request;
#endif

    if (status == RTSP_OK)
    {
        stats.rtspRxValid++;
    }
    else
    {
        stats.rtspRxInvalid++;

        if (request.response.error)
        {
            RTSP_TRY_LOG(rtspSendErrorReply(&request));
        }
    }

    return status;
}


RtspStatus rtspInit(RtspConfig *config)
{
    if (rtspInitialised == true)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    rtspDb.config = *config;
    rtspInitialised = true;

    return RTSP_OK;
}

RtspStatus rtspShutdown(void)
{
    if (rtspInitialised == false)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    memset(&rtspDb.config, 0, sizeof(rtspDb.config));

    rtspInitialised = false;

    return RTSP_OK;
}


RtspStatus rtspResourceInit(RtspResource *config, RtspResource **handle)
{
    uint32_t resourceIdx = 0;

    if (rtspInitialised == false)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    for (resourceIdx = 0; resourceIdx < RTSP_MAX_RESOURCES; resourceIdx++)
    {
        if (rtspDb.resource[resourceIdx].sdp.length == 0)
        {
            *handle = &rtspDb.resource[resourceIdx];

            rtspDb.resource[resourceIdx] = *config;

            /* A bit of a hack, but we'll ignore trailing "/" here. */
            if (rtspDb.resource[resourceIdx].path.length &&
                rtspDb.resource[resourceIdx].path.data[
                    rtspDb.resource[resourceIdx].path.length-1] == '/')
            {
                rtspDb.resource[resourceIdx].path.length -= 1;
            }
            return RTSP_OK;
        }
    }
    RTSP_LOG_AND_RTN(RTSP_ERROR_NO_RESOURCE);
}

RtspStatus rtspResourceShutdown(RtspResource *handle)
{
    uint32_t sessionIdx = 0;
    RtspStatus status = RTSP_OK;

    if (rtspInitialised == false)
    {
        RTSP_LOG_AND_RTN(RTSP_ERROR_INITIALISED);
    }

    /* Find and destroy all sessions which are using this resource */
    for (sessionIdx = 0; sessionIdx < RTSP_MAX_SESSIONS; sessionIdx++)
    {
        if (rtspDb.session[sessionIdx].resource != handle)
        {
            continue;
        }
        status = rtspSessionDestroy(&rtspDb.session[sessionIdx]);
    }

    memset(handle, 0, sizeof(*handle));

    return status;
}
