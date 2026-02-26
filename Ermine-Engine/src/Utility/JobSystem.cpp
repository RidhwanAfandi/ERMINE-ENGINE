/* Start Header ************************************************************************/
/*!
\file       JobSystem.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       21/03/2025
\brief      This file contains the declaration of the job system.
            This file is used to manage the job system for the engine using thread pools

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "JobSystem.h"

using namespace Ermine::job;

/**
 * @brief Get the highest priority job
 * @return Declaration The highest priority job
 */
Declaration ThreadPool::GetHighestPriorityJob()
{
    Declaration highestPriorityJob = m_jobQueue.top();
    m_jobQueue.pop();
    return highestPriorityJob;
}

/**
 * @brief Construct a new Thread Pool object
 * @param numThreads The number of threads to use
 */
ThreadPool::ThreadPool(size_t numThreads)
{
    for (size_t i = 0; i < numThreads; ++i) // For the number of threads, create a worker
    {
        m_workers.emplace_back([this, i]()
        {
           while (true)
           {
               Declaration job;
               {
                   std::unique_lock lock(m_queueMutex);
                   m_condition.wait(lock, [this]() // Wait for a job to be available
                   {
                       return m_stop || !m_jobQueue.empty();
                   });

                   if (m_stop && m_jobQueue.empty())
                   {
                       EE_CORE_TRACE("Worker thread {0} stopped.", i);
                       break;
                   }

                   // Get the highest priority job
                   job = GetHighestPriorityJob();
               }
               
               // Execute the job
               job.m_pEntry(job.m_param);

               // Decrement counter and notify waiting threads if needed
               if (g_jobCounter.count.fetch_sub(1) == 1)
               {
                   std::lock_guard lock(g_jobCounter.mutex);
                   g_jobCounter.condition.notify_all();
               }
           }
        });
    }
}

/**
 * @brief Destroy the Thread Pool object
 */
ThreadPool::~ThreadPool()
{
    WaitForCounter();
    {
        std::unique_lock lock(m_queueMutex);
        m_stop = true;
        EE_CORE_TRACE("Notifying all worker threads to stop.");
        m_condition.notify_all();
    }
    
    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
            worker.join();
    }
    EE_CORE_INFO("All worker threads joined. Completed shutdown.");
}

/**
 * @brief Enqueue a job
 * @param job The job to enqueue
 */
void ThreadPool::EnqueueJob(const Declaration& job)
{
    std::unique_lock lock(m_queueMutex);
    m_jobQueue.push(job);

    m_condition.notify_one();
}

/**
 * @brief Initialize the job system
 * @param numThreads The number of threads to use, default is the number of hardware threads
 */ 
void Ermine::job::Initialize(size_t numThreads)
{
    if (!g_pThreadPool)
    {
        EE_CORE_INFO("Job system initialized with {0} threads", numThreads);
        g_pThreadPool = new ThreadPool(numThreads);
    }
}

/**
 * @brief Shutdown the job system
 */
void Ermine::job::Shutdown()
{
    if (g_pThreadPool)
    {
        EE_CORE_TRACE("Shutting down job system...");
        delete g_pThreadPool;
        g_pThreadPool = nullptr;
    }
}

/**
 * @brief kick a job to the job system to run
 * @param decl a small data structure that contains the job to run
 */
void Ermine::job::KickJob(const Declaration& decl)
{
    g_jobCounter.count.fetch_add(1);
    g_pThreadPool->EnqueueJob(decl);
}

/**
 * @brief kick multiple jobs to the job system to run
 * @param count the number of jobs to run
 * @param aDecl an array of jobs to run
 */
void Ermine::job::KickJobs(int count, const Declaration aDecl[])
{
    g_jobCounter.count.fetch_add(count);
    for (int i = 0; i < count; ++i)
    {
        g_pThreadPool->EnqueueJob(aDecl[i]);
    }
}

/**
 * @brief Check if all jobs are completed without blocking
 * @return true if all jobs are completed, false otherwise
 */
bool Ermine::job::AreJobsCompleted()
{
    return g_jobCounter.count <= 0;
}

/**
 * @brief Wait for all jobs to complete
 */
void Ermine::job::WaitForCounter()
{
    std::unique_lock<std::mutex> lock(g_jobCounter.mutex);
    g_jobCounter.condition.wait(lock, []()
    {
       return g_jobCounter.count.load() == 0; 
    });
}

/**
 * @brief Wait for all jobs to complete with a timeout
 * @param ms The timeout in milliseconds
 * @return true if all jobs completed, false otherwise
 */
bool Ermine::job::WaitForCounterWithTimeout(uint32_t ms)
{
    std::unique_lock<std::mutex> lock(g_jobCounter.mutex);
    return g_jobCounter.condition.wait_for(lock, std::chrono::milliseconds(ms), []()
    {
        return g_jobCounter.count <= 0;
    });
}

/**
 * @brief kick a job to the job system to run and wait for completion
 * @param decl a small data structure that contains the job to run
 */
void Ermine::job::KickJobAndWait(const Declaration& decl)
{
    KickJob(decl);
    WaitForCounter();
}

/**
 * @brief kick multiple jobs to the job system to run and wait for completion
 * @param count the number of jobs to run
 * @param aDecl an array of jobs to run
 */
void Ermine::job::KickJobsAndWait(int count, const Declaration aDecl[])
{
    KickJobs(count, aDecl);
    WaitForCounter();
}
