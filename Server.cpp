// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Server.h"
#include "base/Logging.h"
#include "Util.h"
#include <functional>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


Server::Server(EventLoop *loop, int threadNum, int port)  //执行构造函数   
:   loop_(loop),
    threadNum_(threadNum),
    eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),//loop_表示EventLoopThreadPool的baseloop
    started_(false),
    acceptChannel_(new Channel(loop_)),   //创建只属于此Evenloop的Channel
    port_(port),
    listenFd_(socket_bind_listen(port_))  //此时已经开始listen
{
	//如果向已经关闭的对端调用write，系统会向程序发送SIGPIPE信号，
	//	该信号默认会退出程序，应该捕获该信号。
    handle_for_sigpipe();
	//设置文件描述符非阻塞
    if (setSocketNonBlocking(listenFd_) < 0)
    {
        perror("set socket non block failed");
        abort();
    }
}

void Server::start()
{
    eventLoopThreadPool_->start();    //启动线程池管理器
    //acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
    acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));//处理读(对于server就是创建新连接)
    acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));
    loop_->addToPoller(acceptChannel_, 0);   //将监听的Channel放到红黑树
    started_ = true;
}

void Server::handNewConn()
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    socklen_t client_addr_len = sizeof(client_addr);
    int accept_fd = 0;
    while((accept_fd = accept(listenFd_, (struct sockaddr*)&client_addr, &client_addr_len)) > 0)
    {
        EventLoop *loop = eventLoopThreadPool_->getNextLoop();//分配新的loop
        LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port);
        // cout << "new connection" << endl;
        // cout << inet_ntoa(client_addr.sin_addr) << endl;
        // cout << ntohs(client_addr.sin_port) << endl;
        /*
        // TCP的保活机制默认是关闭的
        int optval = 0;
        socklen_t len_optval = 4;
        getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
        cout << "optval ==" << optval << endl;
        */
        // 限制服务器的最大并发连接数
        if (accept_fd >= MAXFDS)
        {
            close(accept_fd);
            continue;
        }
        // 设为非阻塞模式
        if (setSocketNonBlocking(listenFd_) < 0)
        {
            LOG << "Set non block failed!";
            //perror("Set non block failed!");
            return;
        }

        setSocketNodelay(accept_fd);
        //setSocketNoLinger(accept_fd);

        shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));//构造函数会newChannel
        req_info->getChannel()->setHolder(req_info);//表明此HttpData持有此刚new出来Channel
        loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));//  //将新的Channel放进红黑树的任务队列
    }
    acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}