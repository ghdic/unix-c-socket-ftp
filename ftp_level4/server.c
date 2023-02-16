/*********************************
 *	FTP Server	*
 *********************************/
// 필요 헤더 선언
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>
#include <pwd.h> // username
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>

#define BUFSIZE 65535
#define BUFFER_SIZE 1024
#define MAXSOCK 64
#define MAXBACKLOG 4

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// 함수 프로토타입
void removeFrontSpaces(char *);
void printMsg(char* formatString, ...); // 서버 메시지 기록 루틴
void run(int sock, char* cpath);
void popen_handling(char* msg, char* txbuf);
void cd(char* path, char* cur_path, char* txbuf);
void ls(char* cur_path, char* txbuf);
int read_data(int sock, char* buffer, uint32_t required); // 도중에 signal 발생시 BUFSIZE만큼 한번에 못읽어 오거나 못쓰는 경우가 발생한다
int write_data(int fd, const char *buffer, uint32_t required);
void file_download(char* filepath, int sock);
void file_upload(char* filepath, int sock);
void *command(void *data);
void client_num_handler(int val);

struct info {
	int clisock;
	char cpath[BUFSIZE];
};

// 전역변수 선언
int servport, shmid, semid, *client_num;
struct sembuf p_op = {0, -1, SEM_UNDO},
			v_op = {0, 1, SEM_UNDO};


