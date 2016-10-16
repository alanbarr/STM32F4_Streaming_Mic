/*
A.1 RTP Data Header Validity Checks

   An RTP receiver should check the validity of the RTP header on
   incoming packets since they might be encrypted or might be from a
   different application that happens to be misaddressed.  Similarly, if
   encryption according to the method described in Section 9 is enabled,
   the header validity check is needed to verify that incoming packets
   have been correctly decrypted, although a failure of the header
   validity check (e.g., unknown payload type) may not necessarily
   indicate decryption failure.

   Only weak validity checks are possible on an RTP data packet from a
   source that has not been heard before:

   o  RTP version field must equal 2.

   o  The payload type must be known, and in particular it must not be
      equal to SR or RR.

   o  If the P bit is set, then the last octet of the packet must
      contain a valid octet count, in particular, less than the total
      packet length minus the header size.

   o  The X bit must be zero if the profile does not specify that the
      header extension mechanism may be used.  Otherwise, the extension
      length field must be less than the total packet size minus the
      fixed header length and padding.

   o  The length of the packet must be consistent with CC and payload
      type (if payloads have a known length).

   The last three checks are somewhat complex and not always possible,
   leaving only the first two which total just a few bits.  If the SSRC
   identifier in the packet is one that has been received before, then
   the packet is probably valid and checking if the sequence number is
   in the expected range provides further validation.  If the SSRC
   identifier has not been seen before, then data packets carrying that
   identifier may be considered invalid until a small number of them
   arrive with consecutive sequence numbers.  Those invalid packets MAY
   be discarded or they MAY be stored and delivered once validation has
   been achieved if the resulting delay is acceptable.

   The routine update_seq shown below ensures that a source is declared
   valid only after MIN_SEQUENTIAL packets have been received in
   sequence.  It also validates the sequence number seq of a newly
   received packet and updates the sequence state for the packet's
   source in the structure to which s points.

   When a new source is heard for the first time, that is, its SSRC
   identifier is not in the table (see Section 8.2), and the per-source
   state is allocated for it, s->probation is set to the number of
   sequential packets required before declaring a source valid
   (parameter MIN_SEQUENTIAL) and other variables are initialized:

      init_seq(s, seq);
      s->max_seq = seq - 1;
      s->probation = MIN_SEQUENTIAL;

   A non-zero s->probation marks the source as not yet valid so the
   state may be discarded after a short timeout rather than a long one,
   as discussed in Section 6.2.1.

   After a source is considered valid, the sequence number is considered
   valid if it is no more than MAX_DROPOUT ahead of s->max_seq nor more
   than MAX_MISORDER behind.  If the new sequence number is ahead of
   max_seq modulo the RTP sequence number range (16 bits), but is
   smaller than max_seq, it has wrapped around and the (shifted) count
   of sequence number cycles is incremented.  A value of one is returned
   to indicate a valid sequence number.

   Otherwise, the value zero is returned to indicate that the validation
   failed, and the bad sequence number plus 1 is stored.  If the next
   packet received carries the next higher sequence number, it is
   considered the valid start of a new packet sequence presumably caused
   by an extended dropout or a source restart.  Since multiple complete
   sequence number cycles may have been missed, the packet loss
   statistics are reset.

   Typical values for the parameters are shown, based on a maximum
   misordering time of 2 seconds at 50 packets/second and a maximum
   dropout of 1 minute.  The dropout parameter MAX_DROPOUT should be a
   small fraction of the 16-bit sequence number space to give a
   reasonable probability that new sequence numbers after a restart will
   not fall in the acceptable range for sequence numbers from before the
   restart.
*/

   void init_seq(source *s, uint16_t seq)
   {
       s->base_seq = seq;
       s->max_seq = seq;
       s->bad_seq = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
       s->cycles = 0;
       s->received = 0;
       s->received_prior = 0;
       s->expected_prior = 0;
       /* other initialization */
   }

   int update_seq(source *s, uint16_t seq)
   {
       uint16_t udelta = seq - s->max_seq;
       const int MAX_DROPOUT = 3000;
       const int MAX_MISORDER = 100;
       const int MIN_SEQUENTIAL = 2;

       /*
        * Source is not valid until MIN_SEQUENTIAL packets with
        * sequential sequence numbers have been received.
        */
       if (s->probation) {
           /* packet is in sequence */
           if (seq == s->max_seq + 1) {
               s->probation--;
               s->max_seq = seq;
               if (s->probation == 0) {
                   init_seq(s, seq);
                   s->received++;
                   return 1;
               }
           } else {
               s->probation = MIN_SEQUENTIAL - 1;
               s->max_seq = seq;
           }
           return 0;
       } else if (udelta < MAX_DROPOUT) {
           /* in order, with permissible gap */
           if (seq < s->max_seq) {
               /*
                * Sequence number wrapped - count another 64K cycle.
                */
               s->cycles += RTP_SEQ_MOD;
           }
           s->max_seq = seq;
       } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
           /* the sequence number made a very large jump */
           if (seq == s->bad_seq) {
               /*
                * Two sequential packets -- assume that the other side
                * restarted without telling us so just re-sync
                * (i.e., pretend this was the first packet).
                */
               init_seq(s, seq);
           }
           else {
               s->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
               return 0;
           }
       } else {
           /* duplicate or reordered packet */
       }
       s->received++;
       return 1;
   }

