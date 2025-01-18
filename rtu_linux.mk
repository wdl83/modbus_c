include linux/Makefile.defs

TARGET = rtu_linux

CFLAGS += \
	-DDEBUG_RTU_MEMORY \
	-DRTU_MEMORY_ADDR=0x1000 \
	-DRTU_MEMORY_SIZE=1024 \
	-DTLOG_SIZE=4096 \
	-DTTY_ASYNC_LOW_LATENCY \
	-I . \
	-I linux \
	-Wfatal-errors

LDFLAGS += -lrt -lpthread

CSRCS = \
	linux/crc.c \
	linux/gnu.c \
	linux/log.c \
	linux/main.c \
	linux/rtu_impl.c \
	linux/rtu_log_impl.c \
	linux/time_util.c \
	linux/tty.c \
	linux/util.c \
	rtu.c \
	rtu_memory.c

include linux/Makefile.rules
