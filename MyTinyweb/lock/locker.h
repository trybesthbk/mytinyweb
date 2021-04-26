#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//封装信号量
class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        //int sem_init(sem_t *sem, int pshared, unsigned int value);
        //参数2：0：该进程的线程下共享，参数3：信号量大小
        {
            throw std::exception();
        }
    }
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()  //信号量-1
    {
        return sem_wait(&m_sem) == 0;
    }
    bool post()  //信号量+1
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

//封装互斥锁
class locker
{
public:
    locker()  //线程锁初始化
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~locker()  //销毁线程锁
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()  //加锁
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()  //解锁
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

//封装条件变量，设置条件阻塞线程
class cond 
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)  //初始化线程条件变量
        {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    bool wait(pthread_mutex_t *m_mutex)  //等待某个锁
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;   //成功
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) //设置限制时间执行
    {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;  //发一个信号给等待锁的线程，使其脱离阻塞
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0; //向所有阻塞线程发信号使其脱离阻塞
    }

private:
    pthread_cond_t m_cond;
};
#endif
