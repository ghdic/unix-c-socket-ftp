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

#define BUFFER_SIZE 1024

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
void file_download(char* filepath, int sock);
void file_upload(char* filepath, int sock);
void process_commend(int sock, int port);
void eof_handling(char* path, int sock);
void popen_handling(char* msg, int sock);
void error_handling(char* msg);
void error_manage(enum ERROR error);
char* substr(int s, int e, char *str);
int create_socket(uint16_t port);
int accept_conn(int sock);

int main(int argc, char* argv[]) {
	uint16_t port;
	int server_sock;
	int client_sock;

	if(argc == 1) {
		port = htons(50000);
	} else if(argc == 2) {
		port = htons(atoi(argv[1]));
	} else {
		printf("사용법 ./ftp_server <포트번호>\n");
		return -1;
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
void file_download(char* filepath, int sock) {
	FILE* fp;
	// int data_sock, data_port = port + 1;
	char buf[BUFFER_SIZE] = {0x00, };
	char *filename, *bp;

	filename = strrchr(filepath, '/'); // '/'의 마지막 위치, 없으면 NULL반환 파일명만 빼냄
	if(filename == NULL)
		filename = filepath;
	else
		filename = filename + 1;

	if((fp = fopen(filename, "w")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		write(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write(sock, buf, BUFFER_SIZE);
	}

	read(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%d %s %s\n", sock, buf, bp); // 이런식으로도 복구가능 ㅋㅋ(strtok 할때 사이에 \0 있기 때문)
		return;
	}

	// data_sock = create_socket(data_port);

	// while(1) {
	// 	if(data_sock < 0) {
	// 		if(data_sock == BIND_ERROR) {
	// 			data_port += 1; // BIND_ERROR시 이미 해당 포트를 사용하고 있기 때문에 다른 포트를 할당
	// 		} else {
	// 			error_manage(data_sock);
	// 		}
	// 	} else {
	// 		break;
	// 	}
	// }

	// snprintf(buf, BUFFER_SIZE, ":PORT %d", data_port);
	// write(sock, buf, BUFFER_SIZE);
	// data_sock = accept_conn(data_sock);
	// if(data_sock < 0) {
	// 	error_manage(data_sock);
	// }

	read(sock, buf, BUFFER_SIZE);
	int file_size, num_bulk; // 전송받을 횟수

	sscanf(buf, "%d %d", &file_size, &num_bulk);

	while(num_bulk-- && read(sock, buf, BUFFER_SIZE) != 0) {
		fwrite(buf, sizeof(char), strlen(buf), fp);
	}

	fclose(fp);
	// close(data_sock);
}

// server -> client
void file_upload(char* filepath, int sock) {
	FILE* fp;
	// int data_sock, data_port = port + 10000;
	char buf[BUFFER_SIZE] = {0x00, };
	char* bp;

	if((fp = fopen(filepath, "r")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		write(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write(sock, buf, BUFFER_SIZE);
	}

	read(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%d: %s %s\n", sock, buf, bp);
		return;
	}
	
	// data_sock = create_socket(data_port);

	// while(1) {
	// 	if(data_sock < 0) {
	// 		if(data_sock == BIND_ERROR) {
	// 			data_port += 1; // BIND_ERROR시 이미 해당 포트를 사용하고 있기 때문에 다른 포트를 할당
	// 		} else {
	// 			error_manage(data_sock);
	// 		}
	// 	} else {
	// 		break;
	// 	}
	// }

	// snprintf(buf, BUFFER_SIZE, ":PORT %d", data_port);
	// write(sock, buf, BUFFER_SIZE);

	fseek (fp, 0, SEEK_END);
	int file_size = ftell(fp);
	int num_bulk = file_size / BUFFER_SIZE + (file_size % BUFFER_SIZE != 0 ? 1:0);
	rewind(fp);

	snprintf(buf, BUFFER_SIZE, "%d %d", file_size, num_bulk);
	write(sock, buf, BUFFER_SIZE);

	// while(feof(fp) == 0) {
	for(int i = 0; i < num_bulk; i++) {
		fread(buf, sizeof(char), BUFFER_SIZE, fp);
		write(sock, buf, BUFFER_SIZE);
	}


	fclose(fp);
	// close(data_sock);
}

void process_commend(int sock, int port) {
	char cur_path[1024] = {0x00, };
	char msg[BUFFER_SIZE] = {0x00, };
	char buf[BUFFER_SIZE] = {0x00, };

	snprintf(cur_path, sizeof(cur_path), "/home/%s", getUserName());
	if(chdir(cur_path) == -1) {
		snprintf(cur_path, sizeof(cur_path), "/");
		chdir(cur_path);
	}

	eof_handling(cur_path, sock); // 최초 연결시 경로 보냄
	while(read(sock, msg, BUFFER_SIZE) != 0) { // 연결 끊기면 0byte 반환함
		
		char * const sep_at = strchr(msg, ' '); // strtok 사용하지 않는 이유는 문자열 복구가 이게 더 간단
		if(sep_at != NULL) // 공백이 있는 포인터인 sep_at 기준으로 문자열 분리
			*sep_at = '\0';
		
		if(strcmp(msg, "cd") == 0) {
			char *path = sep_at + 1;
			if(strcmp(path, "~") == 0) {
				snprintf(path, sizeof(path), "/%s", getUserName());
			}
			if(chdir(path) == -1) {
				snprintf(buf, BUFFER_SIZE, "bash: cd: %s: %s\n", path, strerror(errno));
				write(sock, buf, BUFFER_SIZE);
			}
			getcwd(cur_path, sizeof(cur_path));
		} else if(strcmp(msg, "put") == 0) {
			char *file_path = sep_at + 1;
			file_download(file_path, sock);
		} else if(strcmp(msg, "get") == 0) {
			char *file_path = sep_at + 1;
			file_upload(file_path, sock);
		} else {
			if(sep_at != NULL) // 짤랐던 구간 복구
				*sep_at = ' ';
			popen_handling(msg, sock);		
		}
		
		// 메세지 보낸게 끝났다는 EOF처리 & 마무리
		eof_handling(cur_path, sock);
	}
	printf("disconnected from client : %d\n", sock);
	
	close(sock);
}

void eof_handling(char* path, int sock) {
	char buf[BUFFER_SIZE] = {0x00, };
	struct utsname info;

	// 메세지 전부 보냈다는 것을 알림
	snprintf(buf, BUFFER_SIZE, ":EOF");
	write(sock, buf, BUFFER_SIZE);

	// 현재 유저명 & 경로를 보내줌
	if(uname(&info) == -1) {
		error_manage(UNAME_ERROR);
	}

	snprintf(buf, BUFFER_SIZE, "%s@%s:%s# ", getUserName(), info.nodename, path);
	write(sock, buf, BUFFER_SIZE);
}

void popen_handling(char* msg, int sock) {
	FILE* fp = NULL;
	char buf[BUFFER_SIZE] = {0x00, };
	
	snprintf(buf, BUFFER_SIZE, "%s 2>&1", msg); // stderr -> stdout으로 디스크립터 옮겨 에러내용도 전달가능하게 
	fp = popen(buf, "r");
	if(fp == NULL) {
		// error_manage(POPEN_ERROR);
		snprintf(buf, BUFFER_SIZE, "%s\n", strerror(errno));
		write(sock, buf, BUFFER_SIZE);
	} else {
		while(fgets(buf, BUFFER_SIZE, fp)) {
			write(sock, buf, BUFFER_SIZE);
		}
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

int create_socket(uint16_t port) {
	// sockaddr_in은 소켓 주소의 틀, AF_INET인 경우 사용
	int listenfd;
	struct sockaddr_in server_addr;
	
	listenfd = socket(PF_INET, SOCK_STREAM, 0); // domain, type, protocol(type에서 이미 지정한 경우 0)

	if(listenfd == -1)
		return SOCKET_ERROR;
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET; // ipv4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY - 컴퓨터에 존재하는 랜카드 중 사용 가능한 랜카드의 IP주소를 사용하라(localhost)
	server_addr.sin_port = port;
	
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