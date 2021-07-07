#
# Build RAS drivers
#
# In Linux 2.6 kernels modules must be built by the kernel's kbuild
# system, with a path to the kernel module source directory. Kbuild
# expects a general purpose Makefile to exist and optionally an extra
# file named Kbuild with the kernel module build details.
# This Makefile is a 'backwards compatible' (see file "modules.txt").
#
DEBUG = n

ifneq ($(KERNELRELEASE),)

#
# Kbuild backwards compatibility part:
# Load Kbuild to specify module targets and options.
#
include Kbuild

else

#
# Standard invocation:
#
# Export variables to environment and pass control to kernel tools
#  ARCH       Target architecture: l1om or k1om
#  KERNELDIR  Top of MIC kernel tree (not repo source tree)
#  DRIVERDIR  Top of MPSS drivers build tree (not repo source tree)
#

ARCH := $(or $(ARCH), $(shell cat $(CURDIR)/../.arch 2>/dev/null))

ifeq ($(DRIVERDIR),)
ifeq ($(shell /usr/bin/test -d ../source-root/$(ARCH)-hybrid && echo Y),Y)
DRIVERDIR = $(PWD)/../source-root/$(ARCH)-hybrid
KERNELDIR ?= $(DRIVERDIR)/card/kernel
else ifeq ($(shell /usr/bin/test -d ../source-root/$(ARCH)-internal && echo Y),Y)
DRIVERDIR = $(PWD)/../source-root/$(ARCH)-internal
KERNELDIR ?= $(DRIVERDIR)/card/kernel
endif
endif
KERNELDIR ?= ../../miclinux

SCIF_SYM = $(DRIVERDIR)/card/driver/Module.symvers
SCIF_LIB = $(DRIVERDIR)/host/scif_lib
SCIF_HEADER = $(DRIVERDIR)/include

EXTRA_CFLAGS += $(KERNWARNFLAGS)
ifeq ($(ARCH),l1om)
  EXTRA_CFLAGS += -DMIC_IS_L1OM
else ifeq ($(ARCH),k1om)
  EXTRA_CFLAGS += -DMIC_IS_K1OM
else
  $(error $$(ARCH) must be l1om or k1om)
endif
EXTRA_CFLAGS += -DINTERNAL_REG=1 -Wall
EXTRA_CFLAGS += $(SPOOKY_MIC_CFLAGS)

CROSS_COMPILE = x86_64-$(ARCH)-linux-

ifeq ($(shell which $(CROSS_COMPILE)gcc 2>/dev/null),)
  ifeq ($(shell which ../cross/bin/$(CROSS_COMPILE)gcc 2>/dev/null),)
    $(error $$(PATH) must include $(CROSS_COMPILE)gcc)
  else
    CROSS_COMPILE = $(PWD)/../cross/bin/x86_64-$(ARCH)-linux-
  endif
endif

default: modules tests

modules:
	@ echo "$(MAKE) -C $(KERNELDIR) M=$(CURDIR) ARCH=$(ARCH) modules"
	@ $(MAKE) -C $(KERNELDIR) M=$(CURDIR) ARCH=$(ARCH) \
		V=0 DEBUG=$(DEBUG) \
		SPOOKY_MIC_CFLAGS=$(SPOOKY_MIC_CFLAGS) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		KBUILD_EXTRA_SYMBOLS=$(SCIF_SYM) \
		modules

install: modules_install

modules_install:
	@ echo "$(MAKE) -C $(KERNELDIR) M=$(CURDIR) ARCH=$(ARCH) install"
	@ $(MAKE) -C $(KERNELDIR) M=$(CURDIR) ARCH=$(ARCH) \
		V=0 DEBUG=$(DEBUG) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		SPOOKY_MIC_CFLAGS=$(SPOOKY_MIC_CFLAGS) \
		KBUILD_EXTRA_SYMBOLS=$(SCIF_SYM) \
		INSTALL_MOD_PATH=$(DESTDIR) \
		modules_install

#
# Test programs, expects that compilers and SCIF libraries are present.
#
host-tools = edecode gdecode
host-tests = cp mc ttl tmp cutl proc ukill fan smc fsc pm trbo ptrig cp32 p-in-host p-out-host
card-tests = p-in-card p-out-card suid load

tests: $(host-tools) $(host-tests) $(card-tests)

cp:	cp.c micras_api.h
	@ echo gcc -O2 cp.c -o cp -lscif
	@ gcc -O2 cp.c -o cp $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

cp32:	cp32.c micras_api.h
	@ echo gcc -O2 cp32.c -o cp32 -lscif
	@ gcc -O2 cp32.c -o cp32 $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

