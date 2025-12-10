//实现了一个多线程的房间（会议）管理服务，支持用户创建 加入房间、消息广播和资源清理

#include "unp.h"
#include "unpthread.h"
#include "msg.h"
#include <map>

#define SENDTHREAD_NUM 5		// 发送线程池大小
//用户创建/加入成功后的需要的属性 管理房间内的所有用户

// 房间状态枚举
enum STATE
{
	CLOSE = 0,
	ON
};
static volatile int maxfd;//设置为static可以默认初始化 和 限制了作用域只在本文件使用	// 当前最大文件描述符（用于 select）
STATE volatile roomstate = STATE::CLOSE;		// 房间状态
MSG_Queue<MESG> send_queue;		// 全局消息队列

// 房间用户池
typedef struct pool
{
	int owner_fd;//房主fd
	int state[1024 + 10];//房间每个位置的状态 CLOSE ON
	fd_set fdset;//所有客户的fd 用于select监听
	std::map<int, uint32_t> fd_To_ip;//key 为fd value为对应的ip
	int num;//房间人数
	pthread_mutex_t lock;	// 互斥锁保护共享数据

	pool()
	{
		owner_fd = 0;
		memset(state, 0, sizeof(state));
		FD_ZERO(&fdset);
		num = 0;
		lock = PTHREAD_MUTEX_INITIALIZER;
	}
	// 清理房间资源
	void clearPool()
	{
		Pthread_mutex_lock(&lock);
		roomstate = CLOSE;
		owner_fd = 0;
		for (int i = 0; i <= maxfd; i++)
			if (state[i] == ON)
				Close(i);	// 关闭所有用户连接
		memset(state, 0, sizeof(state));
		num = 0;
		FD_ZERO(&fdset);
		fd_To_ip.clear();
		Pthread_mutex_unlock(&lock);
	}
}Pool;

Pool* user_pool = new Pool();	// 全局用户池

