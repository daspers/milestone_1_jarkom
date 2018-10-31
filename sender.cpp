#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <queue>
#include <ctime>
#include <algorithm>

#define MAX_DATA_LENGTH 1024
#define timeout_limit 1
#define ACK_NUMBER 0x6
#define NAK_NUMBER 0x21

struct pack_timeout
{
	int packet_no;
	time_t timeout;
	pack_timeout(){
		packet_no = 0;
		timeout = 0;
	}
	pack_timeout(int _packet_no, time_t _timeout){
		packet_no = _packet_no;
		timeout = _timeout;
	}
};

struct frame
{
	uint32_t length;
	uint8_t data[1024];
	time_t timeout;
};

class FileReader
{
public:
	size_t size;
	size_t rem_size;
	FileReader(char *filename, uint32_t buffer_size) : max_buffer_size(buffer_size){
		struct stat st;
		reader = fopen(filename, "rb");
		buffer = (uint8_t *) malloc(max_buffer_size);
		fill_buffer();
		stat(filename, &st);
		rem_size = size = st.st_size;
	}
	~FileReader(){
		fclose(reader);
	}
	void read_data(uint8_t *data, uint32_t length){
		if(length == 0)
			return;
		if(offset == current_size)
			fill_buffer();
		if(current_size - offset < length){
			int len = std::min(length, current_size - offset);
			memcpy(data, buffer + offset, len);
			offset += len;
			rem_size -= len;
			read_data(data+len, length-len);
		}
		else{
			memcpy(data, buffer + offset, length);
			offset += length;
			rem_size -= length;
		}
	}
private:
	FILE *reader;
	const uint32_t max_buffer_size;
	uint32_t current_size;
	uint32_t offset;
	uint8_t *buffer;
	void fill_buffer(){
		offset = 0;
		current_size = fread(buffer, sizeof(uint8_t), max_buffer_size, reader);
	}
};

void fill_int_to_bytes(uint8_t *data, uint32_t val){
	for(int i=3;i>=0;--i){
		data[i] = val & 0xFFFF;
		val >>= 8;
	}
}

uint8_t checksum(const uint8_t *data, uint32_t length){
	uint32_t result = 0x0;
	for (uint32_t n=0 ; n<length ; n++ ) {
		result += data[n];
	}
	result += result>>8;
	return ~((uint8_t)result);
}

uint32_t bytes_to_int(uint8_t *data){
	uint32_t res = 0;
	for(int i=0;i<4;++i){
		res <<= 8;
		res += data[i];
	}
	return res;
}

bool verify_data(const uint8_t *data, uint32_t len){
	if(data[0] != 0x1)
		return false;
	if(checksum(data, len-1) != data[len-1])
		return false;
	return true;
}

bool verify_response(const uint8_t *data, uint32_t len){
	return checksum(data, len-1) == data[len-1];
}

void sent_packet(int seq_num, uint8_t *data, int data_length, int sd, struct sockaddr_in &in_name){
	uint8_t packet[MAX_DATA_LENGTH + 10];
	packet[0] = 0x1;
	fill_int_to_bytes(packet+1, seq_num);
	fill_int_to_bytes(packet+5, data_length);
	memcpy(packet+9, data, data_length);
	// checksum begin not ready :(
	packet[data_length+9] = checksum(packet, data_length+9);
	// checksum end
	printf("Send packet NO: %d\n", seq_num);
	if(sendto(sd, packet, (size_t) (data_length + 10), 0, (struct sockaddr *) &in_name, sizeof in_name) < 0)
		perror("send");
}

