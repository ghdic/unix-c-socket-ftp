#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

// gcc -o chat_client chat_client.c -lpthread

int create_socket(int port);
void signal_handler(int sig);
void* read_thread(void* data);
void* chat_thread(void* data);
struct ifreq get_ifreq(int sock);
void error_handling(char* msg);

enum Chat {
    READ = 0,
    WRITE
};

struct thread_args {
    int sock;
};

volatile int stop = 0; // 사용 할 때마다 메모리 접근, 쓰레드 사용시 공유자원이 바뀐지 모르고 돌아갈수있음
pthread_t p_thread[2];

int main(int argc, char* argv[]) {
	int client_sock;
	int port;
	
	if(argc < 2) {
		port = 50000;
	} else {
		port = atoi(argv[1]);
	}
	
	client_sock = create_socket(port);
	////////////// READ ////////
	int thr_id;
    int result;
	struct thread_args* info;

    info = malloc(sizeof(struct thread_args));
    info->sock = client_sock;

	thr_id = pthread_create(&p_thread[READ], NULL, read_thread, (void*)info);
	if (thr_id == -1) {
        error_handling("thread create error");
    }

    info = malloc(sizeof(struct thread_args));
    info->sock = client_sock;

    thr_id = pthread_create(&p_thread[WRITE], NULL, chat_thread, (void*)info);
	if (thr_id == -1) {
        error_handling("thread create error");
    }
	
    pthread_join(p_thread[READ], (void*)&result);
    pthread_join(p_thread[WRITE], (void*)&result);
	
	close(client_sock);
	
	return 0;
}

int create_socket(int port) {
	struct sockaddr_in server_addr;
	int sock;
	
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket error");
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	
	if(connect(sock, (struct sockaddr* )&server_addr, sizeof(server_addr)) == -1)
		error_handling("connect error");
	else
		printf("connect success!!\n");
	
	return sock;
}

void signal_handler(int sig) {
    stop = 1;
}

void* read_thread(void* data) {
    struct thread_args* info = data;
    int sock = info->sock;
	char buf[1024] = {0x00, };
    
    signal(SIGUSR1, signal_handler);
	
	while(!stop) {
		int readResult = read(sock, buf, sizeof(buf));
		
		if(readResult == -1)
			error_handling("read error");
		if(readResult == 0) { // 상대가 연결을 끊을 경우 read 0bytes
			printf("disconnected from server\n");
			break;
		}
	
		printf("%s", buf);
	}

    printf("read thread finished!\n");

    free(data);
    pthread_kill(p_thread[WRITE], SIGUSR1);
}

void* chat_thread(void* data) {
    struct thread_args* info = data;
    int sock = info->sock;
    char buf[1024] = {0x00, };
    char nickname[38] = {'\0', }; // 닉네임 최대 20글자 \n, \0자리 포함해서 입력받으므로 최대 22 + 16(ip)


    signal(SIGUSR1, signal_handler);

	printf("채팅서버에 연결되었습니다! 종료를 원할 경우 !quit을 입력해주세요\n");
	printf("닉네임을 입력해주세요 >> ");
	fgets(nickname, 22, stdin);
    nickname[strlen(nickname) - 1] = '@'; // \n 제거, 닉네임이 20글자보다 많을 경우 20자에서 잘리게됌
    // struct sockaddr_in addr;
    // socklen_t addr_len = sizeof(struct sockaddr_in);
    // getsockname(sock, (struct sockaddr *) &addr, &addr_len);
    struct ifreq ifr = get_ifreq(sock);
    strncpy(nickname + strlen(nickname), inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), 16);
    write(sock, nickname, sizeof(nickname));
    fflush(stdin);
	while(!stop) {
        printf("%s>@@@", nickname);
		fgets(buf, sizeof(buf), stdin);
		
		if(strcmp(buf, "!quit\n") == 0)
			break;
		
		if(write(sock, buf, sizeof(buf)) == -1)
			error_handling("write error");
		
	}
    printf("chat_thread finished!\n");

    free(data);
    pthread_kill(p_thread[READ], SIGUSR1);
}

struct ifreq get_ifreq(int sock) {
    struct ifreq ifr;

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

    ioctl(sock, SIOCGIFADDR, &ifr);

    return ifr;
}

void error_handling(char* msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(-1);
}