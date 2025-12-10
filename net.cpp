#include "unp.h"

//根据对方的fd返回对应的ip	// 根据对方的文件描述符 fd 返回对应的 IP 地址（32 位无符号整数）
uint32_t getpeerip(int fd)
{
	sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	if (getpeername(fd, (sockaddr*)&addr, &addrlen) < 0)	// 获取对端地址
	{
		err_msg("getpeerip error");		// 非致命错误，仅打印消息
		return -1;
	}
	return ntohl(addr.sin_addr.s_addr);	// 转换网络字节序到主机字节序
}

// 封装 select 系统调用，处理中断信号（EINTR）
int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout)
{
	int n = 0;
	while (true)
	{
		n = select(nfds, readfds, writefds, exceptfds, timeout);
		if (n < 0)
			if (errno == EINTR)		// 被信号中断，重试
				continue;
			else
				err_quit("select error");	// 致命错误，退出程序
		else
			break;
	}
	return n;
}

// 将套接字地址转换为可读字符串（支持 IPv4 和 IPv6）
char* Sock_ntop(char* str, int size, const sockaddr* sa, socklen_t salen)
{
	switch (sa->sa_family)
	{
	case AF_INET:
	{
		struct sockaddr_in* sin = (struct sockaddr_in*)sa;
		if (inet_ntop(AF_INET, &sin->sin_addr, str, size) == NULL)	// 转换 IP
		{
			err_msg("inet_ntop error");
			break;
		}
		if (ntohs(sin->sin_port) > 0)	// 附加端口号
			snprintf(str + strlen(str), size - strlen(str), ":<%d>", sin->sin_port);
		return str;
	}
	case AF_INET6:	// IPv6
	{
		struct sockaddr_in6* sin = (struct sockaddr_in6*)sa;
		if (inet_ntop(AF_INET6, &sin->sin6_addr, str, size) == NULL)
		{
			err_msg("inet_ntop error");
			break;
		}
		if (ntohs(sin->sin6_port) > 0)
			snprintf(str + strlen(str), size - strlen(str), ":<%d>", sin->sin6_port);
		return str;
	}
	default:
		return "Sock_ntop error";		// 不支持的协议族
	}
	return NULL;
}

// 创建监听套接字（支持 IPv4/IPv6）
int Tcp_listen(const char* host, const char* serv, socklen_t* addrlen)
{
	int listen_fd = 0;
	struct addrinfo hints, * res, * p;
	memset(&hints, 0, sizeof(hints));
	const int on = 1;

	//设置期望返回的协议族和套接字类型
	hints.ai_family = AF_UNSPEC;//任意ip协议族 ipv4/ipv6
	hints.ai_flags = AI_PASSIVE;//用于绑定地址
	hints.ai_socktype = SOCK_STREAM;//TCP
	int ret = getaddrinfo(host, serv, &hints, &res);	// 解析地址和服务
	if (ret != 0)
	{
		printf("getaddrinfo error : %s\n", gai_strerror(ret));
		err_quit("getaddrinfo error");

	}
	char addr[MAXSOCKADDR] = { 0 };
	for (p = res; p != NULL; p = p->ai_next)
	{
		// 创建套接字
		if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
		{
			err_msg("socket error");
			continue;
		}
		// 设置 SO_REUSEADDR 选项
		if (setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0)
		{
			close(listen_fd);
			err_msg("setsockopt error");
			continue;
		}
		// 绑定地址
		if (bind(listen_fd, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listen_fd);
			err_msg("bind error");
			continue;
		}
		printf("server address : %s\n", Sock_ntop(addr, MAXSOCKADDR, p->ai_addr, p->ai_addrlen));
		if (addrlen != NULL)
			*addrlen = p->ai_addrlen;
		//开始监听
		if (listen(listen_fd, LISTENQ) < 0)
		{
			close(listen_fd);
			err_msg("bind error");
			continue;
		}

		//成功
		freeaddrinfo(res);		// 释放地址信息
		return listen_fd;	// 返回监听套接字
	}
	//到这监听失败
	freeaddrinfo(res);
	err_quit("Tcp_listen error");	// 所有地址尝试失败
	return -1;
}

