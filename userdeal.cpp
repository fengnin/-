#include "unp.h"
#include "unpthread.h"
#include "msg.h"
#include "nethead.h"
pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;	// 保护 Accept 的互斥锁
extern int listen_fd;		// 监听套接字（全局变量）
extern socklen_t addrlen;	// 地址长度（全局变量）
extern Room_Pool* room;		// 房间池（全局变量，管理所有房间进程）
extern int process_num;		// 房间进程数量（全局变量）

// 将消息（MESG）封装并发送到客户端
void write_to_connfd(MESG* msg,int connfd)
{
	char* buf = (char*)malloc(20);	// 分配缓冲区
	memset(buf, 0, 20);
	int pos = 0;
	buf[pos++] = '$';		// 消息头
	//printf("pos : %d\n", pos);

	// 类型（网络字节序）
	uint16_t tmp = ntohs(msg->type);
	MSG_TYPE type = (MSG_TYPE)tmp;
	memcpy(buf + pos, &type, sizeof(uint16_t));
	pos += 2;
	//printf("pos : %d\n", pos);

	pos += 4;//跳过ip
	//printf("pos : %d\n", pos);

	// 数据长度（网络字节序）
	uint32_t len = ntohl(msg->len);
	memcpy(buf + pos, &len, sizeof(uint32_t));
	pos += 4;
	//printf("pos : %d\n", pos);

	// 数据
	memcpy(buf + pos, msg->data, msg->len);
	pos += msg->len;
	//printf("pos : %d\n", pos);

	buf[pos++] = '#';		// 消息尾
	//printf("pos : %d\n", pos);

	// 发送消息
	size_t ret = writen(connfd, buf, pos);
	//printf("writen ret : %d\n", ret);
	if (ret < pos)
		err_msg("write_to_connfd error");

	// 释放资源
	if (msg->data)
		free(msg->data);
	if (msg)
		free(msg);
}

