/** @file pthreadWorkerPool.h
 *  @brief  A header-only thread automization library to make your multithreaded-lives easier.
 *  To add to your project just copy this header to your code and don't forget to link with
 *  pthreads, for example : gcc -O3 -pthread yourProject.c -o threadsExample
 *  Repository : https://github.com/AmmarkoV/PThreadWorkerPool
 *  @author Ammar Qammaz (AmmarkoV)
 */

#ifndef PTHREADWORKERPOOL_H_INCLUDED
#define PTHREADWORKERPOOL_H_INCLUDED

//The star of the show
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

static const char pthreadWorkerPoolVersion[]="0.34";

#define SPIN_SLEEP_TIME_MICROSECONDS 120

#define DEBUG_LOG 0

/**
 * @brief Structure representing a thread context.
 */
struct threadContext
{
    void * argumentToPass;
    struct workerPool * pool;
    unsigned int threadID;
    char threadInitialized;
};

/**
 * @brief Structure representing a worker pool.
 */
struct workerPool
{
    //---------------------
    volatile char initialized;
    volatile char work;
    volatile char mainThreadWaiting;
    //---------------------
    volatile int activeWorkers;
    volatile int completedWorkNumber;
    //---------------------
    pthread_attr_t initializationAttribute;

    //Start conditions..
    pthread_mutex_t startWorkMutex;
    pthread_cond_t  startWorkCondition;
    //---------------------

    //End conditions..
    pthread_mutex_t completeWorkMutex;
    pthread_cond_t  completeWorkCondition;
    //---------------------

    unsigned int numberOfThreads;
    //---------------------
    struct threadContext *workerPoolContext;
    pthread_t * workerPoolIDs;
};

#include <stdarg.h>
static void logmsg(const char *format, ...)
{
 #if DEBUG_LOG
    va_list args;

    // Start processing the variable arguments
    va_start(args, format);

    // Log to stderr (you can change this to write to a file if needed)
    vfprintf(stderr, format, args);

    // End processing the variable arguments
    va_end(args);
 #endif // DEBUG_LOG
}


#include <unistd.h>
#include <time.h>

static unsigned long tickBaseTPMN = 0;
/**
 * @brief Return number of clock ticks for our system in microseconds
 * @return Returns number of ticks in microseconds.
 */
static unsigned long GetTickCountMicrosecondsT()
{
    struct timespec ts;
    if ( clock_gettime(CLOCK_MONOTONIC,&ts) != 0)
        {
            return 0;
        }

    if (tickBaseTPMN==0)
        {
            tickBaseTPMN  = ts.tv_sec*1000000 + ts.tv_nsec/1000;
            return 0;
        }

    return ( ts.tv_sec*1000000 + ts.tv_nsec/1000 ) - tickBaseTPMN ;
}

/**
 * @brief Function for sleeping for a specified amount of time.
 * @param nanoseconds The number of nanoseconds to sleep.
 * @return Returns 0 on success, -1 on failure.
 */
static int nanoSleepT(long nanoseconds)
{
    struct timespec req, rem;

    req.tv_sec = 0;
    req.tv_nsec = nanoseconds;

    return nanosleep(&req, &rem);
}

#include <pwd.h>
#include <sched.h>
/**
 * @brief Function to stick the thread that calls the function to a specific CPU core, if the core does not exist it will wrap-around depending on the number of cores.
 * @param Core we want to associate this thread with.
 * @return Returns 0 on success, see pthread_setaffinity_np return values for possible failure codes.
 */
static int stick_this_thread_to_core(int core_id)
{
   #if _GNU_SOURCE
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   int core_id_mod_cores = core_id % num_cores;

   //if (core_id < 0 || core_id >= num_cores)
   //   return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id_mod_cores, &cpuset);

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
   #else
    fprintf(stderr,"Cannot stick thread to core without GNU source extensions during compilation\n");
    return EFAULT;
   #endif
}

/**
 * @brief Function for setting the real-time priority of a thread.
 * @return Returns 0 on success, -1 on failure.
 */
