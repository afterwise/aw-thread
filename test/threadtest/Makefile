
export CFLAGS += -std=c99 -Wall -Wextra

ifeq ($(shell uname -s),Linux)
export CFLAGS += -pthread
endif

ifeq ($(shell uname -s),Linux)
export LDFLAGS += -pthread
endif

test: test.o ../../libaw-thread.a
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.x
	$(CC) $(CFLAGS) -I../.. -xc -c $< -o $@

../../libaw-thread.a:
	$(MAKE) -C../..

.PHONY: clean
clean:
	rm -f test test.o

