#include "unp.h"

#include <cstdarg>

// 内部函数：实际处理错误消息的格式化和输出
static void err_doit(int errnoflag, int error, const char* fmt, va_list ap)	
{
	char buf[MAXLINE];		// 缓冲区，用于存储格式化后的错误消息
	vsnprintf(buf, MAXLINE - 1, fmt, ap);	// 安全地格式化可变参数到 buf

	// 如果需要追加系统错误描述（如 errno 对应的字符串）
	if (errnoflag)
		snprintf(buf + strlen(buf), MAXLINE - 1 - strlen(buf), ": %s", strerror(error));
	strcat(buf, "\n");	// 添加换行符
	fflush(stdout);		// 刷新标准输出（确保之前的输出不被缓冲）
	fputs(buf, stderr);	// 将错误消息输出到标准错误流
	fflush(NULL);		// 刷新所有打开的流（确保错误消息立即显示）
}

// 致命错误：打印消息并退出程序
void err_quit(const char* fmt, ...)
{
	va_list ap;		// 声明可变参数列表
	va_start(ap, fmt);	// 初始化可变参数（从 fmt 开始）
	err_doit(1, errno, fmt, ap);	// 调用 err_doit，追加 errno 描述
	va_end(ap);		// 清理可变参数列表
	exit(-1);
}

// 非致命错误：仅打印消息，不退出
void err_msg(const char* fmt, ...)
{
	va_list ap;		// 声明可变参数列表
	va_start(ap, fmt);	// 初始化可变参数
	err_doit(1, errno, fmt, ap);	// 调用 err_doit，追加 errno 描述
	va_end(ap);	// 清理可变参数列表
}	