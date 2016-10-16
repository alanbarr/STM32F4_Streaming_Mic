/*
   We provide examples of C code for aspects of RTP sender and receiver
   algorithms.  There may be other implementation methods that are
   faster in particular operating environments or have other advantages.
   These implementation notes are for informational purposes only and
   are meant to clarify the RTP specification.

   The following definitions are used for all examples; for clarity and
   brevity, the structure definitions are only valid for 32-bit big-
   endian (most significant octet first) architectures.  Bit fields are
   assumed to be packed tightly in big-endian bit order, with no
   additional padding.  Modifications would be required to construct a
   portable implementation.
*/
/*
 * rtp.h  --  RTP header file
 */
#ifndef __RTP_PROTOCOL_H__
#define __RTP_PROTOCOL_H__ 

#include <stdint.h>

/*
 * Current protocol version.
 */
#define RTP_VERSION         2

#define RTP_SEQ_MOD         (1<<16)
#define RTP_MAX_SDES        255      /* maximum text length for SDES */

/*
 * Big-endian mask for version, padding bit and packet type pair
 */
#define RTCP_VALID_MASK     (0xc000 | 0x2000 | 0xfe)
#define RTCP_VALID_VALUE    ((RTP_VERSION << 14) | RTCP_SR)

#define HTON32(H32)         (__builtin_bswap32(H32))
#define HTON16(H16)         (__builtin_bswap16(H16))
#define NTOH32(N32)         (HTON32(N32))
#define NTOH16(N16)         (HTON16(N16))

#define COMPILE_ASSERT(EXPR) \
    extern int (*compileAssert(void)) \
                    [sizeof(struct {unsigned int assert : (EXPR) ? 1 : -1;})]

typedef enum {
    RTCP_SR   = 200,
    RTCP_RR   = 201,
    RTCP_SDES = 202,
    RTCP_BYE  = 203,
    RTCP_APP  = 204
} rtpControlType;

typedef enum {
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/*
 * RTP data header
 */
typedef struct {
    /* 0 1 2 3 4 5 6 7
     *|V  |P|X|  CC   | */
    uint8_t verPadExCC;
    /* 0 1 2 3 4 5 6 7
     *|M|     PT      | */
    uint8_t markerPayloadType;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[0];
} rtpDataHeader;

COMPILE_ASSERT(sizeof(rtpDataHeader) == RTP_HEADER_LENGTH);

/*
 * SDES item
 */
typedef struct {
    uint8_t type;               /* type of item  */
    uint8_t length;             /* length of item (in octets) */
    char data[0];               /* text, not null-terminated */
} RtcpSourceDescpItem;

COMPILE_ASSERT(sizeof(RtcpSourceDescpItem) == 2);

/*
 * RTCP common header word
 */
typedef struct {
    unsigned int version:2;   /* protocol version */
    unsigned int p:1;         /* padding flag */
    unsigned int count:5;     /* varies by packet type */
    unsigned int pt:8;        /* RTCP packet type */
    uint16_t length;          /* pkt len in words, w/o this word */
} rtcp_common_t;


/******************************************************************************/
/* RTCP */
/******************************************************************************/
/*
 * RTCP Common Header 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|    RC   |   PT=SR=200   |             length            |
 */
#define RTCP_COMMON_HEADER_GET_VERSION(CH)      (HTON32(CH) & (0x03<<30) >> 30)
#define RTCP_COMMON_HEADER_GET_RECEPTION_RC(CH) (HTON32(CH) & (0x1F<<24) >> 24)
#define RTCP_COMMON_HEADER_GET_PACKET_TYPE(CH)  (HTON32(CH) & (0xFF<<16) >> 16)
#define RTCP_COMMON_HEADER_GET_LENGTH(CH)       (HTON32(CH) & (0xFFFF))

#define RTCP_COMMON_HEADER_SET(PAD, RC, PT, LEN) \
    HTON32((LEN  & 0xFFFF)        |          \
           ((PT  & 0x00FF) <<16)  |          \
           ((RC  & 0x001F) <<24)  |          \
           ((PAD & 0x0001) <<29)  |          \
            (2             <<30))

/*
 * Reception report block
 */
typedef struct {
    uint32_t ssrc;             /* data source being reported */
    /* Faction Lost : 8 bits
     * Lost Packets : 24 bits (signed) ?
     */
    uint32_t fractionLost;
    uint32_t lastSeqNoRx;       /* extended last seq. no. received */
    uint32_t jitter;            /* interarrival jitter */
    uint32_t lastSr;            /* last SR packet from this source */
    uint32_t lastDelay;         /* delay since last SR packet */
} RtcpRxReportBlock;

COMPILE_ASSERT(sizeof(RtcpRxReportBlock) == 24);

/* sender report (SR) */
typedef struct {
    uint32_t commonHeader;      /* common header */
    uint32_t ssrc;              /* sender generating this report */
    uint32_t ntpTimeS;          /* NTP timestamp */
    uint32_t ntpTimeF;
    uint32_t rtpTime;           /* RTP timestamp */
    uint32_t packetsTx;         /* packets sent */
    uint32_t octetsTx;          /* octets sent */
    RtcpRxReportBlock report[0];  /* variable-length list */
} RtcpPktSenderReport;

COMPILE_ASSERT(sizeof(RtcpPktSenderReport) == 28);

/* reception report (RR) */
typedef struct {
    uint32_t commonHeader;      /* common header */
    uint32_t ssrc;              /* receiver generating this report */
    RtcpRxReportBlock report[0];  /* variable-length list */
} RtcpPktRecvReport;

COMPILE_ASSERT(sizeof(RtcpPktRecvReport) == 8);

/* source description (SDES) */
typedef struct {
    uint32_t commonHeader;          /* common header */
    uint32_t src;                   /* first SSRC/CSRC */
    RtcpSourceDescpItem item[0];    /* list of SDES items */
} RtcpPktSourceDescp;

COMPILE_ASSERT(sizeof(RtcpPktSourceDescp) == 8);

/* BYE */
typedef struct {
    uint32_t commonHeader;      /* common header */
    uint32_t src[0];            /* list of sources */
} RtcpPktBye;

COMPILE_ASSERT(sizeof(RtcpPktBye) == 4);



#if 0
/*
 * Per-source state information
 */
typedef struct {
    uint16_t max_seq;        /* highest seq. number seen */
    uint32_t cycles;         /* shifted count of seq. number cycles */
    uint32_t base_seq;       /* base seq number */
    uint32_t bad_seq;        /* last 'bad' seq number + 1 */
    uint32_t probation;      /* sequ. packets till source is valid */
    uint32_t received;       /* packets received */
    uint32_t expected_prior; /* packet expected at last interval */
    uint32_t received_prior; /* packet received at last interval */
    uint32_t transit;        /* relative trans time for prev pkt */
    uint32_t jitter;         /* estimated jitter */
    /* ... */
} source;
#endif


#endif /* Header Guard */
