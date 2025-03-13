
/*
   Copyright (c) 2014-2025 Malte Hildingsson, malte (at) afterwi.se

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

#ifndef _thread_nofeatures
# if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN 1
# elif defined(__linux__)
#  define _BSD_SOURCE 1
#  define _GNU_SOURCE 1
#  define _DEFAULT_SOURCE 1
#  define _POSIX_C_SOURCE 200809L
#  define _SVID_SOURCE 1
# elif defined(__APPLE__)
#  define _DARWIN_C_SOURCE 1
# endif
#endif /* _thread_nofeatures */

#include "aw-thread.h"

#if defined(_WIN32)
# include <windows.h>
#elif defined(__CELLOS_LV2__)
# include <sys/ppu_thread.h>
# include <sys/synchronization.h>
#elif defined(__APPLE__)
# include <mach/mach_init.h>
# include <mach/thread_act.h>
# include <mach/thread_policy.h>
# include <mach/semaphore.h>
# include <mach/task.h>
#elif defined(__linux__)
# include <semaphore.h>
# endif

#if defined(__APPLE__) || defined(__linux__)
# include <errno.h>
# include <pthread.h>
# include <sched.h>
# include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#if defined(__APPLE__)
/* from mach/thread_policy.h */
kern_return_t thread_policy_set(
	thread_t thread, thread_policy_flavor_t flavor,
	thread_policy_t policy_info, mach_msg_type_number_t count);
kern_return_t thread_policy_get(
	thread_t thread, thread_policy_flavor_t flavor,
	thread_policy_t policy_info, mach_msg_type_number_t *count,
	boolean_t *get_default);
#endif

int thread_hardware_concurrency() {
#if defined(_WIN32)
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors;
#elif defined(__CELLOS_LV2__)
	return 2;
#elif defined(__linux__) || defined(__APPLE__)
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

thread_id_t thread_spawn(
		thread_start_t *start, enum thread_priority priority, int affinity,
		size_t stack_size, uintptr_t user_data) {
#if defined(_WIN32)
	HANDLE id = CreateThread(
		NULL, stack_size, (LPTHREAD_START_ROUTINE) start, (void *) user_data,
		CREATE_SUSPENDED, NULL);

	SetThreadPriority(id, 1 - priority);
	if (affinity != THREAD_NO_AFFINITY)
		SetThreadAffinityMask(id, (DWORD_PTR) 1 << affinity);
	ResumeThread(id);

	return (thread_id_t) id;
#elif defined(__CELLOS_LV2__)
	sys_ppu_thread_id_t id;

	sys_ppu_thread_create(
		&id, (void (*)(u64)) start, user_data, (priority + 1) * 1000,
		stack_size, SYS_PPU_THREAD_CREATE_JOINABLE, NULL);

	return id;
#elif defined(__linux__) || defined(__APPLE__)
# if defined(__APPLE__)
	thread_port_t tp;
# endif
	pthread_t id;
	pthread_attr_t attr;
	struct sched_param param;
	int pritab[3];
	int err;

	pritab[0] = sched_get_priority_min(SCHED_FIFO);
	pritab[2] = sched_get_priority_max(SCHED_FIFO);
	pritab[1] = (pritab[0] + pritab[2]) / 2;

	param.sched_priority = pritab[priority];

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, stack_size);
#if defined(__APPLE__)
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
#endif
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	pthread_attr_setschedparam(&attr, &param);
	if (priority == THREAD_HIGH_PRIORITY)
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#if defined(__APPLE__)
	if (affinity != THREAD_NO_AFFINITY || priority == THREAD_HIGH_PRIORITY) {
		if ((err = pthread_create_suspended_np(&id, &attr, (void *(*)(void *)) start, (void *) user_data)) != 0)
			errno = err, perror("pthread_create_suspended_np");
		tp = pthread_mach_thread_np(id);
	} else
#endif
		if ((err = pthread_create(&id, &attr, (void *(*)(void *)) start, (void *) user_data)) != 0)
			errno = err, perror("pthread_create");
	if (affinity != THREAD_NO_AFFINITY) {
#if defined(__linux__)
		cpu_set_t c;
		CPU_ZERO(&c);
		CPU_SET(id, &c);
		pthread_setaffinity_np(id, sizeof c, &c);
#elif defined(__APPLE__)
		thread_affinity_policy_data_t a;
		memset(&a, 0, sizeof a);
		a.affinity_tag = affinity + 1;
		if (thread_policy_set(tp, THREAD_AFFINITY_POLICY, (thread_policy_t) &a,
				THREAD_AFFINITY_POLICY_COUNT) != 0)
			fprintf(stderr, "thread_policy_set: failed\n");
#endif
	}
#if defined(__APPLE__)
	if (priority == THREAD_HIGH_PRIORITY) {
		thread_extended_policy_data_t e;
		memset(&e, 0, sizeof e);
		e.timeshare = 0;
		if (thread_policy_set(tp, THREAD_EXTENDED_POLICY, (thread_policy_t) &e,
				THREAD_EXTENDED_POLICY_COUNT) != 0)
			fprintf(stderr, "thread_policy_set: failed\n");
	}
	if (affinity != THREAD_NO_AFFINITY || priority == THREAD_HIGH_PRIORITY)
		thread_resume(tp);
#endif
	pthread_attr_destroy(&attr);

	return (thread_id_t) id;
#endif
}

