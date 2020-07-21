// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"
#include <getopt.h>
#include <string>

int main(int argc, char *argv[])
{
    int threadNum = 4;
    int port = 80;
    std::string logPath = "./WebServer.log";

    // parse args
    int opt;
    const char *str = "t:l:p:";   //设置三个选项
    while ((opt = getopt(argc, argv, str))!= -1)// getopt()用来分析命令行参数，处理argv，返回一个选项
    {
        switch (opt)
        {
            case 't':
            {
                threadNum = atoi(optarg); //选项‘t’的参数为threadnum
                break;
            }
            case 'l':
            {
                logPath = optarg;       //选项‘l’的参数为logPath
                if (logPath.size() < 2 || optarg[0] != '/')
                {
                    printf("logPath should start with \"/\"\n");  
                    abort();//立即终止当前进程，产生异常程序终止，进程终止时不会销毁任何对象
                }
                break;   
            }
            case 'p':
            {
                port = atoi(optarg);  //设置端口
                break;
            }
            default: break;
        }
    }
    Logger::setLogFileName(logPath);              //静态成员函数直接调用
    // STL库在多线程上应用
    #ifndef _PTHREADS
        LOG << "_PTHREADS is not defined !";    //LOG为流
    #endif
    EventLoop mainLoop;
    Server myHTTPServer(&mainLoop, threadNum, port);//初始化了EventLoop的指针变量 ， myHTTPServer和mainLoop绑定了；初始化了threadNum ， 和port；，然后执行了socket bind  listen 获得了监听socket，把socket设置了非阻塞方式，创建了只属于自己的channel。
    myHTTPServer.start();//开启线程池管理 ， 给自己的channel上注册事件 ， 绑定回调函数 ， 然后加到自己的epoll红黑树上。
    mainLoop();//开启poller轮询器开始轮询 ， 如果有新连接到来 ， 则在IO线程里面找一个接管。
    return 0;
}
