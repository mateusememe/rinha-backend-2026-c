CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O3 -ffast-math -march=native -D_GNU_SOURCE -Isrc
LDFLAGS = -lz -lpthread
SANITIZERS = -fsanitize=address,undefined

# On Linux, add -lrt for shm
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -lrt
endif

# Check architecture for AVX2
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
	CFLAGS += -mavx2
endif

SRC_COMMON = src/common/shm.c src/common/utils.c
SRC_FRAUD = src/fraud/knn.c src/fraud/vectorize.c
SRC_API = src/api/http.c src/api/json.c src/api/routes.c

all: bin/fraud-api bin/lb bin/data-loader

bin/fraud-api: src/api/main.c $(SRC_API) $(SRC_COMMON) $(SRC_FRAUD)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/lb: src/lb/main.c src/lb/proxy.c src/lb/upstream.c $(SRC_COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/data-loader: src/loader/main.c src/api/json.c $(SRC_COMMON) $(SRC_FRAUD)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: bin/test_unit
	./bin/test_unit

bin/test_unit: test/unit/test_main.c test/unit/test_shm.c test/unit/test_knn.c test/unit/test_vectorize.c test/unit/test_json.c vendor/unity/unity.c $(SRC_API) $(SRC_COMMON) $(SRC_FRAUD)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SANITIZERS) -Ivendor/unity $^ -o $@ $(LDFLAGS)

clean:
	rm -rf bin obj rinha_shm* *.bin

.PHONY: all test clean
