/*******************************************************************************
Copyright (c) 2016, Alan Barr
All rights reserved.
*
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
*
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#include <stdint.h>
#include <string.h>
#include "lwip/api.h"
#include "sntp.h"

/* AB TODO */
#if 0
#include <time.h>
#endif

#define SNTP_PORT                       123
#define SNTP_SERVER                     "uk.pool.ntp.org"
#define SNTP_VERSION_NUMBER             4

#define NTP_PACKET_HEADER_SIZE          48

#define NTP_LEAP_INDICATOR_SHIFT        (6)
#define NTP_LEAP_INDICATOR_MASK         (0x3<<NTP_LEAP_INDICATOR_SHIFT)
#define NTP_VERSION_NUMBER_SHIFT        (3)
#define NTP_VERSION_NUMBER_MASK         (0x7<<NTP_VERSION_NUMBER_SHIFT)
#define NTP_MODE_MASK                   (0x7)

#define NTP_MODE_CLIENT                 3
#define NTP_MODE_SERVER                 4

#define NTP_SECONDS_INDEX               0
#define NTP_SECONDS_FRACTION_INDEX      1

#define NTP_UNIX_EPOCH_DIFFERENCE       2208988800U

#define compile_time_assert(X)                                              \
    extern int (*compile_assert(void))[sizeof(struct {                      \
                unsigned int compile_assert_failed : (X) ? 1 : -1; })]

typedef struct {
    uint8_t lvm;
    uint8_t stratum;
    uint8_t poll;
    uint8_t percision;
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t referenceIndicator;
    uint32_t referenceTimestamp[2];
    uint32_t originateTimestamp[2];
    uint32_t receiveTimestamp[2];
    uint32_t transmitTimestamp[2];
#if 0 /*Optional */
    uint32_t keyIdentifier;
    uint32_t messageDigest[4];
#endif
} ntpPacketHeader;

compile_time_assert(sizeof(ntpPacketHeader) == NTP_PACKET_HEADER_SIZE);

static StatusCode sntpParseTransmitTimestamp(struct netbuf *buf,
                                             uint32_t *ntpSeconds,
                                             uint32_t *ntpSecondsFraction)
{
    ntpPacketHeader *ph = NULL;
    uint8_t *payload = NULL;
    uint16_t length;
    
    if (ERR_OK != netbuf_data(buf, (void**)&payload, &length))
    {
        return STATUS_ERROR_LIBRARY;
    }

    PRINT("Length: %u", length);

    if (length < sizeof(NTP_PACKET_HEADER_SIZE))
    {
        return STATUS_ERROR_EXTERNAL_INPUT;
    }

    ph = (ntpPacketHeader*)payload;
    
    if ((ph->lvm & NTP_MODE_MASK) != NTP_MODE_SERVER)
    {
        return STATUS_ERROR_EXTERNAL_INPUT;
    }

    if (ph->stratum == 0)
    {
        return STATUS_ERROR_EXTERNAL_INPUT;
    }

    *ntpSeconds    = ntohl(ph->transmitTimestamp[NTP_SECONDS_INDEX]);
    *ntpSecondsFraction = ntohl(ph->transmitTimestamp[NTP_SECONDS_FRACTION_INDEX]);

    return STATUS_OK;
}

static StatusCode sntpConstructRequest(uint8_t *buf)
{
    ntpPacketHeader *ph = (ntpPacketHeader*)buf;

    memset(ph, 0, sizeof(ntpPacketHeader));

    ph->lvm = NTP_MODE_CLIENT |
              SNTP_VERSION_NUMBER << NTP_VERSION_NUMBER_SHIFT;

    return STATUS_OK;
}

static StatusCode sntpRequestReceive(struct netbuf *txBuf, struct netbuf **rxBuf)
{
    ip_addr_t serverAddr;
    struct netconn *connection = NULL;
    err_t lwipError = ERR_OK;

    memset(&serverAddr, 0, sizeof(serverAddr));

    if (ERR_OK != (lwipError = netconn_gethostbyname(SNTP_SERVER, &serverAddr)))
    {
        PRINT("lwipError is %d", lwipError);
        return STATUS_ERROR_LIBRARY;
    }


#if 0
    serverAddr.addr = htonl(serverAddr.addr);

#define IP(A,B,C,D)                         htonl(A<<24 | B<<16 | C<<8 | D)
    serverAddr.addr = IP(192,168,1,154);
#endif

    PRINT("IP ADDR is %u.%u.%u.%u", (serverAddr.addr >> 24 & 0xFF),
                                    (serverAddr.addr >> 16 & 0xFF),
                                    (serverAddr.addr >>  8 & 0xFF),
                                    (serverAddr.addr >>  0 & 0xFF));

    if (NULL == (connection = (netconn_new(NETCONN_UDP))))
    {
        PRINT("FAILED",0);
        return STATUS_ERROR_LIBRARY;
    }

    if (ERR_OK != (lwipError = netconn_sendto(connection,
                                              txBuf,
                                              &serverAddr,
                                              SNTP_PORT)))
    {
        PRINT("FAILED",0);
        return STATUS_ERROR_LIBRARY;
    }

    PRINT("Waiting to rx",0);
    if (ERR_OK != (lwipError = netconn_recv(connection, rxBuf)))
    {
        PRINT("FAILED",0);
        return STATUS_ERROR_LIBRARY;
    }

    PRINT("rx'd something",0);

    if (ERR_OK != (lwipError = netconn_delete(connection)))
    {
        PRINT("FAILED",0);
        return STATUS_ERROR_LIBRARY;
    }

    return STATUS_OK;
}


