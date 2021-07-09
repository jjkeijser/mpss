
Summary: System Intel(R) Xeon Phi(TM) Coprocessor Tools
Name: mpss-systools
Version: 4.4.1
Release: 4974
License: Intel-MPSS-License
Group: Applications/Engineering
Vendor: Intel Corporation
Prefix: /usr
Distribution: MPSS4
URL: http://www.intel.com
Source0: mpss-systools-%{version}.tar.gz
Requires: mpss-daemon, mpss-boot-files, mpss-firmware = %{version}
%if ! %{defined libscif_installed}
BuildRequires: mpss-libscif-devel
%endif
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: mpss-release
%description
A collection of tools to manage one or more Intel(R) Xeon Phi(TM) Coprocessors

# Do not terminate if files are unpackaged
%define _unpackaged_files_terminate_build 0

%if 0%{?suse_version}
%debug_package
%endif

%prep
%setup -D -q -c -T -a 0

%clean

%build
%{__make} %{?_smp_mflags} open Version=%{version} CPPFLAGS="-DMIC_ARCH_KNL -I/opt/qb/buildagent/workspace/1025/knl-linux/tmp/eng/systools_depends_root/include" LDFLAGS="-L/opt/qb/buildagent/workspace/1025/knl-linux/tmp/eng/systools_depends_root/lib64"

%install
%{__make} open_install version=%{version} release=%{release} DESTDIR=%{buildroot} PREFIX=%{_prefix} BINDIR=%{_bindir} SBINDIR=%{_sbindir} LIBDIR=%{_libdir} INCLUDEDIR=%{_includedir} DOCDIR=%{_docdir} MANDIR=%{_mandir} SYSCONFDIR=%{_sysconfdir} INITDDIR=%{_initddir} DATAROOTDIR=%{_datadir}


