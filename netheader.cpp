#include "netheader.h"
#include "logqueue.h"

// 定义全局消息队列（发送、接收、音频接收）
MSG_Queue<MESG> queue_send;
MSG_Queue<MESG> queue_recv;
MSG_Queue<MESG> audio_recv;
// 全局日志队列指针（需在外部初始化）
LogQueue* logqueue = nullptr;

// 日志写入函数（支持可变参数，类似printf）
void log_write(const char *logtext,const char* filename,const char* funcname,const int line,...)    //logtext日志文本，filename文件名，funcname函数名，line行数
{
    // 1. 分配日志结构体内存
    Log* log = (Log*)malloc(sizeof(Log));
    if(log == nullptr)  // 内存分配失败检查
    {
        qDebug()<<"log malloc failed";
        return;
    }
    memset(log,0,sizeof(Log));  // 初始化日志结构体

    // 2. 分配日志缓冲区（1KB大小）
    log->buf = (char*)malloc(1*KB); // KB应为预定义的常量（如1024）
    if(log->buf == nullptr)     // 缓冲区分配失败检查
    {
        qDebug()<<"log buf malloc failed";
        if(log)     // 释放已分配的日志结构体
            free(log);
        return;
    }
    memset(log->buf,0,1*KB);    // 初始化缓冲区
    //处理时间
    time_t tm = time(NULL);     // 获取当前时间
    int pos = 0;//log->buf的偏移量
    // 格式化时间到缓冲区（格式：YYYY-MM-DD HH:MM:SS）
    int ret = strftime(log->buf + pos,KB - 2 - pos,"%F %T",localtime(&tm));
    pos += ret;     // 更新偏移量

    //处理文件/函数名 和 行数
     // 4. 记录源文件位置（文件名::函数名::行号）
    ret = snprintf(log->buf + pos,KB - 2 - pos,"%s::%s::%d>>>",filename,funcname,line);
    pos += ret;     // 更新偏移量

    //处理可变参数
    va_list arg;
    va_start(arg,line);     // 初始化可变参数捕获（从line之后开始）
    // 格式化可变参数到缓冲区
    ret = vsnprintf(log->buf + pos,KB - 2 - pos,logtext,arg);
    pos += ret;     // 更新偏移量

    va_end(arg);    // 结束可变参数捕获
    // 6. 添加换行符并计算总长度
    strcat_s(log->buf,KB - pos,"\n");       // 安全地追加换行符
    log->len = strlen(log->buf);    // 计算日志总长度
    // 7. 将日志推入日志队列（由LogQueue线程处理写入文件） 
    logqueue->push_log(log);

}
