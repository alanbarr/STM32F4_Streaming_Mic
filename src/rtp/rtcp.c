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
#include "rtp.h"
#include "rtp_internal.h"
#include "rtp_protocol.h"

/* 
 * An implementation which is constrained to two-party
 * unicast operation SHOULD still use randomization of the RTCP
 * transmission interval to avoid unintended synchronization of multiple
 * instances operating in the same environment, but MAY omit the "timer
 * reconsideration" and "reverse reconsideration" algorithms in Sections
 * 6.3.3, 6.3.6 and 6.3.7.
 */

static struct {
    thread_t *thread;
    THD_WORKING_AREA(workingArea, 256);
} rtcpTaskData;

/* 
 * Expect the following layout:
 * 1. Source Description 
 * 2. Item - CNAME
 * 3. Item - END
 */
typedef struct {
    /* Total in bytes. RTP advertised in words = total / 4 - 1 */
    uint16_t total;

    struct {
        uint16_t padding;
        /* Total CNAME block length, including padding. */
        uint16_t block;
    } cname;

} RtpSourceDescriptionLength;

static rtpStatus rtcpGetTimestamps(const rtpSessionInternal *session,
                                   uint32_t *ntpTimeS,
                                   uint32_t *ntpTimeF,
                                   uint32_t *estimatedRtpTs)
{
    /* 
     *  RTP timestamp increments be a fixed known number.
     *
     *  If at every RTP transmission we average the NTP timestamp we know the
     *  realtime between tranmissions, ntp_avg
     *
     *  If we decided to send a RTCP now we:
     *      * read ntp_now
     *      * rtp_count = (ntp_now - ntp_last_tx) / ntp_avg * last_rtp
     */

    uint64_t combined = 0;

    RTP_TRY_RTN(rtpGetNtpTime(ntpTimeS, ntpTimeF));

    combined = (uint64_t)*ntpTimeS << 32 | *ntpTimeF;

    *estimatedRtpTs = (combined - session->ntpAtRtpTx.lastTx) /
                       session->ntpAtRtpTx.delta * session->periodicTimestamp;

    return RTP_OK;
}


static rtpStatus rtcpCheckForTimeout(void)
{
        /* SR , SDES */
    return RTP_OK;
}

/* Entire length of block in bytes 
 * RTP length should be this/4 - 1 */
static uint16_t rtcpPktSenderReportLength(const rtpSessionInternal *session)
{
    (void)session;
    /* If reception stats were required it would be necessary here */
    return sizeof(RtcpPktSenderReport);
}

static rtpStatus rtcpPktSenderReportCreate(const rtpSessionInternal *session,
                                           uint8_t *buffer,
                                           uint32_t length)
{
    (void)session;

    RtcpPktSenderReport *netSender  = (RtcpPktSenderReport*)buffer;
    const uint32_t advertisedLength = length / 4 - 1;

    RTP_TRY_RTN(rtcpGetTimestamps(session, 
                                  &netSender->ntpTimeS,
                                  &netSender->ntpTimeF,
                                  &netSender->rtpTime));

    /* Populate the RtcpPktSenderReport in Network Byte Order */
    netSender->commonHeader = RTCP_COMMON_HEADER_SET(0, 0, RTCP_SR, advertisedLength);
    netSender->ssrc         = HTON32(session->config.ssrc);
    netSender->ntpTimeS     = HTON32(netSender->ntpTimeS);
    netSender->ntpTimeF     = HTON32(netSender->ntpTimeF);
    netSender->rtpTime      = HTON32(netSender->rtpTime);
    netSender->packetsTx    = HTON32(session->stats.rtp.txPackets);
    netSender->octetsTx     = HTON32(session->stats.rtp.txOctets);

    /* AB TODO reciever report ? Do we bother. */
#if 0
    /* Populate the RtcpReportBlock in Network Byte Order */
    netBlock->ssrc          = HTON32();
    netBlock->fractionLost  = HTON32();
    netBlock->jitter        = HTON32();
    netBlock->lastSr        = HTON32();
    netBlock->lastDelay     = HTON32();
#endif

    return RTP_OK;
}

/* Entire length of block in bytes 
 * RTP length should be this/4 - 1 */
static void rtcpPktSourceDescriptionLength(const rtpSessionInternal *session,
                                           RtpSourceDescriptionLength *length)
{
    length->cname.padding   = session->config.cname.length % 4;

    length->cname.block     = sizeof(RtcpSourceDescpItem)   +
                              session->config.cname.length  +
                              length->cname.padding;

    length->total           = sizeof(RtcpPktSourceDescp)    +
                              length->cname.block  
#define END_REQUIRED 0
#if END_REQUIRED
                              + sizeof(uint32_t)
#endif
                              ;  /*END*/
}

