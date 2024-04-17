OUTPUT_NAME = waitqueue
BUILD_DIR = /home/dung/workdir/linux-kernel-pi
OUTPUT_DIR = ../output
ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-

HOSTNAME = vanperdung
IP ?= 192.168.1.10
INSTALL_DIR = ~/workdir/modules/.

APP_NAME = waitqueue_app

obj-m += waitqueue.o

all: modules app

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(BUILD_DIR) M=$(shell pwd) $@
# cp $(OUTPUT_NAME).ko $(OUTPUT_DIR)


clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(BUILD_DIR) M=$(shell pwd) $@
# if [ -f $(OUTPUT_DIR)/$(OUTPUT_NAME).ko ]; then \
# rm $(OUTPUT_DIR)/$(OUTPUT_NAME).ko; \
# fi
	rm $(APP_NAME)

install:
	if [ -f $(OUTPUT_NAME).ko ]; then \
	scp $(OUTPUT_NAME).ko $(HOSTNAME)@$(IP):$(INSTALL_DIR); \
	fi
	if [ -f $(APP_NAME) ]; then \
	scp $(APP_NAME) $(HOSTNAME)@$(IP):$(INSTALL_DIR); \
	fi
app:
	aarch64-linux-gnu-gcc $(APP_NAME).c -o $(APP_NAME) 

