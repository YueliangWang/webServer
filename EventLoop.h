// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "base/Thread.h"
#include "Epoll.h"
#include "base/Logging.h"
#include "Channel.h"
#include "base/CurrentThread.h"
#include "Util.h"
#include <vector>
#include <memory>
#include <functional>

#include <iostream>
using namespace std;

//事件分发器。每个线程只能有一个EventLoop实体，他负责IO和定时器事件的分派，他用
//eventfd(2)来异步唤醒，这有别于传统的用一对pipe(2)的办法
//创建了EventLoop对象的线程是IO线程，其主要功能是运行时间循环EventLoop::loop()。
//EventLoop对象的生命周期通常和其所属线程一样长，它不必是heap对象。
class EventLoop
{
public:
    typedef std::function<void()> Functor;
    EventLoop();
    ~EventLoop();
    void loop();                 //开始循环
    void quit();                 //退出循环

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
    void runInLoop(Functor&& cb);
  
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
    void queueInLoop(Functor&& cb);
  
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    void assertInLoopThread()
    {
        assert(isInLoopThread());
    }
    void shutdown(shared_ptr<Channel> channel)
    {
        shutDownWR(channel->getFd());
    }
    void removeFromPoller(shared_ptr<Channel> channel)
    {
        //shutDownWR(channel->getFd());
        poller_->epoll_del(channel);
    }

	//用传入的channel更新红黑树上的channel
    void updatePoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller_->epoll_mod(channel, timeout);
    }
    void addToPoller(shared_ptr<Channel> channel, int timeout = 0)
    {
        poller_->epoll_add(channel, timeout);
    }
    
private:
    // 声明顺序 wakeupFd_ > pwakeupChannel_ 
    bool looping_;                 //当前是否在loop
    shared_ptr<Epoll> poller_;     //指向Epoll的指针
    int wakeupFd_;                    //唤醒fd文件描述符
    bool quit_;                   //是否退出
    bool eventHandling_;          //当前是否在处理事件
    mutable MutexLock mutex_;      //互斥量         
    std::vector<Functor> pendingFunctors_;      //队列中待处理的方法
    bool callingPendingFunctors_;      //队列中是否有待处理函数
    const pid_t threadId_;                 //当前eventloop对象所属线程ID,常量
    shared_ptr<Channel> pwakeupChannel_;   //唤醒通道，shared_ptr是智能指针
    //以下都是内部使用函数
    void wakeup();
    void handleRead();
    void doPendingFunctors();
    void handleConn();
};
