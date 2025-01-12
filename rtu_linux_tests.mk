include linux/Makefile.defs

TARGET = rtu_linux_tests

CFLAGS += \
	-DRTU_MEMORY_ADDR=0x1000 \
	-DRTU_MEMORY_SIZE=1024 \
	-DTLOG_SIZE=4096 \
	-DTTY_ASYNC_LOW_LATENCY \
	-I . \
	-I linux \
	-I utest \
	-Wfatal-errors

LDFLAGS += -lrt -lpthread

CSRCS = \
	linux/buf.c \
	linux/crc.c \
	linux/gnu.c \
	linux/log.c \
	linux/master_impl.c \
	linux/pipe.c \
	linux/rtu_impl.c \
	linux/rtu_log_impl.c \
	linux/tests/rtu_tests.c \
	linux/time_util.c \
	linux/tty.c \
	linux/tty_pair.c \
	linux/util.c \
	master.c \
	rtu.c \
	rtu_memory.c

include linux/Makefile.rules
