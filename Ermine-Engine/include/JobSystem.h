/* Start Header ************************************************************************/
/*!
\file       JobSystem.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       21/03/2025
\brief      This file contains the declaration of the job system.
            This file is used to manage the job system for the engine using thread pools
            Steps to create your own task/job to process
            1. You'll need to create your params (Either primitive type or a struct to hold your values
            2. Write your lambda functions that the job will process (Don't capture anything to lambda, use the jobparams to retain/pass values)
            3. Now Create a declaration/s to pass to the job system to 'Kick' off

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

namespace Ermine::job
{
    // To track active jobs
    struct JobCounter
    {
        std::atomic<int> count{0};
        std::condition_variable condition;
        std::mutex mutex;
    };
        
    // Signature of all job entries
    typedef void EntryPoint(uintptr_t param);

    // Allowable priority levels for jobs
    enum class Priority : uint8_t
    {
        LOW,
        NORMAL,
        HIGH,
        CRITICAL
    };

    // Simple job declaration
    struct Declaration
    {
        EntryPoint* m_pEntry;
        uintptr_t m_param;
        Priority m_priority;
    };

    // Global job counter for the system
    static JobCounter g_jobCounter;

    /**
     * @brief Thread pool for executing jobs. Priority-based job scheduling.
     */
    class ThreadPool
    {
        /**
         * @brief Get the highest priority job
         * @return Declaration The highest priority job
         */
        Declaration GetHighestPriorityJob();

        struct JobComparator
        {
            bool operator()(const Declaration& lhs, const Declaration& rhs) const
            {
                return static_cast<int>(lhs.m_priority) < static_cast<int>(rhs.m_priority);
            }
        };

        std::vector<std::thread> m_workers;
        std::priority_queue<Declaration, std::vector<Declaration>, JobComparator> m_jobQueue;
        std::mutex m_queueMutex;
        std::condition_variable m_condition;
        bool m_stop = false;
    public:
        /**
         * @brief Construct a new Thread Pool object
         * @param numThreads The number of threads to use
         */
        ThreadPool(size_t numThreads);
        /**
         * @brief Destroy the Thread Pool object
         */
        ~ThreadPool();

        // We do not need any copy or move variations of the thread pool
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /**
         * @brief Enqueue a job
         * @param job The job to enqueue
         */
        void EnqueueJob(const Declaration& job);
    };

    // Instance of the thread pool
    static ThreadPool* g_pThreadPool = nullptr;

    /**
     * @brief Initialize the job system
     * @param numThreads The number of threads to use, default is the number of hardware threads
     */ 
    void Initialize(size_t numThreads = std::thread::hardware_concurrency());

    /**
     * @brief Shutdown the job system
     */
    void Shutdown();

    // Kick a job
    /**
     * @brief kick a job to the job system to run
     * @param decl a small data structure that contains the job to run
     */
    void KickJob(const Declaration& decl);
    /**
     * @brief kick multiple jobs to the job system to run
     * @param count the number of jobs to run
     * @param aDecl an array of jobs to run
     */
    void KickJobs(int count, const Declaration aDecl[]);

    /**
     * @brief Check if all jobs are completed without blocking
     * @return true if all jobs are completed, false otherwise
     */
    bool AreJobsCompleted();

    /**
     * @brief Wait for all jobs to complete
     */
    void WaitForCounter();

    /**
     * @brief Wait for all jobs to complete with a timeout
     * @param ms The timeout in milliseconds
     * @return true if all jobs completed, false otherwise
     */
    bool WaitForCounterWithTimeout(uint32_t ms);
    
    /**
     * @brief kick a job to the job system to run and wait for completion
     * @param decl a small data structure that contains the job to run
     */
    void KickJobAndWait(const Declaration& decl);
    /**
     * @brief kick multiple jobs to the job system to run and wait for completion
     * @param count the number of jobs to run
     * @param aDecl an array of jobs to run
     */
    void KickJobsAndWait(int count, const Declaration aDecl[]);
}
