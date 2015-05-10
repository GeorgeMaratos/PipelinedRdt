#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hw6.h"

#define Pipeline 1

int main(int argc, char** argv) {	
	if(argc<3) { fprintf(stderr,"Usage: hw6_sender <remote host> <port>\n"); exit(1);}

	struct sockaddr_in addr; 
	addr.sin_family = AF_INET;

	int sock = rel_socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		perror("Creating socket failed: ");
		exit(1);
	}

	addr.sin_port = htons(atoi(argv[2]));
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sock,(struct sockaddr*)&addr,sizeof(addr))) {
		perror("When binding");
	}

	addr.sin_port = htons(atoi(argv[2])); 
	addr.sin_addr.s_addr = inet_addr(argv[1]);	

	// this simply stores the address for the socket, so we don't have to use sendto()
	if(rel_connect(sock, &addr,sizeof(addr))) {
			perror("When connecting");
	}	

	char buf[MAX_PACKET-sizeof(struct hw6_hdr)];	
	//adding 2d array for file
	char file[100][MAX_PACKET-sizeof(struct hw6_hdr)];
	int read_bytes[100];

	memset(&buf,0,sizeof(buf));

	int starttime = current_msec();
	int totalbytes = 0;
        int max_index = 0, index = 0;

	int readbytes;
	while(read_bytes[index] = fread(buf,1,sizeof(buf),stdin)) { //reads in from stdin and sends it sizeof(buf) at a time
		totalbytes+=read_bytes[index];
//		rel_send(sock, buf, readbytes);							 
//		printf("sent a packet\n");	
		memcpy(file[max_index++],buf,read_bytes[index++]);
	}

	index = 0;
  if(!Pipeline) {
	while(index < max_index) {
		index = rel_send(sock,file[index],read_bytes[index]);
		printf("sent a packet\n");
	}
  }
  else {
  //here I will have a pipeline
	while(index < max_index) {
          pipeline_send(sock,file[index],read_bytes[index]);
	  index++;
	}
  }
/*
  new sender
    uses 2d char array
    reads in whole file 
    then sends file row by row
      this allows me to control which packet sent
*/


	int finished_msec = current_msec();
	fprintf(stderr,"\nFinished sending, closing socket.\n");

	fprintf(stderr,"\nSent %d bytes in %.4f seconds, %.2f kB/s\n",
					totalbytes,
					(finished_msec-starttime)/1000.0,
					totalbytes/(float)(finished_msec-starttime));
	fprintf(stderr,"Estimated RTT: %d ms\n",
					rel_rtt(sock));
	rel_close(sock);
}

