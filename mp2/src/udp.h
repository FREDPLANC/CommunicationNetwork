#ifndef UDP_H
#define UDP_H

#define MSS 2000
#define SSTHRESH_N 256
#define TIMEOUT 100000
#define BUFFERR_RECVMAX 2000

enum sender_state {SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY};

/* Types of packet*/
enum msg_t{
    ACK,
    DATA,
    FIN,
    FIN_ACK
};

typedef struct
{
   int data_size;
   int seq_idx; /* sequence number: the possible index of the first bytes of the packets to send, start from 0 */
   int ack_idx; /* acknowledged number: the index of the first bytes of the expected next packets to recieve, start from 0 */
   msg_t msg_type; /* denote the type of this packet */
   char data[MSS];
}pkt;

#endif


