
export CFLAGS += -Wall -Wextra

test: test.o ../../libaw-thread.a
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I../.. -c $< -o $@

../../libaw-thread.a:
	$(MAKE) -C../..

.PHONY: clean
clean:
	rm -f test test.o

