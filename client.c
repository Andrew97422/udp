/*
 * Шаблон TCP клиента.
 *
 * Компиляция:
 *	cc -Wall -O2 -o client client.c
 *
 * Завершение работы клиента: Ctrl+D.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <limits.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PORT 1027
#define MAXLINE 256

#define SA struct sockaddr 

/*
 * Обработчик фатальных ошибок.
 */
void error(const char *s)
{
	perror(s);
	exit(-1);
}   

/*
 * Функции-обертки.
 */
int Socket(int domain, int type, int protocol)
{
	//printf("In Socket \n");
	//printf("%d domain \n", domain);
	//printf("%d type \n", type);
	//printf("%d protocol \n", protocol);
	int rc;
	
	rc = socket(domain, type, protocol);
	if(rc == -1) error("socket()");

	return rc;
}

void Connect(int socket, const struct sockaddr *addr, socklen_t addrlen)
{
	int rc;
	
	rc = connect(socket, addr, addrlen);
	printf("Connection... \n");
	if(rc == -1) error("connect()");
}

void Close(int fd)
{
	int rc;
	
	for(;;) {
		rc = close(fd);
		if(!rc) break;
		if(errno == EINTR) continue;
		error("close()");
	}
}

void Inet_aton(const char *str, struct in_addr *addr)
{
	int rc;
	
	rc = inet_aton(str, addr);
	if(!rc) {
		/* Функция inet_aton() не меняет errno в случае ошибки. Чтобы
		сообщение, выводимое error(), было более осмысленным,
		присваиваем errno значение EINVAL. */
				
		errno = EINVAL;
		error("inet_aton()");
	}
}

int Select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	int rc;
	
	for(;;) {
		rc = select(n, readfds, writefds, exceptfds, timeout);
		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("select()");
	}
	
	return rc;
}

size_t Read(int fd, void *buf, size_t count)
{
	ssize_t rc;
	
	for(;;) {
		rc = read(fd, buf, count);
		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("read()");
	}
	
	return rc;
}

size_t Write(int fd, const void *buf, size_t count)
{
	ssize_t rc;
	
	for(;;) {
		rc = write(fd, buf, count);

		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("write()");
	}
	
	return rc;
}

/*
 * Запись count байтов в сокет.
 */
size_t writen(int socket, const char *buf, size_t count)
{
	const char *p;
	size_t n, rc;

	/* Проверить корректность переданных аргументов. */
	if(buf == NULL) {
		errno = EFAULT;
		error("writen()");
	}
	
	p = buf;
	n = count;
	while(n) {
		rc = Write(socket, p, n);
		n -= rc;
		p += rc;
	}

	return count;
}

void show_usage()
{
	puts("Usage: client ip_address");	
	exit(-1);
}

void do_work(int socket)
{
	//printf("n");
	int n;
	fd_set readfds;
	char s[MAXLINE];
	ssize_t rc;

	n = MAX(STDIN_FILENO, socket) + 1;	
	for(;;) {
		//printf("ff");
		/* Инициализировать набор дескрипторов. */
		FD_ZERO(&readfds);
		//printf(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(socket, &readfds);

		Select(n, &readfds, NULL, NULL, NULL);
		if(FD_ISSET(STDIN_FILENO, &readfds)) {
			rc = Read(STDIN_FILENO, s, MAXLINE);
			//printf(rc);
			if(!rc) break;
			writen(socket, s, rc);
		}
		if(FD_ISSET(socket, &readfds)) {
			rc = Read(socket, s, MAXLINE);
			//printf(rc);
			if(!rc) break;
			Write(STDOUT_FILENO, s, rc);
		}
	}
}

int main(int argc, char **argv)
{
	int socket;
	struct sockaddr_in servaddr;
	
	if(argc != 2) show_usage();
	//printf("main1 \n");
	socket = Socket(PF_INET, SOCK_STREAM, 0);
	//printf("main2 \n");
	/* Инициализировать структуру адреса сокета. */
	memset(&servaddr, 0, sizeof(servaddr));
	//printf("main3 \n");
	servaddr.sin_family = AF_INET;
	//printf("main4 \n");
	servaddr.sin_port = htons(PORT);
	//printf("main5 \n");
	//printf("%s", argv[1]);
	//printf("\n");
	Inet_aton(argv[1], &servaddr.sin_addr);
	//printf("main6 \n");
	//printf(argv[1]);

	Connect(socket, (SA *) &servaddr, sizeof(servaddr));
	//printf("main7 \n");
	do_work(socket);
	//printf("\n main8 \n");
	Close(socket);
	
	return 0;
}