// 线程：接收父进程传递的文件描述符
void* accept_fd(void* arg)
{
	int fd = *(int*)arg;//与父进程通信的fd	// 与父进程通信的管道文件描述符
	free(arg);
	Pthread_Detach(pthread_self());
	while (true)
	{
		char c;//C/J	// 命令类型（'C' 创建房间，'J' 加入房间）
		int tfd = 0;//目标客户端fd
		//printf("ready read_fd\n");
		size_t ret = Read_fd(fd, &c, 1, &tfd);		// 接收文件描述符
		//printf("Read_fd ret : %d\n", ret);
		//printf("c : %c tfd : %d\n", c, tfd);
		if (ret <= 0)
			err_quit("Read_fd error");
		if (tfd < 0)
			err_quit("target fd error");
		if (c == 'C')		// 创建房间
		{
			//printf("start send\n");
			Pthread_mutex_lock(&user_pool->lock);
			roomstate = ON;
			FD_SET(tfd, &user_pool->fdset);
			user_pool->state[tfd] = ON;
			user_pool->fd_To_ip[tfd] = getpeerip(tfd);
			user_pool->num++;
			user_pool->owner_fd = tfd;		// 设置房主
			maxfd = MAX(tfd, maxfd);
			printf("%d maxfd : %d\n", __LINE__,maxfd);
			Pthread_mutex_unlock(&user_pool->lock);

			// 发送房间创建响应（包含房间号）
			MESG* msg = (MESG*)Calloc(1,sizeof(MESG));
			memset(msg, 0, sizeof(MESG));
			msg->type = CREATE_MEETING_RESPONSE;
			msg->targetfd = tfd;
			msg->ip = user_pool->fd_To_ip[tfd];
			int roomno = getpid();		// 房间号设为进程 ID
			roomno = htonl(roomno);
			msg->len = sizeof(roomno);
			msg->data = (char*)malloc(msg->len);
			memset(msg->data, 0, msg->len);
			memcpy(msg->data, &roomno, msg->len);
			send_queue.push_msg(msg);
			//printf("stop send\n");
		}
		else if (c == 'J')		// 加入房间
		{
			Pthread_mutex_lock(&user_pool->lock);
			if (roomstate == CLOSE)			// 房间已关闭
			{
				Close(tfd);
				Pthread_mutex_unlock(&user_pool->lock);
				printf("room is CLOSE\n");
				continue;
			}
			else
			{
				FD_SET(tfd, &user_pool->fdset);
				user_pool->fd_To_ip[tfd] = getpeerip(tfd);
				user_pool->num++;
				user_pool->state[tfd] = ON;
				maxfd = MAX(tfd, maxfd);
				printf("%d maxfd : %d\n", __LINE__, maxfd);


				Pthread_mutex_unlock(&user_pool->lock);

				// 通知其他用户有新成员加入
				MESG* msg = (MESG*)Calloc(1, sizeof(MESG));
				memset(msg, 0, sizeof(MESG));
				msg->type = PARTNER_JOIN_OTHER;//告诉当前会议里面的所有人新伙伴加入
				msg->ip = user_pool->fd_To_ip[tfd];
				msg->len = 0;
				msg->data = NULL;
				msg->targetfd = tfd;
				send_queue.push_msg(msg);
				//printf("msg packaging success\n");

				// 发送当前房间成员列表给新用户
				MESG* msg1 = (MESG*)Calloc(1, sizeof(MESG));
				memset(msg1, 0, sizeof(MESG));
				msg1->type = PARTNER_JOIN_SELF;//将会议内其他人的信息告诉自己，便于添加partner
				msg1->ip = user_pool->fd_To_ip[tfd];
				msg1->targetfd = tfd;
				msg1->data = (char*)Calloc(1, user_pool->num * sizeof(uint32_t));
				memset(msg1->data, 0, msg->len);
				int pos = 0;
				uint32_t ip = 0;
				for (int i = 0; i <= maxfd; i++)
				{
					if (user_pool->state[i] == ON && i != tfd)
					{
						ip = htonl(user_pool->fd_To_ip[i]);
						memcpy(msg1->data + pos, &ip, sizeof(uint32_t));
						pos += sizeof(uint32_t);
						msg1->len += sizeof(uint32_t);
					}
				}
				//printf("msg1 packaging success\n");
				send_queue.push_msg(msg1);
			}
		}
		else
		{
			err_msg("c read error");
		}
	}
	return NULL;
}

