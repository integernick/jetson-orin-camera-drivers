ifneq ($(KERNELRELEASE),)
obj-m := nunchuck.o
else
KDIR := $(HOME)/jetson-dev/Linux_for_Tegra/source/kernel/kernel-jammy-src
CROSS_COMPILE := ${HOME}/jetson-toolchain/aarch64--glibc--stable-2022.08-1/bin/aarch64-buildroot-linux-gnu-

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD)

clean:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
endif

