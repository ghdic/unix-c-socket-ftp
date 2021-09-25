#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <pwd.h>
#define BUFFER_SIZE 1024

int read_handling(int socket, char* server_path);
const char* getUserName();
void cd(char* path, char* cur_path, int sock);
void popen_handling(char* msg);
void file_download(char* filepath, int sock);
void file_upload(char* filepath, int sock);
void error_handling(char* msg);
void help();

int main(int argc, char* argv[]) {
	uint32_t server_ip;
	uint16_t port;
	int client_sock;
	struct sockaddr_in server_addr;
	char cur_path[1024], server_path[1024];

	if(argc == 1) {
		server_ip = htonl(INADDR_ANY);
		port = htons(50000);
	} else if(argc == 2) {
		server_ip = htonl(INADDR_ANY);
		port = htons(atoi(argv[1]));
	} else if(argc == 3) {
		server_ip = inet_addr(argv[1]);
		port = htons(atoi(argv[2]));

	} else {
		printf("사용법: ./ftp_client <Server IP> <Port>\n");
		return -1;
	}

	snprintf(cur_path, sizeof(cur_path), "/%s", getUserName());
	
	
	client_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(client_sock == -1)
		error_handling("socket error");
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = server_ip;
	server_addr.sin_port = port;
	
	if(connect(client_sock, (struct sockaddr* )&server_addr, sizeof(server_addr)) == -1)
		error_handling("connect error");
	else
		printf("connect success!!\n");
	printf("사용법: !help 명령어 사용\n");
	
		
	char command[1024] = {0x00, };
	char buf[1024] = {0x00, };
	int readResult = 1;

	// 최초 연결시 경로 출력
	read(client_sock, buf, sizeof(buf));
	read(client_sock, buf, sizeof(buf));
	strncpy(server_path, buf, sizeof(buf));
	printf("%s", buf);
	while(1) {
		fgets(command, sizeof(command), stdin);
		command[strlen(command) - 1] = '\0'; // \n 엔터버퍼 지우기
		char * const sep_at = strchr(command, ' ');
		if(sep_at != NULL) *sep_at = '\0';

		if(strcmp(command, "quit") == 0)
			break;

		if(strcmp(command, "!help") == 0) {
			help();
			printf("%s", server_path);
			continue;
		}
		if(strcmp(command, "!cd") == 0) {
			char *path = sep_at + 1;
			cd(path, cur_path, client_sock);
			printf("%s", server_path);
			continue;
		} else if(strcmp(command, "get") == 0) {
			char* file_path = sep_at +1;
			if(sep_at != NULL) *sep_at = ' ';
			if(write(client_sock, command, strlen(command)) == -1)
				error_handling("write error");
			file_download(file_path, client_sock);
		} else if(strcmp(command, "put") == 0) {
			char* file_path = sep_at +1;
			if(sep_at != NULL) *sep_at = ' ';
			if(write(client_sock, command, strlen(command)) == -1)
				error_handling("write error");
			file_upload(file_path, client_sock);
		} else {
			if(sep_at != NULL) *sep_at = ' ';
			if(command[0] == '!') {
				strncpy(command, command + 1, strlen(command + 1));
				popen_handling(command);
				printf("%s", server_path);
				continue;
			} else {
				if(write(client_sock, command, strlen(command)) == -1)
					error_handling("write error");
			}
		}
		
		if(read_handling(client_sock, server_path) == -1) { // 상대측 연결이 끊긴경우
			printf("disconnected from server\n");
			break;
		}
	}
	close(client_sock);
	
	return 0;
}

