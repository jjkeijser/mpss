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

# useful documentation
# RedHat:
#    /usr/lib/rpm/redhat/macros
#    /usr/lib/rpm/redhat/kmodtool
#    https://wiki.centos.org/HowTos/BuildingKernelModules
# SUSE:
#   /etc/rpm/macros.kernel-source
#   /usr/lib/rpm/kernel-module-subpackage
#   https://drivers.suse.com/doc/kmpm/Kmpm-code11.pdf

%{!?kernel_flavor: %global kernel_flavor default}

%if 0%{?suse_version}
%{!?kernel_version: %global kernel_version $(make -s -C /usr/src/linux-obj/%_target_cpu/%kernel_flavor kernelrelease)}
%endif

%global kernel_version 4.18.0-240.el8.x86_64

%{!?kernel_version: %global kernel_version $(uname -r)}
%{!?kernel_src: %global kernel_src /lib/modules/%{kernel_version}/build}
%{!?build_modules_sign: %global build_modules_sign false}
%{!?build_coverage: %global build_coverage false}

Summary:	Host drivers for Intel® Xeon Phi™ coprocessor cards
Name:		mpss-modules
Distribution:	MPSS4
Version:	4.4.1
Release:	4999.el8_3
Group:		System Environment/Kernel
License:	GPLv2
Vendor:		Intel Corporation
URL:		http://www.intel.com
ExclusiveArch: 	x86_64
Source0:	%{name}-%{version}.tar.gz
Source1:	mpss-modules-kmp-preambule
Source2:	mpss-modules-kmp-filelist
Patch:      mpss-modules-4.4.1-%{kernel_version}.patch
BuildRequires:	%kernel_module_package_buildreqs

# disable debug package generation
%define debug_package %{nil}

%if %{_vendor} == redhat
# required for redhat postin script
%define kmod_version %version
%define kmod_release %release
# name on redhat has to be "mic" because it is appended to modules path
%define kmp_name mic
%else
%define kmp_name %name
%endif

%kernel_module_package -n %{kmp_name} -p %{SOURCE1} -f %{SOURCE2} %kernel_flavor

%description
Provides host drivers for Intel® Xeon Phi™ coprocessor cards

%package -n %{name}-devel
Summary: Development header files specific to Intel MIC
Group: Development/System
%description -n %{name}-devel
Provides header and symbol version file for kernel and user space development

%posttrans -n %{name}-devel
for D in /lib/modules/*; do
	/usr/bin/rm -f $D/scif.symvers >& /dev/null || :
	/usr/bin/rm -f $D/source/include/modules/scif.h >& /dev/null || :
	/usr/bin/rm -f $D/source/include/modules/scif_ioctl.h >& /dev/null || :
	if [ -e $D/source ]; then
		/usr/bin/mkdir -p $D/source/include/modules >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif.symvers $D/ >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif.h $D/source/include/modules/ >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif_ioctl.h $D/source/include/modules/ >& /dev/null || :
	fi
done

%triggerin -n %{name}-devel -- kernel-devel kernel-default-devel
for D in /lib/modules/*; do
	/usr/bin/rm -f $D/scif.symvers >& /dev/null || :
	/usr/bin/rm -f $D/source/include/modules/scif.h >& /dev/null || :
	/usr/bin/rm -f $D/source/include/modules/scif_ioctl.h >& /dev/null || :
	if [ -e $D/source ]; then
		/usr/bin/mkdir -p $D/source/include/modules >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif.symvers $D/ >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif.h $D/source/include/modules/ >& /dev/null || :
		/usr/bin/ln -sf /usr/src/mpss-modules/scif_ioctl.h $D/source/include/modules/ >& /dev/null || :
	fi
done

%postun -n %{name}-devel
if [ "$1" = "0" ]; then
	for D in /lib/modules/*; do
		/usr/bin/rm -f $D/scif.symvers >& /dev/null || :
		/usr/bin/rm -f $D/source/include/modules/scif.h >& /dev/null || :
		/usr/bin/rm -f $D/source/include/modules/scif_ioctl.h >& /dev/null || :
		/usr/bin/rmdir $D/source/include/modules >& /dev/null || :
		/usr/bin/rmdir $D/source/include >& /dev/null || :
	done
fi

%prep
%setup -D -q -c -T -a 0
%patch -p1

%build
%{__make} \
	BUILD_CARD=false \
	KERNEL_SRC=%{kernel_src} \
	BUILD_MODULES_SIGN=%{build_modules_sign} \
	BUILD_COVERAGE=%{build_coverage}

%install
%{__make} \
	BUILD_ROOT=%{buildroot} \
	BUILD_CARD=false \
	BUILD_MODULES_SIGN=%{build_modules_sign} \
	BUILD_COVERAGE=%{build_coverage} \
	KERNEL_SRC=%{kernel_src} \
	MODULE_SYMVERS=/usr/src/mpss-modules \
	MODULE_KERNEL_HEADERS=/usr/src/mpss-modules  \
	MODULE_USER_HEADERS=/usr/include \
	DEPMOD="echo" \
	install

%check
# print a warning if the KMP scripts cannot detect the kernel version
# usually it ends with error during processing files for the package
set +x
if [ "%{_vendor}" = "redhat" ] && [ "%kverrel" != "%kernel_version" ]; then
	echo "" >&2
	echo "********************************************************************************" >&2
	echo "***************** KERNEL MODULE PACKAGE COMPATIBILITY WARNING ******************" >&2
	echo "********************************************************************************" >&2
	echo "Your kernel name does not adhere to Red Hat* specifications enforced by kmodtool" >&2
	echo "at %kmodtool. Please ensure your kernel name ends in $(arch)." >&2
	echo "" >&2
fi

%files -n %{name}-devel
%defattr (-,root,root,-)
%dir /usr/src/mpss-modules
/usr/src/mpss-modules/scif.h
/usr/src/mpss-modules/scif_ioctl.h
/usr/src/mpss-modules/scif.symvers
/usr/include/scif_ioctl.h
/usr/include/mic_common.h
/usr/include/mic_ioctl.h

%changelog
* Wed  Mar 10 2021 Jan Just Keijser <janjust@nikhef.nl> 4.4.1-4999.el8_3
 - Port to CentOS 8.3

