#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define BUFSIZE 1024

int main(int argc, char **argv){
	char *port = argv[1];
	int sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0){
		perror("sock_init error");
		exit(1);
	}
	struct sockaddr_in my_addr;
	struct sockaddr_in rem_addr;

	socklen_t addrlen = sizeof rem_addr;
	memset((char *) &my_addr, 0, sizeof my_addr);
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	my_addr.sin_port = htons(atoi(port));
	printf("%d\n", my_addr.sin_addr.s_addr);
	
	if(bind(sd, (struct sockaddr *) &my_addr, sizeof my_addr) < 0){
		perror("bind failed");
		exit(1);
	}

	char data[102];
	while(1){
		int recvlen = recvfrom(sd, data, 100, 0, (struct sockaddr *) &my_addr, &addrlen);
		if(recvlen > 0 && recvlen <= 100){
			data[recvlen] = '\0';
			printf("%s\n", data);
			if(sendto(sd, "accept", 6, 0, (struct sockaddr *) &my_addr, sizeof my_addr) < 0)
				perror("sendto");
		}
		else{
			perror("receive failed");
			close(sd);
			exit(1);
		}
	}
	close(sd);
	return 0;
}
