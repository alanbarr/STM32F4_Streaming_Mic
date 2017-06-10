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
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include "rtsp_to_rtp_interface.h"
#include "rtsp.h"
#include "debug.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "memstreams.h"
#include "audio_tx.h"

#define IP(A,B,C,D)                         htonl(A<<24 | B<<16 | C<<8 | D)


static struct {
    THD_WORKING_AREA(wa, 4096);
    thread_t *handle; 
} rtspThreadData;

static RtspStatus rtspHanderLogging(const char *fmt, va_list ap)
{
    debugSerialPrintVa(fmt, ap);

    return RTSP_OK;
}

/* Modified version of chsnprintf*/
static int rtspHanderSnprint(char *dst,
                             uint32_t size,
                             const char *fmt,
                             va_list ap)
{
    MemoryStream ms;
    BaseSequentialStream *stream;
    int rtn = 0;
  
    msObjectInit(&ms, (uint8_t *)dst, size ? size - 1 : 0, 0);
  
    stream = (BaseSequentialStream *)(void *)&ms;
    rtn = chvprintf(stream, fmt, ap);

    if (ms.eos < size)
    {
        dst[ms.eos] = 0;
    }

    return rtn;
}

static RtspStatus rtspHandlerTransmit(RtspClientRx *client,
                                      const char *buffer,
                                      uint16_t size)
{
    (void)client;

    PRINT("going to try and tx %u bytes", size);

    if (ERR_OK != netconn_write((struct netconn*)client->handle.userDefined,
                                 buffer,
                                 size,
                                 NETCONN_COPY))
    {
        return RTSP_ERROR_CALLBACK;
    }
    
    return RTSP_OK;
}

static RtspStatus rtspHandlerControl(RtspSession *session,
                                     RtspSessionInstruction instruction)
{
    const char *string = NULL;

    switch (instruction)
    {
        case RTSP_SESSION_CREATE:
            string = "create";
            audioTxRtpConfig config;
            memset(&config, 0, sizeof(config));

            /* AB TODO */
            config.ipDest.addr    = IP(192,168,1,154);
            config.localRtpPort   = 4200;
            config.localRtcpPort  = 4201;
            config.remoteRtpPort  = 6000;
            config.remoteRtcpPort = 6001;

            audioTxRtpSetup(&config);

            break;
        case RTSP_SESSION_PLAY:
            string = "play";
            break;
        case RTSP_SESSION_PAUSE:
            string = "pause";
            break;
        case RTSP_SESSION_DELETE:
            string = "delete";
            break;
    }

    (void)session;
    (void)string;
    return RTSP_OK;
}

