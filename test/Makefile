
export CFLAGS += -Wall -Wextra

test: test.o ../libaw-thread.a
	clang -o $@ $^

%.o: %.c
	clang $(CFLAGS) -I.. -c $< -o $@

../libaw-thread.a:
	$(MAKE) -C..

.PHONY: clean
clean:
	rm -f test test.o

