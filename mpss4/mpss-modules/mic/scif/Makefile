# Intel MIC Platform Software Stack (MPSS)
# Copyright (c) 2016, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.

# Makefile - SCIF driver.

scif-y := iova.o
scif-y += scif_api.o
scif-y += scif_debugfs.o
scif-y += scif_dma.o
scif-y += scif_epd.o
scif-y += scif_fd.o
scif-y += scif_fence.o
scif-y += scif_main.o
scif-y += scif_mmap.o
scif-y += scif_nm.o
scif-y += scif_nodeqp.o
scif-y += scif_peer_bus.o
scif-y += scif_ports.o
scif-y += scif_rb.o
scif-y += scif_rma.o
scif-y += scif_rma_list.o

obj-$(CONFIG_SCIF) += scif.o
