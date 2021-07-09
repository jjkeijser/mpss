
Summary: Intel (R) MPSS Management Daemon
Name: mpss-daemon
Distribution: MPSS4
Version: 4.4.1
Release: 4999.el8
License: See COPYING
Group: System Environment/Daemons
Requires: mpss-modules mpss-boot-files
Vendor: Intel Corporation
URL: http://www.intel.com
Source0: %{name}-%{version}.tar.gz
%if ! %{defined libscif_installed}
BuildRequires: mpss-libscif
BuildRequires: mpss-libscif-devel
%endif
Requires: mpss-release
%description
mpss-daemon provides the device management daemon and its control tool. The
daemon is responsible for booting the device, capturing crash dumps, etc.

%package devel
Summary: Intel (R) MPSS Management Daemon - Development files
Group: Development/System
Requires: %{name}
%description devel
mpss-daemon provides the device management daemon and its control tool. The
daemon is responsible for booting the device, capturing crash dumps, etc.
This package contains symbolic links, header files, and related items
necessary for software development.

%if 0%{?suse_version}
%debug_package
%endif

%prep
%setup -q

%clean

%build
%{__make} CPPFLAGS+="-DMIC_ARCH_KNL -I/opt/qb/buildagent/workspace/1025/knl-linux/tmp/eng/mpss-daemon_depends_root/usr/include -I/opt/qb/buildagent/workspace/1025/knl-linux/tmp/eng/mpss-daemon_depends_root/include" 

%install
%{__make} install DESTDIR=%{buildroot} includedir=/usr/include


%post
/usr/bin/rm -f /etc/systemd/system/mpss.suspend.service >& /dev/null || :
/usr/bin/rm -f /etc/systemd/system/mpss.service >& /dev/null || :
/usr/bin/systemctl daemon-reload >& /dev/null || :

%files
%defattr(-,root,root,-)
%dir "/var/mpss"
%dir "/lib/firmware/mic"
"/usr/lib64/libmpssconfig.so"
"/usr/lib64/libmpssconfig.so.0"
"/usr/lib64/libmpssconfig.so.0.0.1"
"/usr/bin/micctrl"
"/usr/bin/mpssd"
%config "/etc/logrotate.d/mpssd"
%dir "/etc/mpss"
"/usr/lib/systemd/system/mpss.service"

%changelog
* Tue Mar 9 2021 Jan Just Keijser <janjust@nikhef.nl> 4.4.1-4999.el8
 - Port to CentOS 8

