
%define debug_package %{nil}

%define defaultbuildroot /
%undefine __find_provides
%undefine __find_requires
# Do not try autogenerate prereq/conflicts/obsoletes and check files
%undefine __check_files
%undefine __find_prereq
%undefine __find_conflicts
%undefine __find_obsoletes
# Be sure buildpolicy set to do nothing
%define __spec_install_post %{nil}
%define _missing_doc_files_terminate_build 0
%define KERNEL_VER 3.10.0-1160.el7.x86_64

BuildArch:     x86_64
Name:          mpss-modules
Version:       3.8.6
Release:       4
License:       GPLv2 
Group:         System Environment/Kernel
Summary:       Host driver for Intel® Xeon Phi™ coprocessor cards
URL:           http://www.intel.com
Vendor:        Intel Corporation

Source:        mpss-modules-%{version}.tar.bz2
Patch:         mpss-modules-%{KERNEL_VER}.patch

Provides:      mpss-modules-%{KERNEL_VER} = %{version}-%{release}
Provides:      mpss-modules-%{KERNEL_VER}(x86-64) = %{version}-%{release}
Requires:      /bin/sh  

%description
Provides host driver for Intel® Xeon Phi™ coprocessor cards


%package %{KERNEL_VER}
Summary:       Host driver for Intel® Xeon Phi™ coprocessor cards

%description %{KERNEL_VER}
Provides host driver for Intel® Xeon Phi™ coprocessor cards

%package dev-%{KERNEL_VER}
Summary:		Header and symbol version file for driver development
%description dev-%{KERNEL_VER}
Provides header and symbol version file for driver development

%prep
%setup -q -c -n %{name}-%{version}
%patch -p1

%build
make MIC_CARD_ARCH=k1om KERNEL_VERSION=%{KERNEL_VER} DESTDIR=${RPM_BUILD_ROOT} prefix=%{_prefix} sysconfdir=%{_sysconfdir}

%install
rm -rf $RPM_BUILD_ROOT
strip --strip-unneeded mic.ko
make MIC_CARD_ARCH=k1om KERNEL_VERSION=%{KERNEL_VER} DESTDIR=${RPM_BUILD_ROOT} prefix=%{_prefix} sysconfdir=%{_sysconfdir} install

%if %{?rhel} > 7 
# Install our modified version of the RHEL7 ifup/ifdown scripts
mkdir -p ${RPM_BUILD_ROOT}/etc/mpss

cp -a network-scripts ${RPM_BUILD_ROOT}/etc/mpss
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /bin/sh
/sbin/depmod -a %{KERNEL_VER}

%postun -p /bin/sh
/sbin/depmod -a %{KERNEL_VER}

%files %{KERNEL_VER}
%attr(0644, root, root) "%{_sysconfdir}/modprobe.d/mic.conf"
%attr(0755, root, root) "%{_sysconfdir}/sysconfig/modules/mic.modules"
%attr(0644, root, root) "%{_sysconfdir}/udev/rules.d/50-udev-mic.rules"
%attr(0644, root, root) "/lib/modules/%{KERNEL_VER}/extra/mic.ko"
%if %{?rhel} > 7 
%dir %attr(0755, root, root) "/etc/mpss/network-scripts/"
%attr(0755, root, root) "/etc/mpss/network-scripts/ifdown"
%attr(0755, root, root) "/etc/mpss/network-scripts/ifup"
%attr(0644, root, root) "/etc/mpss/network-scripts/network"
%attr(0644, root, root) "/etc/mpss/network-scripts/network-functions"
%endif

%files dev-%{KERNEL_VER}
%attr(0644, root, root) "/lib/modules/%{KERNEL_VER}/scif.symvers"
%attr(0644, root, root) "%{_prefix}/src/kernels/%{KERNEL_VER}/include/modules/scif.h"

%changelog
* Fri Dec 18 2020 Jan Just Keijser <janjust@nikhef.nl> 3.8.6-4
 - First version to support RHEL/CentOS 8.2 & 8.3 kernels

* Fri Nov 13 2020 Jan Just Keijser <janjust@nikhef.nl> 3.8.6-3
 - Updated to support RHEL/CentOS 7.9 kernel

* Tue Apr 28 2020 Jan Just Keijser <janjust@nikhef.nl> 3.8.6-2
 - Updated to support RHEL/CentOS 7.8 kernel

* Fri Oct 4 2019 Jan Just Keijser <janjust@nikhef.nl> 3.8.6-1
 - Rebuild for mpss 3.8.6 (same as 3.8.5)

* Fri Oct 4 2019 Jan Just Keijser <janjust@nikhef.nl> 3.8.5-2
 - Updated to support RHEL/CentOS 7.7 kernel

* Wed Feb 20 2019 Jan Just Keijser <janjust@nikhef.nl> 3.8.5-1
 - Updated to support RHEL/CentOS 7.6 kernel

* Wed Aug 22 2018 Jan Just Keijser <janjust@nikhef.nl> 3.8.4-2
 - Added mpss-modules-dev package

* Thu Jul 05 2018 Jan Just Keijser <janjust@nikhef.nl> 3.8.4-1
 - Updated to support RHEL/CentOS 7.5 kernel

