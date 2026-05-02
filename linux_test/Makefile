CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: reg_test

reg_test: reg_test.c ../linux_driver/systolic_array_regs.h
	$(CC) $(CFLAGS) -I../linux_driver reg_test.c -o reg_test

clean:
	rm -f reg_test