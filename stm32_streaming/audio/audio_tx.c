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
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"

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

#define TX_BUFFER_LENGTH                    (RTP_HEADER_LENGTH + \
                                             AUDIO_PAYLOAD_BUFFER_SIZE_BYTES)

typedef struct {
    /* API */
    audioTxRtpConfig config;

    /* Private */

    /* The Remote UDP connection to send audio data towards */
    struct netconn *connRtp;
    
    struct {
        /* The current LWIP buffer being used for transmission */
        struct netbuf *lwipBuffer;
        /* The first bytes of the buffer with MP45DT02 data */
        uint8_t *dataStart;
        uint8_t *dataCurrent;

        struct {
            uint32_t failedNetbufNew;
            uint32_t failedNetbufAlloc;
            uint32_t failedSend;
        } debug;
    } audio;

} audioTxSession;

static audioTxSession activeAudioSession;

/******************************************************************************/
/* Internal Functions                                                         */
/******************************************************************************/

static StatusCode audioRtpGetRandomCb(uint32_t *random)
{
    if (STATUS_OK != randomGet(random))
    {
        return STATUS_ERROR_CALLBACK;
    }

    return STATUS_OK;
}

static void audioTxHandleFullMp45dt02Buffer(float *data,
                                            uint16_t samples)       
{
    uint32_t index = 0;
    int16_t *sample = NULL;
    err_t lwipErr = ERR_OK;

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
                                 TX_BUFFER_LENGTH);

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

    /*****************s********************************************************/ 
    /* Change to network order                                                */
    /**************************************************************************/ 
    for (index = 0, sample = (int16_t *)activeAudioSession.audio.dataCurrent;
         index < samples;
         index++)
    {
        *sample++ = htons((int16_t)(*data++));
    }

    activeAudioSession.audio.dataCurrent += samples * 2;

    /**************************************************************************/ 
    /* Transmit if full                                                       */
    /**************************************************************************/ 
    if (activeAudioSession.audio.dataCurrent >= 
            (activeAudioSession.audio.dataStart + TX_BUFFER_LENGTH))
    {
        if (activeAudioSession.audio.dataCurrent >
            (activeAudioSession.audio.dataStart + TX_BUFFER_LENGTH))
        {
            PRINT_CRITICAL("Overrun %u > %u ", 
                            activeAudioSession.audio.dataCurrent,
                            activeAudioSession.audio.dataStart + TX_BUFFER_LENGTH);
        }
    
        if (STATUS_OK != rtpAddHeader(activeAudioSession.audio.dataStart,
                                   TX_BUFFER_LENGTH))
        {
            PRINT_CRITICAL("Rtp Add Header failed", 0);
        }

        if (ERR_OK != (lwipErr = netconn_sendto(
                                    activeAudioSession.connRtp,
                                    activeAudioSession.audio.lwipBuffer,
                                    &activeAudioSession.config.ipDest,
                                    activeAudioSession.config.remoteRtpPort)))
        {
            activeAudioSession.audio.debug.failedSend++;
        }

        netbuf_delete(activeAudioSession.audio.lwipBuffer);
        memset(&activeAudioSession.audio, 0, sizeof(activeAudioSession.audio));
    }

    return;
}

/******************************************************************************/
/* External Functions                                                         */
/******************************************************************************/

void audioTxRtpSetup(audioTxRtpConfig *setupConfig)
{
    rtpConfig config;
    err_t lwipErr = ERR_OK;

    activeAudioSession.config = *setupConfig;

    if (NULL == (activeAudioSession.connRtp = netconn_new(NETCONN_UDP)))
    {
        PRINT_CRITICAL("RTP UDP Netconn failed",0);
    }
    
    if (ERR_OK != (lwipErr = netconn_bind(activeAudioSession.connRtp,
                                          IP_ADDR_ANY,
                                          activeAudioSession.config.localRtpPort)))
    {
        PRINT_CRITICAL("RTP UDP Bind Failed LWIP Error: %d", lwipErr);
    }

    memset(&config, 0, sizeof(config));

    config.getRandomCb = audioRtpGetRandomCb;
    config.periodicTimestampIncr = 16000 / 1000 * 20;

    if (STATUS_OK != rtpInit(&config))
    {
        PRINT_CRITICAL("RTP Init Failed",0);
    }
}

void audioTxRtpTeardown(void)
{
    mp45dt02Shutdown();

    if (STATUS_OK != rtpShutdown())
    {
        PRINT_CRITICAL("RTP Shutdown failed",0);
    }

    if (ERR_OK != (netconn_delete(activeAudioSession.connRtp)))
    {
        PRINT_CRITICAL("NETCONN Delete failed",0);
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

