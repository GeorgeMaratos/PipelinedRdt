#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "hw6.h"

#define NOTHING -1
#define ACK_RCV -2

int sequence_number;
int RTT = 0;

int timeval_to_msec(struct timeval *t) { 
	return t->tv_sec*1000+t->tv_usec/1000;
}

void msec_to_timeval(int millis, struct timeval *out_timeval) {
	out_timeval->tv_sec = millis/1000;
	out_timeval->tv_usec = (millis%1000)*1000;
}

int current_msec() {
	struct timeval t;
	gettimeofday(&t,0);
	return timeval_to_msec(&t);
}

int check_sum(char *data,int length) {
  int i,sum = 0;
  for(i=0;i<length;i++)
    sum += data[i];
  //printf("CheckSum:%d\n",sum);
  return sum;
}

int rel_connect(int socket,struct sockaddr_in *toaddr,int addrsize) {
		 connect(socket,(struct sockaddr*)toaddr,addrsize);
}

int rel_rtt(int socket) {
		 return RTT;
}

int acks_received[100] = {0}; //assumes max ack is 100, does not reset be careful

int pipeline_reader(int socket) {

  //data structures to read the packet
  char packet[MAX_PACKET];
  memset(&packet,0,sizeof(packet));
  struct hw6_hdr* hdr=(struct hw6_hdr*)packet;	

  struct sockaddr_in fromaddr;
  unsigned int addrlen=sizeof(fromaddr);	
  int recv_count;

//variables for timeout
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fd;
  FD_ZERO(&fd);
  FD_SET(socket,&fd);

  int retval;

  //variables for catching acks
  int sequence_number;

  //operations
  retval = select(socket+1,&fd,NULL,NULL,&tv);
  if(retval == -1) printf("select error\n");
  if(retval){  
    recv_count = recvfrom(socket, packet, MAX_PACKET, 0, (struct sockaddr*)&fromaddr, &addrlen);
    sequence_number = ntohl(hdr->ack_number);
    acks_received[sequence_number]++;
    return 1;
  }
  return 0;
}

int hasTimeout(int socket, int max_index) { //timeout will occur if true timeout happens or final ack is received
//hook
	clock_t start, diff;
	if(acks_received[max_index -1] == 0) {
	  start = clock();
	  diff = clock() - start;
	  while((int)(diff) <= 5) {
	    if(acks_received[max_index -1] == 0)
	      return 1;
	    pipeline_reader(socket);
	    diff = clock() - start;
	  }
	  return 1;
	}
	return 1;
}

int wait_for_ack(int seq_num, int socket) { //wait for ack is done it just needs the right timeout
/*
naive wait_for_ack (sq_num, socket)
  while !timeout
    packet = receive(packet)
    if seq_num == packet.seq)num
      return 1
  return 0

naive wait_for_ack2
  if(select_for_a_time() == timeout)
    return TIMEOUT
  else
    if seq_num == packet.seq)num
      return ACK_RCV
    else return wait_for_ack(seq_num,socket)
  
*/

//variables to read the packet
  char packet[MAX_PACKET];
  memset(&packet,0,sizeof(packet));
  struct hw6_hdr* hdr=(struct hw6_hdr*)packet;	

  struct sockaddr_in fromaddr;
  unsigned int addrlen=sizeof(fromaddr);	
  int recv_count;

//variables for timeout
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fd;
  FD_ZERO(&fd);
  FD_SET(socket,&fd);

  int retval;

//variables for pipelined
  int sequence_number;

  retval = select(socket+1,&fd,NULL,NULL,&tv);
  if(retval == -1) printf("select error\n");
  if(retval){  
    recv_count = recvfrom(socket, packet, MAX_PACKET, 0, (struct sockaddr*)&fromaddr, &addrlen);
//    printf("%d\n",ntohl(hdr->ack_number));
    sequence_number = ntohl(hdr->ack_number);
/*    if(seq_num == ntohl(hdr->ack_number))  //need to convert to host order byte, this is checking if i got what i sent back
      return ACK_RCV;
    else if(0) return 1; //if determine repeat then return repeat
    else return NOTHING; 
*/
    if(acks_received[sequence_number] == 0) {
      acks_received[sequence_number]++;
      return ACK_RCV;
    }
    else {
      acks_received[sequence_number]++;
      return sequence_number;
    }
  }
  else return NOTHING;		
}

void cal_rtt(clock_t a, clock_t b) {
  RTT = (int)((0.875)*(RTT) + (0.275)*(int)(b - a));
//  printf("RTT:%d\n");
}

int sequence_desired = 0;

//now need to read ack received and determine if it is a repeat

