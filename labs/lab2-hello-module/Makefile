ifneq ($(KERNELRELEASE),)
obj-m := hello_version.o
else
KDIR := $(HOME)/jetson-dev/Linux_for_Tegra/source/kernel/kernel-jammy-src

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD)

clean:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
endif
