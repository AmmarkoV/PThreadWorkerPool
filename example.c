/** @file example.c
 *  @brief  An example using pthreadWorkerPool.h to organize a large number of concurrently working threads without
 *  too many lines of code or difficulty understanding what is happening
 *  https://github.com/AmmarkoV/PThreadWorkerPool
 *  @author Ammar Qammaz (AmmarkoV)
 */

//Can also be compiled using :
//gcc -O3 example.c -pthread -lm -o example

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "pthreadWorkerPool.h"


struct workerThreadContext
{
    //Add thread specific stuff here..
    double computationInput;
    double computationOutput;
};

void *workerThread(void * arg)
{
    //We are a worker thread so we need to retrieve our variables ..
    //---------------------------------------------------------------
    struct threadContext * ptr = (struct threadContext *) arg;
    fprintf(stdout,"Thread-%u: Started..!\n",ptr->threadID);
    struct workerThreadContext * contextArray = (struct workerThreadContext *) ptr->argumentToPass;
    struct workerThreadContext * ctx = &contextArray[ptr->threadID];
    //---------------------------------------------------------------

    if (stick_this_thread_to_core(ptr->threadID)!=0)
       { fprintf(stderr,"We where not able to pin thread %u to a specific core\n",ptr->threadID); }

    threadpoolWorkerInitialWait(ptr);
    unsigned int i;
    double work,workStepTwo;

    while (threadpoolWorkerLoopCondition(ptr))
    {
        work=ctx->computationInput;

        fprintf(stdout,"Thread-%u: Starting to work..!\n",ptr->threadID);
        unsigned long workerStartTime = GetTickCountMicrosecondsT();

        // This is the location where batch processing work will be carried out.
        // Emulate performing some sort of computation.. Do busy-work.
        //--------------------------------------------------------------
        for ( i = 0; i < 40000000; i++ )
        {
            work = (double) (work + i + 42.23);
            work = sqrt(work);
            workStepTwo = sqrt(work + (double) i);
        }
        ctx->computationOutput = workStepTwo + ptr->threadID;
        //--------------------------------------------------------------

        unsigned long workerFinishTime = GetTickCountMicrosecondsT();
        fprintf(stdout,"Thread-%u: Finished our part of work in %lu μsec..!\n",ptr->threadID, workerFinishTime - workerStartTime);
        threadpoolWorkerLoopEnd(ptr);
    }

    return 0;
}


int main(int argc, char *argv[])
{
    //Our worker pool ready and clean
    struct workerPool pool={0};

    if (!set_process_nice(-11))
    { fprintf(stdout,"Failed setting real-time process priority.. \n"); }

    if (!set_realtime_thread_priority())
    { fprintf(stdout,"Failed setting real-time thread priority.. \n"); }

    //Sleep for 1000 nano seconds for no reason
    //other than showing that this call exists..
    nanoSleepT(1000);

    int numberOfThreads    = 8;
    int numberOfIterations = 128;
    if (argc>0)
    {
      for (int i=0; i<argc; i++)
      {
        if (strcmp(argv[i],"--threads")==0)
                {
                    numberOfThreads  = atoi(argv[i+1]);
                    fprintf(stdout,"Threads set to %u\n",numberOfThreads);
                } else
        if (strcmp(argv[i],"--iterations")==0)
                {
                    numberOfIterations  = atoi(argv[i+1]);
                    fprintf(stdout,"Iterations set to %u\n",numberOfIterations);
                } else
        if (strcmp(argv[i],"--rt")==0)
                {
                    elevate_nice_priority(-20);
                }


      }
    }

    //We also create one context to be supplied for each thread..
    struct workerThreadContext * context = (struct workerThreadContext *) malloc(sizeof(struct workerThreadContext) * numberOfThreads);

    if (context!=0)
    {
     memset(context,0,sizeof(struct workerThreadContext) * numberOfThreads);

     if ( threadpoolCreate(&pool,numberOfThreads,workerThread,(void *) context) )
     {
        fprintf(stdout,"Worker thread pool created.. \n");
        unsigned int iterationID;
        for (iterationID=0; iterationID<numberOfIterations; iterationID++)
        {
            unsigned long poolStartTime = GetTickCountMicrosecondsT();
            fprintf(stdout,"Iteration %u/%u \n",iterationID+1,numberOfIterations);
            fprintf(stdout,"----------------------------------------------------------------------------\n");
            threadpoolMainThreadPrepareWorkForWorkers(&pool);

            fprintf(stdout,"Main thread preparing tasks..!\n");
            //Prepare random input..
            unsigned int contextID;
            for (contextID=0; contextID<numberOfThreads; contextID++)
            {
                context[contextID].computationInput = (float)rand()/(float)(RAND_MAX/1000);
            }

            //This function will wait forever for the threads to complete their work
            //it is equivalent to threadpoolMainThreadWaitForWorkersToFinishTimeoutSeconds(&pool,0);
            fprintf(stdout,"Main thread waiting for workers to do tasks..!\n");
            threadpoolMainThreadWaitForWorkersToFinish(&pool);

            //Alternatively the next function has a hard timeout limit, if the wait for a thread takes more time
            //the library will abort, terminating the execution of the process to avoid deadlocks..
            //the given limit should be large and this function exists to prevent deadlocks in a very noticeable way
            //threadpoolMainThreadWaitForWorkersToFinishTimeoutSeconds(&pool,10); //wait for up to 10 sec before stopping program execution

            fprintf(stdout,"Main thread collecting results..!\n");
            for (contextID=0; contextID<numberOfThreads; contextID++)
            {
                fprintf(stdout,"Main thread recovered the output of thread %u | Output value : %f\n",contextID,context[contextID].computationOutput);
            }
            unsigned long poolFinishTime = GetTickCountMicrosecondsT();
            fprintf(stdout,"Pool of %u threads finished round %u of work in %lu μsec..!\n",pool.numberOfThreads, iterationID, poolFinishTime - poolStartTime);
            fprintf(stdout,"----------------------------------------------------------------------------\n\n");
        }
    }

    fprintf(stdout,"Releasing our thread pool..!\n");
    threadpoolDestroy(&pool);

    free(context);
    context=0;
    }

    fprintf(stdout,"Done with everything..!\n");
}
