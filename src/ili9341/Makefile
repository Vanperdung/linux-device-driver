OUTPUT_NAME = ili9341
BUILD_DIR = /home/dung/workdir/linux
OUTPUT_DIR = ../output
ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-

HOSTNAME = vanperdung
IP = 192.168.1.10
INSTALL_DIR = ~/workdir/modules/.

obj-m += $(OUTPUT_NAME).o
$(OUTPUT_NAME)-objs := lcd.o tft_ili9341.o

all: modules 

modules:
		$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(BUILD_DIR) M=$(shell pwd) $@

clean:
		$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(BUILD_DIR) M=$(shell pwd) $@

install:
		scp $(OUTPUT_NAME).ko $(HOSTNAME)@$(IP):$(INSTALL_DIR)