/*
   The validity check can be made stronger requiring more than two
   packets in sequence.  The disadvantages are that a larger number of
   initial packets will be discarded (or delayed in a queue) and that
   high packet loss rates could prevent validation.  However, because
   the RTCP header validation is relatively strong, if an RTCP packet is
   received from a source before the data packets, the count could be
   adjusted so that only two packets are required in sequence.  If
   initial data loss for a few seconds can be tolerated, an application
   MAY choose to discard all data packets from a source until a valid
   RTCP packet has been received from that source.

   Depending on the application and encoding, algorithms may exploit
   additional knowledge about the payload format for further validation.
   For payload types where the timestamp increment is the same for all
   packets, the timestamp values can be predicted from the previous
   packet received from the same source using the sequence number
   difference (assuming no change in payload type).

   A strong "fast-path" check is possible since with high probability
   the first four octets in the header of a newly received RTP data
   packet will be just the same as that of the previous packet from the
   same SSRC except that the sequence number will have increased by one.
   Similarly, a single-entry cache may be used for faster SSRC lookups
   in applications where data is typically received from one source at a
   time.

A.2 RTCP Header Validity Checks

   The following checks should be applied to RTCP packets.

   o  RTP version field must equal 2.

   o  The payload type field of the first RTCP packet in a compound
      packet must be equal to SR or RR.

   o  The padding bit (P) should be zero for the first packet of a
      compound RTCP packet because padding should only be applied, if it
      is needed, to the last packet.

   o  The length fields of the individual RTCP packets must add up to
      the overall length of the compound RTCP packet as received.  This
      is a fairly strong check.

   The code fragment below performs all of these checks.  The packet
   type is not checked for subsequent packets since unknown packet types
   may be present and should be ignored.

*/
      uint32_t len;        /* length of compound RTCP packet in words */
      rtcp_t *r;          /* RTCP header */
      rtcp_t *end;        /* end of compound RTCP packet */

      if ((*(uint16_t *)r & RTCP_VALID_MASK) != RTCP_VALID_VALUE) {
          /* something wrong with packet format */
      }
      end = (rtcp_t *)((uint32_t *)r + len);

      do r = (rtcp_t *)((uint32_t *)r + r->common.length + 1);
      while (r < end && r->common.version == 2);

      if (r != end) {
          /* something wrong with packet format */
      }
