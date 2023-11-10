#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>


#include <iostream>
#include <queue>
#include <math.h>
#include <errno.h>
// #include "udp.h"
#include <cstdio>

using namespace std;

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


struct sockaddr_in si_other;
int s, slen;
int pkt_num_total, bytes_total;
int  bytes_sent, seq_idx; 
int pkt_acked;
int rp_ack;
sender_state status= SLOW_START;
double cwnd = 1.0;
double ssthresh = SSTHRESH_N * cwnd;

queue<pkt> wait_queue;


void init_param(unsigned long long int bytesToTransfer);
void setSockTimeout(int s);
void timeout_handler();
void state_switch();
void slide_window_send(FILE* fp);
void send_new_pkt(pkt* packet);
void diep(char *s);


void diep(char *s) {
    perror(s);
    exit(1);
}

void send_new_pkt(pkt* packet){
    if (sendto(s, packet, sizeof(pkt), 0, (struct sockaddr*)&si_other, sizeof(si_other))== -1){
        diep("sendto()");
    }
}

void init_param(unsigned long long int bytesToTransfer){
    pkt_num_total = ceil((1.0*bytesToTransfer)/MSS);
    bytes_sent = 0;
    bytes_total = bytesToTransfer;
    pkt_acked = 0;
    rp_ack = 0;
    seq_idx = 0;

    return;
}

void setSockTimeout(int s){
    struct timeval RTT_TO;
    RTT_TO.tv_sec = 0;
    RTT_TO.tv_usec = TIMEOUT;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &RTT_TO, sizeof(RTT_TO)) ==-1) {
        fprintf(stderr, "Error setting socket timeout\n");
        // diep("Error setting socket timeout\n");
        return;
    }
}

/* Process the timeout situation */
void timeout_handler(){
    ssthresh = cwnd /2;
    cwnd = 1.0;
    cout << "Time Out, restart " << endl;
    status = SLOW_START;

    send_new_pkt(&wait_queue.front());
    rp_ack = 0;
}

/* Auto adjust the CWND  according to the condition of throughput of the newwork */
void state_switch(){
    switch (status){
    case SLOW_START:
        if(rp_ack==0){
            if( cwnd < ssthresh){
                cwnd += 1.0;
            }else{
                status = CONGESTION_AVOIDANCE;
                return;
            }
        }else{
            if(rp_ack >= 3){
                ssthresh = cwnd / 2;
                cwnd = ssthresh + 3;
                status = FAST_RECOVERY;
                if (!wait_queue.empty()){
                    send_new_pkt(&wait_queue.front());
                }
                rp_ack = 0;
            }
        }
        break;
    case CONGESTION_AVOIDANCE:
        if(rp_ack==0){
            cwnd += 1.0/cwnd;
        }else{
            if(rp_ack >= 3){
                ssthresh = cwnd / 2;
                cwnd = ssthresh+3;
                status = FAST_RECOVERY;
                if (!wait_queue.empty()){
                    cout << "3 repeated ACKs, resend pkt " << wait_queue.front().seq_idx << endl;
                    send_new_pkt(&wait_queue.front());
                }
                rp_ack = 0;
            }
        }
        break;
    case FAST_RECOVERY:
        if(rp_ack==0){
            cwnd = ssthresh;
            status = CONGESTION_AVOIDANCE;
        }else{
            cwnd+= 1.0;
        }
        break;
    default:
        break;
    }
};

void slide_window_send(FILE* fp){
    if (bytes_sent == bytes_total) return;
    char buf[MSS];
    pkt packet;
    int bytes_rd;
    for( int i = 0; i < ceil(cwnd - wait_queue.size()); i++){ /* Here we assume that rwnd is infinitely large, therefore swnd=min(rwnd, cwnd)=cwnd*/
        int bytes_to_read = min(bytes_total-bytes_sent, int(MSS));
        if((bytes_rd = fread(buf, sizeof(char), bytes_to_read, fp)) > 0){
            packet.data_size = bytes_rd;
            if(bytes_to_read != bytes_rd) printf("read bytes not equal");
            packet.msg_type = DATA;
            packet.seq_idx = seq_idx;
            memcpy(packet.data, &buf, bytes_rd );
            wait_queue.push(packet);
            send_new_pkt(&packet);
            seq_idx += bytes_rd;
            bytes_sent += bytes_rd;
        }
    
    }

}



void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */
    init_param(bytesToTransfer);


    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Set socket timeout */
    setSockTimeout(s);

	/* Send data and receive acknowledgements on s*/
    pkt ack_packet;
    slide_window_send(fp);
    while(bytes_sent < bytes_total || pkt_acked < pkt_num_total){
        if ((recvfrom(s, &ack_packet, sizeof(pkt), 0, NULL, NULL)) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("recvfrom()");
            }
            if (!wait_queue.empty()){
                timeout_handler();
            }
        }
        else{
            if (ack_packet.msg_type == ACK){
                if(ack_packet.ack_idx == wait_queue.front().seq_idx){
                    rp_ack++;
                    state_switch();
                }else if(ack_packet.ack_idx > wait_queue.front().seq_idx){
                    int pop_num = ceil((ack_packet.ack_idx-wait_queue.front().seq_idx) * 1.0 / 1.0*MSS );
                    pkt_acked += pop_num;
                    rp_ack = 0;
                    while(pop_num-- >0){
                        wait_queue.pop();
                    }
                    slide_window_send(fp);
                    state_switch();
                }
            }
        }
    }
    /* Send FIN to stop connection*/
    printf("Closing the socket\n");
    pkt packet_end,packet_recv;
    packet_end.msg_type = FIN;
    packet_end.data_size = 0;
    char recvbuf[sizeof(pkt)];
    while (1){
        send_new_pkt(&packet_end);
        if(recvfrom(s,&recvbuf,sizeof(pkt),0,NULL,NULL)== -1 ){
            diep("receive FIN failed");
        }
        memcpy(&packet_recv, recvbuf, sizeof(pkt));
        if(packet_recv.msg_type == FIN_ACK){
            cout << "successfully recieved the FIN back" << endl;
            break;
        }
    }
    fclose(fp);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


