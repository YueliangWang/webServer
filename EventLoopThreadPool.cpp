// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, int numThreads)
:   baseLoop_(baseLoop),
    started_(false),
    numThreads_(numThreads),
    next_(0)
{
    if (numThreads_ <= 0)
    {
        LOG << "numThreads_ <= 0";
        abort();
    }
}

void EventLoopThreadPool::start()
{
    baseLoop_->assertInLoopThread();  //判断当前线程是否是在创建mainLoop的线程中(主IO线程)
    started_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        std::shared_ptr<EventLoopThread> t(new EventLoopThread()); //创建线程
        threads_.push_back(t);
        loops_.push_back(t->startLoop());  //这里start线程，开启subloop循环
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    baseLoop_->assertInLoopThread();//判断当前线程是否是主IO线程
    assert(started_);
    EventLoop *loop = baseLoop_;
    if (!loops_.empty())
    {
        loop = loops_[next_];    //分配一个EvenLoop并返回给调用者
        next_ = (next_ + 1) % numThreads_;
    }
    return loop;   //当if不执行时，返回baseLoop_地址，即mainloop
}