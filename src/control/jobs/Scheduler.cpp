#include "Scheduler.h"
#include <stdio.h>
#include "../../cfg.h"


#ifdef SHEDULER_DEBUG
#define SDEBUG(msg, ...) printf("Scheduler:: " msg, __VA_ARGS__)
#else
#define SDEBUG(msg, ...) {}
#endif


Scheduler::Scheduler() {
	// Thread
	this->threadRunning = true;
	this->jobQueueCond = g_cond_new();
	this->jobQueueMutex = g_mutex_new();
	this->jobRunningMutex= g_mutex_new();

	// Queue
	GQueue init = G_QUEUE_INIT;
	this->queueUrgent = init;
	this->queueHigh = init;
	this->queueLow = init;
	this->queueNone = init;
	this->jobQueue[JOB_PRIORITY_URGENT] = &this->queueUrgent;
	this->jobQueue[JOB_PRIORITY_HIGH] = &this->queueHigh;
	this->jobQueue[JOB_PRIORITY_LOW] = &this->queueLow;
	this->jobQueue[JOB_PRIORITY_NONE] = &this->queueNone;

	this->thread = g_thread_create((GThreadFunc)jobThreadCallback, this, true, NULL);
}

Scheduler::~Scheduler() {
	this->threadRunning = false;
	g_cond_broadcast(this->jobQueueCond);
	g_thread_join(this->thread);
	g_mutex_free(this->jobQueueMutex);
	g_mutex_free(this->jobRunningMutex);
	g_cond_free(this->jobQueueCond);
}

void Scheduler::addJob(Job * job, JobPriority priority) {
	g_mutex_lock(this->jobQueueMutex);

	g_queue_push_tail(this->jobQueue[priority], job);
	g_cond_broadcast(this->jobQueueCond);

	SDEBUG("add job: %ld\n", (long)job);

	g_mutex_unlock(this->jobQueueMutex);
}

Job * Scheduler::getNextJobUnlocked() {
	Job * job = NULL;

	for (int i = JOB_PRIORITY_URGENT; i < JOB_N_PRIORITIES; i++) {
		job = (Job *) g_queue_pop_head(this->jobQueue[i]);
		if (job) {
			break;
		}
	}

	return job;
}

gpointer Scheduler::jobThreadCallback(Scheduler * scheduler) {
	while (scheduler->threadRunning) {
		g_mutex_lock(scheduler->jobQueueMutex);
		Job * job = scheduler->getNextJobUnlocked();
		SDEBUG("get job: %ld\n", (long)job);
		if (!job) {
			g_cond_wait(scheduler->jobQueueCond, scheduler->jobQueueMutex);
			g_mutex_unlock(scheduler->jobQueueMutex);
			continue;
		}

		SDEBUG("do job: %ld\n", (long)job);

		g_mutex_unlock(scheduler->jobQueueMutex);

		g_mutex_lock(scheduler->jobRunningMutex);

		CHECK_MEMORY(job);

		job->execute();

		job->free();
		g_mutex_unlock(scheduler->jobRunningMutex);

		SDEBUG("next\n", NULL);
	}

	SDEBUG("finished\n", NULL);

	return NULL;
}
