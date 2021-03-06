#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "micoro.h"
#include "mt_utils.h"

#define LOOP 100

struct wait_list {
	struct wait_list *next;
	coro_t coro;
};

static struct wait_list * volatile g_wait_list;
static MICORO_LOCK_T g_lock = MICORO_LOCK_INITVAL;

void* coro_func(void *arg)
{
	struct wait_list wl;

	assert(coro_self(&wl.coro) == 0);
	assert(wl.coro.ctx != NULL);

	MICORO_LOCK(&g_lock);
	wl.next = g_wait_list;
	g_wait_list = &wl;
	MICORO_UNLOCK(&g_lock);

	coro_yield(NULL);

	return NULL;
}

void* worker(void *arg)
{
	while (1) {
		struct wait_list *wl = NULL;

		MICORO_LOCK(&g_lock);
		if (g_wait_list) {
			wl = g_wait_list;
			g_wait_list = g_wait_list->next;
		}
		MICORO_UNLOCK(&g_lock);

		if (wl)
			coro_resume(&wl->coro, NULL);
		else
			usleep(1000);
	}
	return NULL;
}

void* launcher(void *arg)
{
	coro_t coro;

	while (1) {
		int i;
		for (i = 0; i < LOOP; i++) {
			assert(coro_create(&coro, coro_func) == 0);
			coro_resume(&coro, NULL);
		}
		usleep(1000);
	}
	return NULL;
}

int main()
{
	pthread_t lt1, lt2, wt1, wt2;
	struct coro_stat cur_stat, last_stat = {0};

	assert(coro_init(4096*8, 10000) == 0);
	assert(pthread_create(&lt1, NULL, launcher, NULL) == 0);
	assert(pthread_create(&lt2, NULL, launcher, NULL) == 0);
	assert(pthread_create(&wt1, NULL, worker, NULL) == 0);
	assert(pthread_create(&wt2, NULL, worker, NULL) == 0);

	while (1) {
		coro_get_stat(&cur_stat);
		printf("create: %llu\tdestroy: %llu\tresume: %llu\tyield: %llu\tpending: %llu\n",
#define diff_stat(field) ((unsigned long long)(cur_stat.field - last_stat.field))
				diff_stat(create_count), diff_stat(destroy_count),
				diff_stat(resume_count), diff_stat(yield_count),
				(unsigned long long)cur_stat.alive_coro_num);
		last_stat = cur_stat;
		sleep(1);
	}
	return 0;
}
