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
#ifndef __RTSP_INTERNAL_H__
#define __RTSP_INTERNAL_H__

#include "rtsp.h"
#include "rtsp_buffer.h"

#define MIN(A,B)                (((A) < (B)) ? (A) : (B))

#define RTSP_SERVER_STRING "Server: MinimalRTSPServer/0.1\r\n"


#if 0
#define RTSP_LOG(STR, ...) printf("(%s: %d) " STR "\n", \
                                  __FILE__, __LINE__, __VA_ARGS__)
#else

#define RTSP_LOG(STR,...) rtspLogger("(%s: %d) " STR "\n", \
                                  __FILE__, __LINE__, __VA_ARGS__)

#endif

#define RTSP_LOG_AND_RTN(STATUS) \
        RTSP_LOG("Status: %s", rtspErrorString[(STATUS)]);\
        return (STATUS);

#define RTSP_TRY_RTN(TRY) \
    do {\
        RtspStatus status__ = (TRY);\
        if (status__)\
        {\
            RTSP_LOG_AND_RTN(status__);\
        }\
    } while (0)

#define RTSP_TRY_LOG(TRY) \
    do {\
        RtspStatus status__ = (TRY);\
        if (status__)\
        {\
            RTSP_LOG("Status: %s", rtspErrorString[(status__)]);\
        }\
    } while (0)

/* The maximum number of require headers we bother to store and report back to
 * the server as being unsupported.
 * Chosen arbitrarily */
#define RTSP_REQUIRE_MAX    3

typedef enum {
    RTSP_CONNECTION_UNRECOGNISED,
    RTSP_CONNECTION_CLOSE,
    RTSP_CONNECTION_KEEPALIVE
} rtspConnection;

typedef struct {
    /* The method of the RTSP request */
    RtspMethod method;

    /* URI of this method. */
    RtspBuffer uri;

    /* Path in the URI of this method. */
    RtspBuffer path;

    struct {
        /* CSEQ of this method. */
        RtspBuffer cseq;

        /* Accept header */
        struct {
            /* "application/sdp" */
            uint32_t appSdp:1;
        } accept;

        /* Transport header */
        struct {
            uint32_t unicast :1;
            uint32_t multicast :1;
            uint32_t rtp :1;

            struct {
                /* index 0 is Start, 1 is end */
                uint16_t clientPortRange[2];
            } rtpData;
        } transport;

        struct {
            char data[RTSP_MAX_SESSION_LENGTH];
            uint16_t length;
        } session;

        struct {
            uint32_t count;
            RtspBuffer unsupported[RTSP_REQUIRE_MAX];
        } require;

        rtspConnection connection;
    } header;

    /* Data about the error encountered while processing this request */
    struct {
        RtspProtocolError error;
    } response;

    /* Just store a point to the user data here.
     * The session will store a more permentant record */
    RtspClientRx *client;

    /* The resource being requested. */
    RtspResource *resource;

} RtspRequest;

extern const char * const rtspMethodString[RTSP_METHOD_LAST];
extern const char * const rtspErrorString[RTSP_ERROR_LAST];
extern const char * const rtspHeaderStrings[RTSP_HEADER_LAST];

RtspStatus rtspTcpTransmitCallback(RtspClientRx *client,
                                   const char *data,
                                   uint16_t length);
RtspStatus rtspSessionControlCallback(RtspSession *session,
                                      RtspSessionInstruction instruction);
RtspStatus parseHeaders(RtspRxBuffer *workBuf, RtspRequest *request);


RtspStatus rtspSessionCreate(RtspRequest *request,
                             RtspSession **session);
RtspStatus rtspSessionFind(RtspRequest *request,
                           RtspSession **session);
RtspStatus rtspSessionDestroy(RtspSession *session);

RtspStatus rtspFindResourceByPath(const char *pathData,
                                  uint16_t pathLength,
                                  RtspResource **resource);

#endif
