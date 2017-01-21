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

#ifndef __RTSP_H__
#define __RTSP_H__

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#define RTSP_MAX_RESOURCES      1
#define RTSP_MAX_SESSIONS       1
#define RTSP_MAX_URI_LENGTH     100
#define RTSP_MAX_SESSION_LENGTH 10

#define RTSP_METHOD_LIST        \
    RTSP_METHOD(DESCRIBE)       \
    RTSP_METHOD(ANNOUNCE)       \
    RTSP_METHOD(GET_PARAMETER)  \
    RTSP_METHOD(OPTIONS)        \
    RTSP_METHOD(PAUSE)          \
    RTSP_METHOD(PLAY)           \
    RTSP_METHOD(RECORD)         \
    RTSP_METHOD(REDIRECT)       \
    RTSP_METHOD(SETUP)          \
    RTSP_METHOD(SET_PARAMETER)  \
    RTSP_METHOD(TEARDOWN)

#define RTSP_HEADER_LIST\
   RTSP_HEADER(Accept,              RTSP_HEADER_ACCEPT)\
   RTSP_HEADER(Connection,          RTSP_HEADER_CONNECTION)\
   RTSP_HEADER(Content-Encoding,    RTSP_HEADER_CONTENT_ENCODING)\
   RTSP_HEADER(Content-Language,    RTSP_HEADER_CONTENT_LANUAGE)\
   RTSP_HEADER(Content-Length,      RTSP_HEADER_CONTENT_LENGTH)\
   RTSP_HEADER(Content-Type,        RTSP_HEADER_CONTENT_TYPE)\
   RTSP_HEADER(CSeq,                RTSP_HEADER_CSEQ)\
   RTSP_HEADER(Public,              RTSP_HEADER_PUBLIC)\
   RTSP_HEADER(Require,             RTSP_HEADER_REQUIRE)\
   RTSP_HEADER(RTP-Info,            RTSP_HEADER_RTP_INFO)\
   RTSP_HEADER(Session,             RTSP_HEADER_SESSION)\
   RTSP_HEADER(Server,              RTSP_HEADER_SERVER)\
   RTSP_HEADER(Transport,           RTSP_HEADER_TRANSPORT)

#define RTSP_ERROR_LIST \
    RTSP_ERROR(RTSP_OK)\
    RTSP_ERROR(RTSP_ERROR_INVALID)\
    RTSP_ERROR(RTSP_ERROR_INTERNAL)\
    RTSP_ERROR(RTSP_ERROR_CALLBACK)\
    RTSP_ERROR(RTSP_ERROR_NOT_FOUND)\
    RTSP_ERROR(RTSP_ERROR_NO_RESOURCE)\
    RTSP_ERROR(RTSP_ERROR_INITIALISED)