uint32_t fill_buffer(frame * data, uint32_t sz, uint32_t wl, uint32_t wr, FileReader &reader){
	uint32_t counter = 0;
	if(wl < wr){
		for(int i=wr;i<sz && reader.rem_size > 0;++i){
			data[i].length = std::min(reader.rem_size, (size_t)MAX_DATA_LENGTH);
			reader.read_data(data[i].data, data[i].length);	
			++counter;
		}
		for(int i=0;i<=wl && reader.rem_size > 0;++i){
			data[i].length = std::min(reader.rem_size, (size_t)MAX_DATA_LENGTH);
			reader.read_data(data[i].data, data[i].length);	
			++counter;
		}
	}
	else{
		for(int i=wr;i<=wl && reader.rem_size > 0;++i){
			data[i].length = std::min(reader.rem_size, (size_t)MAX_DATA_LENGTH);
			reader.read_data(data[i].data, data[i].length);	
			++counter;
		}
	}
	return counter;
}

int main(int argc, char **argv){
	if(argc != 6){
		puts("Error! wrong format");
		exit(1);
	}
	char *filename = argv[1];
	int window_size = atoi(argv[2]);
	int buff_size = atoi(argv[3]); 
	char *host = argv[4];
	char *port = argv[5];
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	FileReader reader(filename, buff_size * 1024); 

	if(sd < 0){
		perror("sock_init failed");
		exit(1);
	}

	struct sockaddr_in in_name;
	struct hostent *hptr;
	socklen_t addrlen = sizeof in_name;
	memset((char *) &in_name, 0, sizeof in_name);
	in_name.sin_family = AF_INET;
	in_name.sin_port = htons(atoi(port));
	hptr = gethostbyname(host);
	memcpy((void *)&(in_name.sin_addr), hptr->h_addr_list[0], hptr->h_length);

	/* Timeout */
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 50;
	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv); 

	/* Number of packet */
	int num_of_packet = (reader.size + MAX_DATA_LENGTH - 1)/MAX_DATA_LENGTH;

	int lar =0, lfs = 0;
	frame *data = (frame *) malloc(buff_size * sizeof(frame));
	uint32_t rem_buff_size = 0;
	uint8_t resp[6];
	std::queue<pack_timeout> timeout;

	printf("No Of frame :%d\n", num_of_packet);

	/* Sliding Window */
	while(lar < num_of_packet){
		/* Proccess timeout */
		for(int i=lar+1;i<=lfs;++i){
			int idx = i % buff_size;
			if(data[idx].timeout < time(NULL)){
				sent_packet(i, data[idx].data, data[idx].length, sd, in_name);
				data[idx].timeout = time(NULL)+timeout_limit;
			}
		}

		/* Sending Data */
		while(lfs < num_of_packet && lfs-lar < window_size){
			if(rem_buff_size == 0){
				rem_buff_size = fill_buffer(data, buff_size, lar % buff_size, (lfs+1) % buff_size, reader);
			}
			int idx = ++lfs % buff_size;
			// data[idx].length = std::min(reader.rem_size, (size_t) MAX_DATA_LENGTH);
			sent_packet(lfs, data[idx].data, data[idx].length, sd, in_name);
			data[idx].timeout = time(NULL) + timeout_limit;
			--rem_buff_size;
		}

		/* Get Response */
		int tmp_len;
		while((tmp_len = recvfrom(sd, resp, 10, 0, (struct sockaddr *) &in_name, &addrlen)) >= 0){
			printf("Get response %d\n", tmp_len);
			if(verify_response(resp, tmp_len)){
				int seq_num = bytes_to_int(resp+1)-1;
				if(resp[0] == NAK_NUMBER){
					printf("recieve NAK: %d : ", seq_num);
					printf("%d %d %d %d %d %d\n", (int)resp[0], (int)resp[1], (int)resp[2], (int)resp[3], (int)resp[4], (int)resp[5]);
					data[seq_num%buff_size].timeout = 0;
				}
				else if(resp[0] == ACK_NUMBER){
					printf("recieve ACK: %d : ", seq_num);
					printf("%d %d %d %d %d %d\n", (int)resp[0], (int)resp[1], (int)resp[2], (int)resp[3], (int)resp[4], (int)resp[5]);
					lar = std::max(lar, seq_num);
				}
			}
		}
	}
	sent_packet(++lfs, (uint8_t *)"", 0, sd, in_name);

	close(sd);
	return 0;
}