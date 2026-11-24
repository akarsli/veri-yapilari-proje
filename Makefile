# Kent İçi Rota ve Hat Planlayıcı — derleme
#   make            -> bin/rota
#   make test       -> birim testleri
#   make sanitize   -> testleri AddressSanitizer + UBSan ile çalıştır
#   make bench      -> performans ölçümleri (results/*.csv); make bench-quick kısa sürüm
#   make clean

CC       = cc
CPPFLAGS = -Iinclude -MMD -MP
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS  =
LDLIBS   = -lm

LIB_SRC = src/common.c src/text.c src/stack.c src/queue.c src/heap.c \
          src/hash.c src/trie.c src/sort.c src/graph.c src/io_csv.c src/route.c \
          src/history.c src/gen.c
LIB_OBJ = $(LIB_SRC:src/%.c=build/%.o)

.PHONY: all test sanitize clean bench bench-quick
.SECONDARY:

all: bin/rota

TESTS = bin/test_ds bin/test_route

bin/rota: $(LIB_OBJ) build/main.o | bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

bin/test_%: $(LIB_OBJ) build/test_%.o | bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/test_%.o: tests/test_%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

bin/bench: $(LIB_OBJ) build/bench.o | bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

build/bench.o: bench/bench.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build bin:
	mkdir -p $@

test: $(TESTS)
	./bin/test_ds
	./bin/test_route

bench: bin/bench
	mkdir -p results
	./bin/bench

bench-quick: bin/bench
	mkdir -p results
	./bin/bench --quick

sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	             LDFLAGS="-fsanitize=address,undefined"

clean:
	rm -rf build bin

-include $(LIB_OBJ:.o=.d) build/main.d build/test_ds.d build/test_route.d build/bench.d