/*
A.3 Determining Number of Packets Expected and Lost

   In order to compute packet loss rates, the number of RTP packets
   expected and actually received from each source needs to be known,
   using per-source state information defined in struct source
   referenced via pointer s in the code below.  The number of packets
   received is simply the count of packets as they arrive, including any
   late or duplicate packets.  The number of packets expected can be
   computed by the receiver as the difference between the highest
   sequence number received (s->max_seq) and the first sequence number
   received (s->base_seq).  Since the sequence number is only 16 bits
   and will wrap around, it is necessary to extend the highest sequence
   number with the (shifted) count of sequence number wraparounds
   (s->cycles).  Both the received packet count and the count of cycles
   are maintained the RTP header validity check routine in Appendix A.1.

      extended_max = s->cycles + s->max_seq;
      expected = extended_max - s->base_seq + 1;

   The number of packets lost is defined to be the number of packets
   expected less the number of packets actually received:

      lost = expected - s->received;

   Since this signed number is carried in 24 bits, it should be clamped
   at 0x7fffff for positive loss or 0x800000 for negative loss rather
   than wrapping around.

   The fraction of packets lost during the last reporting interval
   (since the previous SR or RR packet was sent) is calculated from
   differences in the expected and received packet counts across the
   interval, where expected_prior and received_prior are the values
   saved when the previous reception report was generated:

      expected_interval = expected - s->expected_prior;
      s->expected_prior = expected;
      received_interval = s->received - s->received_prior;
      s->received_prior = s->received;
      lost_interval = expected_interval - received_interval;
      if (expected_interval == 0 || lost_interval <= 0) fraction = 0;
      else fraction = (lost_interval << 8) / expected_interval;

   The resulting fraction is an 8-bit fixed point number with the binary
   point at the left edge.


*/

/*
A.4 Generating RTCP SDES Packets

   This function builds one SDES chunk into buffer b composed of argc
   items supplied in arrays type, value and length.  It returns a
   pointer to the next available location within b.

   */
   char *rtp_write_sdes(char *b, uint32_t src, int argc,
                        rtcp_sdes_type_t type[], char *value[],
                        int length[])
   {
       rtcp_sdes_t *s = (rtcp_sdes_t *)b;
       rtcp_sdes_item_t *rsp;
       int i;
       int len;
       int pad;

       /* SSRC header */
       s->src = src;
       rsp = &s->item[0];

       /* SDES items */
       for (i = 0; i < argc; i++) {
           rsp->type = type[i];
           len = length[i];
           if (len > RTP_MAX_SDES) {
               /* invalid length, may want to take other action */
               len = RTP_MAX_SDES;
           }
           rsp->length = len;
           memcpy(rsp->data, value[i], len);
           rsp = (rtcp_sdes_item_t *)&rsp->data[len];
       }

       /* terminate with end marker and pad to next 4-octet boundary */
       len = ((char *) rsp) - b;
       pad = 4 - (len & 0x3);
       b = (char *) rsp;
       while (pad--) *b++ = RTCP_SDES_END;

       return b;
   }

/*

A.5 Parsing RTCP SDES Packets

   This function parses an SDES packet, calling functions find_member()
   to find a pointer to the information for a session member given the
   SSRC identifier and member_sdes() to store the new SDES information
   for that member.  This function expects a pointer to the header of
   the RTCP packet.

   */
   void rtp_read_sdes(rtcp_t *r)
   {
       int count = r->common.count;
       rtcp_sdes_t *sd = &r->r.sdes;
       rtcp_sdes_item_t *rsp, *rspn;
       rtcp_sdes_item_t *end = (rtcp_sdes_item_t *)
                               ((uint32_t *)r + r->common.length + 1);
       source *s;

       while (--count >= 0) {
           rsp = &sd->item[0];
           if (rsp >= end) break;
           s = find_member(sd->src);

           for (; rsp->type; rsp = rspn ) {
               rspn = (rtcp_sdes_item_t *)((char*)rsp+rsp->length+2);
               if (rspn >= end) {
                   rsp = rspn;
                   break;
               }
               member_sdes(s, rsp->type, rsp->data, rsp->length);
           }
           sd = (rtcp_sdes_t *)
                ((uint32_t *)sd + (((char *)rsp - (char *)sd) >> 2)+1);
       }
       if (count >= 0) {
           /* invalid packet format */
       }
   }

