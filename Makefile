KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := rootkit.o

.PHONY: all clean

all: rootkit.ko configure

rootkit.ko: rootkit.c
	make -C $(KDIR) M=$(PWD) modules

configure: configure.c
	gcc configure.c -o configure

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm configure