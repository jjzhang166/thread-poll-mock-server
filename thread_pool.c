//
//  thread_pool.c
//  thread_pool
//
//  Created by Mehmet İNAÇ on 02/05/2017.
//  Copyright © 2017 Mehmet İNAÇ. All rights reserved.
//

#include "thread_pool.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0
#define MAX_THREAD_COUNT 32

typedef struct bsem {
    pthread_mutex_t mutex;
    pthread_cond_t   cond;
    int v;
} bsem;

typedef struct job{
    struct job*  next;
    void   (*function)(void* arg);
    void*  arg;
} job;

typedef struct jobqueue{
    pthread_mutex_t rwmutex;
    job  *first;
    job  *last;
    bsem *has_jobs;
} jobqueue;

typedef struct thread{
    int       id;
    pthread_t pthread;
    struct thread_pool* thread_pool;
} thread;

struct thread_pool {
    thread** threads;
    jobqueue jobqueue;
    pthread_mutex_t thread_count_lock;
    int alive_threads_count;
    int stay_awake;
};

static void bsem_post(bsem *semaphore) {
    pthread_mutex_lock(&semaphore->mutex);
    semaphore->v = 1;
    pthread_cond_signal(&semaphore->cond);
    pthread_mutex_unlock(&semaphore->mutex);
}

static void bsem_wait(struct bsem* semaphore) {
    pthread_mutex_lock(&semaphore->mutex);
    while (semaphore->v != 1) {
        pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
    }
    semaphore->v = 0;
    pthread_mutex_unlock(&semaphore->mutex);
}

struct job* fetch_job(struct jobqueue* queue) {
    pthread_mutex_lock(&(queue->rwmutex));
    job* first_job = queue->first;
    
    if(first_job == NULL) {
        pthread_mutex_unlock(&(queue->rwmutex));
        return NULL;
    }
    
    queue->first = first_job->next;
    
    if(queue->first == NULL) {
        queue->last = NULL;
    } else {
        bsem_post(queue->has_jobs);
    }
    pthread_mutex_unlock(&(queue->rwmutex));
    
    return first_job;
}

void add_job(struct thread_pool* pool, job* new_job) {
    jobqueue* queue = &pool->jobqueue;
    
    pthread_mutex_lock(&(queue->rwmutex));
    
    // Which means the queue is empty at all
    if(queue->last == NULL) {
        queue->first = new_job;
        queue->last  = new_job;
    } else {
        job* previous_last  = queue->last;
        previous_last->next = new_job;
        queue->last = new_job;
    }
    
    pthread_mutex_unlock(&(queue->rwmutex));
    bsem_post(pool->jobqueue.has_jobs);
}

void create_job(struct thread_pool* pool, void (*function_p)(void*), void* arg_p) {
    job* new_job;
    
    new_job = (job*)malloc(sizeof(job));
    
    if(new_job == NULL) {
        printf("Could not allocate memory for new job!");
        return;
    }
    
    new_job->function = function_p;
    new_job->arg = arg_p;
    new_job->next = NULL;
    
    add_job(pool, new_job);
}

void schedule_thread(struct thread* thread) {
    struct thread_pool* pool = thread->thread_pool;
    
    pthread_mutex_lock(&(pool->thread_count_lock));
    pool->alive_threads_count += 1;
    pthread_mutex_unlock(&(pool->thread_count_lock));
    
    while(pool->stay_awake) {
        
        bsem_wait(pool->jobqueue.has_jobs);
        
        job* job = fetch_job(&(pool->jobqueue));
        
        if(job != NULL) {
            void (*function)(void*);
            void* arguments;
            
            function  = job->function;
            arguments = job->arg;
            function(arguments);
            free(job);
        }
    }
}

struct thread_pool* init_thread_pool() {
    // init pool
    struct thread_pool* pool = (struct thread_pool*)malloc(sizeof(struct thread_pool));
    
    if(pool == NULL) {
        printf("Could not allocate memory for thread pool!\n");
        return NULL;
    }
    
    pthread_mutex_init(&(pool->thread_count_lock), NULL);
    pool->alive_threads_count = 0;
    pool->stay_awake = TRUE;
    
    // init job queue
    pool->jobqueue.first = NULL;
    pool->jobqueue.last  = NULL;
    pthread_mutex_init(&(pool->jobqueue.rwmutex), NULL);
    
    // init semaphore
    pool->jobqueue.has_jobs = (struct bsem*)malloc(sizeof(struct bsem));
    pthread_mutex_init(&(pool->jobqueue.has_jobs->mutex), NULL);
    pthread_cond_init(&(pool->jobqueue.has_jobs->cond), NULL);
    pool->jobqueue.has_jobs->v = 0;
    
    // init threads
    pool->threads = (struct thread**)malloc(MAX_THREAD_COUNT * sizeof(struct thread*));
    
    if(pool->threads == NULL) {
        printf("Could not allocate memory for threads!\n");
        return NULL;
    }
    
    int i;
    
    for(i = 0; i < MAX_THREAD_COUNT; i++) {
        pool->threads[i] = (struct thread*)malloc(sizeof(struct thread));
        
        if(pool->threads[i] == NULL) {
            printf("Could not allocate memory for %d. thread!\n", i);
            exit(0);
        }
        
        pool->threads[i]->id = i;
        pool->threads[i]->thread_pool = pool;
        
        pthread_create(&pool->threads[i]->pthread, NULL, (void *)schedule_thread, pool->threads[i]);
        pthread_detach(pool->threads[i]->pthread);
    }
    
    while(pool->alive_threads_count != MAX_THREAD_COUNT) {}
    
    return pool;
}
