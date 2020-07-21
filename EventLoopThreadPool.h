// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "base/noncopyable.h"
#include "EventLoopThread.h"
#include "base/Logging.h"
#include <memory>
#include <vector>


/*
EventLoopThreadPool是一个线程池，线程池中每一个线程都有一个自己的EventLoop，
而每一个EventLoop底层都是一个poll或者epoll，
它利用了自身的poll或者epoll在没有事件的时候阻塞住，
在有事件发生的时候，epoll监听到了事件就会去处理事件。
*/
class EventLoopThreadPool : noncopyable
{
public:
    EventLoopThreadPool(EventLoop* baseLoop, int numThreads);

    ~EventLoopThreadPool()
    {
        LOG << "~EventLoopThreadPool()";
    }
    void start();

    EventLoop *getNextLoop();

private:
    EventLoop* baseLoop_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::shared_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};