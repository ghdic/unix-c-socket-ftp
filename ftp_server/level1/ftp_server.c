#include <stdio.h>
#include <stdlib.h> // atoi
#include <string.h> // memset
#include <unistd.h> // sockaddr_in, read, write, chdir
#include <sys/socket.h>
#include <arpa/inet.h> // htonl, htons, INADDR_ANY, sockaddr_in
#include <sys/utsname.h> // uname
#include <pwd.h> // username
#include <errno.h>
#include <fcntl.h> // open

enum ERROR{
	DEFAULT_ERROR = -1,
	SOCKET_ERROR = -2,
	BIND_ERROR = -3,
	LISTEN_ERROR = -4,
	ACCEPT_ERROR = -5,
	POPEN_ERROR = -6,
	UNAME_ERROR = -7
};

const char* getUserName();
void file_download(char* filename, int sock, int port);
void file_upload(char* filename, int sock, int port);
void process_commend(int sock, int port);
void eof_handling(char* path, int sock);
void popen_handling(char* msg, char* path, int sock);
void error_handling(char* msg);
void error_manage(enum ERROR error);
char* substr(int s, int e, char *str);
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
	if(server_sock < 0)
		error_manage(server_sock);
	client_sock = accept_conn(server_sock);
	if(client_sock < 0)
		error_manage(client_sock);
	process_commend(client_sock, port);
	
	// close(server_sock);
	
	return 0;
}

const char* getUserName() {
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);
	if (pw) {
		return pw->pw_name;
	}

	return "";
}

// client -> server
void file_download(char* filename, int sock, int port) {
	FILE* fp;
	int data_sock, data_port = port + 10000;
	char buf[1024] = {0x00, };

	if((fp = fopen(filename, "w")) == NULL) {
		snprintf(buf, sizeof(buf), ":ERROR %s", strerror(errno));
		write(sock, buf, sizeof(buf));
		return;
	}

	data_sock = create_socket(data_port);

	while(1) {
		if(data_sock < 0) {
			if(data_sock == BIND_ERROR) {
				data_port += 1; // BIND_ERROR시 이미 해당 포트를 사용하고 있기 때문에 다른 포트를 할당
			} else {
				error_manage(data_sock);
			}
		} else {
			break;
		}
	}

	snprintf(buf, sizeof(buf), ":PORT %d", data_port);
	write(sock, buf, sizeof(buf));
	data_sock = accept_conn(data_sock);
	if(data_sock < 0) {
		error_manage(data_sock);
	}

	while(read(data_sock, buf, sizeof(buf)) != 0) {
		if(strcmp(buf, ":DONE") == 0) break;
		fwrite(buf, sizeof(char), sizeof(buf)/sizeof(char), fp);
	}

	fclose(fp);
	close(data_sock);
}

// server -> client
void file_upload(char* filename, int sock, int port) {
	FILE* fp;
	int data_sock, data_port = port + 10000;
	char buf[1024] = {0x00, };

	if((fp = fopen(filename, "r")) == NULL) {
		snprintf(buf, sizeof(buf), ":ERROR %s", strerror(errno));
		write(sock, buf, sizeof(buf));
		return;
	}
	
	

	data_sock = create_socket(data_port);

	while(1) {
		if(data_sock < 0) {
			if(data_sock == BIND_ERROR) {
				data_port += 1; // BIND_ERROR시 이미 해당 포트를 사용하고 있기 때문에 다른 포트를 할당
			} else {
				error_manage(data_sock);
			}
		} else {
			break;
		}
	}

	snprintf(buf, sizeof(buf), ":PORT %d", data_port);
	write(sock, buf, sizeof(buf));

	while(feof(fp) == 0) {
		fgets(buf, sizeof(buf), fp);
		write(data_sock, buf, sizeof(buf));
	}

	write(data_sock, ":DONE", 6);
	fclose(fp);
	close(data_sock);
}

