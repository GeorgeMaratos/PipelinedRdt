#include <sys/time.h>

struct hw6_hdr {
	uint32_t sequence_number;
	uint32_t ack_number;
	uint32_t checksum;
};

#define MAX_PACKET 1400
#define MAX_SEGMENT (MAX_PACKET-sizeof(struct hw6_hdr))

int rel_socket(int domain, int type, int protocol);
int rel_connect(int socket,struct sockaddr_in *toaddr,int addrsize);
int rel_send(int sock, void *buf, int len);
void pipeline_send(int sequence_number, int sock, void *buf, int len);
int pipeline_reader(int socket);
int hasTimeout(int socket, int max_index);
int rel_recv(int sock, void * buffer, size_t length);
int rel_close(int sock);
int rel_rtt(int sock);

int timeval_to_msec(struct timeval *t);
void msec_to_timeval(int millis, struct timeval *out_timeval);
int current_msec();

