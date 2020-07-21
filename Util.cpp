// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Util.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

const int MAX_BUFF = 4096;

/*
readn函数功能：在网络编程的读取数据中，通常会需要用到一个读指定字节才返回的函数，
linux系统调用中没有给出，需要自己封装。

前提：文件描述符为非阻塞模式，epoll为ET触发方式

*/
ssize_t readn(int fd, void *buff, size_t n)   //ssize_t有符数，size_t无符数
{
    size_t nleft = n;          //剩余需要读取的字节数
    ssize_t nread = 0;
    ssize_t readSum = 0;       //已读取的总字节数
    char *ptr = (char*)buff;   //被一次循环读取的首地址
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)          //阻塞的系统调用，被中断
                nread = 0;
            else if (errno == EAGAIN)    //非阻塞的系统调用，当前不可读写
            {
                return readSum;
            }
            else
            {
                return -1;
            }  
        }
        else if (nread == 0)    // 读完了，写端关闭
            break;
        readSum += nread;
        nleft -= nread;
        ptr += nread;         //更新首地址
    }
    return readSum;
}

ssize_t readn(int fd, std::string &inBuffer, bool &zero)   //读数据到字符串里
{
    ssize_t nread = 0;
    ssize_t readSum = 0;
    while (true)
    {
        char buff[MAX_BUFF];
        if ((nread = read(fd, buff, MAX_BUFF)) < 0)   //非阻塞模式，errno
        {
            if (errno == EINTR)      //慢调用被系统中断时返回的错误，需要冲洗read，所以用continue
                continue;
            else if (errno == EAGAIN)//表示没有数据可读，在ET边缘触发方式下，要求我们做到读无可读
            {
                return readSum;     //直接返回，调用的readn的时候用while(readSum<n)       ?
            }  
            else
            {
                perror("read error");
                return -1;
            }
        }
        else if (nread == 0)   //读完，写端(客户端)关闭
        {
            //printf("redsum = %d\n", readSum);
            zero = true;             //将zero置为true
            break;
        }
        //printf("before inBuffer.size() = %d\n", inBuffer.size());
        //printf("nread = %d\n", nread);
        readSum += nread;
        //buff += nread;
        inBuffer += std::string(buff, buff + nread);   //字符数组转为字符串
        //printf("after inBuffer.size() = %d\n", inBuffer.size());
    }
    return readSum;
}


ssize_t readn(int fd, std::string &inBuffer)   //没有zero
{
    ssize_t nread = 0;
    ssize_t readSum = 0;
    while (true)
    {
        char buff[MAX_BUFF];    //中间变量
        if ((nread = read(fd, buff, MAX_BUFF)) < 0)
        {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN)
            {
                return readSum;
            }  
            else
            {
                perror("read error");
                return -1;
            }
        }
        else if (nread == 0)
        {
            //printf("redsum = %d\n", readSum);
            break;
        }
        //printf("before inBuffer.size() = %d\n", inBuffer.size());
        //printf("nread = %d\n", nread);
        readSum += nread;
        //buff += nread;
        inBuffer += std::string(buff, buff + nread);
        //printf("after inBuffer.size() = %d\n", inBuffer.size());
    }
    return readSum;
}


ssize_t writen(int fd, void *buff, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    char *ptr = (char*)buff;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR)
                {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                {
                    return writeSum;   //调用的writen的时候用while(writeSum<n)       ?
                }
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

ssize_t writen(int fd, std::string &sbuff)
{
    size_t nleft = sbuff.size();
    ssize_t nwritten = 0;
    ssize_t writeSum = 0;
    const char *ptr = sbuff.c_str();    //获取字符串首地址
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0)
            {
                if (errno == EINTR)
                {
                    nwritten = 0;
                    continue;
                }
                else if (errno == EAGAIN)
                    break;
                else
                    return -1;
            }
        }
        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    if (writeSum == static_cast<int>(sbuff.size()))
        sbuff.clear();
    else
        sbuff = sbuff.substr(writeSum);
    return writeSum;
}

/*
如果向已经关闭的对端调用write，系统会向程序发送SIGPIPE信号，
该信号默认会退出程序，应该捕获该信号。
*/
void handle_for_sigpipe()   
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;     //新的信号处理函数，SIG_IGN表示信号被忽略，这个地方也可以是函数
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))//SIGPIPE为捕捉的信号类型，sa为新的信号处理方式，一个结构体
        return;
}

int setSocketNonBlocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);   //取得文件描述符状态标志
    if(flag == -1)
        return -1;

    flag |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flag) == -1)  //设置文件描述符状态标志为非阻塞模式
        return -1;
    return 0;
}

void setSocketNodelay(int fd) //禁用nagle算法
{
    int enable = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));
}


/*
设置socket描述符linger结构属性设置链接属性、
这种方式下，在调用closesocket的时候不会立刻返回，内核会延迟一段时间，
这个时间就由l_linger的值来决定。如果超时时间到达之前，
发送完未发送的数据(包括FIN包)并得到另一端的确认，closesocket会返回正确，
socket描述符优雅性退出。否则，closesocket会直接返回错误值，未发送数据丢失，
socket描述符被强制性退出。需要注意的时，如果socket描述符被设置为非阻塞型，
则closesocket会直接返回值。
*/
void setSocketNoLinger(int fd) 
{
    struct linger linger_;
    linger_.l_onoff = 1;
    linger_.l_linger = 30;
    setsockopt(fd, SOL_SOCKET, SO_LINGER,(const char *) &linger_, sizeof(linger_));
}

void shutDownWR(int fd)
{
    shutdown(fd, SHUT_WR);  //关闭对文件描述的写
    //printf("shutdown\n");
}

//外部函数
int socket_bind_listen(int port)
{
    // 检查port值，取正确区间范围 
    if (port < 0 || port > 65535)          //无符号整型的范围是 0-65535
        return -1;

    // 创建socket(IPv4 + TCP)，返回监听描述符
    int listen_fd = 0;
    if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    // 消除bind时"Address already in use"错误
    int optval = 1;
    if(setsockopt(listen_fd, SOL_SOCKET,  SO_REUSEADDR, &optval, sizeof(optval)) == -1)
        return -1;

    // 设置服务器IP和Port，和监听描述符绑定
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);
       printf("%d\n", listen_fd);
    if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
        printf("%d\n", listen_fd);
        return -1;
    }

    // 开始监听，最大等待队列长为LISTENQ
    if(listen(listen_fd, 2048) == -1)
        return -1;

    // 无效监听描述符
    if(listen_fd == -1)
    {
        close(listen_fd);
        return -1;
    }
    return listen_fd;
}