int main(int argc, char * argv[]) {
	int servsock, clisock, status;	//서버 소켓, 통신용 소켓 변수
	int cliaddrlen;		// Client Address 구조체 길이 저장 변수
	struct sockaddr_in servaddr, cliaddr;
	pthread_t p_thread[MAXBACKLOG];

	shmid = shmget(IPC_PRIVATE, sizeof(*client_num), 0666);
	client_num = (int *)shmat(shmid, NULL, 0);
	semid = semget(IPC_PRIVATE, 1, 0666);
	semctl(semid, 0, SETVAL, 1);
	*client_num = 0;
	
	char cpath[BUFSIZE] = {};	//고정 경로 저장

	getcwd(cpath, BUFSIZE);

	if(argc < 2) {
		printf("Usage : %s <Port#>\n", argv[0]);
		exit(1);
	}

	servport=atoi(argv[1]);

	// 소켓 생성
	if((servsock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("sock() error");
		exit(1);
	}

	// 서버 소켓 주소 구조체 정보
	memset(&servaddr, 0x00, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(servport);

	// 서버 소켓번호와 서버 주소, 서버 포트 바인딩
	if(bind(servsock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("bind() error");
		exit(1);
	}
	printf("Waiting for connection! \n");	
	listen(servsock, MAXBACKLOG);	//클라이언트 접속 4명 까지 요청 대기

	cliaddrlen = sizeof(cliaddr);
	while(1) {
		if((clisock = accept(servsock, (struct sockaddr *)&cliaddr, &cliaddrlen)) < 0) {
			perror("accept() error");
			exit(1);
		} else {
			if(*client_num < MAXBACKLOG) {
				printf("Client Sock connection %d\n", clisock);
				client_num_handler(1);
				printf("현재 연결되어 있는 클라이언트 수: %d\n", *client_num);
				struct info f;
				f.clisock = clisock;
				memcpy(f.cpath, cpath, BUFSIZE);
				pthread_create(&p_thread[*client_num], NULL, command, (void *)&f);
			} else {
				printf("Connected Client Number is OverFlow!!\n");
				close(clisock);
			}
		}
	}

	shmdt(client_num);
	shmctl(shmid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID, NULL);

	close(servsock);
	return 0;
}

void popen_handling(char* msg, char* txbuf) {
	FILE* fp = NULL;
	char buf[BUFSIZE] = {0x00, };
	
	snprintf(buf, BUFSIZE, "%s 2>&1", msg); // stderr -> stdout으로 디스크립터 옮겨 에러내용도 전달가능하게 
	fp = popen(buf, "r");
	if(fp == NULL) {
		snprintf(txbuf, BUFSIZE, "%s\n", strerror(errno));
	} else {
		while(fgets(buf, BUFSIZE, fp)) {
			strncpy(txbuf + strlen(txbuf), buf, strlen(buf));
		}
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

void cd(char* path, char* cur_path, char* txbuf) {
	if(chdir(path) == -1) {
		char buf[1024];
		snprintf(txbuf, BUFSIZE, "bash: cd: %s: %s\n", path, strerror(errno));
		return;
	}
	getcwd(cur_path, BUFSIZE);
	snprintf(txbuf, BUFSIZE, "change server path [%s]\n", cur_path);
}

void ls(char* cur_path, char* txbuf) {
	DIR* dir = NULL;
	struct dirent* entry = NULL;
	char buf[BUFSIZE] = {0x00, };

	if((dir = opendir(cur_path)) == NULL) {
		snprintf(txbuf, BUFSIZE, "current directory path error : %s\n", cur_path);
		closedir(dir);
		return;
	}

	while((entry = readdir(dir))!= NULL) {
		if(strcmp(entry->d_name, ".") == 0) continue;
		if(strcmp(entry->d_name, "..") == 0) continue;
		snprintf(buf, BUFSIZE, "%s ", entry->d_name);
		strncpy(txbuf + strlen(txbuf), buf, strlen(buf));
	}
	strncpy(txbuf + strlen(txbuf), "\n", strlen("\n"));

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

// client -> server
void file_download(char* filepath, int sock) {
	FILE* fp;
	char buf[BUFFER_SIZE] = {0x00, };
	char *filename, *bp;

	filename = strrchr(filepath, '/'); // '/'의 마지막 위치, 없으면 NULL반환 파일명만 빼냄
	if(filename == NULL)
		filename = filepath;
	else
		filename = filename + 1;

	if((fp = fopen(filename, "w")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		write_data(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write_data(sock, buf, BUFFER_SIZE);
	}

	read_data(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		remove(filename);
		bp = strtok(NULL, "");
		printf("%d %s %s\n", sock, buf, bp); // 이런식으로도 복구가능 ㅋㅋ(strtok 할때 사이에 \0 있기 때문)
		return;
	}

	read_data(sock, buf, BUFFER_SIZE);
	uint32_t file_size, len, size = 0;

	sscanf(buf, "%d", &file_size);

	while(1) {
		uint32_t readable = (file_size - size) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - size);
		if((len = read(sock, buf, readable)) > 0) {
			fwrite(buf, sizeof(char), len, fp);
			size += len;
			if(size == file_size) {
				break;
			}
		} else if(len == 0) {
			break;
		} else {
			if(errno == EINTR) continue;
			else break;
		}
	}
	fclose(fp);
}

// server -> client
void file_upload(char* filepath, int sock) {
	FILE* fp;
	char buf[BUFFER_SIZE] = {0x00, };
	char* bp;

	if((fp = fopen(filepath, "r")) == NULL) {
		snprintf(buf, BUFFER_SIZE, ":ERROR %s", strerror(errno));
		write_data(sock, buf, BUFFER_SIZE);
		return;
	} else {
		snprintf(buf, BUFFER_SIZE, ":SUCCESS");
		write_data(sock, buf, BUFFER_SIZE);
	}

	read_data(sock, buf, BUFFER_SIZE);
	bp = strtok(buf, " ");
	if(strcmp(bp, ":ERROR") == 0) {
		bp = strtok(NULL, "");
		printf("%d: %s %s\n", sock, buf, bp);
		return;
	}

	fseek (fp, 0, SEEK_END);
	uint32_t file_size = ftell(fp), size = 0, len;
	rewind(fp);

	snprintf(buf, BUFFER_SIZE, "%d", file_size);
	write_data(sock, buf, BUFFER_SIZE);

	while(feof(fp) == 0) {
		uint32_t writeable = (file_size - size) > BUFFER_SIZE ? BUFFER_SIZE : (file_size - size);
		fread(buf, sizeof(char), writeable, fp);
		if((len = write(sock, buf, writeable)) > 0) {
			size += len;
			if(size == file_size)
				break;
		} else if (len == 0) {
			break;
		} else {
			if(errno == EINTR) continue;
			else break;
		}
	}

	fclose(fp);
}

void *command(void *data) {
	char rxbuf[BUFSIZE] = {0,};	// 수신 저장
	char txbuf[BUFSIZE] = {0,};	// 송신 저장

	struct info f = *((struct info *)data);

	chdir(f.cpath);
	
	while(1) {
		long len = read_data(f.clisock, rxbuf, BUFSIZE);
		if(len == -1) {
				perror("recv() 에러");
				exit(-1);
			}
		if(len == 0)  { // 클라이언트 연결 단절 확인
			printf("클라이언트 단절: s= %d\n", f.clisock);
			break;
		}
		else {
			memset(txbuf, 0, BUFSIZE);
			// ls 명령 
			if(strncmp(rxbuf, "ls",2)==0) {
				if(strcmp(rxbuf, "ls") == 0) {
					ls(f.cpath, txbuf);
				} else {
					popen_handling(rxbuf, txbuf);
				}
				write_data(f.clisock, txbuf, BUFSIZE);
			}
			// cd 명령
			else if(strncmp(rxbuf, "cd",2)==0) {
				char path[BUFSIZE] = {'\0'};
				sscanf(rxbuf, "cd %s", path);
				cd(path, f.cpath, txbuf);
				write_data(f.clisock, txbuf, BUFSIZE);
			}
			// get 명령
			else if(strncmp(rxbuf, "get",3)==0) {
				char * const sep_at = strchr(rxbuf, ' ');
				char *file_path = sep_at + 1;
				file_upload(file_path, f.clisock);
			}
			// put 명령
			else if(strncmp(rxbuf, "put",3)==0) {
				char * const sep_at = strchr(rxbuf, ' ');
				char *file_path = sep_at + 1;
				file_download(file_path, f.clisock);
			}
			// quit 명령 
			else if(strncmp(rxbuf, "quit",4)==0) {
				printf("Quit: a client out \n");
				break;
			}
			// 허용 안되는 명령
			else printf("Illegal command \n");
		}
	}
	close(f.clisock);
	client_num_handler(-1);
	printf("현재 연결되어 있는 클라이언트 수: %d\n", *client_num);
}

void client_num_handler(int val) {
	semop(semid, &p_op, 1);
	*client_num = *client_num + val;
	semop(semid, &v_op, 1);
}