%files
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/*.pyc
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/*.pyo
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/*.pyc
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/*.pyo
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/*.pyc
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/*.pyo
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/linux/*.pyc
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/linux/*.pyo
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/windows/*.pyc
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/windows/*.pyo
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/windows/__init__.py
%exclude %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/windows/tests.py
%exclude %{_datadir}/mpss/micbios/micbios-%{version}/_micbios/*.pyc
%exclude %{_datadir}/mpss/micbios/micbios-%{version}/_micbios/*.pyo
%exclude %{_datadir}/mpss/micbios/micbios-%{version}/*.pyc
%exclude %{_datadir}/mpss/micbios/micbios-%{version}/*.pyo

%defattr(-,root,root,-)
%doc COPYING
%{_bindir}/micdebug
%{_bindir}/micinfo
%{_bindir}/micfw
%{_bindir}/micflash.py
%{_bindir}/micflash
%{_bindir}/micsmc-cli

%{_bindir}/miccheck
%dir %{_datadir}/mpss/miccheck/
%dir %{_datadir}/mpss/miccheck/miccheck-%{version}
%{_datadir}/mpss/miccheck/miccheck-%{version}/miccheck.py
%dir %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/__init__.py
%dir %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/base.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/tests.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/printing.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/version.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/main.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/__init__.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/testrunner.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/exceptions.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/common/testtypes.py
%dir %{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/linux/
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/linux/tests.py
%{_datadir}/mpss/miccheck/miccheck-%{version}/_miccheck/linux/__init__.py

%{_bindir}/micbios
%dir %{_datadir}/mpss/micbios/
%dir %{_datadir}/mpss/micbios/micbios-%{version}
%{_datadir}/mpss/micbios/micbios-%{version}/micbios.py
%dir %{_datadir}/mpss/micbios/micbios-%{version}/_micbios/
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/errormb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/parsermb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/commandsmb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/scifmb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/systoolsd_api.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/constantsmb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/utils.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/appmb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/version.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/devicemb.py
%{_datadir}/mpss/micbios/micbios-%{version}/_micbios/__init__.py




%changelog

%package -n mpss-libsystools0
Summary: System Intel(R) Xeon Phi(TM) Coprocessor Tools Library
Provides: mpss-libsystools
Group: System Environment/Libraries
Requires: mpss-release
%description -n mpss-libsystools0
System Intel(R) Xeon Phi(TM) Coprocessor Tools Library provides a user-space interface to manage one or more Intel(R) Xeon Phi(TM) Coprocessors.

%files -n mpss-libsystools0
%defattr(-,root,root,-)
%doc COPYING
%{_libdir}/libsystools.so.0
%{_libdir}/libsystools.so.0.1

%post -n mpss-libsystools0
/sbin/ldconfig

%postun -n mpss-libsystools0
/sbin/ldconfig


%package -n mpss-libsystools-devel
Summary: System Intel(R) Xeon Phi(TM) Coprocessor Tools Library development package
Requires: mpss-libsystools0
Group: Development/Libraries
%description -n mpss-libsystools-devel
System Intel(R) Xeon Phi(TM) Coprocessor Tools Library development package

%files -n mpss-libsystools-devel
%defattr(-,root,root,-)
%doc COPYING
%{_libdir}/libsystools.so
%{_includedir}/libsystools.h
%{_includedir}/micsdkerrno.h
%dir %{_docdir}/systools/examples
%{_docdir}/systools/examples/examples.c
%{_docdir}/systools/examples/Makefile
%{_mandir}/man3/Coprocessor_OS_Power_Management_Configuration.3.gz
%{_mandir}/man3/Coprocessor_core_Information.3.gz
%{_mandir}/man3/Core_Utilization_Information.3.gz
%{_mandir}/man3/Device_Access.3.gz
%{_mandir}/man3/Error_Reporting.3.gz
%{_mandir}/man3/Free_Memory_Utilities.3.gz
%{_mandir}/man3/General_Device_Information.3.gz
%{_mandir}/man3/Identify_Available_Coprocessors.3.gz
%{_mandir}/man3/LED_Mode_Information.3.gz
%{_mandir}/man3/Memory_Information.3.gz
%{_mandir}/man3/Memory_Utilization_Information.3.gz
%{_mandir}/man3/PCI_Configuration_Information.3.gz
%{_mandir}/man3/Power_Limits.3.gz
%{_mandir}/man3/Power_Utilization_Information.3.gz
%{_mandir}/man3/Processor_Information.3.gz
%{_mandir}/man3/Query_Coprocessor_State.3.gz
%{_mandir}/man3/SMC_Configuration_Information.3.gz
%{_mandir}/man3/Thermal_Information.3.gz
%{_mandir}/man3/Throttle_State_Information.3.gz
%{_mandir}/man3/Turbo_State_Information.3.gz
%{_mandir}/man3/Version_Information.3.gz
%{_mandir}/man3/libsystools.3.gz
%{_mandir}/man3/mic_close_device.3.gz
%{_mandir}/man3/mic_enter_maint_mode.3.gz
%{_mandir}/man3/mic_free_core_util.3.gz
%{_mandir}/man3/mic_free_sensor_list.3.gz
%{_mandir}/man3/mic_get_coprocessor_os_version.3.gz
%{_mandir}/man3/mic_get_corec6_mode.3.gz
%{_mandir}/man3/mic_get_cores_count.3.gz
%{_mandir}/man3/mic_get_cores_frequency.3.gz
%{_mandir}/man3/mic_get_cores_voltage.3.gz
%{_mandir}/man3/mic_get_cpufreq_mode.3.gz
%{_mandir}/man3/mic_get_device_name.3.gz
%{_mandir}/man3/mic_get_device_type.3.gz
%{_mandir}/man3/mic_get_ecc_mode.3.gz
%{_mandir}/man3/mic_get_error_string.3.gz
%{_mandir}/man3/mic_get_flash_version.3.gz
%{_mandir}/man3/mic_get_fsc_strap.3.gz
%{_mandir}/man3/mic_get_jiffy_counter.3.gz
%{_mandir}/man3/mic_get_led_alert.3.gz
%{_mandir}/man3/mic_get_me_version.3.gz
%{_mandir}/man3/mic_get_memory_info.3.gz
%{_mandir}/man3/mic_get_memory_utilization_info.3.gz
%{_mandir}/man3/mic_get_ndevices.3.gz
%{_mandir}/man3/mic_get_num_cores.3.gz
%{_mandir}/man3/mic_get_part_number.3.gz
%{_mandir}/man3/mic_get_pc3_mode.3.gz
%{_mandir}/man3/mic_get_pc6_mode.3.gz
%{_mandir}/man3/mic_get_pci_config.3.gz
%{_mandir}/man3/mic_get_post_code.3.gz
%{_mandir}/man3/mic_get_power_hmrk.3.gz
%{_mandir}/man3/mic_get_power_lmrk.3.gz
%{_mandir}/man3/mic_get_power_phys_limit.3.gz
%{_mandir}/man3/mic_get_power_sensor_list.3.gz
%{_mandir}/man3/mic_get_power_sensor_value_by_index.3.gz
%{_mandir}/man3/mic_get_power_sensor_value_by_name.3.gz
%{_mandir}/man3/mic_get_power_throttle_info.3.gz
%{_mandir}/man3/mic_get_processor_info.3.gz
%{_mandir}/man3/mic_get_serial_number.3.gz
%{_mandir}/man3/mic_get_silicon_sku.3.gz
%{_mandir}/man3/mic_get_smc_boot_loader_ver.3.gz
%{_mandir}/man3/mic_get_smc_fwversion.3.gz
%{_mandir}/man3/mic_get_smc_hwrevision.3.gz
%{_mandir}/man3/mic_get_smc_persistence_flag.3.gz
%{_mandir}/man3/mic_get_thermal_sensor_list.3.gz
%{_mandir}/man3/mic_get_thermal_throttle_info.3.gz
%{_mandir}/man3/mic_get_thermal_version_by_index.3.gz
%{_mandir}/man3/mic_get_thermal_version_by_name.3.gz
%{_mandir}/man3/mic_get_threads_core.3.gz
%{_mandir}/man3/mic_get_tick_count.3.gz
%{_mandir}/man3/mic_get_time_window0.3.gz
%{_mandir}/man3/mic_get_time_window1.3.gz
%{_mandir}/man3/mic_get_turbo_mode.3.gz
%{_mandir}/man3/mic_get_turbo_state.3.gz
%{_mandir}/man3/mic_get_turbo_state_valid.3.gz
%{_mandir}/man3/mic_get_uuid.3.gz
%{_mandir}/man3/mic_in_maint_mode.3.gz
%{_mandir}/man3/mic_in_ready_state.3.gz
%{_mandir}/man3/mic_is_device_avail.3.gz
%{_mandir}/man3/mic_is_smc_boot_loader_ver_supported.3.gz
%{_mandir}/man3/mic_leave_maint_mode.3.gz
%{_mandir}/man3/mic_open_device.3.gz
%{_mandir}/man3/mic_open_device_by_name.3.gz
%{_mandir}/man3/mic_set_led_alert.3.gz
%{_mandir}/man3/mic_set_power_limit0.3.gz
%{_mandir}/man3/mic_set_power_limit1.3.gz
%{_mandir}/man3/mic_set_smc_persistence_flag.3.gz
%{_mandir}/man3/mic_set_turbo_mode.3.gz
%{_mandir}/man3/mic_update_core_util.3.gz
%{_mandir}/man3/mic_get_state.3.gz

