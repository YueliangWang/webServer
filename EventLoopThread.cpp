// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoopThread.h"
#include <functional>


EventLoopThread::EventLoopThread()
:   loop_(NULL),    //初始化时置为空
    exiting_(false),
    thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"), //初始化传递一个函数回调,构造线程，线程还没有start
    mutex_(),                  //初始化互斥量
    cond_(mutex_)             //初始化一个条件变量
{ }

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL)
    {
        loop_->quit();
        thread_.join();
    }
}

/*
这个函数返回新线程中EventLoop对象的地址，因此用条件变量等待创建的线程运行。
*/
EventLoop* EventLoopThread::startLoop()  
{
    assert(!thread_.started());
    thread_.start();   //创建线程，会调用thread_中一系列函数，最终调用threadFunc()，并让其开始工作
    {
        MutexLockGuard lock(mutex_);
        // 一直等到threadFun在Thread里真正跑起来
        while (loop_ == NULL)
            cond_.wait();
    }
    return loop_;  //返回新创建的EventLoop的地址
}


/*
这个函数运行之后，运行loop循环，且唤醒startLoop()，从而返回EventLoop地址
*/
void EventLoopThread::threadFunc()
{
    EventLoop loop;          //初始化一个Evenloop

    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;     ////到这里loop_就不为NULL了，即条件变量等到了
        cond_.notify();   //唤醒startLoop()
    }

    loop.loop();
    //assert(exiting_);
    loop_ = NULL;   //这个函数退出之后loop_指针就失效了。
}