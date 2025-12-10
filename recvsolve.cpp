#include "recvsolve.h"

extern MSG_Queue<MESG> queue_recv;

RecvSolve::RecvSolve(QObject *parent)
    : QThread{parent}
{}


void RecvSolve::stopImmediately()
{
    QMutexLocker locker(&_iscanRun_mutex);
    _is_canRun = false;
}

void RecvSolve::run()
{
    _is_canRun = true;
    while(true)
    {
        {
            QMutexLocker locker(&_iscanRun_mutex);
            if(_is_canRun == false)
            {
                qDebug()<<"recvsolve线程退出";
                return;
            }
        }
        MESG* data = queue_recv.pop_msg();
        if(data == nullptr)
            continue;
        //qDebug()<<"data : "<<data;
        emit dataDispatch(data);
    }
}

