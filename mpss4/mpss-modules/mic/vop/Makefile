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

# Makefile - Intel MIC VOP Linux driver.

vop-y := vop_core.o
vop-y += vop_debugfs.o

vop-$(CONFIG_INTEL_MIC_CARD) += vop_card.o
vop-$(CONFIG_INTEL_MIC_HOST) += vop_host.o
vop-y += $(vop-m)

obj-$(CONFIG_INTEL_MIC) += vop.o
