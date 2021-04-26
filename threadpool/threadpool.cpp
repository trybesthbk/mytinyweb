#include <exception>
#include <pthread.h>
#include "threadpool.h"
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

threadpool::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) \
: m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();    //抛出异常
    m_threads = new pthread_t[m_thread_number];  //创建线程数组
    if (!m_threads)  //创建失败则抛出异常
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        //创建线程处理工作，创建失败则抛出异常
        //worker去线程池提取任务
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        //分离该线程：自己执行，不等待
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

threadpool::~threadpool()
{
    delete[] m_threads;  //销毁线程数组
}


bool threadpool::append(http_conn *request, int state)  //扩展
{
    m_queuelocker.lock();  //队列锁
    if (m_workqueue.size() >= m_max_requests)  //工作队列已满
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);  //在工作队列里添加该请求
    m_queuelocker.unlock();
    m_queuestat.post();  //加入任务
    return true;
}

bool threadpool::append_p(http_conn *request)  //无状态扩展
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

void *threadpool::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;  //启动线程池
    pool->run();
    return pool;
}

void threadpool::run()
{
    while (true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())  //不断查看工作队列
        {
            m_queuelocker.unlock();
            continue;
        }
        http_conn *request = m_workqueue.front();  //提取任务
        m_workqueue.pop_front();  //队列中去除该任务
        m_queuelocker.unlock(); 
        if (!request)  //这是个空任务
            continue;
        if (1 == m_actor_model)  //工作模式1
        {
            if (0 == request->m_state)  //状态0
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