/*
A.6 Generating a Random 32-bit Identifier

   The following subroutine generates a random 32-bit identifier using
   the MD5 routines published in RFC 1321 [32].  The system routines may
   not be present on all operating systems, but they should serve as
   hints as to what kinds of information may be used.  Other system
   calls that may be appropriate include

   o  getdomainname(),

   o  getwd(), or

   o  getrusage().

   "Live" video or audio samples are also a good source of random
   numbers, but care must be taken to avoid using a turned-off
   microphone or blinded camera as a source [17].

   Use of this or a similar routine is recommended to generate the
   initial seed for the random number generator producing the RTCP
   period (as shown in Appendix A.7), to generate the initial values for
   the sequence number and timestamp, and to generate SSRC values.
   Since this routine is likely to be CPU-intensive, its direct use to
   generate RTCP periods is inappropriate because predictability is not
   an issue.  Note that this routine produces the same result on
   repeated calls until the value of the system clock changes unless
   different values are supplied for the type argument.
*/
   /*
    * Generate a random 32-bit quantity.
    */
   #include <sys/types.h>   /* u_long */
   #include <sys/time.h>    /* gettimeofday() */
   #include <unistd.h>      /* get..() */
   #include <stdio.h>       /* printf() */
   #include <time.h>        /* clock() */
   #include <sys/utsname.h> /* uname() */
   #include "global.h"      /* from RFC 1321 */
   #include "md5.h"         /* from RFC 1321 */

   #define MD_CTX MD5_CTX
   #define MDInit MD5Init
   #define MDUpdate MD5Update
   #define MDFinal MD5Final

   static u_long md_32(char *string, int length)
   {
       MD_CTX context;
       union {
           char   c[16];
           u_long x[4];
       } digest;
       u_long r;
       int i;

       MDInit (&context);


       MDUpdate (&context, string, length);
       MDFinal ((unsigned char *)&digest, &context);
       r = 0;
       for (i = 0; i < 3; i++) {
           r ^= digest.x[i];
       }
       return r;
   }                               /* md_32 */

   /*
    * Return random unsigned 32-bit quantity.  Use 'type' argument if
    * you need to generate several different values in close succession.
    */
   uint32_t random32(int type)
   {
       struct {
           int     type;
           struct  timeval tv;
           clock_t cpu;
           pid_t   pid;
           u_long  hid;
           uid_t   uid;
           gid_t   gid;
           struct  utsname name;
       } s;

       gettimeofday(&s.tv, 0);
       uname(&s.name);
       s.type = type;
       s.cpu  = clock();
       s.pid  = getpid();
       s.hid  = gethostid();
       s.uid  = getuid();
       s.gid  = getgid();
       /* also: system uptime */

       return md_32((char *)&s, sizeof(s));
   }                               /* random32 */

