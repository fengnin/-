#ifndef __UNP_H__
#define __UNP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/un.h>

#define LISTENQ 1024
#define MAXROOM 1024//房间最大人数极限
#define MB 1024*1024

/* Miscellaneous constants */
#define	MAXLINE		4096	/* max text line length */
#define	MAXSOCKADDR  128	/* max socket address structure size */
#define	BUFFSIZE	8192	/* buffer size for reads and writes */

#define MAXLINE     4096    /* max text line length */
#define LISTENQ     1024    /* 2nd argument to listen() */
#define BUFFSIZE    8192    /* buffer size for reads and writes */

#define SA struct sockaddr

/* Define bzero() as a macro if it's not already defined */
#ifndef bzero
#define bzero(ptr,n)        memset(ptr, 0, n)
#endif

#define	MIN(a,b)	((a) < (b) ? (a) : (b))
#define	MAX(a,b)	((a) > (b) ? (a) : (b))

typedef void Sigfunc(int);

/* Prototypes for our own library functions */
int Accept(int fd, struct sockaddr* sa, socklen_t* salenptr);
void Bind(int fd, const struct sockaddr* sa, socklen_t salen);
void Connect(int fd, const struct sockaddr* sa, socklen_t salen);
void Listen(int fd, int backlog);
int Socket(int family, int type, int protocol);
void Close(int fd);
void Setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen);
ssize_t Read(int fd, void* ptr, size_t nbytes);
ssize_t Write(int fd, const void* ptr, size_t nbytes);
ssize_t Readn(int fd, void* vptr, size_t n);
ssize_t Writen(int fd, const void* vptr, size_t n);
ssize_t Readline(int fd, void* vptr, size_t maxlen);
void err_sys(const char* fmt, ...);
void err_quit(const char* fmt, ...);
void err_msg(const char* fmt, ...);
void err_ret(const char* fmt, ...);
void Inet_pton(int family, const char* strptr, void* addrptr);
const char* Inet_ntop(int family, const void* addrptr, char* strptr, size_t len);
int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
int Tcp_connect(const char* host, const char* serv);
int Tcp_listen(const char* host, const char* serv, socklen_t* addrlen);
void Writen(int fd, void* ptr, size_t nbytes);
void Write_fd(int fd, void* ptr, size_t nbytes, int sendfd);
ssize_t write_fd(int fd, void* ptr, size_t nbytes, int sendfd);
ssize_t Read_fd(int fd, void* ptr, size_t nbytes, int* recvfd);
void Socketpair(int family, int type, int protocol, int* sockfd);
char* Sock_ntop(char* str, int size, const sockaddr* sa, socklen_t salen);
ssize_t writen(int fd, void* ptr, size_t nbytes);
uint32_t getpeerip(int fd);
Sigfunc* Signal(int signo, Sigfunc* func);
void sig_child(int signo);

void* Calloc(size_t n, size_t size);
/* POSIX requires that an #include of <poll.h> DefinE INFTIM, but many
   systems still DefinE it in <sys/stropts.h>.  We don't want to include
   all the STREAMS stuff if it's not needed, so we just DefinE INFTIM here. */
#ifndef INFTIM
#define INFTIM          (-1)    /* infinite poll timeout */
#endif

#endif /* __UNP_H__ */
