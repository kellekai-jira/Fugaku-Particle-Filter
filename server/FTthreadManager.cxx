#ifdef WITH_FTI_THREADS

#include "FTthreadManager.h"
#include <iostream>
#include <algorithm>
#include <chrono>


FTthreadManager::FTthreadManager() : 
    m_online(true),
    m_requests(0),
    m_pending(0)
{
}

FTthreadManager::~FTthreadManager() 
{
    if( m_online == true ) {
        m_online = false;
        m_scheduler_thread.join();
    }
}

void FTthreadManager::init( int max_threads )
{
    std::lock_guard<std::mutex> lck(m_mutex);
    m_max_threads = max_threads;
    init_runner_queue();
    m_scheduler_thread = std::thread(&FTthreadManager::start_scheduler, this);
}

void FTthreadManager::fini()
{
    m_online = false;
    m_scheduler_thread.join();
}

void FTthreadManager::init_runner_queue()
{
    m_task_queue_running.resize( m_max_threads );
}
void FTthreadManager::start_scheduler() 
{
    while( m_online ) {
        for( auto & task : m_task_queue_running ) {
            if( task.active.load(std::memory_order_acquire) ) {
                if( task.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready ) {
                    task.th.join();
                    task.active.store(false, std::memory_order_release);
                    m_requests.fetch_sub( 1, std::memory_order_release);
                }
            }
        }
        if( m_pending.load( std::memory_order_acquire ) > 0 ) {
            auto it = find_if( m_task_queue_running.begin(), m_task_queue_running.end(), [this]( task_t & task )
                    {
                        return !task.active.load(std::memory_order_acquire);
                    });
            if( it != m_task_queue_running.end() ) { 
                it->promise = std::promise<bool>();
                it->future = it->promise.get_future();
                it->th = std::thread( &FTthreadManager::task_wrapper, this, m_task_queue_pending.front(), std::ref(it->promise) );
                {
                    std::lock_guard<std::mutex> lck(m_mutex);
                    m_task_queue_pending.pop();
                }
                it->active.store(true, std::memory_order_release);
                m_pending.fetch_sub( 1, std::memory_order_release );
            }
        }
        std::this_thread::yield();
    }
}

void FTthreadManager::task_wrapper( std::function<int()> f, std::promise<bool> & p )
{
    f();
    p.set_value(true);
}
        
void FTthreadManager::synchronize()
{
    while( (m_pending.load( std::memory_order_acquire ) > 0) || (m_requests.load(std::memory_order_acquire) > 0) ) {
        std::this_thread::yield();
    }
}

#endif //WITH_FTI_THREADS
