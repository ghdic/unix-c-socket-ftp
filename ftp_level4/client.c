/*********************************
 *	FTP Client	*
 *********************************/

/*필요 헤더 선언*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h> // username
#include <dirent.h>

// Optimized buffer sizes for better performance
#define BUFSIZE 8192        // Reduced from 65535 for better memory usage
#define BUFFER_SIZE 8192    // Increased from 1024 for better I/O throughput
#define MAXSOCK 128
#define IPNum "127.0.0.1" //Server IP 주소

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// 함수 프로토타입(Prototype) 
void removeFrontSpaces(char *);	// remove spaces 
void cmdHelp();			// help
void popen_handling(char* msg);
const char* getUserName();
void cd(char* path, char* cur_path);
void ls(char* cur_path);
int read_data(int sock, char* buffer, uint32_t required);
int write_data(int fd, const char *buffer, uint32_t required);
void file_download(char* filepath, int sock);
void file_upload(char* filepath, int sock);
static inline void set_socket_options(int sock);

// Socket optimization function
static inline void set_socket_options(int sock) {
	int opt = 1;
	int bufsize = 65536;
	
	// Enable TCP_NODELAY for lower latency
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
	
	// Set send and receive buffer sizes
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
}

int main(int argc, char * argv[])
{
	int sock;			//소켓디스크립터 저장 int형 변수
	int fsize;			// 파일 크기
	char charbuf[1]={0};
	
	struct sockaddr_in ServerAddr;	//bind 시킬 구조체 변수
	char inbuf[BUFSIZE] = {0,};	//명령 라인 저장용 배열
	char txbuf[BUFSIZE] = {0,};	//명령 라인 전송용 배열
	char rxbuf[BUFSIZE] = {0,};	//데이터 받을 char형 배열
	char cpath[BUFSIZE] = {0,};	//현재 경로 저장 char형 배열
	unsigned short int x; // 전송받는 버퍼 크기 받는 변수

	getcwd(cpath, sizeof(cpath));
	
	// 사용법
	if(argc < 2) {
		printf("Usage : %s <PortNum>\n", argv[0]);
		exit(1);
	}

	// 소켓 생성 
	if((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("sock() error");
		exit(1);
	}

	//  소켓 주소 구조체 정보 저장*/
	memset(&ServerAddr, 0x00, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr(IPNum);
	ServerAddr.sin_port = htons(atoi(argv[1]));

	if(connect(sock, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) < 0) {
		perror("connect() error");
		exit(1);
	}
	
	// Optimize client socket
	set_socket_options(sock);
	
	printf("FTP Server connected!\n");
	
	while(1) {
		printf("ftp>> ");
		fgets(inbuf, BUFSIZE, stdin); //키보드 입력 저장
		removeFrontSpaces(inbuf);
		inbuf[strlen(inbuf) - 1] = '\0'; // \n 엔터버퍼 비우기
		if (strlen(inbuf)==0) 
			continue; 
		strncpy(txbuf, inbuf, BUFSIZE);
		
		char *endptr = (char*)inbuf + MIN(strlen(inbuf), BUFSIZE - 1);
		char *cmdptr = strtok(inbuf, " \n");
		
		char *argptr = inbuf + strlen(cmdptr) +1;
		if (argptr > endptr || *argptr == '\0') {
			argptr = NULL;
		}

		

		// printf("Inbuf argptr = %s \n", argptr);
		// printf("%s\n", cmdptr);
		
		// cls 명령
		if(strcmp(cmdptr, "cls")==0) {
			if(argptr == NULL)
				ls(cpath);
			else {
				snprintf(txbuf, BUFSIZE, "ls %s", argptr);
				popen_handling(txbuf);
			}
		}
		
		// ccd 명령
		else if(strcmp(cmdptr, "ccd")==0) {
			if(argptr == NULL) continue;
			cd(argptr, cpath);
		}
		
		// ls 명령
		else if(strcmp(cmdptr, "ls")==0) {
			write_data(sock, txbuf, BUFSIZE);
			read_data(sock, rxbuf, BUFSIZE);
			printf("%s", rxbuf);
		}
		
		// cd 명령
		else if(strcmp(cmdptr, "cd")==0) {
			write_data(sock, txbuf, BUFSIZE);
			read_data(sock, rxbuf, BUFSIZE);
			printf("%s", rxbuf);
		}
		
		// get 명령
		else if(strcmp(cmdptr, "get")==0) {
			write_data(sock, txbuf, BUFSIZE);
			char * const sep_at = strchr(txbuf, ' ');
			char *file_path = sep_at + 1;
			file_download(file_path, sock);
		}
			
		// put 명령
		else if(strcmp(cmdptr, "put")==0) {
			write_data(sock, txbuf, BUFSIZE);
			char * const sep_at = strchr(txbuf, ' ');
			char *file_path = sep_at + 1;
			file_upload(file_path, sock);
		}
		
		// help 명령
		else if(strcmp(cmdptr, "help")==0) {
			cmdHelp();
		}

		// quit 명령 
		else if(strcmp(cmdptr, "quit")==0) {
			write_data(sock, txbuf, BUFSIZE);
			printf("종료 되었습니다\n");
			close(sock);
			break;
		}
		else printf("CMD error: Type 'help'\n");
	}	
	return 0;
}

