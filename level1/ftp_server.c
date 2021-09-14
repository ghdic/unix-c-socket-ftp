#include <stdio.h>
#include <stdlib.h> // atoi
#include <string.h> // memset
#include <unistd.h> // sockaddr_in, read, write
#include <sys/socket.h>
#include <arpa/inet.h> // htonl, htons, INADDR_ANY, sockaddr_in


void error_handling(char* msg);
char* substr(int s, int e, char *str);


int main(int argc, char* argv[]) {
	int port = atoi(argv[1]);
	int server_sock;
	int client_sock;
	
	// sockaddr_in은 소켓 주소의 틀, AF_INET인 경우 사용
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	
	server_sock = socket(PF_INET, SOCK_STREAM, 0); // domain, type, protocol(type에서 이미 지정한 경우 0)

	if(server_sock == -1)
		error_handling("socket error");
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; // ipv4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY - 컴퓨터에 존재하는 랜카드 중 사용 가능한 랜카드의 IP주소를 사용하라(localhost)
	server_addr.sin_port = htons(port);
	
	if(bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1)
		error_handling("bind error");
		
	if(listen(server_sock, 1) == -1)
		error_handling("listen error");
	
	client_addr_size = sizeof(client_addr);
	client_sock = accept(server_sock, (struct sockaddr* )&client_addr, &client_addr_size);

	if(client_sock == -1)
		error_handling("accept error");
	
	// === 데이터 전송 ===
	char msg[1024] = {0x00, };
	int msg_len = -1;
	while((msg_len = read(client_sock, msg, sizeof(msg))) != 0) { // 연결 끊기면 0byte 반환함
		char buf[1024];
		FILE *fp;
		
		char * const sep_at = strchr(msg, ' '); // strtok 사용하지 않는 이유는 파일 경로에 공백이 있을 수도 있기 때문
		if(sep_at != NULL) // 공백이 있는 포인터인 sep_at 기준으로 문자열 분리
			*sep_at = '\0';
		
		if(strcmp(msg, "put") == 0) {
			printf("input put!!\n");
			char *file_path = sep_at + 1;
			printf("%s\n", file_path);
		} else if(strcmp(msg, "get") == 0) {
			printf("input get!!\n");
			char *file_path = sep_at + 1;
		} else {
		
		
			fp = popen(msg, "r");
			if(fp == NULL)
			error_handling("popen error");
		
			while(fgets(buf, sizeof(buf), fp)) {
				printf("%s", buf);
				write(client_sock, buf, sizeof(buf));
			}
			
		}
		char eof[1024] = ":EOF";
		write(client_sock, eof, sizeof(eof));
	}
	
	close(client_sock);
	close(server_sock);
	
	return 0;
}

void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}

char* substr(int s, int e, char *str) {
	char* new_str = (char *)malloc(sizeof(char) * (e - s + 2));
	strncpy(new_str, str + s, e - s + 1);
	new_str[e - s + 1] = '\0';
	return new_str;
}