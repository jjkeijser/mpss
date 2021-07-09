TOPDIR ?= ../../

-include $(TOPDIR)/common.mk

prefix ?= /usr
micbios_builddir = $(TOPDIR)/apps/micbios

micbios_bin_path = $(micbios_builddir)/micbios.py
micbios_modules = $(micbios_builddir)/_micbios
verfile = $(micbios_modules)/version.py

PYTHON_PREFIX = $(DATAROOTDIR)/mpss/micbios/micbios-$(version)

all:
	-
test:
	echo No tests yet

local_test:
	echo No tests yet

install: $(micbios_modules) $(micbios_bin_path)
	$(INSTALL_d) $(DESTDIR)/$(PYTHON_PREFIX)
	cp -r $(micbios_modules) $(DESTDIR)/$(PYTHON_PREFIX)
	$(INSTALL_x) $(micbios_bin_path) $(DESTDIR)/$(PYTHON_PREFIX)
	$(INSTALL_d) $(DESTDIR)/$(BINDIR)
	ln -f -s $(PYTHON_PREFIX)/micbios.py $(DESTDIR)/$(BINDIR)/micbios

.PHONY: all install micbios-binary dirs test
