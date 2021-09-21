# FTP LEVEL 1

서버와 클라이언트 1대1 통신방식

* 1대1 통신
* 터미널 명령어 및 커스텀 명령어 만들기(help, quit, cls, ls, cd, get, put)
* Well know port는 사용하지 않음(프로그램 실행시 포트 입력받음, ip는 localhost)

## 컴파일
```
gcc -o ftp_server ftp_server.c
gcc -o ftp_client ftp_client.c
```

or

```
make
make clean
```