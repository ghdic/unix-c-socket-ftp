#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

// gcc -o chat_server chat_server.c

void error_handling(char* msg);
int create_socket(int port);
int accept_conn(int sock);

int main(int argc, char* argv[]) {
    int port;
    int server_sock;
    int client_sock;
    

    if(argc < 2) {
        port = 50000;
    } else {
        port = atoi(argv[1]);
    }

    server_sock = create_socket(port);

    client_sock = accept_conn(server_sock);

    char nickname[38] = {'\0', }; // nickname[20]@ip_addr[16]
    read(client_sock, nickname, sizeof(nickname));

    char buf[1024];

    while(read(client_sock, buf, sizeof(buf)) != 0) {
        printf("%s>%s", nickname, buf);
        snprintf(buf, sizeof(buf), "%s>%s", nickname, buf);
        write(client_sock, buf, sizeof(buf));
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

int create_socket(int port) {
	// sockaddr_in은 소켓 주소의 틀, AF_INET인 경우 사용
	int listenfd;
	struct sockaddr_in server_addr;
	
	listenfd = socket(PF_INET, SOCK_STREAM, 0); // domain, type, protocol(type에서 이미 지정한 경우 0)

	if(listenfd == -1)
		error_handling("socket error");
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; // ipv4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY - 컴퓨터에 존재하는 랜카드 중 사용 가능한 랜카드의 IP주소를 사용하라(localhost)
	server_addr.sin_port = htons(port);
	
	if(bind(listenfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1)
		error_handling("bind error");
		
	if(listen(listenfd, 30) == -1)
		error_handling("listen error");

	return listenfd;
}

int accept_conn(int sock) {
	int connfd;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	
	client_addr_size = sizeof(client_addr);
	connfd = accept(sock, (struct sockaddr* )&client_addr, &client_addr_size);

	if(connfd == -1)
		error_handling("accept error");

	return connfd;
}