/*
A.7 Computing the RTCP Transmission Interval

   The following functions implement the RTCP transmission and reception
   rules described in Section 6.2.  These rules are coded in several
   functions:

   o  rtcp_interval() computes the deterministic calculated interval,
      measured in seconds.  The parameters are defined in Section 6.3.




   o  OnExpire() is called when the RTCP transmission timer expires.

   o  OnReceive() is called whenever an RTCP packet is received.

   Both OnExpire() and OnReceive() have event e as an argument.  This is
   the next scheduled event for that participant, either an RTCP report
   or a BYE packet.  It is assumed that the following functions are
   available:

   o  Schedule(time t, event e) schedules an event e to occur at time t.
      When time t arrives, the function OnExpire is called with e as an
      argument.

   o  Reschedule(time t, event e) reschedules a previously scheduled
      event e for time t.

   o  SendRTCPReport(event e) sends an RTCP report.

   o  SendBYEPacket(event e) sends a BYE packet.

   o  TypeOfEvent(event e) returns EVENT_BYE if the event being
      processed is for a BYE packet to be sent, else it returns
      EVENT_REPORT.

   o  PacketType(p) returns PACKET_RTCP_REPORT if packet p is an RTCP
      report (not BYE), PACKET_BYE if its a BYE RTCP packet, and
      PACKET_RTP if its a regular RTP data packet.

   o  ReceivedPacketSize() and SentPacketSize() return the size of the
      referenced packet in octets.

   o  NewMember(p) returns a 1 if the participant who sent packet p is
      not currently in the member list, 0 otherwise.  Note this function
      is not sufficient for a complete implementation because each CSRC
      identifier in an RTP packet and each SSRC in a BYE packet should
      be processed.

   o  NewSender(p) returns a 1 if the participant who sent packet p is
      not currently in the sender sublist of the member list, 0
      otherwise.

   o  AddMember() and RemoveMember() to add and remove participants from
      the member list.

   o  AddSender() and RemoveSender() to add and remove participants from
      the sender sublist of the member list.


   These functions would have to be extended for an implementation that
   allows the RTCP bandwidth fractions for senders and non-senders to be
   specified as explicit parameters rather than fixed values of 25% and
   75%.  The extended implementation of rtcp_interval() would need to
   avoid division by zero if one of the parameters was zero.

   double rtcp_interval(int members,
                        int senders,
                        double rtcp_bw,
                        int we_sent,
                        double avg_rtcp_size,
                        int initial)
   {
       /*
        * Minimum average time between RTCP packets from this site (in
        * seconds).  This time prevents the reports from `clumping' when
        * sessions are small and the law of large numbers isn't helping
        * to smooth out the traffic.  It also keeps the report interval
        * from becoming ridiculously small during transient outages like
        * a network partition.
        */
       double const RTCP_MIN_TIME = 5.;
       /*
        * Fraction of the RTCP bandwidth to be shared among active
        * senders.  (This fraction was chosen so that in a typical
        * session with one or two active senders, the computed report
        * time would be roughly equal to the minimum report time so that
        * we don't unnecessarily slow down receiver reports.)  The
        * receiver fraction must be 1 - the sender fraction.
        */
       double const RTCP_SENDER_BW_FRACTION = 0.25;
       double const RTCP_RCVR_BW_FRACTION = (1-RTCP_SENDER_BW_FRACTION);
       /*
       /* To compensate for "timer reconsideration" converging to a
        * value below the intended average.
        */
       double const COMPENSATION = 2.71828 - 1.5;

       double t;                   /* interval */
       double rtcp_min_time = RTCP_MIN_TIME;
       int n;                      /* no. of members for computation */

       /*
        * Very first call at application start-up uses half the min
        * delay for quicker notification while still allowing some time
        * before reporting for randomization and to learn about other
        * sources so the report interval will converge to the correct
        * interval more quickly.


        */
       if (initial) {
           rtcp_min_time /= 2;
       }
       /*
        * Dedicate a fraction of the RTCP bandwidth to senders unless
        * the number of senders is large enough that their share is
        * more than that fraction.
        */
       n = members;
       if (senders <= members * RTCP_SENDER_BW_FRACTION) {
           if (we_sent) {
               rtcp_bw *= RTCP_SENDER_BW_FRACTION;
               n = senders;
           } else {
               rtcp_bw *= RTCP_RCVR_BW_FRACTION;
               n -= senders;
           }
       }

       /*
        * The effective number of sites times the average packet size is
        * the total number of octets sent when each site sends a report.
        * Dividing this by the effective bandwidth gives the time
        * interval over which those packets must be sent in order to
        * meet the bandwidth target, with a minimum enforced.  In that
        * time interval we send one report so this time is also our
        * average time between reports.
        */
       t = avg_rtcp_size * n / rtcp_bw;
       if (t < rtcp_min_time) t = rtcp_min_time;

       /*
        * To avoid traffic bursts from unintended synchronization with
        * other sites, we then pick our actual next report interval as a
        * random number uniformly distributed between 0.5*t and 1.5*t.
        */
       t = t * (drand48() + 0.5);
       t = t / COMPENSATION;
       return t;
   }

   void OnExpire(event e,
                 int    members,
                 int    senders,
                 double rtcp_bw,
                 int    we_sent,
                 double *avg_rtcp_size,


                 int    *initial,
                 time_tp   tc,
                 time_tp   *tp,
                 int    *pmembers)
   {
       /* This function is responsible for deciding whether to send an
        * RTCP report or BYE packet now, or to reschedule transmission.
        * It is also responsible for updating the pmembers, initial, tp,
        * and avg_rtcp_size state variables.  This function should be
        * called upon expiration of the event timer used by Schedule().
        */

       double t;     /* Interval */
       double tn;    /* Next transmit time */

       /* In the case of a BYE, we use "timer reconsideration" to
        * reschedule the transmission of the BYE if necessary */

       if (TypeOfEvent(e) == EVENT_BYE) {
           t = rtcp_interval(members,
                             senders,
                             rtcp_bw,
                             we_sent,
                             *avg_rtcp_size,
                             *initial);
           tn = *tp + t;
           if (tn <= tc) {
               SendBYEPacket(e);
               exit(1);
           } else {
               Schedule(tn, e);
           }

       } else if (TypeOfEvent(e) == EVENT_REPORT) {
           t = rtcp_interval(members,
                             senders,
                             rtcp_bw,
                             we_sent,
                             *avg_rtcp_size,
                             *initial);
           tn = *tp + t;
           if (tn <= tc) {
               SendRTCPReport(e);
               *avg_rtcp_size = (1./16.)*SentPacketSize(e) +
                   (15./16.)*(*avg_rtcp_size);
               *tp = tc;

               /* We must redraw the interval.  Don't reuse the
                  one computed above, since its not actually
                  distributed the same, as we are conditioned
                  on it being small enough to cause a packet to
                  be sent */

               t = rtcp_interval(members,
                                 senders,
                                 rtcp_bw,
                                 we_sent,
                                 *avg_rtcp_size,
                                 *initial);

               Schedule(t+tc,e);
               *initial = 0;
           } else {
               Schedule(tn, e);
           }
           *pmembers = members;
       }
   }

   void OnReceive(packet p,
                  event e,
                  int *members,
                  int *pmembers,
                  int *senders,
                  double *avg_rtcp_size,
                  double *tp,
                  double tc,
                  double tn)
   {
       /* What we do depends on whether we have left the group, and are
        * waiting to send a BYE (TypeOfEvent(e) == EVENT_BYE) or an RTCP
        * report.  p represents the packet that was just received.  */

       if (PacketType(p) == PACKET_RTCP_REPORT) {
           if (NewMember(p) && (TypeOfEvent(e) == EVENT_REPORT)) {
               AddMember(p);
               *members += 1;
           }
           *avg_rtcp_size = (1./16.)*ReceivedPacketSize(p) +
               (15./16.)*(*avg_rtcp_size);
       } else if (PacketType(p) == PACKET_RTP) {
           if (NewMember(p) && (TypeOfEvent(e) == EVENT_REPORT)) {
               AddMember(p);
               *members += 1;
           }
           if (NewSender(p) && (TypeOfEvent(e) == EVENT_REPORT)) {


               AddSender(p);
               *senders += 1;
           }
       } else if (PacketType(p) == PACKET_BYE) {
           *avg_rtcp_size = (1./16.)*ReceivedPacketSize(p) +
               (15./16.)*(*avg_rtcp_size);

           if (TypeOfEvent(e) == EVENT_REPORT) {
               if (NewSender(p) == FALSE) {
                   RemoveSender(p);
                   *senders -= 1;
               }

               if (NewMember(p) == FALSE) {
                   RemoveMember(p);
                   *members -= 1;
               }

               if (*members < *pmembers) {
                   tn = tc +
                       (((double) *members)/(*pmembers))*(tn - tc);
                   *tp = tc -
                       (((double) *members)/(*pmembers))*(tc - *tp);

                   /* Reschedule the next report for time tn */

                   Reschedule(tn, e);
                   *pmembers = *members;
               }

           } else if (TypeOfEvent(e) == EVENT_BYE) {
               *members += 1;
           }
       }
   }


