#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

int main(int argc, char **argv){
	char *host = argv[1];
	char *port = argv[2];
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0){
		perror("sock_init failed");
		exit(1);
	}
	struct sockaddr_in in_name;
	struct hostent *hptr;
	memset((char *) &in_name, 0, sizeof in_name);
	in_name.sin_family = AF_INET;
	in_name.sin_port = htons(atoi(port));
	hptr = gethostbyname(host);
	memcpy((void *)&(in_name.sin_addr), hptr->h_addr_list[0], hptr->h_length);
	char data[100];
	strcpy(data, argv[3]);
	if(sendto(sd, data, strlen(data), 0, (struct sockaddr *) &in_name, sizeof in_name) < 0){
		perror("sendto failed");
		close(sd);
		exit(1);
	}
	close(sd);
	return 0;
}