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

gen_cov_info: test
	lcov -c --list-full-path --directory $(OBJ_DIR) -output-file $(OBJ_DIR)/cov.info

gen_cov_report_html: gen_cov_info
	genhtml $(OBJ_DIR)/cov.info --output-directory $(OBJ_DIR)/cov_report_html

purge:
	-rm $(OBJ_DIR) -rf