static THD_FUNCTION(rtspServer, msg)
{
    StatusCode rtn = STATUS_OK;
    bool critcalError = false;
    err_t lwipStatus = ERR_OK;
    struct netconn *rtspServerConn = NULL;
    struct netconn *rtspClientConn = NULL;
    struct netbuf *recvBuf = NULL;
    RtspClientRx client;
    RtspResource *rtspResource = (RtspResource*)msg;
    RtspStatus rtspStatus = RTSP_ERROR_CALLBACK;

    char *recvData = NULL;
    uint16_t recvLength = 0;

    chRegSetThreadName(__FUNCTION__);

    PRINT("In %s", __FUNCTION__);

    memset(&client, 0, sizeof(client));

    /* Create the socket */
    rtspServerConn = netconn_new(NETCONN_TCP);

    if (rtspServerConn == NULL)
    {
        chThdExit(STATUS_ERROR_LIBRARY_LWIP);
    }

    /* Bind the socket */
    if (ERR_OK != netconn_bind(rtspServerConn,
                               IP_ADDR_ANY,
                               rtspResource->localPort))
    {
        rtn = STATUS_ERROR_LIBRARY_LWIP;
        goto server_socket_close;
    }

    /* Listen for incomming connections */
    if (ERR_OK != netconn_listen(rtspServerConn))
    {
        rtn = STATUS_ERROR_LIBRARY_LWIP;
        goto server_socket_close;
    }

    PRINT("RTSP listening on %u",rtspResource->localPort);

    while (1)
    {
        if (ERR_OK != netconn_accept(rtspServerConn, &rtspClientConn))
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

        if (rtspClientConn == NULL)
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

        client.handle.userDefined = rtspClientConn;

        while (1)
        {
            if (ERR_OK != (lwipStatus = netconn_recv(rtspClientConn, &recvBuf)))
            {
                rtn = STATUS_ERROR_LIBRARY_LWIP;
                PRINT("netconn_recv failed = %d",lwipStatus);
                goto client_socket_close;
            }

            if (recvBuf == NULL)
            {
                rtn = STATUS_ERROR_LIBRARY_LWIP;
                goto client_socket_close;
            }

            if (ERR_OK != netbuf_data(recvBuf, (void**)&recvData, &recvLength))
            {
                PRINT("netbuf_data failed",0);
                goto client_socket_close;
            }

            if (RTSP_OK != (rtspStatus = rtspRx(&client, recvData, recvLength)))
            {
                PRINT("RTSP status code: %u", rtspStatus);
                goto client_socket_close;
            }

            netbuf_delete(recvBuf);
            recvBuf = NULL;
        }

    client_socket_close:
        if (recvBuf)
        {
            netbuf_delete(recvBuf);
        }

        if (ERR_OK != netconn_close(rtspClientConn))
        {
            PRINT("netconn_close failed",0);
            rtn = STATUS_ERROR_LIBRARY_LWIP;
        }

        client.handle.userDefined = NULL;

        if (critcalError)
        {
            break;
        }
    }

server_socket_close:
    PRINT("Going to close RTSP listener thread. rtn is %u", rtn);

    /* Close the socket */
    if (ERR_OK != netconn_close(rtspServerConn))
    {

    }

    chThdExit(rtn);
}

StatusCode rtspRtpSessionStart(RtspRtpSessionData *controlData)
{
    static char sdpData[256];

    RtspConfig config;
    RtspResource resource;
    RtspResource *resourceHandle;

    memset(&config, 0, sizeof(config));

    config.callback.tx      = rtspHandlerTransmit;
    config.callback.control = rtspHandlerControl;
    config.callback.log     = rtspHanderLogging;
    config.callback.snprint = rtspHanderSnprint;

    if (RTSP_OK != rtspInit(&config))
    {
        SC_ASSERT(STATUS_ERROR_LIBRARY_LWIP);
    }

    memset(&resource, 0, sizeof(resource));
    memcpy(resource.path.data,
            controlData->sessionName,
            sizeof(controlData->sessionName));
    resource.path.data[sizeof(controlData->sessionName)-1] = 0;
    resource.path.length = strlen(resource.path.data);

    chsnprintf(sdpData,
               sizeof(sdpData),
               "v=0\r\n" 
               "o=derp 1 1 IN IPV4 %u.%u.%u.%u\r\n"
               "s=derp\r\n"
               "t=0 0\r\n"
               "m=audio %u RTP/AVP 98\r\n"
               "a=rtpmap:98 L16/16000/18\r\n",
               (controlData->localIp & 0xFF000000) >> 24,
               (controlData->localIp & 0x00FF0000) >> 16,
               (controlData->localIp & 0x0000FF00) >>  8,
               (controlData->localIp & 0x000000FF) >>  8,
               controlData->rtpLocalPort[0]);

    resource.sdp.data    = sdpData;
    resource.sdp.length  = strlen(sdpData);

    resource.localPort          = controlData->rtspLocalPort;
    resource.serverPortRange[0] = controlData->rtpLocalPort[0];
    resource.serverPortRange[1] = controlData->rtpLocalPort[1];

    if (RTSP_OK != rtspResourceInit(&resource, &resourceHandle))
    {
        SC_ASSERT(STATUS_ERROR_LIBRARY_LWIP);
    }

    memset(&rtspThreadData, 0, sizeof(rtspThreadData));

    /* Create RTSP Thread */
    rtspThreadData.handle = chThdCreateStatic(rtspThreadData.wa,
                                              sizeof(rtspThreadData.wa),
                                              HIGHPRIO,
                                              rtspServer,
                                              resourceHandle); 

    return STATUS_OK;

}


