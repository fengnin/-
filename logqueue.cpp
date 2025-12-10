#include "logqueue.h"

// 构造函数，初始化日志队列并打开日志文件
LogQueue::LogQueue()
{
    // 使用安全的文件打开方式
    errno_t r = fopen_s(&_logfile,"./log.txt","a"); // 以追加模式打开日志文件
    if(r != 0)  // 如果打开失败
        qDebug()<<"打开文件失败";

}
// 析构函数，关闭日志文件
LogQueue::~LogQueue()
{
    fclose(_logfile);   // 关闭文件句柄
}
// 将日志指针推入队列
void LogQueue::push_log(Log* log)
{
    _log_queue.push_msg(log);   // 调用底层队列的push方法
}
//is_canRun可能被多线程修改 主线程调用stopImmediately，或者run时都可能对其进行修改，但两个是不同的线程
// 日志线程的主循环（可能被多线程调用）
void LogQueue::run()
{
    _is_canRun = true;// 设置运行标志为true
    while(true) // 无限循环
    {
        {
            // 检查是否应该继续运行（线程安全）
            QMutexLocker locker(&_iscanRun_mutex);  // 加锁保护共享变量
            if(_is_canRun == false) // 如果收到停止信号
            {
                qDebug()<<"log线程退出";    // 输出调试信息
                fclose(_logfile);   // 关闭日志文件
                break;  // 退出循环
            }
        }
        // 从队列中弹出日志
        Log* log = _log_queue.pop_msg();
        if(log == nullptr || log->buf == nullptr)   // 无效日志检查
        {
            continue;   // 跳过本次循环
        }
        //写入文件
        qint64 hastowrite = log->len;   // 需要写入的总字节数
        qint64 ret = 0,haswrite = 0;    // 初始化返回值和已写入字节数
        while(haswrite < hastowrite)    // 循环直到全部写入
        {
            // 写入文件（每次写入剩余部分）
            ret = fwrite((char*)log->buf,1,hastowrite - haswrite,_logfile);
            if(ret == 0)     // 写入失败处理
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK) // 非阻塞错误（可重试）
                {
                    ret = 0;    // 重置返回值
                    continue;   // 继续尝试
                }
                else
                {
                    qDebug()<<"写入日志文件出错";   // 输出错误信息
                    break;  // 退出写入循环
                }
            }
            haswrite += ret;    // 累加已写入字节数
        }
        // 释放内存（避免内存泄漏）
        if(log->buf)
            free(log->buf); // 释放日志缓冲区
        if(log)
            free(log);  // 释放日志结构体
    }
}
// 立即停止日志线程（线程安全）
void LogQueue::stopImmediately()
{
    QMutexLocker locker(&_iscanRun_mutex);  // 加锁保护共享变量
    _is_canRun = false;     // 设置运行标志为false
}
