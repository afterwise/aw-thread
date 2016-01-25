
# export CFLAGS += -Wall -Wextra
export CFLAGS += -Wno-inline-new-delete -Wno-undefined-bool-conversion -Wno-deprecated-declarations

test: test.o ../../libaw-thread.a
	$(CC) $(LDFLAGS) -lc++ -o $@ $^

%.o: %.cpp relacy
	$(CC) $(CFLAGS) -I../.. -Irelacy -include relacy/relacy.hpp -c $< -o $@

../../libaw-thread.a:
	$(MAKE) -C../..

relacy:
	git clone https://github.com/dvyukov/relacy.git

.PHONY: clean
clean:
	rm -f test test.o

.PHONY: distclean
distclean: clean
	rm -rf relacy