// 确保写入 nbytes 字节的数据（处理部分写入和中断）
ssize_t writen(int fd, void* ptr, size_t nbytes)
{
	ssize_t ret = 0, total = nbytes;
	ssize_t haswrite = 0;
	char* buf = (char*)ptr;
	while (haswrite < total)
	{
		ret = write(fd, buf + haswrite, total - haswrite);
		if (ret < 0)
		{
			if (errno == EINTR)		// 被信号中断，重试
			{
				ret = 0;
				continue;
			}
			else
			{
				return -1;			// 其他错误
			}
		}
		else if (ret == 0)		// 无数据写入（通常不应发生）
			break;
		haswrite += ret;
	}
	return haswrite;
}

// 发送数据并附带一个文件描述符
ssize_t write_fd(int fd, void* ptr, size_t nbytes, int sendfd)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	struct iovec iov[1];

	union 
	{
		struct cmsghdr cm;
		char control[CMSG_LEN(sizeof(int))];
	}control_un;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	*((int*)CMSG_DATA(cmptr)) = sendfd;		// 设置要传递的文件描述符

	size_t n = sendmsg(fd, &msg, 0);	// 发送消息

	return n;
}

//接收数据ptr并且接收一个文件描述符
ssize_t Read_fd(int fd, void* ptr, size_t nbytes, int* recvfd)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	struct iovec iov[1];//设置msg缓冲区的大小

	union 
	{
		struct cmsghdr cm;
		char control[CMSG_LEN(sizeof(int))];//存放一个文件描述符
	}control_un;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	//设置数据缓冲区
	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	int n = recvmsg(fd, &msg, 0);	// 接收消息
	if (n <= 0)
	{
		return n;	// 错误或连接关闭
	}

	struct cmsghdr* cmptr = CMSG_FIRSTHDR(&msg);
	if (cmptr && cmptr->cmsg_len == CMSG_LEN(sizeof(int)))	// 检查控制消息
	{
		if (cmptr->cmsg_level != SOL_SOCKET)
			err_msg("control level != SOL_SOCKET");
		if (cmptr->cmsg_type != SCM_RIGHTS)
			err_msg("control type != SCM_RIGHTS");
		*recvfd = *((int*)CMSG_DATA(cmptr));		// 提取文件描述符
	}
	else
	{
		*recvfd = -1;		// 无文件描述符
	}
	return n;
}

// 创建一对相互连接的套接字（Socketpair）
void Socketpair(int family, int type, int protocol, int* sockfd)
{
	int n = socketpair(family, type, protocol, sockfd);
	if (n < 0)
		err_msg("socketpair error");
}

// 确保读取 nbytes 字节的数据（处理部分读取和中断）
ssize_t Readn(int fd, void* vptr, size_t n)
{
	size_t total = n;
	size_t ret = 0;
	size_t hasread = 0;
	char* ptr = (char*)vptr;
	while (hasread < total)
	{
		ret = read(fd, ptr + hasread, n - hasread);
		if (ret < 0)
		{
			if (errno == EINTR)	// 被信号中断，重试
			{
				ret = 0;
				continue;
			}
			else
			{
				return -1;	// 其他错误
			}
		}
		else if (ret == 0)//eof
			break;
		hasread += ret;
	}
	return hasread;
}

// 关闭套接字（封装 close 并处理错误）
void Close(int fd)
{
	int n = close(fd);
	if (n < 0)
		err_msg("close fd error");
}

//配置套接字（封装 setsockopt）
void Setsockopt(int fd, int level, int optname, const void* optval, socklen_t optlen)
{
	int n = setsockopt(fd, level, optname, optval, optlen);
	if (n < 0)
		err_msg("Setsockopt error");
}


// 封装 accept 系统调用，处理中断信号（EINTR）
int Accept(int fd, struct sockaddr* sa, socklen_t* salenptr)
{
	while (true)
	{
		int n = accept(fd, sa, salenptr);
		if (n < 0)
		{
			if (errno == EINTR)		// 被信号中断，重试
				continue;
			else
				err_quit("Accept error");		// 致命错误
		}
		else
		{	
			return n;		// 返回新连接的套接字
		}
	}
}

// 封装 bind 系统调用
void Bind(int fd, const struct sockaddr* sa, socklen_t salen)
{
	int n = bind(fd, sa, salen);
	if (n < 0)
		err_quit("Bind error");		// 致命错误
}

// 封装 listen 系统调用
void Listen(int fd, int backlog)
{
	int n = listen(fd, backlog);
	if (n < 0)
		err_quit("listen error");
}