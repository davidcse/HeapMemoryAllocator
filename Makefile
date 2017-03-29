CC = gcc
CFLAGS = -Wall -Werror -DCOLOR -g 
BIN = sfmmtest
LIB = libsfutil.a
LIBS = lib/
SRC_DIR = src/
INCLUDE = include/

TESTSRC = sfmmtest.c
SRC = $(wildcard $(SRC_DIR)*.c)
SRC_OBJS = $(patsubst %.c, %.o, $(SRC))
OBJS = $(wildcard $(LIBS)*.o)

all: $(BIN)

$(BIN): $(TESTSRC) $(LIB)
	$(CC) -I $(INCLUDE) $(CFLAGS) $^ -o $@

$(LIB): $(SRC_OBJS)
	ar -cr $@ $(OBJS)

$(SRC_OBJS): $(SRC)
	$(CC) -I $(INCLUDE) $(CFLAGS) -c $^
	mv *.o $(LIBS)

.PHONY: clean
clean:
	rm -f $(LIBS)sfmm.o $(LIB)
	rm -f $(BIN) *.o
