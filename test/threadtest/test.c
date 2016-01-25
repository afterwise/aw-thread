
#include "aw-atomic.h"
#include "aw-thread.h"
#include <stdio.h>

struct tdata {
	sema_id_t sema;
	const char *str;
};

static atomic_once_t once;

void tmain(uintptr_t data) {
	struct tdata *tdata = (struct tdata *) data;
	int uno = 0;

	if (atomic_once_init(&once)) {
		uno = 1;
		atomic_once_end(&once);
	}

	sema_acquire(tdata->sema, 1);
	printf("tmain: %s uno=%d\n", tdata->str, uno);

	thread_exit();
}

int main(int argc, char *argv[]) {
	thread_id_t a, b;
	sema_id_t s;
	struct tdata x, y;

	(void) argc;
	(void) argv;

	s = sema_create();

	x.sema = s;
	x.str = "thread #a";

	y.sema = s;
	y.str = "thread #b";

	a = thread_spawn(&tmain, THREAD_LOW_PRIORITY, THREAD_NO_AFFINITY, 8192, (uintptr_t) &x);
	b = thread_spawn(&tmain, THREAD_HIGH_PRIORITY, 1, 8192, (uintptr_t) &y);

	sema_release(s, 2);

	thread_join(b);
	thread_join(a);

	sema_destroy(s);

	return 0;
}

