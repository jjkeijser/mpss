# Copyright (c) 2017, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.

TOPDIR := $(CURDIR)

CARD_DELIVERABLES = apps/systoolsd
HOST_OPEN_DELIVERABLES = apps/micdebug apps/micfw apps/micinfo apps/micsmc-cli miclib apps/miccheck apps/micbios
HOST_DELIVERABLES = \
    $(HOST_OPEN_DELIVERABLES) \

DELIVERABLES = $(CARD_DELIVERABLES) $(HOST_DELIVERABLES)
LIBS = common-framework sdk
COMPONENTS = $(LIBS) $(DELIVERABLES)
UNIT_TESTS = \
    $(CARD_DELIVERABLES) \
    $(HOST_OPEN_DELIVERABLES) \

all: $(addsuffix _all, $(HOST_DELIVERABLES))
open: $(addsuffix _all, $(HOST_OPEN_DELIVERABLES))

install: $(addsuffix _install, $(HOST_DELIVERABLES))
open_install: $(addsuffix _install, $(HOST_OPEN_DELIVERABLES))
open_install_ut: $(addsuffix _install_ut, $(HOST_OPEN_DELIVERABLES) apps/systoolsd)

test: $(addsuffix _test, $(UNIT_TESTS))

leakchk: $(addsuffix _leakchk, $(UNIT_TESTS))

clean: $(addsuffix _clean, $(COMPONENTS)) \

sdk_all: common-framework_all
sdk_test: common-framework_test

$(addsuffix _all, $(DELIVERABLES)): sdk_all
$(addsuffix _test, $(DELIVERABLES)): sdk_test

SUBMAKE = +$(MAKE) TOPDIR=$(TOPDIR) -C $*

%_all:
	$(SUBMAKE) all

%_install: %_all
	$(SUBMAKE) install

%_install_ut: %_all
	$(SUBMAKE) install_ut

%_test:
	echo "Unit tests have been disabled"
	#$(SUBMAKE) test

%_leakchk: %_all
	$(SUBMAKE) leakchk

%_clean:
	$(SUBMAKE) clean

.PHONY: all install test leakchk clean \
        open open_install open_install_ut \

