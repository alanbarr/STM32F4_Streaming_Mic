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
#include <stdint.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "rtp.h"
#include "random.h"
#include "audio_tx.h"
#include "debug.h"
#include "mp45dt02_processing.h"
#include "time_mgmt.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"

#define RX_READY_CONNECTION_NUM             20

/******************************************************************************/
/* Output Buffer - 20 ms worth of processed audio data */
/******************************************************************************/

/* Duration of samples to collect */
#define AUDIO_PAYLOAD_DURATION_MS           20

/* Size of the buffer holding output data */
#define AUDIO_PAYLOAD_BUFFER_SIZE_BYTES     (MP45DT02_DECIMATED_BUFFER_SIZE * \
                                             AUDIO_PAYLOAD_DURATION_MS      / \
                                             MP45DT02_RAW_SAMPLE_DURATION_MS * \
                                             sizeof(uint16_t))

#define IP(A,B,C,D)                         htonl(A<<24 | B<<16 | C<<8 | D)


typedef struct {

    /* API */
    audioTxRtpConfig config;

    /* Private */
    struct netconn *connRtp;
    struct netconn *connRtcp;
    
    struct {
        uint32_t count;
        struct netbuf *lwipBuffer;
        uint8_t *dataStart;
        uint8_t *dataCurrent;

        struct {
            uint32_t failedNetbufNew;
            uint32_t failedNetbufAlloc;
        } debug;
    } audio;

    rtpSession *rtpSessionHandle;
} audioTxSession;

static struct {
    mailbox_t readyMailbox;
    msg_t     readyMsgs[RX_READY_CONNECTION_NUM];
    thread_t  *thread;
    THD_WORKING_AREA(threadWa, 1024);
} audioRxData;

static audioTxSession activeAudioSession;

static rtpStatus audioRtpTransmitCb(const rtpSession *session, 
                                    rtpType type,
                                    void *networkBufer,
                                    uint8_t *data,
                                    uint32_t length)
{
    (void)data;
    (void)length;

    uint16_t remotePort;
    struct netconn *udpConn = NULL;
    struct netbuf *lwipBuffer = (struct netbuf*)networkBufer;
    audioTxSession *txData = (audioTxSession*)session->userData;
    err_t lwipErr = ERR_OK;

    if (type == RTP_MSG_TYPE_DATA)
    {
        udpConn    = txData->connRtp;
        remotePort = txData->config.remoteRtpPort;
    }
    else if (type == RTP_MSG_TYPE_CONTROL)
    {
        udpConn    = txData->connRtcp;
        remotePort = txData->config.remoteRtcpPort;
    }
    else
    {
        DEBUG_CRITICAL("",0);
    }

    if (ERR_OK != (lwipErr = netconn_sendto(udpConn,
                                            lwipBuffer,
                                            &txData->config.ipDest,
                                            remotePort)))
    {
        return RTP_ERROR_CB;
    }

    return RTP_OK;
}

static rtpStatus audioRtpControlBufAllocCb(const rtpSession *session, 
                                           void **networkBuffer,
                                           uint8_t **data,
                                           uint32_t length)
{
    (void)session;
    struct netbuf *lwipBuffer = NULL;
    uint8_t *lwipPayload = NULL;

    if ((lwipBuffer = netbuf_new()) == NULL)
    {
        return RTP_ERROR_CB;
    }

    if ((lwipPayload = netbuf_alloc(lwipBuffer, length)) == NULL)
    {
        return RTP_ERROR_CB;
    }

    *networkBuffer = (void*)lwipBuffer;
    *data          = lwipPayload;

    return RTP_OK;
}

static rtpStatus audioRtpControlBufFreeCb(const rtpSession *session, 
                                          void *networkBuffer,
                                          uint8_t *data)
{
    (void)session;
    (void)data;

    netbuf_delete((struct netbuf *)networkBuffer);

    return RTP_OK;
}

static rtpStatus audioRtpGetNtpTimestampCb(uint32_t *seconds,
                                           uint32_t *fraction)
{
    if (STATUS_OK != timeNtpGet(seconds, fraction))
    {
        return RTP_ERROR_CB;
    }

    return RTP_OK;
}

static rtpStatus audioRtpGetRandomCb(uint32_t *random)
{
    if (STATUS_OK != randomGet(random))
    {
        return RTP_ERROR_CB;
    }

    return RTP_OK;
}

static rtpStatus audioRtpLog(const char *fmt, va_list argList)
{
    debugSerialPrintVa(fmt,argList);
    return RTP_OK;
}

