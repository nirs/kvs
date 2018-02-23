src = kvs.c db.c

CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -Werror -std=c99
LDLIBS = -llmdb

obj = $(src:.c=.o)
dep = $(src:.c=.d)

.PHONY: clean

kvs: $(obj)
	$(LINK.o) $^ $(LDLIBS) -o $@

mmap: mmap.c
	$(LINK.c) $^ -pthread -o $@

clean:
	rm -f kvs mmap *.[do]

%.o: %.c %.d
	$(COMPILE.c) $< -MMD -MF $*.d -o $@

%.d: ;

-include $(dep)