// 线程：发送消息（广播或单播）
void* send_msg(void* arg)
{
	//printf("send_msg start\n");

	(void)arg;
	Pthread_Detach(pthread_self());
	char* sendbuf = (char*)Calloc(1, 4 * MB);	// 发送缓冲区
	memset(sendbuf, 0, 4 * MB);

	while (true)
	{
		MESG* msg = send_queue.pop_msg();		// 从队列获取消息
		//printf("msg : %x\n", msg);
		//转大端字节序发出
		int pos = 0;
		sendbuf[pos++] = '$';		// 消息头
		//printf("pos : %d\n", pos);
		//type

		// 类型（网络字节序）
		MSG_TYPE type = (MSG_TYPE)htons(msg->type);
		memcpy(sendbuf + pos, &type, sizeof(uint16_t));
		pos += sizeof(uint16_t);
		//printf("pos : %d\n", pos);

		//ip
		// IP（网络字节序）
		uint32_t ip = htonl(msg->ip);
		memcpy(sendbuf + pos, &ip, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		//printf("pos : %d\n", pos);

		//len
		// 数据长度（网络字节序）
		uint32_t len = htonl(msg->len);
		memcpy(sendbuf + pos, &len, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		//printf("pos : %d\n", pos);
		//printf("msg->len : %d\n", msg->len);

		//data
		// 数据
		if (msg->len > 0)
		{
			memcpy(sendbuf + pos, msg->data, msg->len);
			pos += msg->len;
			//printf("pos : %d\n", pos);
		}

		sendbuf[pos++] = '#';	// 消息尾
		//printf("pos : %d\n", pos);

		//判断类型 广播/单发数据
		// 根据消息类型处理
		if (msg->type == CREATE_MEETING_RESPONSE)
		{
			//printf("type is CREATE_MEETING_RESPONSE\n");
			size_t ret = writen(msg->targetfd, sendbuf, pos);	// 单发给房主
			if (ret < 0)
			{
				err_msg("writen error");
			}
		}
		else if (msg->type == PARTNER_EXIT || msg->type == IMG_RECV || msg->type == AUDIO_RECV || msg->type == TEXT_RECV || msg->type == CLOSE_CAMERA || msg->type == PARTNER_JOIN_OTHER)
		{
			printf("type : %d\n",msg->type);
			Pthread_mutex_lock(&user_pool->lock);
			printf("%d maxfd : %d\n", __LINE__, maxfd);

			for (int i = 0; i <= maxfd; i++)
			{
				if (user_pool->state[i] == ON && i != msg->targetfd)//除了数据发送者 其他广播数据
				{
					size_t ret = writen(i, sendbuf, pos);
					printf("writen ret : %d\n", ret);
					if (ret < 0)
					{
						err_msg("writen error");
					}
				}
			}
			Pthread_mutex_unlock(&user_pool->lock);
			if (msg->type == PARTNER_EXIT)//用户退出关闭套接字
				Close(msg->targetfd);		// 退出用户关闭连接
		}
		else if (msg->type == PARTNER_JOIN_SELF)
		{
			Pthread_mutex_lock(&user_pool->lock);
			printf("%d maxfd : %d\n", __LINE__, maxfd);

			for (int i = 0; i <= maxfd; i++)
			{
				if (user_pool->state[i] == ON && i == msg->targetfd)//将会议里的所有人ip通知给自己
				{
					size_t ret = writen(i, sendbuf, pos);
					if (ret < 0)
					{
						err_msg("writen error");
					}
				}
			}
			Pthread_mutex_unlock(&user_pool->lock);
		}

		// 释放资源
		if (msg->data)
			free(msg->data);
		if (msg)
			free(msg);
	}
	if (sendbuf)
		free(sendbuf);
}

// 处理用户退出
void fd_close(int i, int pipefd)
{
	if (user_pool->owner_fd == i)
	{
		//房主退出 整个房间关闭
		user_pool->clearPool();
		printf("clear room\n");
		char c = 'E';				// 通知父进程房间关闭
		size_t ret = writen(pipefd, &c, 1);
		if (ret < 1)
		{
			err_msg("fd_close writen error");
		}
	}
	else  // 普通用户退出
	{
		//普通客户退出
		printf("partner exit\n");
		char c = 'Q';	// 通知父进程用户退出
		size_t ret = writen(pipefd, &c, 1);
		if (ret < 1)
		{
			err_msg("fd_close writen error");
		}
		MESG* msg = (MESG*)Calloc(1, sizeof(MESG));
		memset(msg, 0, sizeof(MESG));
		msg->targetfd = i;
		msg->type = PARTNER_EXIT;
		msg->ip = user_pool->fd_To_ip[i];
		msg->data = NULL;
		msg->len = 0;
		send_queue.push_msg(msg);		// 广播退出消息
	}
}

// 主处理函数
void process_main(int fd)//发送数据到父进程的pipefd
{
	signal(SIGPIPE, SIG_IGN);//防止写入已经关闭的套接字导致崩溃	// 忽略 SIGPIPE 信号
	pthread_t tid;
	int* ptr = (int*)malloc(sizeof(int));
	*ptr = fd;
	printf("room %d start\n", getpid());
	
	//开启一个线程执行读取父进程传来的C/J信息
	Pthread_Create(&tid, NULL, accept_fd, (void*)ptr);
	//开启多个线程执行回响客户端	// 启动发送线程池
	pthread_t* tids = (pthread_t*)malloc(SENDTHREAD_NUM * sizeof(pthread_t));
	for (int i = 0; i < SENDTHREAD_NUM; i++)
	{
		Pthread_Create(&tids[i], NULL, send_msg, NULL);
	}

	//主线程执行已连接后的发送数据类型	// 主循环：监听用户消息
	while (true)
	{
		fd_set rset = user_pool->fdset;
		int ready = 0;
		struct timeval time;		// 立即返回
		memset(&time, 0, sizeof(timeval));
		//select当套接字关闭时，会触发可读信号，但实际数据是EOF 后续Readn读取到EOF会返回0(对端关闭) / -1错误
		//printf("ready select\n");
		printf("%d maxfd : %d\n", __LINE__, maxfd);

		// 等待可读事件
		while ((ready = Select(maxfd + 1, &rset, NULL, NULL, &time)) == 0)
		{
			rset = user_pool->fdset;//更新新加进来的fd
		}

		//监听到数据
		printf("%d maxfd : %d\n", __LINE__, maxfd);
		// 处理可读事件
		for (int i = 0; i <= maxfd; i++)
		{
			printf("ready FD_ISSET\n");
			if (FD_ISSET(i, &rset))
			{
				printf("FD_ISSET i : %d\n", i);
				char head[15] = { 0 };
				size_t ret = Readn(i, &head, 11);//读取报头
				printf("ret : %d\n", ret);
				if (ret <= 0)
				{
					printf("peer close\n");
					fd_close(i, fd);
				}
				else
				{
					if (head[0] == '$')
					{
						printf("start packing data\n");
						MESG* msg = (MESG*)Calloc(1, sizeof(MESG));
						memset(msg, 0, sizeof(MESG));
						uint16_t type = 0;
						memcpy(&type, head + 1, sizeof(uint16_t));
						MSG_TYPE msg_type = (MSG_TYPE)ntohs(type);
						printf("msg_type : %d\n", msg_type);

						uint32_t ip = 0;
						memcpy(&ip, head + 3, sizeof(uint32_t));
						msg->ip = ntohl(ip);
						printf("msg->ip : %d\n", msg->ip);

						uint32_t len = 0;
						memcpy(&len, head + 7, sizeof(uint32_t));
						msg->len = ntohl(len);
						msg->targetfd = i;
						printf("msg->len : %d\n", msg->len);

						char* buf = (char*)Calloc(1, msg->len + 1);
						char tail;
						ret = Readn(i, buf, msg->len + 1);
						printf("ret : %d\n", ret);

						if (ret <= 0)
						{
							printf("peer close\n");
							fd_close(i, fd);
							break;
						}
						if (ret < msg->len + 1)
						{
							err_msg("Readn too short\n");
						}
						else
						{
							printf("buf[msg->len] : %c\n", buf[msg->len]);
							tail = buf[msg->len];
							printf("tail : %c\n", tail);

							if (tail != '#')
							{
								err_msg("data format error #");
							}
							else
							{
								//分析类型
								if (msg_type == TEXT_SEND || msg_type == AUDIO_SEND || msg_type == IMG_SEND)
								{
									switch (msg_type)
									{
									case TEXT_SEND:
									{
										msg->type = TEXT_RECV;
										//printf("TEXT_RECV\n");
										break;
									}
									case AUDIO_SEND:
									{
										msg->type = AUDIO_RECV;
										//printf("AUDIO_RECV\n");
										break;
									}
									case IMG_SEND:
									{
										msg->type = IMG_RECV;
										//printf("IMG_RECV\n");
										break;
									}
									default:
										break;
									}
									msg->data = (char*)Calloc(1, msg->len);
									memset(msg->data, 0, msg->len);
									memcpy(msg->data, buf, msg->len);
									free(buf);
									buf = NULL;
									send_queue.push_msg(msg);
								}
								else if (msg_type == CLOSE_CAMERA)
								{
									msg->data = NULL;
									msg->type = CLOSE_CAMERA;
									send_queue.push_msg(msg);
								}
							}
						}
						
					}
					else
					{
						err_msg("data format error $");
					}
				}
				--ready;
				if (ready <= 0)
					break;
			}
		}
	}
}