static void audioTxHandleFullMp45dt02Buffer(uint16_t *data,
                                            uint16_t length)       
{
    uint32_t index = 0;
    uint16_t *sample = NULL;
    uint16_t bufferLength = RTP_HEADER_LENGTH + AUDIO_PAYLOAD_BUFFER_SIZE_BYTES;

    static struct {
        uint32_t timesCalled;
        uint32_t timesTx;
    } dbg = {0};

    dbg.timesCalled++;

    /**************************************************************************/ 
    /* Check if we need a new buffer                                          */
    /**************************************************************************/ 
    if (activeAudioSession.audio.lwipBuffer == NULL)
    {
        activeAudioSession.audio.lwipBuffer = netbuf_new();

        if (activeAudioSession.audio.lwipBuffer == NULL)
        {
            activeAudioSession.audio.debug.failedNetbufNew++;
            PRINT("NULL",0);
            return;
        }

        activeAudioSession.audio.dataStart =
                    netbuf_alloc(activeAudioSession.audio.lwipBuffer, 
                                 bufferLength);

        if (activeAudioSession.audio.dataStart == NULL)
        {
            netbuf_delete(activeAudioSession.audio.lwipBuffer);
            activeAudioSession.audio.lwipBuffer = NULL;
            activeAudioSession.audio.debug.failedNetbufAlloc++;
            return;
        }

        activeAudioSession.audio.dataCurrent =  
            activeAudioSession.audio.dataStart + RTP_HEADER_LENGTH;
    }

    /**************************************************************************/ 
    /* Change to network order                                                */
    /**************************************************************************/ 
    for (index = 0, sample = (uint16_t *)activeAudioSession.audio.dataCurrent;
         index < length;
         index++, sample++)
    {
        *sample = htons(*data);
    }

    activeAudioSession.audio.dataCurrent += length;

    /**************************************************************************/ 
    /* Transmit if full                                                       */
    /**************************************************************************/ 
    if (activeAudioSession.audio.dataCurrent >= 
            (activeAudioSession.audio.dataStart + bufferLength))
    {
        dbg.timesTx++;
        if (activeAudioSession.audio.dataCurrent >
            (activeAudioSession.audio.dataStart + bufferLength))
        {
            DEBUG_CRITICAL("Overrun %u > %u ", 
                            activeAudioSession.audio.dataCurrent,
                            activeAudioSession.audio.dataStart + bufferLength);
        }
    
        if (RTP_OK != rtpTxData(activeAudioSession.rtpSessionHandle,
                                activeAudioSession.audio.lwipBuffer,
                                activeAudioSession.audio.dataStart,
                                length))
        {
            /* Log? */
            PRINT("TX FAILED",0);
        }

        netbuf_delete(activeAudioSession.audio.lwipBuffer);
        memset(&activeAudioSession.audio, 0, sizeof(activeAudioSession.audio));
    }

    return;
}


static THD_FUNCTION(audioRxThreadFunc, arg) 
{
    (void)arg;

    err_t lwipErr = ERR_OK;
    struct netconn *rxConn = NULL;
    struct netbuf *udpRecvBuf = NULL;
    ip_addr_t *remoteIp = NULL;
    uint16_t remotePort = 0;
    uint32_t rxDataLength = 0;
    uint8_t *rxData = NULL;
    rtpSession *rtpSession = NULL;

    chRegSetThreadName(__FUNCTION__);

    while (TRUE) 
    {
        chMBFetch(&audioRxData.readyMailbox,
                  (msg_t *)&rxConn,
                  TIME_INFINITE);

        if (chThdShouldTerminateX())
        {
            return;
        }

        BOARD_LED_GREEN_TOGGLE();

        if (ERR_OK != (lwipErr = netconn_recv(rxConn, &udpRecvBuf)))
        {

        }

        remoteIp     = netbuf_fromaddr(udpRecvBuf);
        remotePort   = netbuf_fromport(udpRecvBuf);
        rxDataLength = netbuf_len(udpRecvBuf);

        if (remotePort == activeAudioSession.config.localRtpPort)
        {
            if (RTP_OK != rtpRxData(rtpSession,
                                    rxData,
                                    rxDataLength))
             {

             }

        }
        else if (remotePort == activeAudioSession.config.localRtcpPort)
        {
            if (RTP_OK != rtpRxControl(rtpSession,
                                       rxData,
                                       rxDataLength))
            {

            }

        }
        else
        {

        }

        netbuf_delete(udpRecvBuf);
    }
}

