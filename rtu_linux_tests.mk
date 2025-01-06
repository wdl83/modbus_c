include linux/Makefile.defs

TARGET = rtu_linux_tests

CFLAGS += \
	-DRTU_ADDR_BASE=0x1000 \
	-DTLOG_SIZE=4096 \
	-DTTY_ASYNC_LOW_LATENCY \
	-I . \
	-I linux \
	-I utest \
	-Wfatal-errors

LDFLAGS += -lrt -lpthread

CSRCS = \
	linux/buf.c \
	linux/gnu.c \
	linux/log.c \
	linux/tests/tty_dev_tests.c \
	linux/time_util.c \
	linux/tty.c \
	linux/tty_pair.c \
	linux/util.c

include linux/Makefile.rules
