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

#ifndef __RTP_INTERNAL_H__
#define __RTP_INTERNAL_H__

#include <stdint.h>
#include <stdbool.h>

#define RTP_LOG(FMT, ...)                                               \
        rtpLogger("(%s:%d) " FMT "\n\r", __FILE__, __LINE__, __VA_ARGS__)  

#define RTP_TRY_RTN(CALL)                                       \
{                                                               \
    rtpStatus _status = (CALL);                                 \
    if (RTP_OK != _status)                                      \
    {                                                           \
        RTP_LOG("Error: %s", rtpStatusToString(_status));       \
        {                                                       \
            return _status;                                     \
        }                                                       \
    }                                                           \
}

#define RTP_LOG_FAIL(CALL)                                      \
{                                                               \
    rtpStatus _status = (CALL);                                 \
    if (RTP_OK != _status)                                      \
    {                                                           \
        RTP_LOG("Error: %s", rtpStatusToString(_status));       \
    }                                                           \
}


typedef struct {

    /* initial: Flag that is true if the application has not yet sent
     * an RTCP packet. */
    bool haveSentFirstRtcp;

    /* tp: the last time an RTCP packet was transmitted */
    uint32_t lastTxTime;

    /* tn: the next scheduled transmission time of an RTCP packet */
    uint32_t nextTxTime;

    /* pmembers: the estimated number of session members at the time tn
     * was last recomputed */
    uint32_t estimatedSessionMembers;
 
    /* members: the most current estimate for the number of session members */
    uint32_t currentMembers;
 
    /* senders: the most current estimate for the number of senders in the
     * session */
    uint32_t currentSenders;

    /* rtcp_bw: The target RTCP bandwidth, i.e., the total bandwidth
     * that will be used for RTCP packets by all members of this session,
     * in octets per second.  This will be a specified fraction of the
     * "session bandwidth" parameter supplied to the application at startup. */
    uint32_t totalBandwidth;

    /* we_sent: Flag that is true if the application has sent data
     * since the 2nd previous RTCP report was transmitted.  */
    bool haveSentRecentRtp;

    /* avg_rtcp_size: The average compound RTCP packet size, in octets,
     * over all RTCP packets sent and received by this participant.  The
     * size includes lower-layer transport and network protocol headers
     * (e.g., UDP and IP) as explained in Section 6.2. */
    uint32_t avgRtcpSize;
 
} rtcpSessionData;

/* Information about a remote */
typedef struct {
    uint8_t fractionLost;
    uint32_t packetsLost;
    uint32_t extendedHighestSequenceNumberRx;
    uint32_t interarrivalJitter;
    uint32_t lastSrTimestamp;
    uint32_t delaySinceLastSr;
} rtpRemoteReceiverReport;

typedef struct {
    /* The remotes SSRC */
    uint32_t ssrc;
    rtpRemoteReceiverReport lastReceiverReport;
} rtpRemote;

typedef struct {
    rtpSession config;

    /* Internal */

    /* Last transmitted sequence number in RTP */
    uint16_t sequenceNumber;

    /* If RTP packets are generated periodically, the nominal sampling instant
     * as determined from the sampling clock is to be used, not a reading of the
     * system clock.  */
    uint32_t periodicTimestamp;

    struct {
        /* Seconds | Fraction */
        uint32_t lastTx;
        /* Seconds | Fraction */
        uint64_t delta;
    } ntpAtRtpTx;

    struct {
        struct {
            uint32_t txOctets;
            uint32_t txPackets;
            uint32_t txFailedPackets;
        } rtp;

        struct {
            uint32_t txOctets;
            uint32_t txPackets;
            uint32_t txFailedPackets;
        } rtcp;
    } stats;
    
    rtcpSessionData rtcp;

    rtpRemote remote;

} rtpSessionInternal;


rtpStatus rtpBufferGet(const rtpSession *sessionHandle, 
                       void **networkBuffer,
                       uint8_t **data,
                       uint32_t length);

rtpStatus rtpBufferFree(const rtpSession *sessionHandle, 
                       void *networkBuffer,
                       uint8_t *data);

rtpStatus rtpTx(const rtpSession *sessionHandle, 
                rtpType type,
                void *networkBuffer,
                uint8_t *data,
                uint32_t length);

rtpStatus rtpGetNtpTime(uint32_t *seconds,
                        uint32_t *fraction);

rtpStatus rtpGetRand(uint32_t *random);

rtpStatus rtpLogger(const char *fmt, ...);

const char *rtpStatusToString(rtpStatus status);

rtpStatus rtcpTaskInit(rtpSessionInternal *session);
rtpStatus rtcpTaskShutdown(void);

#endif /* Header Guard */
