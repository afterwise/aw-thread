
/*
   Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se

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

#ifndef AW_THREAD_H
#define AW_THREAD_H

#include <stddef.h>

#if __GNUC__
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum thread_priority {
	THREAD_HIGH_PRIORITY = 0,
	THREAD_NORMAL_PRIORITY = 1,
	THREAD_LOW_PRIORITY = 2
};

#define THREAD_NO_AFFINITY (-1)

#if __CELLOS_LV2__
typedef unsigned long long thread_id_t;
#else
typedef uintptr_t thread_id_t;
#endif

typedef uintptr_t sema_id_t;

typedef void (thread_start_t)(uintptr_t user_data);

int thread_hardware_concurrency();

thread_id_t thread_spawn(
	thread_start_t *start, enum thread_priority priority, int affinity,
	size_t stack_size, uintptr_t user_data);

void thread_exit(void);
void thread_join(thread_id_t id);

void thread_yield(void);

sema_id_t sema_create(void);
void sema_destroy(sema_id_t id);

void sema_acquire(sema_id_t id, unsigned count);
void sema_release(sema_id_t id, unsigned count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_THREAD_H */

