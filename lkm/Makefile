LINUX_DIR=linux-5.13.1
LINUX_DIR=/lib/modules/$(shell uname -r)/build
obj-m += explorer.o
explorer-objs += explorer_main.o path.o pmc.o

all: explorer.ko testclient

explorer.ko: explorer_main.c path.c pmc.c
	make -C $(LINUX_DIR) M=$(PWD) modules

testclient: testclient/testclient.c
	make -C testclient

.PHONY: clean
clean:
	make -C $(LINUX_DIR) M=$(PWD) clean
	make -C testclient