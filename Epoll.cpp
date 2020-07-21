// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Epoll.h"
#include "Util.h"
#include "base/Logging.h"
#include <sys/epoll.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <queue>
#include <deque>
#include <assert.h>

#include <arpa/inet.h>
#include <iostream>
using namespace std;


const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;   //十秒

typedef shared_ptr<Channel> SP_Channel;

Epoll::Epoll():                                //构造函数
    epollFd_(epoll_create1(EPOLL_CLOEXEC)),    //将epollFd_设为红黑树的头结点   
    events_(EVENTSNUM)                          //装epoll_event的vector容器，有4096个epoll_event          
{
    assert(epollFd_ > 0);              //断言红黑是是否成功建立
}
Epoll::~Epoll()
{ }


// 注册新描述符
void Epoll::epoll_add(SP_Channel request, int timeout)  //request是shared_ptr<Channel> ，智能指针，指向Channel
{
    int fd = request->getFd();             //获取此Channel负责的描述符，每个channel对象自始至终只负责一个文件描述符fd的IO事件的分发
    if (timeout > 0)
    {
        add_timer(request, timeout);
        fd2http_[fd] = request->getHolder(); //获取持有此Channel的HttpData
    }
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();      //获取请求注册Channel的events：EPOLLIN/OUT/ERR

    request->EqualAndUpdateLastEvents();      //将Channel上一次事件置为此次事件类型

    fd2chan_[fd] = request;
    if(epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0)
    {
        perror("epoll_add error");
        fd2chan_[fd].reset();
    }
}


// 修改描述符状态
void Epoll::epoll_mod(SP_Channel request, int timeout)
{
    if (timeout > 0)
        add_timer(request, timeout);
    int fd = request->getFd();
    if (!request->EqualAndUpdateLastEvents())  //当前描述符状态与最近的状态不一致返回false
    {                                           //不一致执行
        struct epoll_event event;
        event.data.fd = fd;
        event.events = request->getEvents();
        if(epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0)
        {
            perror("epoll_mod error");
            fd2chan_[fd].reset();       //若没有另外引用此Channel，释放Channel
        }
    }
}


// 从epoll中删除描述符
void Epoll::epoll_del(SP_Channel request)
{
    int fd = request->getFd();
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getLastEvents();
    //event.events = 0;
    // request->EqualAndUpdateLastEvents()
    if(epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) < 0)
    {
        perror("epoll_del error");
    }
    fd2chan_[fd].reset();
    fd2http_[fd].reset();
}




// 返回活跃事件数
//通过此调用获得一个vector<Channel*>activeChannels_的就绪事件集合
std::vector<SP_Channel> Epoll::poll()
{
    while (true)   //一直阻塞在此
    {
    //阻塞十秒
    //&*events_.begin()，作用是vector转数组，可以用&events_[0]代替
        int event_count = epoll_wait(epollFd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
        if (event_count < 0)
            perror("epoll wait error");
        std::vector<SP_Channel> req_data = getEventsRequest(event_count);
        if (req_data.size() > 0)
            return req_data;    //有活跃事件就返回，结束阻塞
    }
}

void Epoll::handleExpired()
{
    timerManager_.handleExpiredEvent();
	//
}

// 分发处理函数
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num)
{
    std::vector<SP_Channel> req_data;
    for(int i = 0; i < events_num; ++i)
    {
          // 获取有事件产生的描述符
        int fd = events_[i].data.fd;

        SP_Channel cur_req = fd2chan_[fd];
            
        if (cur_req)     //此时fd2chan_中有负责这个待处理fd的Channel，即合法的Channel
        {
            cur_req->setRevents(events_[i].events);      
            cur_req->setEvents(0);                       //更新fd2chan_[fd]状态
            // 加入线程池之前将Timer和request分离
            //cur_req->seperateTimer();
            req_data.push_back(cur_req);        //加入线程池
        }
        else
        {
            LOG << "SP cur_req is invalid";
        }
    }
    return req_data;
}

void Epoll::add_timer(SP_Channel request_data, int timeout)
{
    shared_ptr<HttpData> t = request_data->getHolder();   //返回持有该Channel的HttpData*
    if (t)
        timerManager_.addTimer(t, timeout);
    else
        LOG << "timer add fail";
}