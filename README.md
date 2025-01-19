Modbus RTU library for uC
=========================

Cloning
-------

```console
git clone --recurse-submodules https://github.com/wdl83/modbus_c
```

Supported targets
-----------------

1. ATmega328p, slave only
1. STM8S003F3, slave only
1. STM32F103C8, slave only
1. Linux, slave and master

All targets use same implementation:
    - [state machine](https://github.com/wdl83/modbus_c/blob/master/rtu.c)
    - [memory](https://github.com/wdl83/modbus_c/blob/master/memory.c)

Linux RTU
---------

Utilizes linux ***tty*** device as serial I/O. Allows validation and quick
prototyping.

```console
make -f rtu_linux.mk
```

ATMega328p RTU
--------------

```console
make -f rtu_atmega328p.mk
```

Testing
-------

Current state of RTU [tests](https://github.com/wdl83/modbus_c/blob/master/linux/tests/rtu_tests.c)

Building
```console
make -f rtu_linux_tests.mk
```

Running
```console
make -f rtu_linux_tests.mk run
```

Tests can be executed with different targets

1. default, software RTU (linux RTU running in separate thread)
    ```console
    ./obj/rtu_linux_tests
    ```
1. HW loopback. Use 2 USB to SERIAL converters with crossed RX/TX pins
    ```console
    ./obj/rtu_linux -d /dev/ttyUSB1 -a 32 -t 10000 -T 20000 -D 1024 -p E
    ./obj/rtu_linux_tests -d /dev/ttyUSB0 -a 32 -t 10000 -T 20000 -p E
    ```
1. HW target. ATmega328p  with obj_atmega328p/rtu_atmega328p.hex connected to PC
    ```console
    ./obj/rtu_linux_tests -d /dev/ttyUSB0 -a 15 -p E
    ```

Same set of tests work with all targets listed above.

Coverage report
---------------

[report](https://wdl83.github.io/modbus_c)
