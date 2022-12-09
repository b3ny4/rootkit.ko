KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := rootkit.o

rootkit.ko: rootkit.c
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean