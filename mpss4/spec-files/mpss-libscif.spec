
Summary: SCIF library for Intel MIC co-processors
Name: mpss-libscif
Distribution: MPSS4
Version: 4.4.1
Release: 4999.el8
License: See COPYING
Group: System Environment/Libraries
Vendor: Intel Corporation
Prefix: /usr
URL: http://www.intel.com
Source: %{name}-%{version}.tar.gz
Patch: %{name}-%{version}.el8.patch
Requires: mpss-release
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
%description
mpss-libscif provides the userspace interface of the Symmetric Communications
Interface (SCIF), a low-level IPC mechanism which allows communication
between programs running on Intel MIC PCIe co-processors and/or their
'host'.

%package doc
Summary: SCIF library for Intel MIC co-processors - Documentation files
Group: Documentation
Requires: mpss-release
%description doc
mpss-libscif provides the userspace interface of the Symmetric Communications
Interface (SCIF), a low-level IPC mechanism which allows communication
between programs running on Intel MIC PCIe co-processors and/or their
'host'.  This package contains documentation.

%package devel
Summary: SCIF library for Intel MIC co-processors - Development files
Group: Development/Libraries
Requires: mpss-libscif
%description devel
mpss-libscif provides the userspace interface of the Symmetric Communications
Interface (SCIF), a low-level IPC mechanism which allows communication
between programs running on Intel MIC PCIe co-processors and/or their
'host'.  This package contains symbolic links, header files, and related
items necessary for software development.

%if 0%{?suse_version}
%debug_package
%endif

%prep
%setup -q -c -T -a 0
%patch -p1

%clean

%build
%{__make} -j16 CPPFLAGS+="-DMIC_ARCH_KNL -I/opt/qb/buildagent/workspace/1025/knl-linux/tmp/eng/libscif_depends_root/usr/include" 

%install
%{__make} install DESTDIR=%{buildroot} libdir=/usr/lib64 mandir=/usr/share/man includedir=/usr/include

%post
/sbin/ldconfig

%preun

%postun
/sbin/ldconfig

%files
%defattr(-,root,root,-)
"/usr/lib64/libscif.so.0"
"/usr/lib64/libscif.so.0.0.1"

%files doc
%defattr(-,root,root,-)
"/usr/share/man/man3/scif_writeto.3.gz"
"/usr/share/man/man3/scif_accept.3.gz"
"/usr/share/man/man3/scif_open.3.gz"
"/usr/share/man/man3/scif_fence_wait.3.gz"
"/usr/share/man/man3/scif_register.3.gz"
"/usr/share/man/man3/scif_recv.3.gz"
"/usr/share/man/man3/scif_unregister.3.gz"
"/usr/share/man/man3/scif_get_fd.3.gz"
"/usr/share/man/man3/scif_send.3.gz"
"/usr/share/man/man3/scif_munmap.3.gz"
"/usr/share/man/man3/scif_get_nodeIDs.3.gz"
"/usr/share/man/man3/scif_bind.3.gz"
"/usr/share/man/man3/scif_connect.3.gz"
"/usr/share/man/man3/scif_vreadfrom.3.gz"
"/usr/share/man/man3/scif_mmap.3.gz"
"/usr/share/man/man3/scif_poll.3.gz"
"/usr/share/man/man3/scif_fence_mark.3.gz"
"/usr/share/man/man3/scif_close.3.gz"
"/usr/share/man/man3/scif_fence_signal.3.gz"
"/usr/share/man/man3/scif_readfrom.3.gz"
"/usr/share/man/man3/scif_vwriteto.3.gz"
"/usr/share/man/man3/scif_listen.3.gz"

%files devel
%defattr(-,root,root,-)
"/usr/include/scif.h"
"/usr/lib64/libscif.so"

%changelog
* Tue Mar 9 2021 Jan Just Keijser <janjust@nikhef.nl> 4.4.1-4999.el8
 - Port to CentOS 8