#define RTSP_PROTOCOL_ERROR_LIST\
    RTSP_PROTOCOL_ERROR(100, Continue,                         CONTINUE)\
    RTSP_PROTOCOL_ERROR(200, OK,                               OK)\
    RTSP_PROTOCOL_ERROR(201, Create,                           CREATE)\
    RTSP_PROTOCOL_ERROR(250, Low on Storage Space,             LOW_ON_STORAGE_SPACE)\
    RTSP_PROTOCOL_ERROR(300, Multiple Choices,                 MULTIPLE_CHOICES)\
    RTSP_PROTOCOL_ERROR(301, Moved Permanently,                MOVED_PERMANENTLY)\
    RTSP_PROTOCOL_ERROR(302, Moved Temporarily,                MOVED_TEMPORARILY)\
    RTSP_PROTOCOL_ERROR(303, See Other,                        SEE_OTHER)\
    RTSP_PROTOCOL_ERROR(305, Use Proxy,                        USE_PROXY)\
    RTSP_PROTOCOL_ERROR(400, Bad Request,                      BAD_REQUEST)\
    RTSP_PROTOCOL_ERROR(401, Unauthorized,                     UNAUTHORIZED)\
    RTSP_PROTOCOL_ERROR(402, Payment Required,                 PAYMENT_REQUIRED)\
    RTSP_PROTOCOL_ERROR(403, Forbidden,                        FORBIDDEN)\
    RTSP_PROTOCOL_ERROR(404, Not Found,                        NOT_FOUND)\
    RTSP_PROTOCOL_ERROR(405, Method Not Allowed,               METHOD_NOT_ALLOWED)\
    RTSP_PROTOCOL_ERROR(406, Not Acceptable,                   NOT_ACCEPTABLE)\
    RTSP_PROTOCOL_ERROR(407, Proxy Authentication Required,    PROXY_AUTHENTICATION_REQUIRED)\
    RTSP_PROTOCOL_ERROR(408, Request Timeout,                  REQUEST_TIMEOUT)\
    RTSP_PROTOCOL_ERROR(410, Gone,                             GONE)\
    RTSP_PROTOCOL_ERROR(411, Length Required,                  LENGTH_REQUIRED)\
    RTSP_PROTOCOL_ERROR(412, Precondition Failed,              PRECONDITION_FAILED)\
    RTSP_PROTOCOL_ERROR(413, Request Entity Too Large,         REQUEST_ENTITY_TOO_LARGE)\
    RTSP_PROTOCOL_ERROR(414, Request-URI Too Long,             REQUEST_URI_TOO_LONG)\
    RTSP_PROTOCOL_ERROR(415, Unsupported Media Type,           UNSUPPORTED_MEDIA_TYPE)\
    RTSP_PROTOCOL_ERROR(451, Invalid Parameter,                INVALID_PARAMETER)\
    RTSP_PROTOCOL_ERROR(452, Illegal COnference Identifier,    ILLEGAL_CONFERENCE_IDENTIFIER)\
    RTSP_PROTOCOL_ERROR(453, Not Enough Bandwidth,             NOT_ENOUGH_BANDWIDTH)\
    RTSP_PROTOCOL_ERROR(454, Session Not Found,                SESSION_NOT_FOUND)\
    RTSP_PROTOCOL_ERROR(455, Method Not Valid In This State,   METHOD_NOT_VALID_IN_THIS_STATE)\
    RTSP_PROTOCOL_ERROR(456, Header Field Not Valid,           HEADER_FIELD_NOT_VALID)\
    RTSP_PROTOCOL_ERROR(457, Invalid Range,                    INVALID_RANGE)\
    RTSP_PROTOCOL_ERROR(458, Parameter is Read-Only,           PARAMETER_IS_READ_ONLY)\
    RTSP_PROTOCOL_ERROR(459, Aggregate Operation Not Allowed,  AGGREGATE_OPERATION_NOT_ALLOWED)\
    RTSP_PROTOCOL_ERROR(460, Only Aggregate Operation Allowed, ONLY_AGGREGATE_OPERATION_ALLOWED)\
    RTSP_PROTOCOL_ERROR(461, Unsupported Transport,            UNSUPPORTED_TRANSPORT)\
    RTSP_PROTOCOL_ERROR(462, Destination Unreachable,          DESTINATION_UNREACHABLE)\
    RTSP_PROTOCOL_ERROR(500, Internal Server Error,            INTERNAL_SERVER_ERROR)\
    RTSP_PROTOCOL_ERROR(501, Not Implemented,                  NOT_IMPLEMENTED)\
    RTSP_PROTOCOL_ERROR(502, Bad Gateway,                      BAD_GATEWAY)\
    RTSP_PROTOCOL_ERROR(503, Service Unavailable,              SERVICE_UNAVAILABLE)\
    RTSP_PROTOCOL_ERROR(504, Gateway Timeout,                  GATEWAY_TIMEOUT)\
    RTSP_PROTOCOL_ERROR(505, RTSP Version Not Supported,       RTSP_VERSION_NOT_SUPPORTED)\
    RTSP_PROTOCOL_ERROR(551, Option not Support,               OPTION_NOT_SUPPORT)

