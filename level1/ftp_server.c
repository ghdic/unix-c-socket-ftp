#include <stdio.h>
#include <stdlib.h> // atoi
#include <string.h> // memset
#include <direct.h>	//chdir
#include <unistd.h> // sockaddr_in, read, write
#include <sys/socket.h>
#include <arpa/inet.h> // htonl, htons, INADDR_ANY, sockaddr_in
#include <sys/utsname.h> // uname
#include <pwd.h> // username

void eof_handling(char* path, int socket);
void popen_handling(char* msg, char* path, int socket)
void error_handling(char* msg);
char* substr(int s, int e, char *str);




int main(int argc, char* argv[]) {
	int port = atoi(argv[1]);
	int server_sock;
	int client_sock;
	char cur_path[1024] = "~"; // 시작 위치는 home
	
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
	char buf[1024] = {0x00, };
	int msg_len = -1;
	while((msg_len = read(client_sock, msg, sizeof(msg))) != 0) { // 연결 끊기면 0byte 반환함
		
		char * const sep_at = strchr(msg, ' '); // strtok 사용하지 않는 이유는 파일 경로에 공백이 있을 수도 있기 때문
		if(sep_at != NULL) // 공백이 있는 포인터인 sep_at 기준으로 문자열 분리
			*sep_at = '\0';
		
		if(strcmp(msg, "cd") == 0) {
			char *path = sep_at + 1;
			if(chdir(path) == -1) {
				fprintf(buf, "bash: cd: %s: %s\n", path, strerror(errno));
				write(client_sock, buf, sizeof(buf));
			}
			getcwd(cur_path, sizeof(cur_path));
		} else if(strcmp(msg, "put") == 0) {
			char *file_path = sep_at + 1;
			printf("%s\n", file_path);
		} else if(strcmp(msg, "get") == 0) {
			char *file_path = sep_at + 1;
		} else if(strcmp(msg, "quit") == 0) {
			printf("input quit!!\n"); // TODO: quit 할때 클라이언트에서 소켓을 끊어버리면 알아서 read에서 0 리턴하는데 여기서 처리해줄필요가 있을까? 클라쪽에서 그냥 끊어버리면 되는거아닌감? 질문
			break;
		} else {
			if(sep_at != NULL) // 짤랐던 구간 복구
				*sep_at = ' ';
			popen_handling(msg, cur_path, client_sock);		
		}
		
		// 메세지 보낸게 끝났다는 EOF처리 & 마무리
		eof_handling(cur_path, client_sock);
	}
	
	close(client_sock);
	close(server_sock);
	
	return 0;
}

const char* getUserName()
{
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);
	if (pw) {
		return pw->pw_name;
	}

	return "";
}

void eof_handling(char* path, int socket) {
	char buf[1024] = {0x00, };
	struct utsname info;

	// 메세지 전부 보냈다는 것을 알림
	fprintf(buf, ":EOF");
	write(socket, buf, sizeof(buf));

	// 현재 유저명 & 경로를 보내줌
	if(uname(&info) == -1) {
		error_handling("uname error");
	}

	fprintf(buf, "%s@%s:%s# ", getUserName(), info.nodename, path);
	write(socket, buf, sizeof(buf));
}

void popen_handling(char* msg, char* path, int socket) {
	FILE* fp = NULL;
	char buf[1024] = {0x00, };
	
	chdir(path); // 매번 이렇게 루트 위치를 옮겨줘야 되나? popen이 fork로 하위 프로세스 만들어서 하는건데 유지안되나?..
	
	fp = ppopen(msg, 'r');
	if(fp == NULL)
		error_handling("popen error");
	
	while(fgets(buf, sizeof(buf), fp)) {
		write(socket, buf, sizeof(buf));
	}
	
	pclose(fp);
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