int pipeline_sequence = 0;

void pipeline_send(int sequence, int sock, void *buf, int len) {

//start making packet
	char packet[1400];
	struct hw6_hdr *hdr = (struct hw6_hdr*)packet;
	hdr->sequence_number = htonl(sequence);
	memcpy(hdr+1,buf,len); //hdr+1 is where the payload starts
	send(sock, packet, sizeof(struct hw6_hdr)+len, 0);

}
int rel_send(int sock, void *buf, int len)  //have rel_send, return the next sequence it wants
{
/*  
naive sender (socket)
1  packet = make_packet(buffer);
2  send(packet)
3  while (1)
4    if wait_for_ack() == timeout
5      send(packet)
6    if wait_for_ack() == received_ack
7	if check_ack == true
8	  sequence_number++
9	  return
*/


//1
 	// make the packet = header + buf
	char packet[1400];
	struct hw6_hdr *hdr = (struct hw6_hdr*)packet;
	hdr->sequence_number = htonl(sequence_number);
	memcpy(hdr+1,buf,len); //hdr+1 is where the payload starts

//hook
	//for timer
	clock_t start, diff;

        //for pipeline implementation
	int wait_return;


//2	
	start = clock();
	//printf("Packet from sender:");check_sum(buf,len);
	send(sock, packet, sizeof(struct hw6_hdr)+len, 0);
//3
	while(1) {
          wait_return = wait_for_ack(sequence_number,sock);
	  switch(wait_return) {
	  case NOTHING: //here I want to keep sending packets
	    diff = clock() - start;
//	    printf("time%d\n",diff / 100000);
	    if((int)(diff) >= RTT) //on the timeout I need to send the right sequence number packet
	      send(sock, packet, sizeof(struct hw6_hdr)+len, 0);
	  break; 
	  case ACK_RCV:   //process it and set sending packet accordingly
	    sequence_number++;
	    cal_rtt(start,clock());
  	    return ++sequence_desired;
	  break; 
	  default: //if anything besides these two are returned its a repeat sequence number
	    return ++wait_return;
	  break;
	  }
	}

}

int rel_socket(int domain, int type, int protocol) {
	sequence_number = 0;
	return socket(domain, type, protocol);
}



int seq_expected = 0;

int rel_recv(int sock, void * buffer, size_t length) {


/* global.ack_expected = 0

naive receiver (socket)
1     packet = receive(packet)
2       send_ack(packet.sequence)
*/



	char packet[MAX_PACKET];
	memset(&packet,0,sizeof(packet));
	struct hw6_hdr* hdr=(struct hw6_hdr*)packet;	

	struct sockaddr_in fromaddr;
	unsigned int addrlen=sizeof(fromaddr);	
	int recv_count = recvfrom(sock, packet, MAX_PACKET, 0, (struct sockaddr*)&fromaddr, &addrlen);		//1

	int seq;

	// this is a shortcut to 'connect' a listening server socket to the incoming client.
	// after this, we can use send() instead of sendto(), which makes for easier bookkeeping
	if(connect(sock, (struct sockaddr*)&fromaddr, addrlen)) {
		perror("couldn't connect socket");
	}

//2

	//receiver must send an ack of the segment
	//if expected is received then send the ack and return
	//if sequence is lower than expected, then send last ack and wait for response
	//if sequence is higher than expected, then do the same

//no change needed for now, but should check for sending right ack

     while(1) {
	seq = ntohl(hdr->sequence_number);
	if(seq == seq_expected) {
	  hdr->ack_number = htonl(seq);
//	  printf("sending ack:%d\n",ntohl(hdr->ack_number));
	  send(sock, packet, sizeof(struct hw6_hdr)+length, 0);   //not sure if length is supposed to be here
	  //printf("Packet from receiver:");check_sum(buffer,length);
	  seq_expected++;
	  memcpy(buffer, packet+sizeof(struct hw6_hdr), recv_count-sizeof(struct hw6_hdr));
	  return recv_count - sizeof(struct hw6_hdr);
	}
	else{
	  hdr->ack_number = htonl(seq_expected - 1);
	  send(sock, packet, sizeof(struct hw6_hdr)+length, 0);   //not sure if length is supposed to be here
	  recv_count = recvfrom(sock, packet, MAX_PACKET, 0, (struct sockaddr*)&fromaddr, &addrlen);		//1
	}
     }
//	fprintf(stderr, "Got packet %d\n", ntohl(hdr->sequence_number));

}

int rel_close(int sock) {
	rel_send(sock, 0, 0); // send an empty packet to signify end of file
	close(sock);
}