// 处理用户请求（创建/加入会议）
void deal_user_requests(int connfd)
{
	//$ type ip len data #
	char head[15] = { 0 };	// 消息头缓冲区
	while (true)
	{
		// 读取消息头（11 字节：$ + type(2) + ip(4) + len(4)）
		size_t ret = Readn(connfd, head, 11);
		printf("Readn ret : %d\n",ret);
		if (ret <= 0)	// 连接关闭或错误
		{
			close(connfd);
			printf("Readn error\n");
			return;
		}
		else if (ret < 11)
		{
			printf("Package too short\n");
		}
		else if (head[0] != '$')	// 非法消息头
		{
			printf("data format error $\n");
			continue;
		}
		else
		{
			//获取正确报头
			// 解析消息类型
			MSG_TYPE type;
			uint16_t tmp = 0;
			memcpy(&tmp, head + 1, sizeof(uint16_t));
			type = (MSG_TYPE)ntohs(tmp);

			// 解析 IP 和数据长度
			uint32_t ip = 0;
			memcpy(&ip, head + 3, sizeof(uint32_t));
			ip = ntohl(ip);

			uint32_t len = 0;
			memcpy(&len, head + 7, sizeof(uint32_t));
			len = ntohl(len);

			// 分配消息结构体
			MESG* msg = (MESG*)Calloc(1, sizeof(MESG));
			memset(msg, 0, sizeof(MESG));
			//printf("type : %d\n", type);

			// 处理创建会议请求
			if (type == CREATE_MEETING)
			{
				char tail;
				size_t c_ret = Readn(connfd, &tail, 1);	// 读取结束符 #
				if (ret < 0)
				{
					close(connfd);
					printf("CREATE_MEETING Readn error\n");
					return;
				}
				if (len == 0 && tail == '#')		// 格式正确
				{
					msg->type = CREATE_MEETING_RESPONSE;
					char* c_ip = (char*)&ip;
					printf("create meeting ip : %d.%d.%d.%d\n", (u_char)c_ip[3], (u_char)c_ip[2], (u_char)c_ip[1], (u_char)c_ip[0]);

					// 检查是否有空闲房间
					if (room->spare_room == 0)//无房间
					{
						printf("no spare room\n");
						msg->data = (char*)malloc(sizeof(int));
						if (msg->data == NULL)
						{
							close(connfd);
							free(msg);
							errno = ENOMEM;
							err_quit("CREATE_MEETING deal_user_requests : msg->data malloc error\n");
						}
						memset(msg->data, 0, sizeof(int));
						int roomno = 0;//无房间
						memcpy(msg->data, &roomno, sizeof(int));
						msg->len = sizeof(int);
						write_to_connfd(msg, connfd);		// 发送响应
					}
					else//查找空闲房间
					{
						int i;
						Pthread_mutex_lock(&room->mutex);
						for (i = 0; i < process_num; i++)
						{
							if (room->room[i].room_state == 0)
								break;
						}
						//双重判断 防止多进程执行的时候 房间被占用但未设置room_state
						if (i == process_num)//无空闲房间
						{
							printf("no spare room\n");
							msg->data = (char*)malloc(sizeof(int));
							if (msg->data == NULL)
							{
								close(connfd);
								free(msg);
								errno = ENOMEM;
								err_msg("CREATE_MEETING deal_user_requests : msg->data malloc error\n");
							}
							memset(msg->data, 0, sizeof(int));
							int roomno = 0;//无房间
							memcpy(msg->data, &roomno, sizeof(int));
							msg->len = sizeof(int);
							write_to_connfd(msg, connfd);
						}
						else//找到空闲房间
						{
							//传到对应的房间进程 传过去connfd 和 字符C
							printf("room num is : %d\n", room->room[i].child_pid);
							char c = 'C';		// 命令类型：创建会议
							size_t n = write_fd(room->room[i].child_pipefd, &c, 1, connfd);	// 传递文件描述符
							printf("write_fd ret : %ld\n", n);
							if (n < 0)
							{
								err_msg("CREATE_MEETING write_fd error");
							}
							else//创建成功
							{
								//这里父进程关闭文件描述符不会影响子进程，write_fd发送的connfd到子进程内核是深拷贝过去的
								close(connfd);
								room->spare_room--;
								room->room[i].total++;
								room->room[i].room_state = 1;
								Pthread_mutex_unlock(&room->mutex);
								return;
							}
						}
						Pthread_mutex_unlock(&room->mutex);
					}
				}
				else
				{
					printf("data format error #\n");
				}
			}

			// 处理加入会议请求
			else if (type == JOIN_MEETING)
			{
				msg->type = JOIN_MEETING_RESPONSE;
				char* c_ip = (char*)&ip;
				printf("join meeting ip : %d.%d.%d.%d\n", (u_char)c_ip[3], (u_char)c_ip[2], (u_char)c_ip[1], (u_char)c_ip[0]);
				uint32_t roomno = 0;
				char buf[len + 2];//存储data和#		// 存储数据 + 结束符 #
				size_t ret = Readn(connfd, &buf, len + 1);
				printf("Readn ret : %d\n", ret);
				if (ret <= 0)
				{
					close(connfd);
					err_quit("JOIN_MEETING Readn error\n");
				}
				if (ret < len + 1)
				{
					printf("JOIN_MEETING data too short\n");
				}
				else
				{
					if (buf[len] == '#' && len != 0)	// 格式正确
					{
						memcpy(&roomno, buf, len);		// 提取房间号
						roomno = ntohl(roomno);
						//查看房间号是否存在
						int i;
						bool isExist = false;
						for (i = 0; i < process_num; i++)
						{
							if (room->room[i].child_pid == roomno)
							{
								isExist = true;
								break;
							}
						}
						if (isExist)
						{
							//存在房间 判断是否为满
							if (room->room[i].total >= MAXROOM)	// 房间已满
							{
								printf("JOIN_MEETING room is full\n");
								int32_t full = -1;
								msg->len = sizeof(int32_t);
								msg->data = (char*)malloc(sizeof(int32_t));
								if (msg->data == NULL)
								{
									close(connfd);
									free(msg);
									errno = ENOMEM;
									err_quit("JOIN_MEETING deal_user_requests : msg->data malloc error\n");
								}
								memset(msg->data, 0, msg->len);
								memcpy(msg->data, &full, msg->len);
								write_to_connfd(msg, connfd);
							}
							else
							{
								//成功加入
								Pthread_mutex_lock(&room->mutex);
								char c = 'J';		// 命令类型：加入会议
								size_t ret = write_fd(room->room[i].child_pipefd, &c, 1, connfd);
								if (ret < 0)
								{
									err_msg("JOIN_MEETING write_fd error");
								}
								else // 成功传递
								{
									msg->len = sizeof(uint32_t);
									msg->data = (char*)malloc(sizeof(uint32_t));
									if (msg->data == NULL)
									{
										close(connfd);
										free(msg);
										errno = ENOMEM;
										err_quit("JOIN_MEETING deal_user_requests : msg->data malloc error\n");
									}
									memset(msg->data, 0, msg->len);
									memcpy(msg->data, &roomno, msg->len);
									write_to_connfd(msg, connfd);
									room->room[i].total++;
									close(connfd);
									pthread_mutex_unlock(&room->mutex);
									return;
								}
								Pthread_mutex_unlock(&room->mutex);
							}
						}
						else  // 房间不存在
						{
							printf("JOIN_MEETING room no exist\n");
							uint32_t no_exist = 0;
							msg->len = sizeof(uint32_t);
							msg->data = (char*)malloc(msg->len);
							if (msg->data == NULL)
							{
								close(connfd);
								free(msg);
								errno = ENOMEM;
								err_quit("JOIN_MEETING deal_user_requests : msg->data malloc error\n");
							}
							memset(msg->data, 0, msg->len);
							memcpy(msg->data, &no_exist, msg->len);
							write_to_connfd(msg, connfd);
							printf("write_to_connfd success\n");
						}
					}
					else
					{
						printf("JOIN_MEETING data format error\n");
					}
				}
			}
			else
			{
				printf("type error\n");
			}
		}
	}
}

// 工作线程主函数
void* thread_main(void* arg)
{
	printf("thread_main\n");
	//处理连接后 执行处理用户请求
	int i = *(int*)arg;		// 线程编号
	printf("i : %d\n",i);
	free(arg);
	printf("thread %d start...\n", i);
	Pthread_Detach(pthread_self());		// 分离线程
	int connfd = 0;
	sockaddr* client_addr = (sockaddr*)Calloc(1, addrlen);
	memset(client_addr, 0, sizeof(client_addr));
	socklen_t client_len = addrlen;
	char buf[MAXSOCKADDR] = { 0 };
	while (true)
	{
		//处理客户端连接  
		//accept如果不加锁，会导致所有线程阻塞等待，当一个新连接，系统会唤醒全部线程，但只有一个线程得到成功获取连接，其他线程会继续阻塞，
		//这是惊群效应，浪费cpu资源，而且会导致资源分配不平均，可能一些线程会饿死

		Pthread_mutex_lock(&accept_mutex);
		connfd = Accept(listen_fd, client_addr, &client_len);
		Pthread_mutex_unlock(&accept_mutex);
		printf("connection from %s connfd : %d\n", Sock_ntop(buf, MAXSOCKADDR, client_addr, client_len),connfd);
		deal_user_requests(connfd);		// 处理用户请求
	}
	return NULL;
}