#
# Trace Capture module
#

export ARCH = l1om

KERNELDIR = $(CURDIR)/../../mic_linux
KBUILD := $(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) M=$(CURDIR)

ifneq ($(DESTDIR),)
INSTALL_MOD_PATH = $(DESTDIR)
endif

ifeq ($(shell \which x86_64-$(ARCH)-linux-gcc 2>/dev/null),)
export PATH := $(PATH):$(CURDIR)/../cross/bin
endif

.PHONY: default modules install modules_install clean

default: modules tests

modules:
	+$(KBUILD) $@

install: modules_install

modules_install:
	+$(KBUILD) INSTALL_MOD_PATH=$(DESTDIR) modules_install

clean:
	+$(KBUILD) clean

tests:
	echo no tests