int read_handling(int socket, char* server_path) {
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
	strncpy(server_path, user_info, sizeof(user_info));
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

void cd(char* path, char* cur_path, int sock) {
	
	if(strcmp(path, "~") == 0) {
		snprintf(path, sizeof(path), "/%s", getUserName());
	}
	if(chdir(path) == -1) {
		char buf[1024];
		snprintf(buf, BUFFER_SIZE, "bash: cd: %s: %s\n", path, strerror(errno));
		write(sock, buf, strlen(buf));
	}
	getcwd(cur_path, sizeof(cur_path));
}

void popen_handling(char* msg) {
	FILE* fp = NULL;
	char buf[BUFFER_SIZE] = {0x00, };

	snprintf(buf, BUFFER_SIZE, "%s 2>&1", msg); // stderr -> stdout으로 디스크립터 옮겨 에러내용도 전달가능하게 
	fp = popen(buf, "r");
	if(fp == NULL)
		error_handling("popen error");
	
	while(fread(buf, sizeof(char), BUFFER_SIZE, fp)) {
		printf("%s", buf);
	}
	
	pclose(fp);
}

// server -> client
void file_download(char* filepath, int sock) {
	FILE* fp;
	char buf[BUFFER_SIZE] = {0x00, };
	char* bp, *filename;

	filename = strrchr(filepath, '/'); // '/'의 마지막 위치, 없으면 NULL반환 파일명만 빼냄
	if(filename == NULL)
		filename = filepath;
	else
		filename = filename + 1;

	read(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%s %s\n", buf, bp);
		return;
	}

	if((fp = fopen(filename, "w")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		printf("%s\n", buf);
		write(sock, buf, strlen(buf));
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write(sock, buf, strlen(buf));
	}

	read(sock, buf, BUFFER_SIZE);
	int file_size, num_bulk; // 전송받을 횟수
	sscanf(buf, "%d %d", &file_size, &num_bulk);

	for(int i = 0; i < num_bulk; i++) {
		printf("Processing: %0.2lf%%\n", 100.0 * i / num_bulk);
		if(read(sock, buf, BUFFER_SIZE) == 0) break;
		fwrite(buf, sizeof(char), strlen(buf), fp); // 마지막 버퍼에 쓰레기값까지 채워서 들어가는것 방지
	}
	printf("Processing: 100%%\n");
	printf("Download File Size: %d bytes, File Name: %s\n", file_size, filename);
	fclose(fp);
}

// client -> server
void file_upload(char* filepath, int sock) {
	FILE* fp;
	char buf[BUFFER_SIZE] = {0x00, };

	char* bp;

	read(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%s %s\n", buf, bp);
		return;
	}

	if((fp = fopen(filepath, "r")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		printf("%s\n", buf);
		write(sock, buf, strlen(buf));
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write(sock, buf, strlen(buf));
	}

	fseek (fp, 0, SEEK_END);
	int file_size = ftell(fp);
	int num_bulk = file_size / BUFFER_SIZE + (file_size % BUFFER_SIZE != 0 ? 1:0);
	rewind(fp);

	snprintf(buf, BUFFER_SIZE, "%d %d", file_size, num_bulk);
	write(sock, buf, strlen(buf));

	// while(feof(fp) == 0) {
	for(int i = 0; i < num_bulk; i++) {
		printf("Processing: %0.2lf%%\n", 100.0 * i / num_bulk);
		fread(buf, sizeof(char), BUFFER_SIZE, fp);
		write(sock, buf, strlen(buf));
	}

	printf("Processing: 100%%\n");
	printf("Upload File Size: %d bytes, File Path: %s\n", file_size, filepath);

	fclose(fp);
}

void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}

void help() {
	printf("\
\n\n\
=========================================================\n\
!help: 사용 가능한 명령어 리스트를 출력한다\n\
quit: 클라이언트를 종료한다\n\
get [서버 파일경로]: 서버로부터 해당 파일을 현재 로컬경로에 다운받는다\n\
put [클라이언트 파일경로]: 클라이언트로부터 해당 파일을 현재 서버경로에 다운받는다\n\
![명령어]: 로컬에서 커맨드 명령을 수행한다\n\
[명령어]: 서버에서 커맨드 명령을 수행한다\n\
=========================================================\n\
\n\n");
}