StatusCode sntpGetTime(uint32_t *ntpSeconds,
                       uint32_t *ntpSecondsFraction)
{
    StatusCode rtn = STATUS_OK;
    struct netbuf *txBuf = NULL;
    uint8_t *txPayload = NULL;
    struct netbuf *rxBuf = NULL;
    uint8_t *rxPayload = NULL;

    if (NULL == (txBuf = netbuf_new()))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

    if (NULL == (txPayload = netbuf_alloc(txBuf, NTP_PACKET_HEADER_SIZE)))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

    if (NULL == (rxBuf = netbuf_new()))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

    if (STATUS_OK != (rtn = sntpConstructRequest(txPayload)))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

    if (STATUS_OK != (rtn = sntpRequestReceive(txBuf, &rxBuf)))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

    if (STATUS_OK != (rtn = sntpParseTransmitTimestamp(rxBuf, 
                                                      ntpSeconds,
                                                      ntpSecondsFraction)))
    {
        PRINT("Failed",0);
        goto cleanup;
    }

cleanup:
    netbuf_delete(txBuf);
    netbuf_delete(rxBuf);
    
    return rtn;
}

#if 0

StatusCode clarityTimeFromSntp(clarityTimeDate clarTD, uint32_t sntp)
{
    if (sntp <= NTP_UNIX_EPOCH_DIFFERENCE)
    {
        return CLARITY_ERROR_RANGE;
    }
    
    return clarityTimeFromUnix(clarTD, sntp - NTP_UNIX_EPOCH_DIFFERENCE);
}

StatusCode clarityTimeFromUnix(clarityTimeDate clarTD, time_t unix)
{
    struct tm result;
    time_t time = unix;

    memset(&result, 0, sizeof(result));

    if (gmtime_r(&time, &result) == NULL)
    {
        return CLARITY_ERROR_RANGE;
    }

    if (result.tm_year < 100 || result.tm_year > 199)
    {
        return CLARITY_ERROR_RANGE;
    }

    clarTD->time.hour   = result.tm_hour;
    clarTD->time.minute = result.tm_min;
    clarTD->time.second = result.tm_sec;

    clarTD->date.year  = result.tm_year-100;
    clarTD->date.month = result.tm_mon+1;
    clarTD->date.date  = result.tm_mday;

    if (result.tm_wday == 0)
    {
        clarTD->date.day = 7;
    } 

    else 
    {
        clarTD->date.day = result.tm_wday; 
    }

    return CLARITY_SUCCESS;
}


StatusCode clarityTimeToUnix(clarityTimeDate clarTD, time_t *unix)
{
    struct tm unixBD;
    memset(&unixBD, 0, sizeof(unixBD));

    unixBD.tm_hour = clarTD->time.hour;
    unixBD.tm_min  = clarTD->time.minute;
    unixBD.tm_sec  = clarTD->time.second;

    unixBD.tm_year = clarTD->date.year+100;
    unixBD.tm_mon = clarTD->date.month-1;
    unixBD.tm_mday = clarTD->date.date;
    
    unixBD.tm_isdst = -1; /XXX */

    if((*unix = mktime(&unixBD)) == -1)
    {
        return CLARITY_ERROR_UNDEFINED;
    }

    return CLARITY_SUCCESS;
}

StatusCode clarityTimeIncrement(clarityTimeDate timeDate, uint32_t seconds)
{
    StatusCode rtn;
    time_t unix;

    if ((rtn = clarityTimeToUnix(timeDate, &unix)) != CLARITY_SUCCESS)
    {
        return rtn;
    }

    unix += seconds;

    if ((rtn = clarityTimeFromUnix(timeDate, unix)) != CLARITY_SUCCESS)
    {
        return rtn;
    }

    return CLARITY_SUCCESS;
}
#endif
