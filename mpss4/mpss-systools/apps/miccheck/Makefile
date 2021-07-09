TOPDIR ?= ../../

-include $(TOPDIR)/common.mk

prefix ?= /usr
miccheck_builddir = $(TOPDIR)/apps/miccheck

miccheck_bin_path = $(miccheck_builddir)/miccheck.py
miccheck_modules = $(miccheck_builddir)/_miccheck

PYTHON_PREFIX = $(DATAROOTDIR)/mpss/miccheck/miccheck-$(version)

all:
	-
test:
	if [ -z "$(PYTEST)" ]; then \
		echo "PYTEST not set. Skipping..."; \
		exit 0; \
	fi; \
	export PYTHONPATH=$$(readlink -f ./ut):$(pytest_installroot_sitepackages):$$PYTHONPATH; \
	$(PYTEST) --color=no -v ./ut

local_test:
	PYTHONPATH=$$(pwd)/ut py.test . ut

install: $(miccheck_modules) $(miccheck_bin_path)
	$(INSTALL_d) $(DESTDIR)/$(PYTHON_PREFIX)
	cp -r $(miccheck_modules) $(DESTDIR)/$(PYTHON_PREFIX)
	$(INSTALL_x) $(miccheck_bin_path) $(DESTDIR)/$(PYTHON_PREFIX)
	$(INSTALL_d) $(DESTDIR)/$(BINDIR)
	ln -f -s $(PYTHON_PREFIX)/miccheck.py $(DESTDIR)/$(BINDIR)/miccheck

.PHONY: all install miccheck-binary dirs test
