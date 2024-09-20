
#include "aw-atomic.h"
#include <assert.h>
#include <stdio.h>

struct queue_test : rl::test_suite<queue_test, 2> {
	struct atomic_ring ring;
	char buf[32];

	void before() {
		atomic_ring_init(&ring, buf, sizeof buf);
	}

	void thread(unsigned thread_index) {
		struct __attribute__((packed)) weird { int i; char c; } x = {1}, y;
		const int count = 50;
		if (thread_index == 0)
			while (x.i <= count) {
				while (!atomic_enqueue(&ring, &x, sizeof x))
					sched_yield();
				++x.i;
			}
		else
			while (y.i != count) {
				while (!atomic_dequeue(&ring, &y, sizeof y))
					sched_yield();
				RL_ASSERT(memcmp(&x, &y, sizeof x) == 0);
				++x.i;
			}
	}

	void invariant() {
	}

	void after() {
	}
};

struct stream_test : rl::test_suite<stream_test, 2> {
	struct atomic_ring ring;
	char buf[32];

	void before() {
		atomic_ring_init(&ring, buf, sizeof buf);
	}

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
		return memchr(tmp, '>', len) == NULL;
	}

	void thread(unsigned thread_index) {
		const int count = 300;
		if (thread_index == 0) {
			for (int i = 0; i < count; ++i)
				write("<%d", i);
			write(">", 0);
		} else
			while (read())
				;
	}

	void invariant() {
	}

	void after() {
	}
};

int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	rl::test_params p;
	p.iteration_count = 10000;
	rl::simulate<queue_test>(p);
	rl::simulate<stream_test>(p);

	return 0;
}