static void audioTxLwipCb(struct netconn *conn, 
                          enum netconn_evt event,
                          u16_t len)
{
    (void)len;

    PRINT("event is %u", event);
    if (event == NETCONN_EVT_RCVMINUS)
    {
        chMBPost(&audioRxData.readyMailbox, (msg_t)conn, TIME_INFINITE);
    }
}

void audioTxInit(void)
{
    rtpConfig config;

    memset(&config, 0, sizeof(config));

    config.txCb        = audioRtpTransmitCb;
    config.bufAllocCb  = audioRtpControlBufAllocCb;
    config.bufFreeCb   = audioRtpControlBufFreeCb;
    config.getNtpCb    = audioRtpGetNtpTimestampCb;
    config.getRandomCb = audioRtpGetRandomCb;
    config.logCb       = audioRtpLog;

    if (RTP_OK != rtpInit(&config))
    {
        /* AB TODO XXX */
        while(1);
    }

    chMBObjectInit(&audioRxData.readyMailbox,
                   audioRxData.readyMsgs,
                   RX_READY_CONNECTION_NUM);
}

void audioTxShutdown(void)
{
    /* Mailbox */
    /* RTP */
}


void audioTxRtpSetup(audioTxRtpConfig *setupConfig)
{
    rtpSession config;
    err_t lwipErr = ERR_OK;
#if 1
    const char *const cname = "192.168.50.60";
#else
    const char *const cname = "outChannel";
#endif

    activeAudioSession.config = *setupConfig;

    if (NULL == (activeAudioSession.connRtp = 
                        netconn_new_with_proto_and_callback(
                                        NETCONN_UDP,
                                        0,
                                        audioTxLwipCb)))
    {
        DEBUG_CRITICAL("RTP UDP Netconn failed",0);
    }
    
    if (ERR_OK != (lwipErr = netconn_bind(activeAudioSession.connRtp,
                                          IP_ADDR_ANY,
                                          activeAudioSession.config.localRtpPort)))
    {
        DEBUG_CRITICAL("RTP UDP Bind Failed LWIP Error: %d", lwipErr);
    }

    if (NULL == (activeAudioSession.connRtcp = 
                    netconn_new_with_proto_and_callback(
                                        NETCONN_UDP,
                                        0,
                                        audioTxLwipCb)))
    {
        DEBUG_CRITICAL("RTCP UDP Netconn failed",0);
    }
    
    if (ERR_OK !=(lwipErr = netconn_bind(activeAudioSession.connRtcp,
                                         IP_ADDR_ANY,
                                         activeAudioSession.config.localRtcpPort)))
    {
        DEBUG_CRITICAL("RTCP UDP Bind Failed LWIP Error: %d", lwipErr);
    }

    memset(&config, 0, sizeof(config));

    config.userData              = (void*)&activeAudioSession;
    config.periodicTimestampIncr = 16000 / 1000 * 20;

    config.cname.length = strlen(cname);
    memcpy(config.cname.text, cname, config.cname.length);

    if (RTP_OK != rtpSessionInit(&config, &(activeAudioSession.rtpSessionHandle)))
    {
        DEBUG_CRITICAL("RTP Init Failed",0);
    }

#if 0
    audioRxData.thread = chThdCreateStatic(audioRxData.threadWa, 
                                           sizeof(audioRxData.threadWa),
                                           LOWPRIO,
                                           audioRxThreadFunc,
                                           &activeAudioSession);
#endif

#if 0

    struct netbuf *buf = NULL;
    uint8_t *data = NULL;

    while (1)
    {
        buf = netbuf_new();
        data = netbuf_alloc(buf, 400);
        BOARD_LED_BLUE_TOGGLE();

        lwipErr = netconn_sendto(activeAudioSession.connRtp,
                                                buf,
                                                &activeAudioSession.ipDest,
                                                20000);
        netbuf_delete(buf);
    }

#endif
}

void audioTxRtpTeardown(void)
{
    mp45dt02Shutdown();

    if (RTP_OK != rtpSessionShutdown(activeAudioSession.rtpSessionHandle))
    {
        while(1);
    }

    if (ERR_OK != (netconn_delete(activeAudioSession.connRtp)))
    {
        while(1);
    }
}

void audioTxRtpPlay(void)
{
    mp45dt02Config micConfig;
    memset(&micConfig, 0, sizeof(micConfig));
    micConfig.fullbufferCb = audioTxHandleFullMp45dt02Buffer;
    mp45dt02Init(&micConfig);
}

void audioTxRtpPause(void)
{
    mp45dt02Shutdown();
}




