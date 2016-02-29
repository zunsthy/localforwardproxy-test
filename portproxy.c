#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
//#include <sys/time.h>
#include <string.h>
#include <errno.h>

#define MAXTEXT 255
#define MAXLINE 1024
#define LISTENQ 1024
#define MAXFD 64

typedef struct sockaddr SA;

void equit(const char *str){
	printf("\nerror[%d]: %s", errno, str);
	exit(-1);
}
void vquit(const char *str){
	printf("\nerror: '%s' function is coming...\n", str);
	exit(-5);
}

#define max(a,b) (a>b?a:b)

void startProxy(int sockfr, int sockto){
	int maxfdp1, val, stdineof;
	ssize_t	n, nwritten;
	fd_set rset, wset;
	char to[MAXLINE], fr[MAXLINE];
	char *toiptr, *tooptr, *friptr, *froptr;

	if((val = fcntl(sockto, F_GETFL, 0)) == -1)
		equit("fcntl() error");
	if(fcntl(sockto, F_SETFL, val | O_NONBLOCK) == -1)
		equit("fcntl() error in sockto");

	if((val = fcntl(sockfr, F_GETFL, 0)) == -1)
		equit("fcntl() error");
	if(fcntl(sockfr, F_SETFL, val | O_NONBLOCK) == -1)
		equit("fcntl() error in sockfr");

	toiptr = tooptr = to;	
	friptr = froptr = fr;
	stdineof = 0;

	maxfdp1 = max(sockto, sockfr) + 1;
	for( ; ; ){
		FD_ZERO(&rset);
		FD_ZERO(&wset);
		if(stdineof == 0 && toiptr < &to[MAXLINE])
			FD_SET(sockfr, &rset);
	
		if(friptr < &fr[MAXLINE]) FD_SET(sockto, &rset);
		if(tooptr != toiptr) FD_SET(sockto, &wset);
		if(froptr != friptr) FD_SET(sockfr, &wset);

		if(select(maxfdp1, &rset, &wset, NULL, NULL) == -1)
			equit("nothing in select()");

		if(FD_ISSET(sockfr, &rset)){
			if((n = read(sockfr, toiptr, &to[MAXTEXT] - toiptr)) < 0){
				if(errno != EWOULDBLOCK)
					equit("read error on sockfr");
			} else if(n == 0){
				//printf("EOF on sockfr\n");
				stdineof = 1;
				if(tooptr == toiptr){
					if(shutdown(sockto, SHUT_WR) != 0)
						equit("shutdown() error");
				}
			} else {
				//printf("read %d bytes from sockfr\n", n);
				toiptr += n;
				FD_SET(sockto, &wset);	
			}
		}

		if(FD_ISSET(sockto, &rset)){
			if((n = read(sockto, friptr, &fr[MAXTEXT] - friptr)) < 0){
				if(errno != EWOULDBLOCK)
					equit("read error on sockto");
			} else if(n == 0){
				//printf("EOF on sockto\n");
				if(stdineof)
					return;
				else
					return;
				equit("str_cli() error for connect peer terminated prematurely");
			} else {
				//printf("read %d bytes from sockto\n", n);
				friptr += n;
				FD_SET(sockfr, &wset);	
			}
		}

		if(FD_ISSET(sockfr, &wset) && ((n = friptr - froptr) > 0)){
			if((nwritten = write(sockfr, froptr, n)) < 0){
				if(errno != EWOULDBLOCK)
					equit("write error to sockfr");
			} else {
				//printf("%s: wrote %d bytes to sockfr\n", nwritten);
				froptr += nwritten;
				if(froptr == friptr)
					froptr = friptr = fr;	
			}
		}

		if(FD_ISSET(sockto, &wset) && ( (n = toiptr - tooptr) > 0)){
			if((nwritten = write(sockto, tooptr, n)) < 0){
				if(errno != EWOULDBLOCK)
					equit("write error to sockto");
			} else {
				//printf("write %d bytes to sockto\n", nwritten);
				tooptr += nwritten;	
				if(tooptr == toiptr){
					toiptr = tooptr = to;	
					if(stdineof){
						if(shutdown(sockto, SHUT_WR) != 0)
							equit("shutdown() error");
					}
				}
			}
		}
	}	
}

