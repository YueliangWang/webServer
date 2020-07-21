// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
/*
#pragma once是一个比较常用的C/C++预处理指令，只要在头文件的最开始加入这条预处理指令，
就能够保证头文件只被编译一次。
*/
#include "EventLoop.h"
#include "Channel.h"
#include "EventLoopThreadPool.h"
#include <memory>


class Server
{
public:
    Server(EventLoop *loop, int threadNum, int port);
    ~Server() { }
    EventLoop* getLoop() const { return loop_; }
    void start();       
    void handNewConn();
    void handThisConn() { loop_->updatePoller(acceptChannel_); }

private:
    EventLoop *loop_;   //主线程的Evenloop对象，一个指针，为Server专用，为主线程提供loop循环
    int threadNum_;
	//同一时刻只能有一个unique_ptr指向给定对象
    std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_;//线程池管理器。创建和管理线程池
    bool started_;
    std::shared_ptr<Channel> acceptChannel_;  //负责监听描述符的Channel
    int port_;
    int listenFd_;                      //监听描述符
    static const int MAXFDS = 100000;
};