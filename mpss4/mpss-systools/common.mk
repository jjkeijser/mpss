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


PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
SBINDIR ?= $(PREFIX)/sbin
SYSCONFDIR ?= $(PREFIX)/etc
INCLUDEDIR ?= $(PREFIX)/include
SRCDIR ?= $(PREFIX)/src
INITDDIR ?= $(SYSCONFDIR)/init.d
DATAROOTDIR ?= $(PREFIX)/share
MANDIR ?= $(DATAROOTDIR)/man
DOCDIR ?= $(DATAROOTDIR)/doc/systools
LIBEXECDIR ?= $(PREFIX)/libexec
DOXYGEN ?= doxygen

UT_BINDIR ?= $(DATAROOTDIR)/mpss/test/systools-ut

INSTALL_r ?= install -m 0644
INSTALL_x ?= install -m 0755
INSTALL_d ?= install -d

STDCPP += -std=c++11 -Wall -Werror -Wextra -fstack-protector

ifdef EXENAME
  EXECUTABLE = bin/$(EXENAME)

  ifndef UT_EXENAME
    UT_EXENAME = $(EXENAME)-ut
  endif
endif

ifdef LIBNAME
  UT_LIBNAME = $(LIBNAME)-ut

  LIBRARY = lib/lib$(LIBNAME).a
  UT_LIBRARY = lib/lib$(UT_LIBNAME).a

  LIBOBJECTS = $(LIBSOURCES:.cpp=.o)

  ifndef UT_LIBSOURCES
    UT_LIBSOURCES = $(LIBSOURCES)
  endif

  ifndef UT_EXENAME
    UT_EXENAME = lib$(LIBNAME)-ut
  endif

  UT_LIBOBJECTS = $(UT_LIBSOURCES:.cpp=_ut.o)
endif

ifdef UT_EXENAME
  UT_EXECUTABLE = bin/$(UT_EXENAME)
endif

BINARIES += $(LIBRARY) $(UT_LIBRARY) $(EXECUTABLE)

ifndef TARGETS
  TARGETS = $(BINARIES)
endif

all: $(TARGETS)

ifdef LIBRARY
  $(LIBRARY): $(LIBOBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $^
endif

ifdef UT_LIBRARY
  $(UT_LIBRARY): $(UT_LIBOBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $^
endif

UT_OBJECTS=$(UT_SOURCES:.cpp=_ut.o)
OBJECTS=$(SOURCES:.cpp=.o)

ifdef EXECUTABLE
  $(EXECUTABLE): $(OBJECTS)
	mkdir -p bin
	$(CXX) $(OBJECTS) $(ALL_LDFLAGS) -o $(EXECUTABLE)

  install: $(EXECUTABLE)
	$(INSTALL_d) $(DESTDIR)/$(BINDIR)
	$(INSTALL_x) $(EXECUTABLE) $(DESTDIR)/$(BINDIR)

endif

ifdef UT_EXECUTABLE
  $(UT_EXECUTABLE): $(UT_OBJECTS) $(UT_LIBRARY)
	mkdir -p bin
	$(CXX) $^ $(UT_ALL_LDFLAGS) -o $@

  test: $(UT_EXECUTABLE)
	$(UT_EXECUTABLE) --gtest_shuffle --gtest_color=no

  local_test: $(UT_EXECUTABLE)
	$(UT_EXECUTABLE) --gtest_shuffle --gtest_color=no

  leakchk: $(UT_EXECUTABLE)
	sudo valgrind --leak-check=full $(UT_EXECUTABLE) --gtest_shuffle --gtest_color=no

  install_ut: $(UT_EXECUTABLE)
	$(INSTALL_d) $(DESTDIR)/$(UT_BINDIR)
	$(INSTALL_x) $(UT_EXECUTABLE) $(DESTDIR)/$(UT_BINDIR)

endif


clean:
	$(RM) $(LIBRARY) $(UT_LIBRARY) $(LIBOBJECTS) $(UT_LIBOBJECTS) $(OBJECTS) $(UT_OBJECTS) $(EXECUTABLE) $(UT_EXECUTABLE)

%.o: %.cpp
	$(CXX) $(STDCPP) $(ALL_CPPFLAGS) -c -o $@ $<

%_ut.o: %.cpp
	$(CXX) $(STDCPP) $(UT_ALL_CPPFLAGS) -c -o $@ $<

ifdef LIBMAJOR
ifdef LIBMINOR
  LIBABI = lib$(LIBNAME).so.$(LIBMAJOR).$(LIBMINOR)
  SHARED_LIBRARY = lib/lib$(LIBNAME).so.$(LIBMAJOR).$(LIBMINOR)

  BINARIES += $(SHARED_LIBRARY)

  $(SHARED_LIBRARY): $(LIBOBJECTS)
	mkdir -p lib
	$(CXX) $(LIBOBJECTS) $(ALL_LDFLAGS) -shared -Wl,-soname,$(LIBABI) -o $@

  all: $(SHARED_LIBRARY)

  install_shared: $(SHARED_LIBRARY)
	$(INSTALL_d) $(DESTDIR)/$(LIBDIR)
	$(INSTALL_x) $(SHARED_LIBRARY) $(DESTDIR)/$(LIBDIR)
	ln -f -s lib$(LIBNAME).so.$(LIBMAJOR).$(LIBMINOR) $(DESTDIR)/$(LIBDIR)/lib$(LIBNAME).so.$(LIBMAJOR)
	ln -f -s lib$(LIBNAME).so.$(LIBMAJOR) $(DESTDIR)/$(LIBDIR)/lib$(LIBNAME).so

    install_headers:
	$(INSTALL_d) $(DESTDIR)/$(INCLUDEDIR)
	$(INSTALL_r) include/* $(DESTDIR)/$(INCLUDEDIR)
    ifeq ($(LIBNAME), systools)
	$(INSTALL_r) ../sdk/include/micsdkerrno.h $(DESTDIR)/$(INCLUDEDIR)
    endif

  clean_shared:
	$(RM) $(SHARED_LIBRARY)

  install: install_shared install_headers

  clean: clean_shared

  .PHONY: install_shared install_headers clean_shared
endif
endif

.PHONY: all test leakchk local_test install install_shared install_headers install_ut clean clean_shared
