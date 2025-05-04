#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   SR protocol.
**********************************************************************/

#define RTT 16.0      /* round trip time.  MUST BE SET TO 16.0 when submitting assignment */
#define WINDOWSIZE 6  /* the maximum number of buffered unacked packet \
                        MUST BE SET TO 6 when submitting assignment */
#define SEQSPACE 12    /* the min sequence space for SR */
#define NOTINUSE (-1) /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for (i = 0; i < 20; i++)
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}

/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE]; /* array for storing packets waiting for ACK */
static bool ack[WINDOWSIZE];          /* ack for pkt*/
static int buffer_index;     /* index of buffer */
static int windowfirst;               /* starting number window sequence */
static int windowcount;               /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;              /* the next sequence number to be used by the sender */

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int buffer_position;
  int i;

  /* if not blocked waiting on ACK */
  if (windowcount < WINDOWSIZE)
  {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    /* create packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (i = 0; i < 20; i++)
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    /* put packet in window buffer */
    /* windowlast will always be 0 for alternating bit; but not for GoBackN */
    buffer_position = (buffer_index + 1) % WINDOWSIZE;
    buffer[buffer_position] = sendpkt;
 ack[buffer_position] = false;
    windowcount++;

    /* send out packet */
    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3(A, sendpkt);

    /* start timer if first packet in window */
    if (windowcount == 1)
      starttimer(A, RTT);

    /* get next sequence number, wrap back to 0 */
    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  }
  /* if blocked,  window is full */
  else
  {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}

/* called from layer 3, when a packet arrives for layer 4
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{
  int pkt_acknum;
  int seqnum;
  int buffer_position;

  /* if received ACK is not corrupted */
  if (!IsCorrupted(packet))
  {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
    total_ACKs_received++;
 
 pkt_acknum = packet.acknum;
 seqnum = (pkt_acknum - windowfirst + SEQSPACE) % SEQSPACE;

    if(seqnum < WINDOWSIZE){
  buffer_position = (buffer_index + seqnum) % WINDOWSIZE;
  
  if(!ack[buffer_position]){
   new_ACKs++;
   ack[buffer_position] = true;
   
   while(windowcount > 0 && ack[buffer_index]){
    ack[buffer_index] = false;
    buffer_index = (buffer_index + 1) % WINDOWSIZE;
    windowfirst = (windowfirst + 1) % SEQSPACE;
    windowcount--;
   }
   /* start timer again if there are still more unacked packets in window */
   stoptimer(A);
   if (windowcount > 0)
     starttimer(A, RTT);
  }
    }
  }
  else if (TRACE > 0)
    printf("----A: corrupted ACK is received, do nothing!\n");
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  int i;
  int seq;

  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  for (i = 0; i < WINDOWSIZE; i++)
  {
 seq = (buffer_index + i) % WINDOWSIZE;
    if (!ack[seq]){
  if (TRACE > 0)
    printf("---A: resending packet %d\n", buffer[seq].seqnum);
  tolayer3(A, buffer[seq]);
  packets_resent++;
 }
  }
  starttimer(A, RTT);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
  int i;
  /* initialise A's window, buffer and sequence number */
  A_nextseqnum = 0; /* A starts with seq num 0, do not change this */
  windowfirst = 0;
  windowcount = 0;
  buffer_index = 0;
  for(i = 0; i < WINDOWSIZE; i++){
   ack[i] = false;
  }
}

/********* Receiver (B)  variables and procedures ************/

static int expectedseqnum; /* the sequence number expected next by the receiver */
static int B_nextseqnum;   /* the sequence number for the next packets sent by B */
static struct pkt receive_buffer[WINDOWSIZE];
static bool receive_ack[WINDOWSIZE];

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;
  int seqnum;
  int position;

  sendpkt.seqnum = B_nextseqnum;
  B_nextseqnum = (B_nextseqnum + 1) % 2;
 
  for(i = 0; i < 20; i++)
   sendpkt.payload[i] = '0';
 
  if (!IsCorrupted(packet)){
   if (TRACE > 0)
  printf("----B: packet %d is correctly received, send ACK!\n", packet.seqnum);
 seqnum = packet.seqnum;
 position = (seqnum - expectedseqnum + SEQSPACE) % SEQSPACE;
 if (position < WINDOWSIZE && !receive_ack[position]) {
     receive_ack[position] = true;
     receive_buffer[position] = packet;
     packets_received++;

     while (receive_ack[0]) {
         tolayer5(B,receive_buffer[0].payload);
         for (i = 0; i < WINDOWSIZE - 1; i++) {
             receive_buffer[i] = receive_buffer[i + 1];
             receive_ack[i]    = receive_ack[i + 1];
         }
         receive_ack[expectedseqnum % WINDOWSIZE] = false;
         expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
     }
     sendpkt.acknum = seqnum;
 } else {
     if (TRACE > 0) 
         printf("----B: packet corrupted or not expected sequence number, resend ACK!\n");
     }
     sendpkt.acknum = (expectedseqnum - 1 + SEQSPACE) % SEQSPACE;
 }


  /* computer checksum */
  sendpkt.checksum = ComputeChecksum(sendpkt);

  /* send out packet */
  tolayer3(B, sendpkt);
}
/* the following routine will be called once (only) before any 
   entity B routines are called. You can use it to do any init */
   void B_init(void)
   {
       int i;
       expectedseqnum = 0;
       B_nextseqnum   = 1;
       for (i = 0; i < WINDOWSIZE; i++) {
           receive_ack[i] = false;
       }
   }
   
/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}
