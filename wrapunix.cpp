//提供了一系列对 Unix 系统调用的封装函数，主要用于错误处理和信号管理

#include <stdlib.h>
#include "unp.h"
#include <signal.h>

// 封装 calloc：分配内存并检查是否成功
void* Calloc(size_t n, size_t size)
{
	void* ptr = calloc(n, size);		// 调用系统 calloc
	if (ptr == NULL)		// 检查分配失败
	{
		errno = ENOMEM;		// 设置错误码为“内存不足”
		err_quit("calloc error");		// 打印错误并退出程序
	}
	return ptr;
}

// 封装 sigaction：设置信号处理函数                        
Sigfunc* Signal(int signo, Sigfunc* func)
{
	struct sigaction act,oact;
	act.sa_handler = func;		// 设置信号处理函数
	sigemptyset(&act.sa_mask);//清空阻塞信号集
	act.sa_flags = 0;		// 默认标志

	if (signo == SIGALRM)//超时信号 不自动重启系统调用  当某些接口设置了时间，超时后触发这个信号，触发后不重新启动这个接口
	{
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif // SA_INTERRUPT
	}
	else  // 其他信号：自动重启被中断的系统调用
	{
#ifdef SA_RESTART
		act.sa_flags |= SA_RESTART;
#endif // SA_RESTART
	}

	// 调用 sigaction 设置信号处理
	int ret = sigaction(signo, &act, &oact);
	if (ret < 0)
	{
		return SIG_ERR;		// 失败时返回 SIG_ERR
	}
	return oact.sa_handler;	// 返回旧的信号处理函数
}

// SIGCHLD 信号处理函数：回收子进程资源
void sig_child(int signo)
{
	pid_t pid;
	int state;
	//WNOHANG + while 预防所有子进程同时结束，而系统只会发送一次信号	// WNOHANG + while 循环：非阻塞回收所有终止的子进程
	while ((pid = waitpid(-1, &state, WNOHANG)) > 0)
	{
		if (WIFEXITED(state))		// 子进程正常退出
		{
			printf("child %d normal termination, exit status = %d\n", pid, WEXITSTATUS(state));
		}
		else if (WIFSIGNALED(state))	// 子进程被信号终止
		{
			printf("child %d abnormal termination, singal number  = %d%s\n", pid, WTERMSIG(state),
#ifdef WCOREDUMP
				WCOREDUMP(state) ? " (core file generated) " : "");
#else
				"");
#endif // WCOREDUMP
		}
	}
}
