not-y := n
not-n := y
m-not-y := n
m-not-n := m

ifeq ($(CONFIG_X86_MICPCI),)
CONFIG_X86_MICPCI := n
endif
ifeq ($(CONFIG_X86_MICPCI)$(MIC_CARD_ARCH),n)
$(error building for host, but $$(MIC_CARD_ARCH) is unset)
endif
ifneq ($(MIC_CARD_ARCH),$(firstword $(filter l1om k1om,$(MIC_CARD_ARCH))))
$(error $$(MIC_CARD_ARCH) must be l1om or k1om)
endif

# Force optimization to -O2 in case the kernel was configured to use
# -Os. The main reason is pretty dumb -- -Os has a warning -O2 doesn't,
# and we compile with -Werror internally. Another reason is that -O2 is
# what we're used to in terms of validation and performance analysis. We
# should probably get rid of this, though.
subdir-ccflags-y += -O2

# Makes it easy to inject "-Werror" from the environment
subdir-ccflags-y += $(KERNWARNFLAGS)

# Bake some information about who built the module(s), and what version
# of the source code they started with. Possibly useful during debug.
subdir-ccflags-y += -DBUILD_NUMBER=\"'$(MPSS_BUILDNO)'\"
subdir-ccflags-y += -DBUILD_BYWHOM=\"'$(MPSS_BUILTBY)'\"
subdir-ccflags-y += -DBUILD_ONDATE=\"'$(MPSS_BUILTON)'\"
subdir-ccflags-y += -DBUILD_SCMVER=\"'$(MPSS_COMMIT)'\"
subdir-ccflags-y += -DBUILD_VERSION=\"'$(or $(MPSS_VERSION),0.0) ($(MPSS_BUILTBY))'\"

# Code common with the host mustn't use CONFIG_M[LK]1OM directly.
# But of course it does anyway. Arrgh.
subdir-ccflags-$(CONFIG_ML1OM) += -DMIC_IS_L1OM
subdir-ccflags-$(CONFIG_MK1OM) += -DMIC_IS_K1OM
ifeq ($(MIC_CARD_ARCH),l1om)
subdir-ccflags-y += -DMIC_IS_L1OM -DCONFIG_ML1OM
endif
ifeq ($(MIC_CARD_ARCH),k1om)
subdir-ccflags-y += -DMIC_IS_K1OM -DCONFIG_MK1OM
endif

# a shorthand for "runs on the card"?
subdir-ccflags-$(CONFIG_X86_MICPCI) += -D_MIC_SCIF_

# "runs on the host"
subdir-ccflags-$(not-$(CONFIG_X86_MICPCI)) += -DHOST -DUSE_VCONSOLE

# always set? what's this thing's purpose?
subdir-ccflags-y += -D__LINUX_GPL__ -D_MODULE_SCIF_

subdir-ccflags-y += -I$(M)/include

obj-$(CONFIG_X86_MICPCI) += dma/ micscif/ pm_scif/ ras/
obj-$(CONFIG_X86_MICPCI) += vcons/ vnet/ mpssboot/ ramoops/ virtio/

obj-$(m-not-$(CONFIG_X86_MICPCI)) += mic.o

mic-objs :=
mic-objs += dma/mic_dma_lib.o
mic-objs += dma/mic_dma_md.o
mic-objs += host/acptboot.o
mic-objs += host/ioctl.o
mic-objs += host/linpm.o
mic-objs += host/linpsmi.o
mic-objs += host/linscif_host.o
mic-objs += host/linsysfs.o
mic-objs += host/linux.o
mic-objs += host/linvcons.o
mic-objs += host/linvnet.o
mic-objs += host/micpsmi.o
mic-objs += host/micscif_pm.o
mic-objs += host/pm_ioctl.o
mic-objs += host/pm_pcstate.o
mic-objs += host/tools_support.o
mic-objs += host/uos_download.o
mic-objs += host/vhost/mic_vhost.o
mic-objs += host/vhost/mic_blk.o
mic-objs += host/vmcore.o
mic-objs += micscif/micscif_api.o
mic-objs += micscif/micscif_debug.o
mic-objs += micscif/micscif_fd.o
mic-objs += micscif/micscif_intr.o
mic-objs += micscif/micscif_nm.o
mic-objs += micscif/micscif_nodeqp.o
mic-objs += micscif/micscif_ports.o
mic-objs += micscif/micscif_rb.o
mic-objs += micscif/micscif_rma_dma.o
mic-objs += micscif/micscif_rma_list.o
mic-objs += micscif/micscif_rma.o
mic-objs += micscif/micscif_select.o
mic-objs += micscif/micscif_smpt.o
mic-objs += micscif/micscif_sysfs.o
mic-objs += micscif/micscif_va_gen.o
mic-objs += micscif/micscif_va_node.o
mic-objs += vnet/micveth_dma.o
mic-objs += vnet/micveth_param.o

version-le = $(shell printf '%s\n' $(1) | sort -t. -k 1,1n -k 2,2n -k 3,3n -k 4,4n -c >/dev/null 2>&1 && echo t)
ifeq ($(call version-le, 2.6.23 $(KERNELRELEASE)),t)
ccflags-y += $(mic-cflags)
else
$(error building against kernels <= 2.6.23 is broken)
endif
