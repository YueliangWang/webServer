// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#pragma once
#include "base/Condition.h"
#include "base/MutexLock.h"
#include "base/Thread.h"
#include "base/noncopyable.h"
#include "EventLoop.h"


/*
EventLoopThread这个类的作用就是开启一个线程，但是这个线程中有一个EventLoop，
并且让这个EventLoop处于loop（）状态，在上篇文章中折腾了很久分析EventLoop，
竟然在一个线程中开启了一个这么庞大的东西！！！！！！
（假如在主程序中实例化一个EventLoopThread，
那么主函数的threadid和EventLoopThread内部的EventLoop所处线程的threadid就不一样,
这也就是为什么有queueInLoop()这个函数了，为什么有wakeupFd()这个函数了，
在别的线程中想传递一个任务给另一个线程的EventLoop，那么就需要queueInLoop,
然后再进行wakeup自己）

EventLoopThread类专门创建一个线程用于执行Reactor的事件循环，
当然这只是一个辅助类，没有说一定要使用它，可以根据自己的情况进行选择，
你也可以不创建线程去执行事件循环，而在主线程中执行事件循环，一切根据自己的需要。
*/
class EventLoopThread :noncopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();
    EventLoop *loop_;           //一个Loop指针loop_（说明内部并没有实例化EventLoop）
    bool exiting_;
    Thread thread_;        //线程    ，这只是一个类，初始化Thread并不会调用pthread_create()
    MutexLock mutex_;       //互斥锁
    Condition cond_;        //条件变量
};