mc:	mc.c micmca_api.h
	@ echo gcc -O2 mc.c -o mc -lscif
	@ gcc -O2 mc.c -o mc $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

cutl:	cutl.c micras_api.h
	@ echo gcc -O2 cutl.c -o cutl -lscif -lncurses
	@ gcc -O2 cutl.c -o cutl $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif -lncurses

cutl2:	cutl2.c micras_api.h
	@ echo gcc -O2 cutl2.c -o cutl2 -lscif -lncurses
	@ gcc -O2 cutl2.c -o cutl2 $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif -lncurses

proc:	proc.c micras_api.h
	@ echo gcc -O2 proc.c -o proc -lscif -lncurses
	@ gcc -O2 proc.c -o proc $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif -lncurses

trbo:	trbo.c micras_api.h
	@ echo gcc -O2 trbo.c -o trbo -lscif
	@ gcc -O2 trbo.c -o trbo $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

fan:	fan.c micras_api.h
	@ echo gcc -O2 fan.c -o fan -lscif
	@ gcc -O2 fan.c -o fan $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

smc:	smc.c micras_api.h
	@ echo gcc -O2 smc.c -o smc -lscif
	@ gcc -O2 smc.c -o smc $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

pm_tst:	pm_tst.c micras_api.h
	@ echo gcc -O2 pm_tst.c -o pm_tst -lscif
	@ gcc -O2 pm_tst.c -o pm_tst $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

fsc:	fsc.c micras_api.h
	@ echo gcc -O2 fsc.c -o fsc -lscif
	@ gcc -O2 fsc.c -o fsc $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

ptrig: ptrig.c micras_api.h
	@ echo gcc -O2 ptrig.c -o ptrig -lscif
	@ gcc -O2 ptrig.c -o ptrig $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

pm:	pm.c micras_api.h micpm_api.h
	@ echo gcc -O2 pm.c -o pm -lscif
	@ gcc -O2 pm.c -o pm $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

tmp:	tmp.c micras_api.h
	@ echo gcc -O2 tmp.c -o tmp -lscif
	@ gcc -O2 tmp.c -o tmp $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

ttl:	ttl.c micras_api.h micpm_api.h
	@ echo gcc -O2 ttl.c -o ttl -lscif
	@ gcc -O2 ttl.c -o ttl $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

ukill:	ukill.c micras_api.h
	@ echo gcc -O2 ukill.c -o ukill -lscif
	@ gcc -O2 ukill.c -o ukill $(EXTRA_CFLAGS) -I$(SCIF_HEADER) -L$(SCIF_LIB) -lscif

edecode: edecode.c
	@ echo gcc -O2 edecode.c -o edecode
	@ gcc -O2 -Wall edecode.c -o edecode

gdecode: gdecode.c
	@ echo gcc -O2 gdecode.c -o gdecode
	@ gcc -O2 -Wall gdecode.c -o gdecode

p-in-host: p-in.c Makefile
	@ echo gcc -O2 p-in.c -o p-in-host
	@ gcc -O2 p-in.c -o p-in-host -DIOK=16

p-out-host: p-out.c Makefile
	@ echo gcc -O2 p-out.c -o p-out-host
	@ gcc -O2 p-out.c -o p-out-host -DIOK=16 -DTXG=64

suid:	suid.c
	@ echo cross-gcc -O2 suid.c -o suid
	@ $(CROSS_COMPILE)gcc -O2 suid.c -o suid

p-in-card: p-in.c Makefile
	@ echo cross-gcc -O2 p-in.c -o p-in-card
	@ $(CROSS_COMPILE)gcc -O2 p-in.c -o p-in-card -DIOK=64

p-out-card: p-out.c Makefile
	@ echo cross-gcc -O2 p-out.c -o p-out-card
	@ $(CROSS_COMPILE)gcc -O2 p-out.c -o p-out-card -DIOK=64 -DTXG=16

load: load.c
	@ echo cross-gcc load.c -o load -pthread -lpthread
	@ $(CROSS_COMPILE)gcc load.c -o load $(EXTRA_CFLAGS) -pthread -lpthread

cpptest: 
	@ echo Dumping compiler defines
	@ echo > nil.c
	@ $(CROSS_COMPILE)gcc -E -dM nil.c | sort
	@ rm nil.c

endif

clean:
	@ echo "  Cleaning .."
	@ rm -fr *.o *~ core .*.sw? .depend .*.cmd *.ko *.mod.c \
		.tmp_versions modules.order Module.symvers
	@ rm -f $(host-tools) $(host-tests) $(card-tests)

