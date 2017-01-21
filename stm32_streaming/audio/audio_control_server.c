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
#include <ctype.h>
#include <stdlib.h>
#include "audio_control_server.h"
#include "ch.h"
#include "hal.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "audio_tx.h"

/* Indexes into the expected management control string */
#define INDEX_IP    6
#define INDEX_PORT  15

static struct {
    AudioControlConfig config;
    THD_WORKING_AREA(workingArea, 1024);
    thread_t *thread;
} audioControlThdData;

/* start "8 hex ip" "4 hex port" */
/* start c0a8019a 1234 */
static StatusCode audioContolProcessRx(const AudioControlConfig *config,
                                       struct netconn *clientConn,
                                       char *buffer, 
                                       uint16_t length)
{
    (void)clientConn;

    audioTxRtpConfig audioCfg;

    memset(&audioCfg, 0, sizeof(audioCfg));

    /* Stop */
    if (strncmp(buffer, "stop", strlen("stop")) == 0)
    {
        PRINT("Stopping audio stream",0);
        audioTxRtpTeardown();
        BOARD_LED_ORANGE_CLEAR();
        return STATUS_OK;
    }
    /* start */
    else
    {
        uint32_t index = 0;
        char tempStr[9];

        if (length < 19)
        {
            SC_ASSERT(STATUS_ERROR_EXTERNAL_INPUT);
        }

        /* recieived string must contain start and then some */
        if (strncmp("start", buffer, length) >= 0)
        {
            SC_ASSERT(STATUS_ERROR_EXTERNAL_INPUT);
        }

        memset(&tempStr, 0, sizeof(tempStr));

        for (index = INDEX_IP; index < INDEX_IP+8; index++)
        {
            if (!isdigit((int)buffer[index]) && !isalpha((int)buffer[index]))
            {
                SC_ASSERT(STATUS_ERROR_EXTERNAL_INPUT);
            }

            tempStr[index - INDEX_IP] = buffer[index];
        }

        audioCfg.ipDest.addr = strtoll(tempStr, NULL, 16);
        audioCfg.ipDest.addr = htonl(audioCfg.ipDest.addr);

        memset(&tempStr, 0, sizeof(tempStr));

        for (index = INDEX_PORT; index < INDEX_PORT + 4; index++)
        {
            if (!isdigit((int)buffer[index]) && !isalpha((int)buffer[index]))
            {
                SC_ASSERT(STATUS_ERROR_EXTERNAL_INPUT);
            }

            tempStr[index - INDEX_PORT] = buffer[index];
        }

        audioCfg.remoteRtpPort = strtol(tempStr, NULL, 16);
        audioCfg.localRtpPort = config->localAudioSourcePort;

        PRINT("Streaming audio to: %u.%u.%u.%u:%u", 
              ip4_addr1(&audioCfg.ipDest.addr),
              ip4_addr2(&audioCfg.ipDest.addr),
              ip4_addr3(&audioCfg.ipDest.addr),
              ip4_addr4(&audioCfg.ipDest.addr),
              audioCfg.remoteRtpPort);

        audioTxRtpSetup(&audioCfg);
        audioTxRtpPlay();

        BOARD_LED_ORANGE_SET();

        return STATUS_OK;
    }

    return STATUS_OK;
}

static THD_FUNCTION(audioControlThd, arg) 
{
    StatusCode rtn = STATUS_OK;
    struct netconn *serverConn = NULL;
    struct netconn *clientConn = NULL;
    struct netbuf *recvBuf = NULL;
    char *recvData = NULL;
    uint16_t recvLength = 0;
    AudioControlConfig *config = (AudioControlConfig*)arg;

    chRegSetThreadName(__FUNCTION__);

    /* Create the socket */
    serverConn = netconn_new(NETCONN_TCP);

    if (serverConn == NULL)
    {
        chThdExit(STATUS_ERROR_LIBRARY_LWIP);
    }

    /* Bind the socket */
    if (ERR_OK != netconn_bind(serverConn,
                               IP_ADDR_ANY,
                               config->localMgmtPort))
    {
        rtn = STATUS_ERROR_LIBRARY_LWIP;
        goto server_socket_close;
    }

    /* Listen for incomming connections */
    if (ERR_OK != netconn_listen(serverConn))
    {
        rtn = STATUS_ERROR_LIBRARY_LWIP;
        goto server_socket_close;
    }

    while (1)
    {
        if (ERR_OK != netconn_accept(serverConn, &clientConn))
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

        if (clientConn == NULL)
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

        PRINT("New management connection from: %u.%u.%u.%u",
              ip4_addr1(&clientConn->pcb.ip->remote_ip.addr),
              ip4_addr2(&clientConn->pcb.ip->remote_ip.addr),
              ip4_addr3(&clientConn->pcb.ip->remote_ip.addr),
              ip4_addr4(&clientConn->pcb.ip->remote_ip.addr));

        if (ERR_OK != netconn_recv(clientConn, &recvBuf))
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto client_socket_close;
        }

        if (recvBuf == NULL)
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto client_socket_close;
        }

        if (ERR_OK != netbuf_data(recvBuf, (void**)&recvData, &recvLength))
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto client_socket_close;
        }

        rtn = audioContolProcessRx(config, clientConn, recvData, recvLength);

        /* A badly parsed message shouldn't break us from the server loop. */
        if (rtn == STATUS_ERROR_EXTERNAL_INPUT)
        {
            rtn = STATUS_OK;
        }

        netbuf_delete(recvBuf);

    client_socket_close:
        if (clientConn)
        {
            if (ERR_OK != netconn_close(clientConn))
            {
                PRINT("close failed", 0);
                rtn = STATUS_ERROR_LIBRARY_LWIP;
            }

            if (ERR_OK != netconn_delete(clientConn))
            {
                PRINT("close failed", 0);
                rtn = STATUS_ERROR_LIBRARY_LWIP;
            }

            clientConn = NULL;
        }

        if (rtn != STATUS_OK)
        {
            PRINT("Breaking from audio control server",0);
            break;
        }
    }

server_socket_close:
    /* Close the socket */
    if (ERR_OK != netconn_close(serverConn))
    {

    }

    if (ERR_OK != netconn_delete(serverConn))
    {

    }

    chThdExit(rtn);
}


/******************************************************************************/
/* External Functions                                                         */
/******************************************************************************/

StatusCode audioControlInit(AudioControlConfig *config)
{
   audioControlThdData.config = *config;

   audioControlThdData.thread = chThdCreateStatic(
                                    audioControlThdData.workingArea,
                                    sizeof(audioControlThdData.workingArea),
                                    NORMALPRIO,
                                    audioControlThd,
                                    &audioControlThdData.config);
    return STATUS_OK;
}


StatusCode audioControlShutdown(void)
{
    chThdTerminate(audioControlThdData.thread);
    chThdWait(audioControlThdData.thread);
    memset(&audioControlThdData, 0, sizeof(audioControlThdData));

    return STATUS_OK;
}
