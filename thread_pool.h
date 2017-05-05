//
//  thread_pool.h
//  thread_pool
//
//  Created by Mehmet İNAÇ on 02/05/2017.
//  Copyright © 2017 Mehmet İNAÇ. All rights reserved.
//

#ifndef thread_pool_h
#define thread_pool_h

typedef struct thread_pool pool;
typedef struct jobqueue queue;
typedef struct thread thread;

struct job* fetch_job(struct jobqueue* queue);

void add_job(struct thread_pool* pool, struct job* new_job);

void create_job(struct thread_pool* pool, void (*function_p)(void*), void* arg_p);

void schedule_thread(struct thread* thread);

struct thread_pool* init_thread_pool();

#endif /* thread_pool_h */