static rtpStatus rtcpPktSourceDescriptionCreate(
                        const rtpSessionInternal *session,
                        uint8_t *buffer,
                        RtpSourceDescriptionLength *lengths)
{

    RtcpPktSourceDescp *netSourceDesc = (RtcpPktSourceDescp*)buffer;
    RtcpSourceDescpItem *item         = (RtcpSourceDescpItem*)(netSourceDesc->item);

    /* Populate the RtcpPktSourceDescp in Network Byte Order */
    netSourceDesc->commonHeader = RTCP_COMMON_HEADER_SET(0,
                                                         1,
                                                         RTCP_SDES, 
                                                         (lengths->total/4 - 1));
    netSourceDesc->src          = HTON32(session->config.ssrc);

    /* Populate the CNAME in Network Byte Order */
    item->type   = RTCP_SDES_CNAME;
    item->length = session->config.cname.length;

    memcpy(item->data, session->config.cname.text, session->config.cname.length);
    memset(item->data + session->config.cname.length, lengths->cname.padding, 0);

    /* Move to Next Item */
    item = (RtcpSourceDescpItem *)buffer + 
            sizeof(RtcpPktSourceDescp)   +
            lengths->cname.block;

#if END_REQUIRED
    /* Populate END in Network Byte Order */
    /* RTCP_SDES_END */
    memset(item, 0, sizeof(uint32_t));
#endif

    return RTP_OK;
}

static rtpStatus rtcpTransmitReport(rtpSessionInternal *session)
{
    void *bufferHandle = NULL;
    uint8_t *bufferStart = NULL;   /* First byte of allocated space */
    uint16_t senderReportLength = 0;
    RtpSourceDescriptionLength sourceDescLengths;
    uint16_t totalLength = 0;
    rtpStatus status = RTP_ERROR_INTERNAL;

    memset(&sourceDescLengths, 0, sizeof(sourceDescLengths));

    /* Calculate Length */
    senderReportLength = rtcpPktSenderReportLength(session);
    rtcpPktSourceDescriptionLength(session, &sourceDescLengths);

    totalLength = senderReportLength + sourceDescLengths.total;

    RTP_LOG("senderReportLength: %u sourceDescLengths: %u total: %u",
             senderReportLength, sourceDescLengths.total, totalLength);

    /* Get buffer - How long ?*/
    RTP_TRY_RTN(rtpBufferGet(&session->config, 
                             &bufferHandle,
                             &bufferStart,
                             totalLength));
    
    RTP_LOG_FAIL(status = rtcpPktSenderReportCreate(session,
                                                    bufferStart,
                                                    senderReportLength));
    if (RTP_OK != status)
    {
        goto free_buffer;
    }

    RTP_LOG_FAIL(status = rtcpPktSourceDescriptionCreate(
                                        session,
                                        bufferStart + senderReportLength,
                                        &sourceDescLengths));
    if (RTP_OK != status)
    {
        goto free_buffer;
    }

    RTP_LOG_FAIL(status = rtpTx(&session->config, 
                                 RTP_MSG_TYPE_CONTROL,
                                 bufferHandle,
                                 bufferStart,
                                 totalLength));
    if (RTP_OK != status)
    {
        session->stats.rtcp.txFailedPackets += 1;
        goto free_buffer;
    }
    else
    {
        session->stats.rtcp.txOctets  += totalLength;
        session->stats.rtcp.txPackets += 1;
    }

free_buffer:
    RTP_LOG_FAIL(status = rtpBufferFree(&session->config, 
                                          bufferHandle,
                                          bufferStart));
    return status;
}

static rtpStatus rtcpTransmitBye(void)
{
    return RTP_OK;
}

static rtpStatus rtcpGetTransmissionInterval(uint32_t *intervalMs)
{
    *intervalMs = 1 * 1000;

    /* AB TODO need to detmine the madness for RTCP calcuilations here + random
     * */


    return RTP_OK;
}

/* AB TODO rename to session task */
static THD_FUNCTION(rtcpTask, arg)
{
    rtpSessionInternal *session = (rtpSessionInternal*)arg;
    uint32_t sleepMs = 0;

    chRegSetThreadName(__FUNCTION__);

    while (1)
    {
        if (chThdShouldTerminateX())
        {
            rtcpTransmitBye();
            return;
        }

        rtcpCheckForTimeout();

        if (RTP_OK != rtcpTransmitReport(session))
        {
            RTP_LOG("Tx Failed",0);
        }

        rtcpGetTransmissionInterval(&sleepMs);

        /* AB TODO replace with one shot timer. */
        chThdSleep(MS2ST(sleepMs));

    }
    return;
}


/* AB TODO rename to session task */
rtpStatus rtcpTaskInit(rtpSessionInternal *session)
{
    rtcpTaskData.thread = chThdCreateStatic(rtcpTaskData.workingArea,
                                            sizeof(rtcpTaskData.workingArea),
                                            LOWPRIO,
                                            rtcpTask,
                                            (void*)session);

    if (rtcpTaskData.thread == NULL)
    {
        RTP_TRY_RTN(RTP_ERROR_OS);
    }

    return RTP_OK;
}

rtpStatus rtcpTaskShutdown(void)
{
    chThdTerminate(rtcpTaskData.thread);
    chThdWait(rtcpTaskData.thread);

    return RTP_OK;
}


rtpStatus rtpRxControl(const rtpSession *sessionHandle,
                       uint8_t *data,
                       uint32_t length)
{
    (void)sessionHandle;
    (void)length;

    if (sessionHandle == NULL)
    {
        RTP_TRY_RTN(RTP_ERROR_API);
    }

    if (data == NULL)
    {
        RTP_TRY_RTN(RTP_ERROR_API);
    }

    while (length)
    {
        length --;
    }

    return RTP_OK;
}



