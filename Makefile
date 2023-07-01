CC = gcc
CFLAGS = -Wall -Wextra -fopenmp -lz -g -O3
TEST_DIR = tests

BIN_DIR = bin

_TEST_OBJ = test_all.o test_gzread.o test_sequential_byte.o testtools.o
TEST_OBJ = $(patsubst %, $(TEST_DIR)/%, $(_TEST_OBJ))

all: $(BIN_DIR)/test

.PHONY: clean

clean:
	rm -f *.o $(TEST_DIR)/*.o $(BIN_DIR)/test *~ core

$(BIN_DIR)/test: $(TEST_OBJ) mgz.o gz64.o
	$(CC) -o $@ $^ $(CFLAGS)

$(TEST_DIR)/test_all.o: $(TEST_DIR)/test_all.c $(TEST_DIR)/test_all.h $(TEST_DIR)/test_gzread.h $(TEST_DIR)/test_sequential_byte.h $(TEST_DIR)/testtools.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(TEST_DIR)/test_gzread.o: $(TEST_DIR)/test_gzread.c $(TEST_DIR)/test_gzread.h $(TEST_DIR)/testtools.h

$(TEST_DIR)/test_sequential_byte.o: $(TEST_DIR)/test_sequential_byte.c $(TEST_DIR)/test_sequential_byte.h $(TEST_DIR)/testtools.h

$(TEST_DIR)/testtools.o: $(TEST_DIR)/testtools.c $(TEST_DIR)/testtools.h

mgz.o: mgz.c mgz.h gz64.h
	$(CC) -c -o $@ $< $(CFLAGS)

gz64.o: gz64.c gz64.h
	$(CC) -c -o $@ $< $(CFLAGS)