void process_commend(int sock, int port) {
	char cur_path[1024] = {0x00, };
	char msg[1024] = {0x00, };
	char buf[1024] = {0x00, };

	snprintf(cur_path, sizeof(cur_path), "/%s", getUserName());

	eof_handling(cur_path, sock); // 최초 연결시 경로 보냄
	while(read(sock, msg, sizeof(msg)) != 0) { // 연결 끊기면 0byte 반환함
		
		char * const sep_at = strchr(msg, ' '); // strtok 사용하지 않는 이유는 문자열 복구가 이게 더 간단
		if(sep_at != NULL) // 공백이 있는 포인터인 sep_at 기준으로 문자열 분리
			*sep_at = '\0';
		
		if(strcmp(msg, "cd") == 0) {
			char *path = sep_at + 1;
			if(strcmp(path, "~") == 0) {
				snprintf(path, sizeof(path), "/%s", getUserName());
			}
			if(chdir(path) == -1) {
				snprintf(buf, sizeof(buf), "bash: cd: %s: %s\n", path, strerror(errno));
				write(sock, buf, sizeof(buf));
			}
			getcwd(cur_path, sizeof(cur_path));
		} else if(strcmp(msg, "put") == 0) {
			char *file_path = sep_at + 1;
			file_download(file_path, sock, port);
		} else if(strcmp(msg, "get") == 0) {
			char *file_path = sep_at + 1;
		} else if(strcmp(msg, "quit") == 0) {
			printf("input quit!!\n"); // TODO: quit 할때 클라이언트에서 소켓을 끊어버리면 알아서 read에서 0 리턴하는데 여기서 처리해줄필요가 있을까? 클라쪽에서 그냥 끊어버리면 되는거아닌감? 질문
			break;
		} else {
			if(sep_at != NULL) // 짤랐던 구간 복구
				*sep_at = ' ';
			popen_handling(msg, cur_path, sock);		
		}
		
		// 메세지 보낸게 끝났다는 EOF처리 & 마무리
		eof_handling(cur_path, sock);
	}
	printf("disconnected from client : %d\n", sock);
	
	close(sock);
}

void eof_handling(char* path, int sock) {
	char buf[1024] = {0x00, };
	struct utsname info;

	// 메세지 전부 보냈다는 것을 알림
	snprintf(buf, sizeof(buf), ":EOF");
	write(sock, buf, sizeof(buf));

	// 현재 유저명 & 경로를 보내줌
	if(uname(&info) == -1) {
		error_manage(UNAME_ERROR);
	}

	snprintf(buf, sizeof(buf), "%s@%s:%s# ", getUserName(), info.nodename, path);
	write(sock, buf, sizeof(buf));
}

void popen_handling(char* msg, char* path, int sock) {
	FILE* fp = NULL;
	char buf[1024] = {0x00, };
	
	chdir(path); // 매번 이렇게 루트 위치를 옮겨줘야 되나? popen이 fork로 하위 프로세스 만들어서 하는건데 유지안되나?..
	
	fp = popen(msg, "r");
	if(fp == NULL)
		error_manage(POPEN_ERROR);
	
	while(fgets(buf, sizeof(buf), fp)) {
		write(sock, buf, sizeof(buf));
	}
	
	pclose(fp);
}

void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}

void error_manage(enum ERROR error) {
	switch(error) {
		case SOCKET_ERROR:
			error_handling("socket error");break;
		case BIND_ERROR:
			error_handling("bind error");break;
		case LISTEN_ERROR:
			error_handling("listen error");break;
		case ACCEPT_ERROR:
			error_handling("accept error");break;
		case POPEN_ERROR:
			error_handling("popen error");break;
		case UNAME_ERROR:
			error_handling("uname error");break;	
		default:
			error_handling(strerror(errno));
	}
}

char* substr(int s, int e, char *str) {
	char* new_str = (char *)malloc(sizeof(char) * (e - s + 2));
	strncpy(new_str, str + s, e - s + 1);
	new_str[e - s + 1] = '\0';
	return new_str;
}

int create_socket(int port) {
	// sockaddr_in은 소켓 주소의 틀, AF_INET인 경우 사용
	int listenfd;
	struct sockaddr_in server_addr;
	
	listenfd = socket(PF_INET, SOCK_STREAM, 0); // domain, type, protocol(type에서 이미 지정한 경우 0)

	if(listenfd == -1)
		return SOCKET_ERROR;
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; // ipv4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY - 컴퓨터에 존재하는 랜카드 중 사용 가능한 랜카드의 IP주소를 사용하라(localhost)
	server_addr.sin_port = htons(port);
	
	if(bind(listenfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1)
		return BIND_ERROR;
		
	if(listen(listenfd, 1) == -1)
		return LISTEN_ERROR;

	return listenfd;
}

int accept_conn(int sock) {
	int connfd;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	
	client_addr_size = sizeof(client_addr);
	connfd = accept(sock, (struct sockaddr* )&client_addr, &client_addr_size);

	if(connfd == -1)
		return ACCEPT_ERROR;

	return connfd;
}