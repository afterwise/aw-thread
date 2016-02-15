
#include "aw-atomic.h"
#include "aw-thread.h"
#include <stdio.h>

struct tdata {
	sema_id_t sema;
	char *str;
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
	(void) argc;
	(void) argv;

	sema_id_t s = sema_create();
	int n = thread_hardware_concurrency();
	struct tdata x[n];

	for (int i = 0; i < n; ++i) {
		x[i].sema = s;
		x[i].str = malloc(64);
		snprintf(x[i].str, 64, "thread#%c", 'A' + i);
	}

	thread_id_t y[n];
	for (int i = 0; i < n; ++i)
		y[i] = thread_spawn(&tmain, THREAD_LOW_PRIORITY, THREAD_NO_AFFINITY, 8192, (uintptr_t) &x[i]);

	sema_release(s, n);

	for (int i = 0; i < n; ++i)
		thread_join(y[i]);

	sema_destroy(s);

	printf("OK\n");
	return 0;
}