static int set_realtime_priority()
{
    int ret;

    // We'll operate on the currently running thread.
    pthread_t this_thread = pthread_self();
    // struct sched_param is used to store the scheduling priority
    struct sched_param params;

    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);

    fprintf(stderr,"Trying to set thread realtime prio = %u \n",params.sched_priority);

    // Attempt to set thread real-time priority to the SCHED_FIFO policy
    ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    if (ret != 0)
    {
        // Print the error
        fprintf(stderr,"Failed setting thread realtime priority\n");
        return 0;
    }

    // Now verify the change in thread priority
    int policy = 0;
    ret = pthread_getschedparam(this_thread, &policy, &params);
    if (ret != 0)
    {
        fprintf(stderr,"Couldn't retrieve real-time scheduling paramers\n");
        return 0;
    }

    // Check the correct policy was applied
    if(policy != SCHED_FIFO)
    {
        fprintf(stderr,"Scheduling is NOT SCHED_FIFO!\n");
    }
    else
    {
        fprintf(stderr,"SCHED_FIFO OK\n");
    }

    // Print thread scheduling priority
    fprintf(stderr,"Thread priority is now %u\n",params.sched_priority);
    return 1;
}



/**
 * @brief Function to change the priority of the process using `nice()`
 * @param priority The niceness value to set (-20 for highest priority, 19 for lowest)
 * @return Returns 0 on success, -1 on failure.
 */
static int set_process_nice(int priority)
{
    int ret = nice(priority);
    if (ret == -1 && errno != 0)  // Check for errors
    {
        perror("Failed to change process priority");
        return 0;
    }

    fprintf(stderr, "Process priority (nice value) set to %d\n", priority);
    return 1;
}



/**
 * @brief Function to increase the process priority using sudo and renice.
 * @param priority The nice value to set (-20 for highest priority, 19 for lowest)
 * @return Returns 0 on success, -1 on failure.
 */
static int elevate_nice_priority(int priority)
{
    pid_t pid = getpid();  // Get current process ID

    // Construct command: sudo renice -n <priority> -p <pid>
    char command[128]={0};
    snprintf(command, sizeof(command), "sudo renice -n %d -p %d", priority, pid);

    fprintf(stderr, "Please give password to execute command: %s\n", command);

    // Execute the system command
    int ret = system(command);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to elevate process priority\n");
        return 0;
    }

    fprintf(stderr, "Process priority successfully elevated to %d\n", priority);
    return 1;
}



/**
 * @brief Function for setting the real-time priority of a thread.
 * @return Returns 0 on success, -1 on failure.
 */
static int set_realtime_thread_priority()
{
    int ret;

    // We'll operate on the currently running thread.
    pthread_t this_thread = pthread_self();
    // struct sched_param is used to store the scheduling priority
    struct sched_param params;

    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);

    fprintf(stderr,"Trying to set thread realtime prio = %u \n",params.sched_priority);

    // Attempt to set thread real-time priority to the SCHED_FIFO policy
    ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    if (ret != 0)
    {
        // Print the error
        fprintf(stderr,"Failed setting thread realtime priority\n");
        return 0;
    }

    // Now verify the change in thread priority
    int policy = 0;
    ret = pthread_getschedparam(this_thread, &policy, &params);
    if (ret != 0)
    {
        fprintf(stderr,"Couldn't retrieve real-time scheduling paramers\n");
        return 0;
    }

    // Check the correct policy was applied
    if(policy != SCHED_FIFO)
    {
        fprintf(stderr,"Scheduling is NOT SCHED_FIFO!\n");
    }
    else
    {
        fprintf(stderr,"SCHED_FIFO OK\n");
    }

    // Print thread scheduling priority
    fprintf(stderr,"Thread priority is now %u\n",params.sched_priority);
    return 1;
}




/**
 * @brief Function for initializing a worker thread and waiting for start signal.
 * @param ctx Pointer to the thread context.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolWorkerInitialWait(struct threadContext * ctx)
{
    if (ctx!=0)
    {
     ctx->threadInitialized = 1;
     pthread_mutex_lock(&ctx->pool->startWorkMutex);
     pthread_cond_wait(&ctx->pool->startWorkCondition,&ctx->pool->startWorkMutex);
     return 1;
    }
    return 0;
}

/**
 * @brief Function for checking the loop condition of a worker thread.
 * @param ctx Pointer to the thread context.
 * @return Returns 1 if the condition is met, 0 otherwise.
 */
