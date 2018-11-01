#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <algorithm>

#define MAX_DATA_LENGTH 1024
#define ACK_NUMBER 0x6
#define NAK_NUMBER 0x21

struct frame
{
	int seq_num;
	int length;
	uint8_t data[1024];
	frame(){
		seq_num = 0;
		length = 0;
	}
};

class FileWriter
{
public:
	FileWriter(char *filename, uint32_t buffer_size) : max_buffer_size(buffer_size){
		writer = fopen(filename, "wb+");
		buffer = (uint8_t *) malloc(max_buffer_size);
		offset = 0;
	}
	~FileWriter(){
		flush();
		fclose(writer);
	}
	void write_data(uint8_t *data, uint32_t length){
		if(length == 0)
			return;
		if(offset == max_buffer_size)
			flush();
		uint32_t len = std::min(max_buffer_size - offset, length);
		memcpy(buffer+offset, data, len);
		offset += len;
		write_data(data+len, length - len);
	}
private:
	const uint32_t max_buffer_size;
	FILE *writer;
	uint8_t *buffer;
	uint32_t offset;
	void flush(){
		fwrite(buffer, sizeof(uint8_t), offset, writer);
		offset = 0;
	}
};

uint8_t checksum(const uint8_t *data, uint32_t length){
	uint32_t result = 0x0;
	for (int n=0 ; n<length ; n++ ) {
		result += data[n];
	}
	result += result>>8;
	return ~((uint8_t)result);
}

bool verify_packet(const uint8_t *data, uint32_t len){
	if(data[0] != 0x1)
		return false;
	if(checksum(data, len-1) != data[len-1])
		return false;
	return true;
}

uint32_t bytes_to_int(uint8_t *data){
	uint32_t res = 0;
	for(int i=0;i<4;++i){
		res <<= 8;
		res += data[i];
	}
	return res;
}

void fill_int_to_bytes(uint8_t *data, uint32_t val){
	for(int i=3;i>=0;--i){
		data[i] = val & 0xFFFF;
		val >>= 8;
	}
}

void send_resp(int head, int n_seq_num, int sd, sockaddr_in &my_addr){
	// printf("Send ack %d : ", n_seq_num);
	uint8_t ack[6];
	ack[0] = head;
	fill_int_to_bytes(ack+1, n_seq_num);
	ack[5] = checksum(ack, 5);
	// printf("%d %d %d %d %d %d\n", (int)ack[0], (int)ack[1], (int)ack[2], (int)ack[3], (int)ack[4], (int)ack[5]);
	if(sendto(sd, ack, 6, 0, (struct sockaddr *) &my_addr, sizeof my_addr) < 0)
		perror("sendto ack");
}

int main(int argc, char **argv){
	char *filename = argv[1];
	int window_size = atoi(argv[2]);
	int buff_size = atoi(argv[3]); 
	char *port = argv[4];
	FileWriter writer(filename, buff_size*MAX_DATA_LENGTH);

	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0){
		perror("sock_init");
		exit(1);
	}
	struct sockaddr_in my_addr;
	struct sockaddr_in rem_addr;

	socklen_t addrlen = sizeof rem_addr;
	memset((char *) &my_addr, 0, sizeof my_addr);
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_port = htons(atoi(port));
	// printf("%d\n", my_addr.sin_addr.s_addr);
	
	if(bind(sd, (struct sockaddr *) &my_addr, sizeof my_addr) < 0){
		perror("bind failed");
		exit(1);
	}
	
	/* Timeout time for program to wait for packet */
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv); 

	/* Start receiving data */
	uint8_t packet[1035];
	frame *data = (frame *) malloc(window_size * sizeof(frame));
	uint32_t lfr = 0, laf = 10;
	while(1){

		/* Read Incoming packet */
		int recvlen = recvfrom(sd, packet, MAX_DATA_LENGTH+10, 0, (struct sockaddr *) &my_addr, &addrlen);
		// printf("receive %d %d\n", recvlen, (int)packet[0]);
		if(recvlen > 0 && recvlen <= MAX_DATA_LENGTH+10){
			if(verify_packet(packet, recvlen)){
				// printf("Packet size : %d\n", recvlen);
				int seq_num = bytes_to_int(packet+1);
				// printf("Packet receive no : %d\n", seq_num);
				if(seq_num > lfr && seq_num <= laf){
					int idx = seq_num % window_size;
					if(data[idx].seq_num != seq_num){
						data[idx].seq_num = seq_num;
						data[idx].length = bytes_to_int(packet+5);
						memcpy(data[idx].data, packet+9, data[idx].length);
						
						/* Move Window */
						while(data[(lfr+1)%window_size].seq_num == lfr+1){
							writer.write_data(data[idx].data, data[idx].length);
							++lfr;
						}
						laf = lfr + window_size;
					}
				}

				/* Send ACK */
				send_resp(ACK_NUMBER, lfr+1, sd, my_addr);
				if(bytes_to_int(packet+5) == 0){
					// for(int i=1;i<5;++i)
					// 	send_resp(ACK_NUMBER, lfr+1, sd, my_addr);
					break;
				}
			}
			else
				send_resp(NAK_NUMBER, lfr+1, sd, my_addr);
		}
	}

	// close(sd);
	return 0;
}