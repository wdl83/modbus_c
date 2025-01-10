OBJ_DIR ?= ${PWD}/obj
export OBJ_DIR

DST_DIR ?= ${PWD}/dst
export DST_DIR

.PHONY: all build test clean purge

all: build test

buid:
	make -f rtu_linux.mk
	make -f rtu_linux_tests.mk
	make -f tty_linux_tests.mk

test: build
	make -f rtu_linux_tests.mk run
	make -f tty_linux_tests.mk run

clean:
	make -f rtu_linux.mk clean
	make -f rtu_linux_tests.mk clean
	make -f tty_linux_tests.mk clean

purge:
	-rm $(OBJ_DIR) -rf
