//提供了一套对 POSIX 线程（pthread）函数的封装，增加了错误处理机制

#include "unpthread.h"
#include "unp.h"

// 封装 pthread_create：创建线程
void Pthread_Create(pthread_t* thread,const pthread_attr_t* attr,THREAD_FUNC* func,void* arg)
{
	int n = pthread_create(thread, attr, func, arg);
	if (n != 0)
	{
		errno = n;
		err_quit("Pthread_Create error");
	}
}

// 封装 pthread_detach：分离线程
void Pthread_Detach(pthread_t tid)
{
	int n = pthread_detach(tid);
	if (n != 0)
	{
		errno = n;
		err_quit("Pthread_Detach error");
	}
}


// 封装 pthread_mutex_lock：加锁
void Pthread_mutex_lock(pthread_mutex_t* mutex)
{
	int n = pthread_mutex_lock(mutex);
	if (n != 0)
	{
		errno = n;
		err_quit("pthread_mutex_lock error");
	}
}

// 封装 pthread_mutex_unlock：解锁
void Pthread_mutex_unlock(pthread_mutex_t* mutex)
{
	int n = pthread_mutex_unlock(mutex);
	if (n != 0)
	{
		errno = n;
		err_quit("pthread_mutex_unlock error");
	}
}

// 封装 pthread_cond_signal：唤醒一个等待条件变量的线程
void Pthread_cond_signal(pthread_cond_t* cond)
{
	int n = pthread_cond_signal(cond);
	if (n != 0)
	{
		errno = n;
		err_quit("pthread_cond_signal error");
	}
}

// 封装 pthread_cond_wait：等待条件变量
void Pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
	int n = pthread_cond_wait(cond, mutex);
	if (n != 0)
	{
		errno = n;
		err_quit("pthread_cond_wait error");
	}
}