void removeFrontSpaces(char *buf)
{ 
    int i1=0;
	int i2=0;
	
    while (buf[i1] == ' ') {
		i1++;
	}
	while (buf[i1]) {
		buf[i2]=buf[i1];
		i1++; i2++;
	}
	buf[i2]='\0';
}

/*help 명령 함수*/
void cmdHelp() {
	printf("-----------------------------------------------------------\n");
	printf("| ls  [option] : list server directory		|\n");
	printf("| cls [option] : list client directory    		|\n");
	printf("| cd  [option] : Change server directory		|\n");
	printf("| ccd [option] : Change client directory		|\n");
	printf("| get filename : Download a file from the server	|\n");
	printf("| put filename: Upload a file from a client	|\n");
	printf("| quit : exit 					|\n");
	printf("-----------------------------------------------------------\n");
}

// === 필요한 명령 함수들을 여기에 작성하세요 ===
// 커맨드 명령어를 받아서 실행
void popen_handling(char* msg) {
	FILE* fp = NULL;
	char buf[BUFSIZE] = {0x00, };

	snprintf(buf, BUFSIZE, "%s 2>&1", msg); // stderr -> stdout으로 디스크립터 옮겨 에러내용도 전달가능하게 
	fp = popen(buf, "r");
	if(fp == NULL)
		printf("popen error\n");
	
	while(fread(buf, sizeof(char), BUFSIZE, fp)) {
		printf("%s", buf);
	}
	
	pclose(fp);
}

const char* getUserName() {
	uid_t uid = geteuid();
	struct passwd *pw = getpwuid(uid);
	if (pw) {
		return pw->pw_name;
	}

	return "";
}

void cd(char* path, char* cur_path) {
	if(chdir(path) == -1) {
		char buf[1024];
		snprintf(buf, BUFSIZE, "bash: cd: %s: %s\n", path, strerror(errno));
		printf("%s", buf);
		return;
	}
	getcwd(cur_path, BUFSIZE);
	printf("change client path [%s]\n", cur_path);
}

void ls(char* cur_path) {
	DIR* dir = NULL;
	struct dirent* entry = NULL;

	if((dir = opendir(cur_path)) == NULL) {
		printf("current directory path error : %s\n", cur_path);
		closedir(dir);
		return;
	}

	while((entry = readdir(dir))!= NULL) {
		if(strcmp(entry->d_name, ".") == 0) continue;
		if(strcmp(entry->d_name, "..") == 0) continue;
		printf("%s ", entry->d_name);
	}
	printf("\n");

	closedir(dir);
}

int read_data(int sock, char* buffer, uint32_t required) {
	uint32_t size = 0, len;

	while(1) {
		if((len = read(sock, buffer + size, required - size)) > 0) {
			size += len;
			if(size == required)
				return size;
		} else if(len == 0) {
			return size;
		} else {
			if(errno == EINTR) continue;
			else return -1;
		}
	}
}