/*
A.8 Estimating the Interarrival Jitter

   The code fragments below implement the algorithm given in Section
   6.4.1 for calculating an estimate of the statistical variance of the
   RTP data interarrival time to be inserted in the interarrival jitter
   field of reception reports.  The inputs are r->ts, the timestamp from
   the incoming packet, and arrival, the current time in the same units.
   Here s points to state for the source; s->transit holds the relative
   transit time for the previous packet, and s->jitter holds the
   estimated jitter.  The jitter field of the reception report is
   measured in timestamp units and expressed as an unsigned integer, but
   the jitter estimate is kept in a floating point.  As each data packet
   arrives, the jitter estimate is updated:

      int transit = arrival - r->ts;
      int d = transit - s->transit;
      s->transit = transit;
      if (d < 0) d = -d;
      s->jitter += (1./16.) * ((double)d - s->jitter);

   When a reception report block (to which rr points) is generated for
   this member, the current jitter estimate is returned:

      rr->jitter = (uint32_t) s->jitter;

   Alternatively, the jitter estimate can be kept as an integer, but
   scaled to reduce round-off error.  The calculation is the same except
   for the last line:

      s->jitter += d - ((s->jitter + 8) >> 4);

   In this case, the estimate is sampled for the reception report as:

      rr->jitter = s->jitter >> 4;

*/

