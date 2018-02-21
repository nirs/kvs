programs := kvs mmap

all: $(programs)

kvs: kvs.c
	$(CC) -g -o $@ $< -llmdb

mmap: mmap.c
	$(CC) -g -o $@ $< -pthread

.PHONY: clean
clean:
	rm -f $(programs)