#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
#endif

void thread_exit(void) {
#if defined(_WIN32)
	ExitThread(0);
#elif defined(__CELLOS_LV2__)
	sys_ppu_thread_exit(0);
#elif defined(__linux__) || defined(__APPLE__)
	pthread_exit(NULL);
#endif
}

void thread_join(thread_id_t id) {
#if defined(_WIN32)
	WaitForSingleObject((HANDLE) id, INFINITE);
#elif defined(__CELLOS_LV2__)
	unsigned long long res;
	sys_ppu_thread_join(id, &res);
#elif defined(__linux__) || defined(__APPLE__)
	void *res;
	pthread_join((pthread_t) id, &res);
#endif
}

void thread_yield(void) {
#if defined(_WIN32)
	SwitchToThread();
#elif defined(__CELLOS_LV2__)
	sys_ppu_thread_yield();
#elif defined(__linux__) || defined(__APPLE__)
	sched_yield();
#endif
}

sema_id_t sema_create(void) {
#if defined(_WIN32)
	return (sema_id_t) CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
#elif defined(__CELLOS_LV2__)
	sys_sema_id_t id;
	sys_sema_attribute_t attr;
	sys_sema_attribute_initialize(attr);
	sys_sema_create(&id, &attr, 0, 0x7fffffff);
	return id;
#elif defined(__APPLE__)
	semaphore_t sem;
	semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO, 0);
	return sem;
#elif defined(__linux__)
	sem_t *sem = malloc(sizeof (sem_t));
	sem_init(sem, 0, 0);
	return (sema_id_t) sem;
#endif
}

void sema_destroy(sema_id_t id) {
#if defined(_WIN32)
	CloseHandle((HANDLE) id);
#elif defined(__CELLOS_LV2__)
	sys_sema_destroy((sys_sema_id_t) id);
#elif defined(__APPLE__)
	semaphore_destroy(mach_task_self(), id);
#elif defined(__linux__)
	sem_destroy((sem_t *) id);
	free((sem_t *) id);
#endif
}

void sema_acquire(sema_id_t id, unsigned count) {
#if defined(_WIN32)
	unsigned i;

	for (i = 0; i < count; ++i)
		WaitForSingleObject((HANDLE) id, INFINITE);
#elif defined(__CELLOS_LV2__)
	sys_sema_wait((sys_sema_id_t) id, count);
#elif defined(__APPLE__)
	unsigned i;

	for (i = 0; i < count; ++i)
		semaphore_wait(id);
#elif defined(__linux__)
	unsigned i;

	for (i = 0; i < count; ++i)
		if (sem_wait((sem_t *) id) < 0)
			abort();
#endif
}

void sema_release(sema_id_t id, unsigned count) {
#if defined(_WIN32)
	ReleaseSemaphore((HANDLE) id, count, NULL);
#elif defined(__CELLOS_LV2__)
	sys_sema_post((sys_sema_id_t) id, count);
#elif defined(__APPLE__)
	unsigned i;

	for (i = 0; i < count; ++i)
		semaphore_signal(id);
#elif defined(__linux__)
	unsigned i;

	for (i = 0; i < count; ++i)
		sem_post((sem_t *) id);
#endif
}