int write_data(int fd, const char *buffer, uint32_t required) {
	uint32_t size = 0, len;

	while(1) {
		if((len = write(fd, buffer + size, required - size)) > 0) {
			size += len;
			if(size == required)
				return size;
		} else if (len == 0) {
			return size;
		} else {
			if(errno == EINTR) continue;
			else return -1;
		}
	}
}


// server -> client - Optimized version
void file_download(char* filepath, int sock) {
	FILE* fp;
	static char buf[BUFFER_SIZE];  // Static to avoid stack allocation
	char* bp, *filename;

	filename = strrchr(filepath, '/'); // '/'의 마지막 위치, 없으면 NULL반환 파일명만 빼냄
	if(filename == NULL)
		filename = filepath;
	else
		filename = filename + 1;

	read_data(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%s %s\n", buf, bp);
		return;
	}

	if((fp = fopen(filename, "wb")) == NULL) {  // Binary mode for better performance
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		printf("%s\n", buf);
		write_data(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write_data(sock, buf, BUFFER_SIZE);
	}

	// Set file buffer for better I/O performance
	setvbuf(fp, NULL, _IOFBF, BUFFER_SIZE);

	read_data(sock, buf, BUFFER_SIZE);
	uint32_t file_size, size = 0, len; // 최대 4GB
	int check = 0;
	sscanf(buf, "%d", &file_size);

	uint32_t last_progress = 0;
	while(size < file_size) {
		uint32_t readable = (file_size - size) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - size);
		if((len = read(sock, buf, readable)) > 0) {
			// Show progress less frequently for better performance
			uint32_t progress = (size * 100) / file_size;
			if(progress != last_progress && progress % 10 == 0) {
				printf("Processing: %d%%\n", progress);
				last_progress = progress;
			}
			if(fwrite(buf, 1, len, fp) != len) {
				fprintf(stderr, "Write error\n");
				break;
			}
			size += len;
		} else if(len == 0) {
			check = 1;
			break;
		} else {
			if(errno == EINTR) continue;
			else break;
		}
	}
	if(size == file_size) {
		check = 1;
		printf("Processing: 100%%\n");
	} else {
		printf("File Download incomplete from unknown error\n");
	}
	printf("Download File Size: %d bytes, File Name: %s\n", size, filename);
	fclose(fp);
}

// client -> server - Optimized version
void file_upload(char* filepath, int sock) {
	FILE* fp;
	static char buf[BUFFER_SIZE];  // Static to avoid stack allocation

	char* bp;
	read_data(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%s %s\n", buf, bp);
		return;
	}
	
	if((fp = fopen(filepath, "rb")) == NULL) {  // Binary mode for better performance
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		printf("%s\n", buf);
		write_data(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write_data(sock, buf, BUFFER_SIZE);
	}

	// Set file buffer for better I/O performance
	setvbuf(fp, NULL, _IOFBF, BUFFER_SIZE);

	fseek (fp, 0, SEEK_END);
	uint32_t file_size = ftell(fp), len, size = 0;
	int check = 0;
	rewind(fp);

	snprintf(buf, BUFFER_SIZE, "%d", file_size);
	write_data(sock, buf, BUFFER_SIZE);

	uint32_t last_progress = 0;
	while(size < file_size && !feof(fp)) {
		uint32_t writeable = (file_size - size) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - size);
		size_t read_bytes = fread(buf, 1, writeable, fp);
		if(read_bytes > 0) {
			if((len = write(sock, buf, read_bytes)) > 0) {
				// Show progress less frequently for better performance
				uint32_t progress = (size * 100) / file_size;
				if(progress != last_progress && progress % 10 == 0) {
					printf("Processing: %d%%\n", progress);
					last_progress = progress;
				}
				size += len;
			} else if (len == 0) {
				check = 1;
				break;
			} else {
				if(errno == EINTR) continue;
				else break;
			}
		} else {
			break;
		}
	}

	if(size == file_size) {
		check = 1;
		printf("Processing: 100%%\n");
	} else {
		printf("File Upload incomplete from unknown error\n");
	}
	printf("Upload File Size: %d bytes, File Path: %s\n", size, filepath);

	fclose(fp);
}