#define RTSP_ERROR(E) E,
typedef enum {
    RTSP_ERROR_LIST
    RTSP_ERROR_LAST
} RtspStatus;
#undef RTSP_ERROR

typedef struct {
    char *buffer;
    uint16_t length;
} RtspBuffer;

#define RTSP_METHOD(M)  RTSP_METHOD_ ## M,
typedef enum {
    RTSP_METHOD_LIST
    RTSP_METHOD_LAST
} RtspMethod;
#undef RTSP_METHOD

#define RTSP_HEADER(STR, ENUM)  ENUM,
typedef enum {
    RTSP_HEADER_LIST
    RTSP_HEADER_LAST
} RtspHeader;
#undef RTSP_HEADER

#define RTSP_PROTOCOL_ERROR(VAL, STR, NAME)  RTSP_PROTOCOL_ERROR_ ## NAME ,
typedef enum {
    RTSP_PROTOCOL_ERROR_NONE = 0,
    RTSP_PROTOCOL_ERROR_LIST
    RTSP_PROTOCOL_ERROR_LAST
} RtspProtocolError;
#undef RTSP_PROTOCOL_ERROR

typedef enum {
    RTSP_SESSION_CREATE,
    RTSP_SESSION_PLAY,
    RTSP_SESSION_PAUSE,
    RTSP_SESSION_DELETE
} RtspSessionInstruction;

typedef struct {

    struct {
        char data[RTSP_MAX_URI_LENGTH];
        uint16_t length;
    } path;

    uint16_t serverPortRange[2];

    struct {
        const char *data;
        uint16_t length;
    } sdp;

} RtspResource;

/* The following information is used transparently by the stack.
 * When a RTSP message is received the application should pass in
 * identifying information which will be rejurgitated by the stack in
 * callbacks. 
 *
 * The data stored in this structure has two puposes:
 *
 * 1. Identify where to transmit RTSP replies to.
 * 2. Identify the client to transmit the streaming data to.
 *
 * On Linux this could be:
 * 1. Socket handle
 * 2. IP address
 *
 * A pointer to user defined data can be set instead. 
 * Note this data can persist for a long time. It _must_ remain vaid in
 * memory for a duration of a session.
 *
 * It will be saved internally when a RTSP Rx event occurs which triggers a
 * RTSP_SESSION_CREATE callback, and will only stop being used after
 * RTSP_SESSION_DELETE is called.
 */
typedef struct {

    union {
        struct {
            uint32_t ipAddress;
            int socket;
        } socket;

        void *userDefined;
    } handle;

} RtspClientRx;

typedef struct {
    /* Handle to the resource this session belongs to */
    RtspResource *resource;

    struct {
        /* User provided handle on message rx */
        RtspClientRx info;

        /* Populated by server on parsing request*/
        uint16_t portRange[2];

        /* Unique session id populated by the reserve on session create */
        struct {
            char data[RTSP_MAX_SESSION_LENGTH];
            uint16_t length;
        } id;
    } client;
} RtspSession;



typedef RtspStatus (*rtspTcpTransmit)(RtspClientRx *client,
                                      const uint8_t *data,
                                      uint16_t length);

typedef RtspStatus (*rtspSessionControl)(RtspSession *session,
                                         RtspSessionInstruction instruction);

typedef struct {
    struct {
        rtspTcpTransmit tx;
        rtspSessionControl control;
    } callback;
} RtspConfig;


RtspStatus rtspInit(RtspConfig *config);
RtspStatus rtspShutdown(void);

RtspStatus rtspResourceInit(RtspResource *config, RtspResource **handle);
RtspStatus rtspResourceShutdown(RtspResource *handle);

RtspStatus rtspRx(RtspClientRx *client, char *buffer, uint16_t length);

#endif /* Header Guard */
