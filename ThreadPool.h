// @Author Lin Ya
// @Email xxbbb@vip.qq.com
// This file has not been used
#pragma once
#include "Channel.h"
#include <pthread.h>
#include <functional>
#include <memory>
#include <vector>

const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;
const int THREADPOOL_GRACEFUL = 1;

const int MAX_THREADS = 1024;      //线程数目
const int MAX_QUEUE = 65535;       //任务队列数

typedef enum
{
    immediate_shutdown = 1,
    graceful_shutdown  = 2
} ShutDownOption;

struct ThreadPoolTask          // 封装了任务 
{
    std::function<void(std::shared_ptr<void>)> fun;
    std::shared_ptr<void> args;
};

/*
线程池本质上也是生产者消费者问题：生产者线程向任务队列添加任务，
消费者线程（在线程队列中）从任务队列取出任务去执行。
*/

//全是静态变量
class ThreadPool
{
private:
    static pthread_mutex_t lock;     //互斥锁
    static pthread_cond_t notify;    //条件变量

    static std::vector<pthread_t> threads;           //线程ID, unsigned long int
    static std::vector<ThreadPoolTask> queue;        //任务队列
    static int thread_count;                        
    static int queue_size;
    static int head;         // head 指向头结点？
   
    static int tail;       // tail 指向尾节点的下一节点
    static int count;
    static int shutdown;
    static int started;
public:
    static int threadpool_create(int _thread_count, int _queue_size);
    static int threadpool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun);
    static int threadpool_destroy(ShutDownOption shutdown_option = graceful_shutdown);
    static int threadpool_free();
    static void *threadpool_thread(void *args);
};
