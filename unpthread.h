#pragma once
#include <pthread.h>
#include "unp.h"

typedef void* (THREAD_FUNC)(void*);

//房间属性
struct Room
{
	int child_pid;//进程id
	int child_pipefd;//子进程对父进程发送信息的管道fd
	int total;//房间人数
	int room_state;//0为空闲 1为占据
};

//管理房间
typedef struct room_pool
{
	Room* room;//存储所有房间信息
	pthread_mutex_t mutex;
	int spare_room;//空闲房间个数

	room_pool(int n)
	{
		mutex = PTHREAD_MUTEX_INITIALIZER;
		room = (Room*)Calloc(n, sizeof(Room));
		spare_room = n;
	}
}Room_Pool;

void Pthread_Create(pthread_t* thread, const pthread_attr_t* attr, THREAD_FUNC* func, void* arg);
void Pthread_Detach(pthread_t tid);
void Pthread_mutex_lock(pthread_mutex_t* mutex);
void Pthread_mutex_unlock(pthread_mutex_t* mutex);
void Pthread_cond_signal(pthread_cond_t* cond);
void Pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);