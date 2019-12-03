#ifndef __FTTHREADMANAGER__
#define __FTTHREADMANAGER__

#include <thread>
#include <vector>
#include <future>
#include <functional>
#include <mutex>
#include <queue>

class FTthreadManager
{
    public:

        FTthreadManager();
        ~FTthreadManager();
        void init( int max_threads );
        void fini();
        template<class F, class... Args>
        void submit(F&& func, Args&&... args);
        void synchronize();
        void start_scheduler();

    private:
       
        struct task_t {
            bool active;
            std::promise<bool> promise;
            std::future<bool> future;
        };
        std::thread m_scheduler_thread;
        std::mutex m_mutex;
        std::mutex m_mutex_dbg;
        void init_runner_queue();
        void task_wrapper( std::function<void()> f, std::promise<bool> & p );
        std::queue< std::function<void()> > m_task_queue_pending;
        std::vector< task_t > m_task_queue_running;
        int m_max_threads;
        bool m_online;

};

template<class F, class... Args>
void FTthreadManager::submit(F&& f, Args&&... args)
{
    m_mutex.lock();
    m_task_queue_pending.push( std::function<void()>() );
    m_task_queue_pending.back() = std::bind( f, args... );
    m_mutex.unlock();
}        

#endif // __FTTHREADMANAGER__
