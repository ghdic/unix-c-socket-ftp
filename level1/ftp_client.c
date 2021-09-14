#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void error_handling(char* msg);

int main(int argc, char* argv[]) {
	int client_sock;
	struct sockaddr_in server_addr;
	char message[1024] = {0x00, };
	
	client_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(client_sock == -1)
		error_handling("socket error");
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(argv[1]));
	
	if(connect(client_sock, (struct sockaddr* )&server_addr, sizeof(server_addr)) == -1)
		error_handling("connect error");
	else
		printf("connect success!!\n");
	
		
	char command[1024] = {0x00, };
	fgets(command, sizeof(command), stdin);
	
	if(write(client_sock, command, sizeof(command)) == -1)
		error_handling("write error");
	
	while(1) {
		int readResult = read(client_sock, message, sizeof(message));
		
		if(readResult == -1)
			error_handling("read error");
		if(readResult == 0)
			break;
		if(strcmp(message, ":EOF") == 0)
			break;
	
		printf("%s", message);
	}
	
	close(client_sock);
	return 0;
}



void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}