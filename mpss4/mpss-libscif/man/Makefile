# Copyright (c) 2016, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.

A2X = a2x
SCIFMANDIR = ./
UMANDIR = $(SCIFMANDIR)/man_3
UMANSRCS = $(wildcard $(UMANDIR)/*.txt)
UMANPAGES = $(UMANSRCS:.txt=.3)

docs: $(UMANPAGES) $(KMANPAGES)

%.3:%.txt
	$(A2X) --xsltproc-opts='--nonet' --doctype manpage -D $(UMANDIR) --format  manpage --attribute=scif_user $<

clean:
	rm -f $(UMANDIR)/*.3
	rm -f $(UMANDIR)/*.xml
