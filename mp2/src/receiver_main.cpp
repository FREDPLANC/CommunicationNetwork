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

#include <queue>
#include <iostream>
// #include "udp.h"


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

struct sockaddr_in si_me, si_other;
int s, slen;
int ack_idx;

void diep(char *s) {
    perror(s);
    exit(1);
}

struct cmp 
{
    bool operator() (pkt a, pkt b) 
    {
        return a.seq_idx > b.seq_idx ; //小顶堆 for priority queue
    }
};

priority_queue<pkt, vector<pkt>, cmp> buffer_q;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);
    ack_idx = 0;
    FILE* fp = fopen(destinationFile, "wb");  
    if (fp == NULL){
        diep("Cannot open the destination file");
    } 

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1){
        diep("bind");
    }
    /* Now receive data and send acknowledgements */    
    while (1){
        pkt recv_pkt;
        pkt send_ack;
        int recv_bytes;
        recv_bytes = recvfrom(s, &recv_pkt, sizeof(pkt), 0, (sockaddr*)&si_other, (socklen_t*)&slen);
        if ( recv_bytes < 0 )
            diep("recvfrom()");
        cout << "Received packet type: " << recv_pkt.msg_type << ", seq_idx: " << recv_pkt.seq_idx << endl;
        if (recv_pkt.msg_type == DATA){
            cout << "Processing DATA packet" << endl;
            if (recv_pkt.seq_idx > ack_idx && buffer_q.size()< BUFFERR_RECVMAX ){ // future in line, push into the buffer queue
                buffer_q.push(recv_pkt);
                cout << "Pushed to buffer: seq_idx=" << recv_pkt.seq_idx << endl;
            }
            else if(recv_pkt.seq_idx == ack_idx){
                fwrite(recv_pkt.data, sizeof(char), recv_pkt.data_size, fp);
                ack_idx += recv_pkt.data_size;
                cout << "Wrote to file: seq_idx=" << recv_pkt.seq_idx << ", ack_idx=" << ack_idx << endl;
                /* Write packets that are at the top of the queue */
                while ((!buffer_q.empty()) && buffer_q.top().seq_idx == ack_idx ){
                    pkt temp_pkt = buffer_q.top();
                    fwrite(temp_pkt.data, sizeof(char), temp_pkt.data_size, fp);
                    ack_idx += temp_pkt.data_size;
                    buffer_q.pop();
                    cout << "Wrote buffered packet to file: seq_idx=" << temp_pkt.seq_idx << endl;
                }
            }
            send_ack.ack_idx = ack_idx;
            send_ack.msg_type = ACK;
            if(sendto(s, &send_ack, sizeof(pkt), 0, (sockaddr*)&si_other,  (socklen_t)sizeof(si_other)) <0){
                diep("send ACK Failed");
            }
            cout << "Sent ACK: ack_idx=" << ack_idx << endl;
        }else if (recv_pkt.msg_type == FIN){
            send_ack.ack_idx = ack_idx;
            send_ack.msg_type = FIN_ACK;
            if(sendto(s, &send_ack, sizeof(pkt), 0, (sockaddr*)&si_other, (socklen_t)sizeof(si_other)) <0){
                diep("send FIN_ACK Failed");
            }
            cout << "Sent FIN_ACK and breaking loop." << endl;
            break;
        }
    }
    fclose(fp);
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
    return (EXIT_SUCCESS);
}

