#include <cstdio>
#include "unpthread.h"
#include "unp.h"

socklen_t addrlen;
int listen_fd;
Room_Pool* room;
pthread_t* tid_pool;//存储线程id
int process_num;
static int maxfd;

void thread_make(int i)
{
    printf("thread_make\n");
    void* thread_main(void* arg);
    void* arg = (void*)Calloc(1, sizeof(int));
    *(int*)arg = i;
    Pthread_Create(&tid_pool[i], NULL, thread_main, arg);
}

int process_make(int i,int listenfd)
{
    //开启子进程
    void process_main(int);
    pid_t pid;
    int pipe[2];
    Socketpair(AF_LOCAL, SOCK_STREAM, 0, pipe);
    if ((pid = fork()) > 0)//父进程
    {
        //父进程关闭写
        Close(pipe[1]);
        room->room[i].child_pid = pid;
        room->room[i].child_pipefd = pipe[0];
        room->room[i].total = 0;
        room->room[i].room_state = 0;
        return pid;
    }
    close(listenfd);
    close(pipe[0]);//子进程关闭读
    process_main(pipe[1]);
}

int main(int argc,char* argv[])
{
    //监听套接字
    if (argc == 4)
        listen_fd = Tcp_listen(NULL, argv[2], &addrlen);
    else if (argc == 5)
        listen_fd = Tcp_listen(argv[1], argv[2], &addrlen);
    else
    {
        printf("Usage method : ./app ip port thread_num process_num\n");
        return 0;
    }
    Signal(SIGCHLD, sig_child);
    maxfd = listen_fd;
    printf("listen_fd : %d\n", listen_fd);
    process_num = atoi(argv[argc - 1]);
    int thread_num = atoi(argv[argc - 2]);
    printf("process_num : %d\n", process_num);
    printf("thread_num : %d\n", thread_num);
    tid_pool = (pthread_t*)Calloc(thread_num, sizeof(pthread_t));
    room = new Room_Pool(process_num);
    fd_set rset, masterset;
    FD_ZERO(&rset);
    FD_ZERO(&masterset);
    //启动进程池 一个进程管理一个房间
    for (int i = 0; i < process_num; i++)
    {
        process_make(i, listen_fd);
        FD_SET(room->room[i].child_pipefd, &masterset);
        maxfd = MAX(maxfd, room->room[i].child_pipefd);
    }
    //启动线程池 监听客户端连接和创建房间/加入房间请求
    for (int i = 0; i < thread_num; i++)
    {
        thread_make(i);
    }
    while (true)
    {
        //监听子进程的信息
        rset = masterset;
        int ready = Select(maxfd + 1, &rset, NULL, NULL, NULL);
        for (int i = 0; i < process_num; i++)
        {
            if (FD_ISSET(room->room[i].child_pipefd, &rset))
            {
                char c;
                size_t ret = Readn(room->room[i].child_pipefd, &c, 1);
                if (ret < 0)
                {
                    c = 0;
                    err_msg("Readn error");
                }

                if (c == 'E')//房主退出
                {
                    Pthread_mutex_lock(&room->mutex);
                    printf("room %d free\n", room->room[i].child_pid);
                    room->spare_room++;
                    room->room[i].room_state = 0;
                    room->room[i].total = 0;
                    Pthread_mutex_unlock(&room->mutex);
                }
                else if (c == 'Q')//普通用户退出
                {
                    Pthread_mutex_lock(&room->mutex);
                    room->room[i].total--;
                    Pthread_mutex_unlock(&room->mutex);
                }
                else
                {
                    err_msg("child send char error");
                }
                ready--;
                if (ready <= 0)
                    break;
            }
        }
    }
    return 0;
}