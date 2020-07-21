// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoop.h"
#include "base/Logging.h"
#include "Util.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <iostream>
using namespace std;

// 当前线程EventLoop对象指针
// 线程局部存储，属于每一个线程的变量
__thread EventLoop* t_loopInThisThread = 0;    


//创建事件fd
int createEventfd()
{

//文件被设置成 EFO_CLOEXEC，创建子进程 (fork) 时不继承父进程的文件描述符。
//文件被设置成 EFO_NONBLOCK，执行 read / write 操作时，不会阻塞。
//文件被设置成 EFD_SEMAPHORE,提供类似信号量语义的 read 操作，简单说就是计数值count递减 1。
int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);   //计数器初始值设为零，属于系统调用。计数器由内核维护
    if (evtfd < 0)
    {
        LOG << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

/*
pwakeupChannel_用于处理wakeupFd_上的readable事件，将事件分发至handleread()函数。其中
pendingFunctors_暴露给了其他线程，因此用mutex保护。Server会写入pendingFunctors_？？
*/

//EventLoop初始化时
//a、looping、quit_、eventHanding_、callingPendingFunctors_、iteration_默认初始化false
//b、设置当前线程ID。threadId_
//c、创建一个Epoll
//d、创建一个定时器队列
//e、创建一个唤醒事件fd
//f、创建一个唤醒事件通道
EventLoop::EventLoop()             //构造函数
:   looping_(false),
    poller_(new Epoll()),         //new一个epoll
    wakeupFd_(createEventfd()),     //创建一个唤醒事件fd
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),   //通过调用函数返回，而不是直接访问CurrentThread的变量
    pwakeupChannel_(new Channel(this, wakeupFd_))  //new一个唤醒事件通道
{
    if (t_loopInThisThread)  // 如果当前线程已经创建了EventLoop对象，终止(LOG_FATAL)
    {
        //LOG << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);   //边沿触发
    pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));//设置唤醒通道的回调读
    pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
    poller_->epoll_add(pwakeupChannel_, 0);  //往红黑树上挂wakeupFd_
}

void EventLoop::handleConn()
{
    //poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET | EPOLLONESHOT), 0);
    updatePoller(pwakeupChannel_, 0);   
}


EventLoop::~EventLoop()
{
    //wakeupChannel_->disableAll();
    //wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = NULL;    //清除线程对应的evenloop对象
}


//这里用到的一个设计就是添加了回调函数到队列，通过往fd写个标志来通知，让阻塞的Poll立马返回去执行回调函数
//唤醒下，写一个8字节数据
/*
由于IO线程平时阻塞在时间循环EventLoop:loop()的epoll()调用中，为了让IO线程能立即执行
用户回调，我们需要设法唤醒它。传统的做法是用pipe(2)，IO线程始终监视此管道的readable
事件，在需要唤醒的时候，其他线程往管道里写一个字节，这样IO线程就从IO multiplexing
阻塞调用返回。现在Linux有了eventfd(2),可以更高效地唤醒，因为它不必管理缓冲区。

唤醒是，主IO线程调用子Eventloop中的wakeup函数，向子epoll中的eventfd写入数据，从而唤醒子epoll
*/
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
    if (n != sizeof one)
    {
        LOG<< "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

/*
EventLoop::wakeup()和EventLoop::handleRead()分别对wakeupFd_
*/
//读下8字节数据
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = readn(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
    //pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
}


//在EventLoop中执行回调函数
//a、如果Loop在自身当前线程，直接执行
//b、如果Loop在别的线程，则将回调放到Loop的方法队列，保证回调在Loop所在的线程中执行
/*
有了这个功能，我们就能轻易地在线程间调配任务，比方说把TimerNodeQueue的成员函数调用移到
其IO线程，这样可以在不用锁的情况下保证线程安全，而且无需用锁
*/
void EventLoop::runInLoop(Functor&& cb)
{
    if (isInLoopThread())
        cb();
    else
        queueInLoop(std::move(cb));  //封装成函数对象放入pendingFunctors_容器中，等待Loop_所属的线程运行时再被执行。
}


/*将回调函数放到待执行队列，并在必要的时候唤醒IO线程：
1、如果调用queueloop()的线程不是子IO线程，那么唤醒是必需的
2、如果在子IO线程调用queueInLoop()，而此时正在调用pendingfunctor，那么也必须唤醒
只有在IO线程的事件回调中调用queueInLoop()才无需wakeup()

主IO线程(而不是子IO线程)调用子循环Eventloop的queueInLoop
*/
void EventLoop::queueInLoop(Functor&& cb)
{
    {                            
        MutexLockGuard lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));   //压入子循环的回调队列
    }

    if (!isInLoopThread() || callingPendingFunctors_)  //不是在子IO线程，callingPendingFunctors_也是属于子Eventloop
        wakeup();
}

 
// 事件循环，该函数不能跨线程调用。事件循环只能在IO线程执行
// 只能在创建该对象的线程中调用
void EventLoop::loop()
{
    assert(!looping_);               //断言是否在循环
    assert(isInLoopThread());       //断言当前处于创建该对象的线程中，头文件有封装函数
    looping_ = true;
    quit_ = false;
    //LOG_TRACE << "EventLoop " << this << " start looping";
    std::vector<SP_Channel> ret;
    while (!quit_)
    {	                                 //定期查询是否有活跃的通道
        //cout << "doing" << endl;
        ret.clear();
        ret = poller_->poll();     //会阻塞十秒
        eventHandling_ = true;           //当前要处理下事件，置位状态。使用结果在下文的removeChannel()体现(似乎没有？)
        for (auto &it : ret)           //若有活跃的通道，则处理每一个Channel的handleEvents
            it->handleEvents();
        eventHandling_ = false;        
        doPendingFunctors();          //处理队列中是否有需要执行的回调方法
        poller_->handleExpired();    //处理超时事件
    }
    looping_ = false;
}


/*
doPendingFunctors()不是简单地在临界区依次调用Functor，而是把回调列表swap()到局部变量
functors中，这样一方面减小了临界区的长度(意味着不会阻塞其他线程调用queueInLoop)，另一方面
也避免了死锁(因为Functor可能再调用queueInLoop())。
*/
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;        //处理队列中的回调函数
    callingPendingFunctors_ = true;

	//交换出来，避免调用时长时间占用锁
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i)
        functors[i]();
    callingPendingFunctors_ = false;  //表示队列中没有待处理函数
}


//loop退出
//a、状态置位
//b、如果在当前线程，就唤醒下IO线程
void EventLoop::quit()
{
    quit_ = true;  //终止事件循环，但不是立刻发生，会在下次loop()检查while(!quit_)的时候起效
    if (!isInLoopThread())
    {
        wakeup();
    }
}