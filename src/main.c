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
#include "ch.h"
#include "hal.h"
#include "debug.h"
#include "audio_tx.h"
#include "lwipthread.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "time_mgmt.h"
#include "random.h"
#include "rtsp_to_rtp_interface.h"

#define IP(A,B,C,D)             htonl(A<<24 | B<<16 | C<<8 | D)
#define LAN8720_IPADDR          IP(192, 168, 1, 60)
#define LAN8720_GATEWAY         IP(192, 168, 1, 254)
#define LAN8720_NETMASK         IP(255, 255, 255, 0)

#define RTSP_PORT_LOCAL         20000
#define RTP_PORT_LOCAL_0        25001
#define RTP_PORT_LOCAL_1        25002

static THD_WORKING_AREA(waBinkingThread, 128);
static THD_FUNCTION(blinkingThread, arg) 
{
    (void)arg;
    chRegSetThreadName(__FUNCTION__);
    while (TRUE) 
    {
        BOARD_LED_GREEN_TOGGLE();
        chThdSleep(MS2ST(500));
    }
}

#if 0
void echoServer(void)
{
    StatusCode rtn = STATUS_OK;
    struct netconn *serverConn = NULL;
    struct netconn *clientConn = NULL;
    struct netbuf *recvBuf = NULL;
    char *recvData = NULL;
    uint16_t recvLength = 0;
    uint16_t port = 10000;

    /* Create the socket */
    serverConn = netconn_new(NETCONN_TCP);

    if (serverConn == NULL)
    {
        chThdExit(STATUS_ERROR_LIBRARY_LWIP);
    }

    /* Bind the socket */
    if (ERR_OK != netconn_bind(serverConn,
                               IP_ADDR_ANY,
                               port))
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
        PRINT("About to accept. hopefully bound to %u", port);
        if (ERR_OK != netconn_accept(serverConn, &clientConn))
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

        PRINT("after accept", 0);

        if (clientConn == NULL)
        {
            rtn = STATUS_ERROR_LIBRARY_LWIP;
            goto server_socket_close;
        }

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

        }

        PRINT("got a message - %s",recvData);

        netbuf_delete(recvBuf);

        if (ERR_OK != netconn_close(clientConn))
        {

        }
        clientConn = NULL;

    client_socket_close:
        if (clientConn)
        {
            if (ERR_OK != netconn_close(clientConn))
            {
                rtn = STATUS_ERROR_LIBRARY_LWIP;
            }
        }

        if (rtn != STATUS_OK)
        {
            break;
        }
    }


server_socket_close:
    /* Close the socket */
    if (ERR_OK != netconn_close(serverConn))
    {

    }

    chThdExit(rtn);
}
#endif


int main(void) 
{
    struct lwipthread_opts opts;
    RtspRtpSessionData rtspData;
    uint8_t optsMAC[6] = {LAN8720_ETHADDR_0,
                          LAN8720_ETHADDR_1,
                          LAN8720_ETHADDR_2,
                          LAN8720_ETHADDR_3,
                          LAN8720_ETHADDR_4,
                          LAN8720_ETHADDR_5};
    halInit();
    chSysInit();
    debugInit();

    PRINT("Main is running",0);

    chThdCreateStatic(waBinkingThread, 
                      sizeof(waBinkingThread),
                      LOWPRIO,
                      blinkingThread,
                      NULL);

    memset(&opts, 0, sizeof(opts));
    opts.macaddress = optsMAC;
    opts.address    = LAN8720_IPADDR;
    opts.netmask    = LAN8720_NETMASK;
    opts.gateway    = LAN8720_GATEWAY;
    lwipInit(&opts);

    SC_ASSERT(randomInit());

#if 0
    while (1)
    {
        echoServer();
    }
#endif

    while (macPollLinkStatus(&ETHD1) == false)
    {
        chThdSleep(1);
    }

    strcpy(rtspData.sessionName, "audio");
    rtspData.localIp         = LAN8720_IPADDR;
    rtspData.rtspLocalPort   = RTSP_PORT_LOCAL;
    rtspData.rtpLocalPort[0] = RTP_PORT_LOCAL_0;
    rtspData.rtpLocalPort[1] = RTP_PORT_LOCAL_1;
    SC_ASSERT(rtspRtpSessionStart(&rtspData));

#if 0
    SC_ASSERT(timeInit());
    SC_ASSERT(timeNtpUpdate());

    audioTxInit();
    audioTxStart();
#endif

    while (1)
    {
        chThdSleep(S2ST(1));
    }
}



