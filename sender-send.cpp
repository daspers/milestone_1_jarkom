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
	socklen_t addrlen = sizeof in_name;
	memset((char *) &in_name, 0, sizeof in_name);
	in_name.sin_family = AF_INET;
	in_name.sin_port = htons(atoi(port));
	hptr = gethostbyname(host);
	memcpy((void *)&(in_name.sin_addr), hptr->h_addr_list[0], hptr->h_length);
	char data[100], inp[100];
	while(1){
		scanf("%s", data);
		if(sendto(sd, data, strlen(data), 0, (struct sockaddr *) &in_name, sizeof in_name) < 0){
			perror("sendto failed");
			close(sd);
			exit(1);
		}
		int recvlen = recvfrom(sd, inp, 100, 0, (struct sockaddr *) &in_name, &addrlen);
		inp[recvlen] = '\0';
		printf("%s\n", inp);
	}
	close(sd);
	return 0;
}