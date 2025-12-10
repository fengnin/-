#pragma once
#include <queue>		// STL 队列
#include <cstdint>		// 固定宽度整数类型（如 uint32_t）
#include "nethead.h"
#include "unpthread.h"

#define MAXQUEUE 10000	// 队列最大容量

// 消息结构体
struct MESG
{
	int targetfd;	// 目标文件描述符
	uint32_t ip;	// IP 地址（32 位无符号整数）
	MSG_TYPE type;	// 消息类型
	char* data;		// 消息数据指针（需动态分配内存）
	int len;		// 数据长度
	// 默认构造函数
	MESG()
	{

	}
	// 带参数的构造函数
	MESG(int fd, uint32_t _ip, MSG_TYPE msg_type, int _len)
	{
		targetfd = fd;
		ip = _ip;
		type = msg_type;
		len = _len;
	}
};

// 线程安全的消息队列模板类
template <class T>
struct MSG_Queue
{
	pthread_cond_t cond;	// 条件变量，用于线程同步
	pthread_mutex_t mutex;	// 互斥锁，保护队列操作
	std::queue<T*> msg_queue;	// STL 队列，存储消息指针

	// 构造函数：初始化条件变量和互斥锁
	MSG_Queue()
	{
		cond = PTHREAD_COND_INITIALIZER;
		mutex = PTHREAD_MUTEX_INITIALIZER;
	}

	// 向队列尾部添加消息（线程安全）
	void push_msg(T* msg)
	{
		Pthread_mutex_lock(&mutex);		// 加锁
		while (msg_queue.size() >= MAXQUEUE)	// 若队列满则等待
		{
			Pthread_cond_wait(&cond, &mutex);
		}
		msg_queue.push(msg);	// 添加消息
		Pthread_cond_signal(&cond);	// 通知消费者
		Pthread_mutex_unlock(&mutex);	// 解锁
	}
	// 从队列头部取出消息（线程安全）
	T* pop_msg()
	{
		Pthread_mutex_lock(&mutex);	// 加锁
		while (msg_queue.size() <= 0)		// 若队列空则等待
		{
			Pthread_cond_wait(&cond, &mutex);
		}
		T* msg = msg_queue.front();		// 获取消息
		msg_queue.pop();				// 移除消息
		Pthread_cond_signal(&cond);		// 通知生产者
		Pthread_mutex_unlock(&mutex);	// 解锁
		return msg;
	}

	void clear()		// 清空队列并释放内存（线程安全）
	{
		Pthread_mutex_lock(&mutex);	// 加锁
		while (!msg_queue.empty())	// 遍历队列
		{
			T* msg = msg_queue.front();
			msg_queue.pop();
			free(msg);		// 释放消息内存（需确保 T* 是动态分配的）
		}
		Pthread_mutex_unlock(&mutex);	// 解锁
	}
};