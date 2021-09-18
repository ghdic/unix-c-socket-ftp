#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int read_handling(int socket);
void error_handling(char* msg);

int main(int argc, char* argv[]) {
	int client_sock;
	struct sockaddr_in server_addr;
	
	
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
	char buf[1024] = {0x00, };
	int readResult = 1;

	// 최초 연결시 경로 출력
	read(client_sock, buf, sizeof(buf));
	read(client_sock, buf, sizeof(buf));
	printf("%s", buf);
	while(1) {
		fgets(command, sizeof(command), stdin);
		command[strlen(command) - 1] = '\0'; // \n 엔터버퍼 지우기

		if(strcmp(command, "quit") == 0)
			break;
		
		if(write(client_sock, command, sizeof(command)) == -1)
			error_handling("write error");
		
		if(read_handling(client_sock) == -1) { // 상대측 연결이 끊긴경우
			printf("disconnected from server\n");
			break;
		}
	}
	close(client_sock);
	return 0;
}

int read_handling(int socket) {
	char message[1024] = {0x00, };
	char user_info[1024] = {0x00, };

	while(1) {
		int readResult = read(socket, message, sizeof(message));
		
		if(readResult == -1)
			error_handling("read error");
		if(readResult == 0) // 상대가 연결을 끊을 경우 read 0bytes
			return -1;
		if(strcmp(message, ":EOF") == 0) // 모든 데이터를 다 받으면 :EOF 메세지를 보냄
			break;
	
		printf("%s", message);
	}
	read(socket, user_info, sizeof(user_info));
	printf("%s", user_info);
	return 0;
}

void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}