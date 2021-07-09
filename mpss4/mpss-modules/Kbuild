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

# Makefile - Intel MIC Linux driver.

version ?= 0.0.0
release ?= 0
subdir-ccflags-y += -DMIC_VERSION='"$(version)-$(release)"'

# force O2 despite of kernel config
subdir-ccflags-y += -O2

# keep the code in check
ifneq ($(BUILD_COVERAGE),true)
subdir-ccflags-y += -Werror
else
# coverage has to be added in ifdef otherwise make clean won't work
obj-$(CONFIG_INTEL_MIC) += libcov-lkm/run/linuxKernel/
endif

obj-$(CONFIG_INTEL_MIC) += mic/
obj-$(CONFIG_INTEL_MIC_X100_DMA) += mic_x100_dma/
obj-$(CONFIG_INTEL_MIC_X200_DMA) += mic_x200_dma/

# add MIC defines based on kernel config
subdir-ccflags-$(CONFIG_INTEL_MIC) += -DCONFIG_INTEL_MIC
subdir-ccflags-$(CONFIG_INTEL_MIC_X200_DMA) += -DCONFIG_INTEL_MIC_X200_DMA
subdir-ccflags-$(CONFIG_INTEL_MIC_X200) += -DCONFIG_INTEL_MIC_X200
subdir-ccflags-$(CONFIG_SCIF_BUS) += -DCONFIG_SCIF_BUS
subdir-ccflags-$(CONFIG_SCIF) += -DCONFIG_SCIF
subdir-ccflags-$(CONFIG_MIC_COSM) += -DCONFIG_MIC_COSM
subdir-ccflags-$(CONFIG_MIC_COSM_CLIENT) += -DCONFIG_MIC_COSM_CLIENT
subdir-ccflags-$(CONFIG_INTEL_MIC_CARD) += -DCONFIG_INTEL_MIC_CARD
subdir-ccflags-$(CONFIG_INTEL_MIC_HOST) += -DCONFIG_INTEL_MIC_HOST
subdir-ccflags-$(CONFIG_INTEL_MIC_X100_DMA) += -DCONFIG_INTEL_MIC_X100_DMA
subdir-ccflags-$(CONFIG_INTEL_MIC_X100) += -DCONFIG_INTEL_MIC_X100
