
#include <stddef.h>
#include <string.h>

/* The ring's bytes live in relaxed atomics, so Relacy can hand a thread
   a stale byte unless the ring's own fences order the accesses. */

static char *ring_base;
static rl::atomic<char> *ring_cells;
static size_t ring_size;

static void *ring_memcpy(void *dst, const void *src, size_t n) {
	char *d = (char *) dst;
	const char *s = (const char *) src;
	for (size_t i = 0; i < n; ++i) {
		char c;
		if (s + i >= ring_base && s + i < ring_base + ring_size)
			c = ring_cells[s + i - ring_base].load(rl::memory_order_relaxed);
		else
			c = s[i];
		if (d + i >= ring_base && d + i < ring_base + ring_size)
			ring_cells[d + i - ring_base].store(c, rl::memory_order_relaxed);
		else
			d[i] = c;
	}
	return dst;
}

#define _atomic_memcpy ring_memcpy
#include "aw-atomic.h"
#include <assert.h>
#include <stdio.h>

template <class T>
struct ring_suite : rl::test_suite<T, 2> {
	struct atomic_ring ring;
	char buf[32];
	rl::atomic<char> cells[32];

	void before() {
		ring_base = buf;
		ring_cells = cells;
		ring_size = sizeof buf;
		for (size_t i = 0; i < sizeof buf; ++i)
			cells[i].store(0, rl::memory_order_relaxed);
		atomic_ring_init(&ring, buf, sizeof buf);
	}

	void invariant() {
	}

	void after() {
	}
};

struct queue_test : ring_suite<queue_test> {
	void thread(unsigned thread_index) {
		struct __attribute__((packed)) weird { int i; char c; } x = {1, 1}, y = {0, 0};
		const int count = 50;
		if (thread_index == 0)
			while (x.i <= count) {
				while (!atomic_enqueue(&ring, &x, sizeof x))
					sched_yield();
				++x.i;
				x.c = (char) -x.i;
			}
		else
			while (y.i != count) {
				while (!atomic_dequeue(&ring, &y, sizeof y))
					sched_yield();
				RL_ASSERT(memcmp(&x, &y, sizeof x) == 0);
				++x.i;
				x.c = (char) -x.i;
			}
	}
};

struct stream_test : ring_suite<stream_test> {
	char expect[2048];
	size_t expect_len, got_len;

	void write(const char *fmt, int i) {
		char tmp[16];
		size_t err, off = 0;
		size_t len = snprintf(tmp, sizeof tmp, fmt, i);
		for (;;) {
			err = atomic_write(&ring, tmp + off, len - off);
			if ((off += err) == len)
				break;
			sched_yield();
		}
	}

	bool read() {
		char tmp[16];
		size_t len;
		while ((len = atomic_read(&ring, tmp, sizeof tmp)) == 0)
			sched_yield();
		RL_ASSERT(got_len + len <= expect_len);
		RL_ASSERT(memcmp(tmp, expect + got_len, len) == 0);
		got_len += len;
		return memchr(tmp, '>', len) == NULL;
	}

	void thread(unsigned thread_index) {
		const int count = 100;
		if (thread_index == 0) {
			for (int i = 0; i < count; ++i)
				write("<%d", i);
			write(">", 0);
		} else {
			expect_len = got_len = 0;
			for (int i = 0; i < count; ++i)
				expect_len += snprintf(expect + expect_len, sizeof expect - expect_len, "<%d", i);
			expect[expect_len++] = '>';
			while (read())
				;
			RL_ASSERT(got_len == expect_len);
		}
	}
};

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	rl::test_params p;
	p.iteration_count = 10000;
	if (!rl::simulate<queue_test>(p))
		return 1;
	if (!rl::simulate<stream_test>(p))
		return 1;

	return 0;
}

