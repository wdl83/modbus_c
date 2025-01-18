DRV ?= atmega328p/atmega328p_drv
OBJ_DIR ?= obj_atmega328p

include $(DRV)/Makefile.defs

CFLAGS += \
	-DASSERT_DISABLE \
	-DEEPROM_ADDR_RTU_ADDR=0x0 \
	-DRTU_MEMORY_ADDR=0x1000 \
	-DRTU_MEMORY_SIZE=1024 \
	-DTLOG_SIZE=200 \
	-DUSART0_RX_NO_BUFFERING \
	-I . \
	-I atmega328p \
	-I$(DRV)

TARGET = rtu_atmega328p
CSRCS = \
	$(DRV)/drv/tlog.c \
	$(DRV)/drv/tmr0.c \
	$(DRV)/drv/usart0.c \
	$(DRV)/drv/util.c \
	$(DRV)/hw.c \
	atmega328p/crc.c \
	atmega328p/rtu_impl.c \
	atmega328p/rtu_main.c \
	atmega328p/rtu_memory_impl.c \
	rtu.c \
	rtu_memory.c

include $(DRV)/Makefile.rules

clean:
	rm $(OBJ_DIR) -rf