int main(int argc, char* argv[])
{
	struct sockaddr_in sock_conn, sock_proxy, sock_lstn;
	struct sockaddr_in6 sock_conn6, sock_proxy6, sock_lstn6;
	socklen_t len = sizeof(sock_proxy), len6 = sizeof(sock_proxy6);
	int fd_lstn, fd_conn, fd_proxy; 

	int param = 1;
	char port_lstn[MAXTEXT] = "21025",
	     addr_lstn[MAXTEXT] = "127.0.0.1",
	     port_conn[MAXTEXT] = "21025",
	     addr_conn[MAXTEXT] = "::1";
	int type_lstn = 4, type_conn = 6;
	     
	for(param = 1; param < argc; ++param){
		if(strncmp(argv[param], "-a", 2) == 0 || strncmp(argv[param], "--port", 6) == 0){
			++param;
			strncpy(addr_lstn, argv[param], MAXTEXT);
		} else if(strncmp(argv[param], "-p", 2) == 0 || strncmp(argv[param], "--addr", 6) == 0){
			++param;
			strncpy(port_lstn, argv[param], MAXTEXT);
		} else if(strncmp(argv[param], "-t", 2) == 0 || strncmp(argv[param], "--type", 6) == 0){
			++param;
			if(strncmp(argv[param], "v4tov6", 6) == 0){
				type_lstn = 4;
				type_conn = 6;
			} else if(strncmp(argv[param], "v4tov4", 6) == 0){
				type_lstn = 4;
				type_conn = 4;
			} else if(strncmp(argv[param], "v6tov4", 6) == 0){
				type_lstn = 6;
				type_conn = 4;
			} else if(strncmp(argv[param], "v6tov6", 6) == 0){
				type_lstn = 6;
				type_conn = 6;
			}
		} else if(strncmp(argv[param], "-c", 2) == 0 || strncmp(argv[param], "--connect_addr", 14) == 0){
			++param;
			strncpy(addr_conn, argv[param], MAXTEXT);
		} else if(strncmp(argv[param], "-s", 2) == 0 || strncmp(argv[param], "--connect_port", 14) == 0){
			++param;
			strncpy(port_conn, argv[param], MAXTEXT);
		} else {
			//######0123456789012345
			printf(	"Usage: portproxy [-a|--addr listen_addr]\n"
				"                 [-p|--port listen_port]\n"
				"                 [-c|--connect_addr connect_addr]\n"
				"                 [-s|--connect_port connect_port]\n"
				"                 [-t|--typt v4tov4|v4tov6]\n"
				"\n");
			exit(0);
		}
	}
	
	// listen and listening
	if(type_lstn == 4){
		if((fd_lstn = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			equit("socket() error for listen_fd");
		memset(&sock_lstn, 0, sizeof(sock_lstn));
		sock_lstn.sin_family 	= AF_INET;
		sock_lstn.sin_port	= htons(atoi(port_lstn));
		if(inet_pton(AF_INET, addr_lstn, &sock_lstn.sin_addr) != 1)
			equit("[ipv4] inet_pton() error");
		if(bind(fd_lstn, (SA *)&sock_lstn, sizeof(sock_lstn)) != 0)
			equit("bind() error");
	} else if(type_lstn == 6){
		if((fd_lstn = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
			equit("socket() error for listen_fd");
		memset(&sock_lstn6, 0, sizeof(sock_lstn6));
		sock_lstn6.sin6_family 	= AF_INET6;
		sock_lstn6.sin6_port	= htons(atoi(port_lstn));
		if(inet_pton(AF_INET6, addr_lstn, &sock_lstn6.sin6_addr) != 1)
			equit("[ipv6] inet_pton() error");
		if(bind(fd_lstn, (SA *)&sock_lstn6, sizeof(sock_lstn6)) != 0)
			equit("bind() error");
	} else {
		vquit("v6tov4' or 'v6tov6");
	}
	if(listen(fd_lstn, LISTENQ) != 0)
		equit("listen() error");
	printf("start listening...\n");

	// connect
	if(type_conn == 6){
		memset(&sock_conn6, 0, sizeof(sock_conn6));
		sock_conn6.sin6_family 	= AF_INET6;
		sock_conn6.sin6_port 	= htons(atoi(port_conn));
		if(inet_pton(AF_INET6, addr_conn, &sock_conn6.sin6_addr) != 1)
			equit("inet_pton() error");
	} else if(type_conn == 4){
		memset(&sock_conn, 0, sizeof(sock_conn));
		sock_conn.sin_family 	= AF_INET;
		sock_conn.sin_port 	= htons(atoi(port_conn));
		if(inet_pton(AF_INET, addr_conn, &sock_conn.sin_addr) != 1)
			equit("inet_pton() error");
	} else {
		vquit("v6tov4' or 'v4tov4");
	}

	for( ; ; ){
		// accept
		if(type_lstn == 4){
			if((fd_proxy = accept(fd_lstn, (SA *)&sock_proxy, &len)) == -1)
				equit("accept() error");
		} else if(type_lstn == 6){
			if((fd_proxy = accept(fd_lstn, (SA *)&sock_proxy6, &len6)) == -1)
				equit("accept() error");
		} else {
			vquit("...");
		}
		// connect
		if(type_conn == 6){
			if((fd_conn = socket(AF_INET6, SOCK_STREAM, 0)) == -1)
				equit("socket() error for connect_fd");
			if(connect(fd_conn, (SA *)&sock_conn6, sizeof(sock_conn6)) != 0)
				equit("connect() error");
		} else if(type_conn == 4){
			if((fd_conn = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				equit("socket() error for connect_fd");
			if(connect(fd_conn, (SA *)&sock_conn, sizeof(sock_conn)) != 0)
				equit("connect() error");
		} else {
			vquit("...");
		}
		// proxy
		startProxy(fd_proxy, fd_conn);
		// on end
		close(fd_conn);
		close(fd_proxy);
	}
	
	close(fd_lstn);
	return(0);
}