static int threadpoolWorkerLoopCondition(struct threadContext * ctx)
{
    if (ctx==0) { return 0; }

    if (ctx->pool->work)
    {
        pthread_mutex_unlock(&ctx->pool->startWorkMutex);
        return 1;
    } else
    {
        pthread_mutex_unlock(&ctx->pool->startWorkMutex);
        pthread_exit(NULL);
        return 0;
    }
}

/**
 * @brief Function for handling the end of a worker thread's loop.
 * @param ctx Pointer to the thread context.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolWorkerLoopEnd(struct threadContext * ctx)
{
    if (ctx==0) { return 0; }

    // Get a lock on "CompleteMutex" and make sure that the main thread is waiting, then set "TheCompletedBatch" to "ThisThreadNumber".  Set "MainThreadWaiting" to "FALSE".
    // If the main thread is not waiting, continue trying to get a lock on "CompleteMutex" until "MainThreadWaiting" is "TRUE".

    //Monitor main thread waiting status and update it!
    pthread_mutex_lock(&ctx->pool->completeWorkMutex);
    while ( !ctx->pool->mainThreadWaiting )
    {
        //If we are here it is our turn to talk to the main thread!
        ctx->pool->mainThreadWaiting = 0;
        pthread_mutex_unlock(&ctx->pool->completeWorkMutex);

        //Should this usleep be removed (?)
        usleep(SPIN_SLEEP_TIME_MICROSECONDS); //Make this spin slower..

        pthread_mutex_lock(&ctx->pool->completeWorkMutex);
        break;
    }
    //--------------------------------------------------

    //We are officially no longer an active worker!
    ctx->pool->activeWorkers -=1;
    ctx->pool->completedWorkNumber = ctx->threadID;

    //We are done communicating with Coordinator on our completed work
    pthread_mutex_unlock(&ctx->pool->completeWorkMutex);


    // Lock the "StartWorkMutex" before we send out the "CompleteCondition" signal.
    // This way, we can enter a waiting state for the next round before the main thread broadcasts the "StartWorkCondition".
    pthread_mutex_lock(&ctx->pool->startWorkMutex); //<- This will get released by threadpoolWorkerLoopCondition
    pthread_cond_signal(&ctx->pool->completeWorkCondition);

    // Wait for the Main thread to send us the next "StartWorkCondition" broadcast.
    // Be sure to unlock the corresponding mutex immediately so that the other worker threads can exit their waiting state as well.
    unsigned long workerLoopBlockStartTime = GetTickCountMicrosecondsT();
    pthread_cond_wait(&ctx->pool->startWorkCondition, &ctx->pool->startWorkMutex);
    unsigned long workerLoopBlockFinishTime = GetTickCountMicrosecondsT();

    //report Resyncing lag with the rest of the thread pool
    logmsg("Thread %u | Re-Sync Lag: %lu μsec \n",ctx->threadID , workerLoopBlockFinishTime-workerLoopBlockStartTime);

    return 1;
}

/**
 * @brief Function for preparing work for worker threads by the main thread.
 * @param pool Pointer to the worker pool.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolMainThreadPrepareWorkForWorkers(struct workerPool * pool)
{
    if (pool==0) { return 0; }

    if (pool->initialized)
    {
        pthread_mutex_lock(&pool->startWorkMutex); //This will be released by threadpoolMainThreadWaitForWorkersToFinishTimeoutSeconds
        return 1;
    }
    return 0;
}

/**
 * @brief Function for waiting worker for threads to finish their task by the main thread.
 * @param pool Pointer to the worker pool.
 * @param timeout Number of seconds to wait for threads to complete their work, otherwise abort process as stuck (0 = wait forever).
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolMainThreadWaitForWorkersToFinishTimeoutSeconds(struct workerPool * pool, int timeoutSeconds)
{
    if (pool==0) { return 0; }

    if (pool->initialized)
    {
        pool->work=1;

        //We consider all workers as active from now on!
        pool->activeWorkers = pool->numberOfThreads;

        pthread_cond_broadcast(&pool->startWorkCondition); //Broadcast starting condition

        //Release lock for starting work that main thread held since threadpoolMainThreadPrepareWorkForWorkers was called
        pthread_mutex_unlock(&pool->startWorkMutex);       //Now start worker threads

        //At this point of the code for the particular iteration all single threaded chains have been executed
        //All parallel threads are running and now we must wait until they are done and gather their output

         struct timespec ts;
         //If we are using a timeout check the time and
         //set the deadline for the correct amount of seconds
         //or else don't bother getting the time
         if (timeoutSeconds!=0)
         {
          clock_gettime(CLOCK_REALTIME, &ts);
          ts.tv_sec += timeoutSeconds;
         }


        //We now wait for "numberOfWorkerThreads" worker threads to finish
        //for (int numberOfWorkerThreadsToWaitFor=0;  numberOfWorkerThreadsToWaitFor<pool->numberOfThreads; numberOfWorkerThreadsToWaitFor++)
        //Signal that we can start and wait for finish...
        pthread_mutex_lock(&pool->completeWorkMutex);      //Make sure worker threads wont fall through after completion
        //From now on we are waiting for the workers to hand-over their finished jobs..
        pool->mainThreadWaiting = 1;

        while (pool->activeWorkers>0)
        {

            // Before entering a waiting state, set "MainThreadWaiting" to "TRUE" while we still have a lock on the "CompleteMutex".
            // Worker threads will be waiting for this condition to be met before sending "CompleteCondition" signals.
            pool->mainThreadWaiting = 1;

            // This is where partial work on the batch data coordination will happen.
            // All of the worker threads will have to finish before we can start the next batch.
            if (timeoutSeconds!=0)
            {
             //we wait for a signal up to the deadline time, if we timeout we will abort!
             int ret = pthread_cond_timedwait(&pool->completeWorkCondition, &pool->completeWorkMutex, &ts);
             if (ret == ETIMEDOUT)
                     {
                        fprintf(stderr, "\n\npthreadWorkerPool: Timeout (%u sec) occurred @ %u of %u, threads are stuck.\n", timeoutSeconds, pool->activeWorkers, pool->numberOfThreads);
                        // Handle the stuck thread (e.g., attempt to cancel or exit).
                        abort();// We just abort to make sure this becomes a visible failure
                     }
            } else
            {
             pthread_cond_wait(&pool->completeWorkCondition, &pool->completeWorkMutex);
             //Waiting forever :S..
            }
          //fprintf(stderr,"Done Waiting!\n");
          pthread_mutex_unlock(&pool->completeWorkMutex);

          //Give time to worker threads to lock common variables
          usleep(SPIN_SLEEP_TIME_MICROSECONDS);

          //Signal that we can start and wait for finish...
          pthread_mutex_lock(&pool->completeWorkMutex);      //Make sure worker threads wont fall through after completion
        }

        //We finished waiting..
        pool->mainThreadWaiting = 0;

        pthread_mutex_unlock(&pool->completeWorkMutex);

        //--------------------------------------------------
        return 1;
    }
    return 0;
}

/**
 * @brief Function for waiting worker for threads to finish their task by the main thread. This function will wait forever if something goes wrong with workers
 * @param pool Pointer to the worker pool.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolMainThreadWaitForWorkersToFinish(struct workerPool * pool)
{
  return threadpoolMainThreadWaitForWorkersToFinishTimeoutSeconds(pool,0);
}

/**
 * @brief Function for creating a worker pool.
 * @param pool Pointer to the worker pool to be created.
 * @param numberOfThreadsToSpawn Number of threads to spawn in the pool.
 * @param workerFunction Pointer to the worker function.
 * @param argument Argument to pass to the worker function.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolCreate(struct workerPool * pool,unsigned int numberOfThreadsToSpawn,void *  workerFunction, void * argument)
{
    if (pool==0)
    {
        return 0;
    }
    if (pool->workerPoolIDs!=0)
    {
        return 0;
    }
    if (pool->workerPoolContext!=0)
    {
        return 0;
    }

    //--------------------------------------------------
    pool->work = 0;
    pool->mainThreadWaiting = 0;
    pool->numberOfThreads = 0;
    pool->workerPoolIDs     = (pthread_t*) malloc(sizeof(pthread_t) * numberOfThreadsToSpawn);
    pool->workerPoolContext = (struct threadContext*) malloc(sizeof(struct threadContext) * numberOfThreadsToSpawn);
    //--------------------------------------------------

    if ( (pool->workerPoolIDs==0) ||  (pool->workerPoolContext==0) )
    {
        fprintf(stderr,"Failed allocating worker pool resources\n");
        if (pool->workerPoolContext!=0)
        {
          free(pool->workerPoolContext);
          pool->workerPoolContext=0;
        }
        if (pool->workerPoolIDs!=0)
        {
          free(pool->workerPoolIDs);
          pool->workerPoolIDs=0;
        }
        return 0;
    }

    //--------------------------------------------------
    pthread_cond_init(&pool->startWorkCondition,0);
    pthread_mutex_init(&pool->startWorkMutex,0);
    pthread_cond_init(&pool->completeWorkCondition,0);
    pthread_mutex_init(&pool->completeWorkMutex,0);
    //--------------------------------------------------
    pthread_attr_init(&pool->initializationAttribute);
    pthread_attr_setdetachstate(&pool->initializationAttribute,PTHREAD_CREATE_JOINABLE);

    int threadsCreated = 0;

    for (unsigned int i=0; i<numberOfThreadsToSpawn; i++)
    {
        pool->workerPoolContext[i].threadID=i;
        pool->workerPoolContext[i].threadInitialized = 0;
        pool->workerPoolContext[i].argumentToPass=argument;
        pool->workerPoolContext[i].pool=pool;

        //Wrap into a call..
        //void ( *callWrapped) (void *) =0;
        //callWrapped = (void(*) (void *) ) workerFunction;

        int result = pthread_create(
                                     &pool->workerPoolIDs[i],
                                     &pool->initializationAttribute,
                                     (void * (*)(void*)) workerFunction,
                                     (void*) &pool->workerPoolContext[i]
                                   );

        threadsCreated += (result == 0);
    }

    //Sleep while threads wake up..
    //If this sleep time is not enough a deadlock might occur, need to fix that
    fprintf(stderr,"Waiting for threads to start : ");
    while (1)
    {
      //nanoSleepT(1000);
      usleep(SPIN_SLEEP_TIME_MICROSECONDS);
      unsigned int threadsThatAreReady=0;
      for (unsigned int i=0; i<threadsCreated; i++)
      {
          threadsThatAreReady+=pool->workerPoolContext[i].threadInitialized;
      }

      if (threadsThatAreReady==threadsCreated)
      {
          break;
      } else
      {
         fprintf(stderr,".");
      }
    }
    fprintf(stderr," done \n");

    pool->numberOfThreads = threadsCreated;
    pool->initialized     = (threadsCreated==numberOfThreadsToSpawn);
    return (threadsCreated==numberOfThreadsToSpawn);
}



/**
 * @brief Function for destroying a worker pool.
 * @param pool Pointer to the worker pool to be destroyed.
 * @return Returns 1 on success, 0 on failure.
 */
static int threadpoolDestroy(struct workerPool *pool)
{
   if ( (pool!=0) && (pool->workerPoolIDs!=0) && (pool->workerPoolContext!=0) )
    {

    pthread_mutex_lock(&pool->startWorkMutex);
    // Set the conditions to stop all threads.
    pool->work = 0;
    pthread_cond_broadcast(&pool->startWorkCondition);
    pthread_mutex_unlock(&pool->startWorkMutex);


    for (unsigned int i=0; i<pool->numberOfThreads; i++)
    {
        pthread_join(pool->workerPoolIDs[i],0);
    }

    // Clean up and exit.
    pthread_attr_destroy(&pool->initializationAttribute);
    pthread_mutex_destroy(&pool->completeWorkMutex);
    pthread_cond_destroy(&pool->completeWorkCondition);
    pthread_mutex_destroy(&pool->startWorkMutex);
    pthread_cond_destroy(&pool->startWorkCondition);

    free(pool->workerPoolContext);
    pool->workerPoolContext=0;
    free(pool->workerPoolIDs);
    pool->workerPoolIDs=0;
    return 1;
    }

  return 0;
}



#ifdef __cplusplus
}
#endif

#endif // PTHREADWORKERPOOL_H_INCLUDED



