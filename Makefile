export CROSS_COMPILE=arm-linux-gnueabihf-
export ARCH=arm
export KDIR=../linux-5.10.19/build/

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m := adxl345.o
else
# normal makefile
KDIR ?= /lib/modules/`uname -r`/build

all: module test

module:
	$(MAKE) -C $(KDIR) M=$$PWD

test:
	arm-linux-gnueabihf-gcc -Wall --static -o main main.c

clean:
	git clean -f
endif
