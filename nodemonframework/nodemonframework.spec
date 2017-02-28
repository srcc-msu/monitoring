Name: nodemonframework
Version: 0.2
Release: cx8
Summary: Node Monitor Framework
License: GPL/BSD
Group: System/Servers
Source: %{name}-%{version}.tar.gz
BuildRoot: %{?buildroot:%{buildroot}}%{!?buildroot:%{_tmppath}/%{name}-%{version}-%{release}.%{_arch}}

%define nmdist clustrx

%if "%{_vendor}" == "redhat"
%define nmdist centos
%define _runtimedir /var/run
%define _initdir %{_initrddir}
%define without_knmonctld 1
%endif

%if "%{_vendor}" == "suse"
%define nmdist suse
%define _runtimedir /var/run
%define _initdir %{_initddir}
%define without_knmonctld 1
%endif

%ifarch ppc64
%define without_knmonctld 1
%endif

%define exclude_modules %{?without_nmperfd:nmperfd}


%description
Node monitor framework is utility suit for monitoring management nodes,
computing nodes and servers.


%package -n libsmart
Summary: library for reading disks S.M.A.R.T. data.
Group: System/Libraries
Requires: libsmart = %{version}-%{release}

%description -n libsmart
libsmart provides API for reading S.M.A.R.T attributes from SATA disks,
checking if smart enable/available or not.


%package -n libsmart-devel
Summary: Headers for libsmart - library for reading disks S.M.A.R.T. data.
Group: Development/C
Requires: libsmart = %{version}-%{release}

%description -n libsmart-devel
libsmart provides API for reading S.M.A.R.T attributes from SATA disks,
checking if smart enable/available or not.
This package contains headers for development with libsmart.


%package -n libsmart-devel-static
Summary: Static library for reading disks S.M.A.R.T. data.
Group: Development/C
Requires: libsmart-devel = %{version}-%{release}

%description -n libsmart-devel-static
libsmart provides API for reading S.M.A.R.T attributes from SATA disks,
checking if smart enable/available or not.
This package contains static libsmart libraries.



%package -n libsysnfo
Summary: library for reading system hardware information.
Group: System/Libraries
Requires: libsmart = %{version}-%{release}
Requires: libsysnfo = %{version}-%{release}

%description -n libsysnfo
libsysnfo provides API for reading system hardware information.


%package -n libsysnfo-devel
Summary: library for reading system hardware information.
Group: Development/C
Requires: libsysnfo = %{version}-%{release}

%description -n libsysnfo-devel
libsysnfo provides API for reading system hardware information.
This package contains headers for development with libsysnfo.


%package -n libsysnfo-devel-static
Summary: library for reading system hardware information.
Group: Development/C
Requires: libsysnfo-devel = %{version}-%{release}

%description -n libsysnfo-devel-static
libsysnfo provides API for reading system hardware information.
This package contains static libsysynfo libraries.


%package -n nmond
Summary: Core monitoring module.
Group: System/Servers
BuildRequires: glibc-devel
BuildRequires: libnmib-devel >= 1.0.0
Requires: nmond = %{version}-%{release}
Requires(post): /sbin/chkconfig

%description -n nmond
nmond is a core of modular monitoring framework. It provides various
submodules for monitoring fs, smart data, and hardware sensors.


%if "x%{?without_knmonctld}" == "x"
%package -n knmonctld
Summary: Userspace module for control kernel monitor modules.
Group: System/Servers
Requires: knmonctld = %{version}-%{release}
Requires(post): /sbin/chkconfig

%description -n knmonctld
knmonctld is a userspace module for contol kernel monitor modules.
%endif

%package -n hybmon
Summary: Hybrid monitor module
Group: System/Servers
Requires: hybmon = %{version}-%{release}
Requires: libsmart = %{version}-%{release}
Requires(post): /sbin/chkconfig

%description -n hybmon
hybmon is a part of modular monitoring framework.
It provades various information about system, services, etc.
through plugins



%prep
%setup

%build
make EXCLUDE_MODULES="%{?exclude_modules}" DIST=%{nmdist} BINDIR=%{_bindir} LIBDIR=%{_libdir} INCDIR=%{_includedir} ETCDIR=%{_sysconfdir} SHDIR=%{_datadir} RUNDIR=%{_runtimedir}

%install
make EXCLUDE_MODULES="%{?exclude_modules}" DIST=%{nmdist} DESTDIR=%{buildroot} BINDIR=%{_bindir} LIBDIR=%{_libdir} INCDIR=%{_includedir} ETCDIR=%{_sysconfdir} SHDIR=%{_datadir} RUNDIR=%{_runtimedir} INITDIR=%{_initdir} INSTALL=install install
install -m 0755 -d %{buildroot}%{_sysconfdir}/modprobe.d
echo "options ipmi_si kipmid_max_busy_us=100" > %{buildroot}%{_sysconfdir}/modprobe.d/ipmi.conf


%post -n hybmon
if [ "$1" -eq 1 ]; then
	/sbin/chkconfig --add hybmond
else
	/sbin/chkconfig hybmond resetpriorities ||:;
	/sbin/service hybmond condrestart ||:;
fi
:;

%preun -n hybmon
if [ "$1" -eq 0 ]; then
	/sbin/service hybmond condstop ||:;
	/sbin/chkconfig --del hybmond ||:;
fi
:;


%post -n nmond
if [ "$1" -eq 1 ]; then
	/sbin/chkconfig --add nmond
else
	/sbin/chkconfig nmond resetpriorities ||:;
	/sbin/service nmond condrestart ||:;
fi
:;

%preun -n nmond
if [ "$1" -eq 0 ]; then
	/sbin/service nmond condstop ||:;
	/sbin/chkconfig --del nmond ||:;
fi
:;


%if "x%{?without_knmonctld}" == "x"
%post -n knmonctld
if [ "$1" -eq 1 ]; then
	/sbin/chkconfig --add knmonctld
else
	/sbin/chkconfig knmonctld resetpriorities ||:;
	/sbin/service knmonctld condrestart ||:;
fi
:;

%preun -n knmonctld
if [ "$1" -eq 0 ]; then
	/sbin/service knmonctld condstop ||:;
	/sbin/chkconfig --del knmonctld ||:;
fi
:;
%endif


%files -n libsmart
%defattr(0644,root,root,-)
%{_libdir}/libsmart.so

%files -n libsmart-devel
%defattr(0644,root,root,-)
%{_includedir}/libsmart.h

%files -n libsmart-devel-static
%defattr(0644,root,root,-)
%{_libdir}/libsmart.a


%files -n libsysnfo
%defattr(0644,root,root,-)
%{_libdir}/libsysnfo.so

%files -n libsysnfo-devel
%defattr(0644,root,root,-)
%{_includedir}/sysnfo.h

%files -n libsysnfo-devel-static
%defattr(0644,root,root,-)
%{_libdir}/libsysnfo.a


%files -n nmond
%defattr(0644,root,root,-)
%attr(0755,root,root) %{_bindir}/nmond
%attr(0755,root,root) %{_bindir}/nmfsd
%attr(0755,root,root) %{_bindir}/nmibd
%attr(0755,root,root) %{_bindir}/nmiod
%attr(0755,root,root) %{_bindir}/nmipmid
%attr(0755,root,root) %{_bindir}/nmsensd
%attr(0755,root,root) %{_bindir}/nmsmartd
%attr(0755,root,root) %{_bindir}/nmhopsad
%attr(0755,root,root) %{_bindir}/nmgpunvd
%if "x%{?without_nmperfd}" == "x"
%attr(0755,root,root) %{_bindir}/nmperfd
%endif
%{_datadir}/nmipmid/*.idf
%{_datadir}/nmsensd/*.hdf
%config(noreplace) %{_sysconfdir}/nmond.conf
%config(noreplace) %{_sysconfdir}/nmon/*.conf
%if %{nmdist} == "clustrx"
%config(noreplace) %{_sysconfdir}/sysconfig/nmond
%endif
%config %{_sysconfdir}/modprobe.d/ipmi.conf
%attr(0755,root,root) %{_initdir}/nmond


%if "x%{?without_knmonctld}" == "x"
%files -n knmonctld
%defattr(0644,root,root,-)
%attr(0755,root,root) %{_bindir}/knmonctld
%config(noreplace) %{_sysconfdir}/knmonctld.conf
%config(noreplace) %{_sysconfdir}/sysconfig/knmonctld
%attr(0755,root,root) %{_initdir}/knmonctld
%endif


%files -n hybmon
%defattr(0644,root,root,-)
%attr(0755,root,root) %{_bindir}/hybmond
%config(noreplace) %{_sysconfdir}/hybmond.conf
%attr(0755,root,root) %{_initdir}/hybmond
%{_libdir}/libhmplugin.so
%{_libdir}/hybmond/*.so



%changelog
* Tue Oct 16 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx8
- hybmond: fixed bug with reading of request

* Tue Oct 16 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx7
- hybmond: add nfsd plugin

* Fri Sep 14 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx6
- nmperfd: fixed incorrect handling of configuration software sensors

* Tue Sep  4 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx5
- nmond: restart failed fix

* Fri Aug 17 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx4
- nmond: fix segfault

* Fri Aug 10 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx3
- nmond: fixed empty fields in the header of the data packet
- nmperfd: fixed empty headers in the TLV

* Fri Aug  3 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx2
- nmond: fixed a segfault in the process of stopping the service
- nmond: starting the service is performed later (start priority 90)
- nmond: fixed stuck modules after a segmentation fault nmond

* Mon Jul 23 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.2-cx1
- nmond: added support for performance counters
- nmond: implemented a protocol to configure version 2
- nmond: many small fixes and improvements

* Tue Jul  3 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx29
- nmond: modified MON_MEMORY_* sensors
- nmipmid: fix typo
- nmibd: fixed problems when working with old libibmad

* Mon Jun 11 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx28
- fixed build for ALT Linux 4.1 HPC

* Mon May 28 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx27
- nmond: fixed restarting modules

* Thu May 24 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx26
- fix knmonctld build

* Thu May 24 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx25
- fix build for clustrx

* Thu May 24 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx24
- NVidia Management Library support
- fix race condition
- improved pre-and post-scripts

* Fri May 11 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx23
- fix defattr in suse

* Fri May 11 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx22
- added chkconfig in post requires

* Thu May 10 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx21
- fix init-script
- added chkconfig on in post-install

* Thu May 10 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx20
- nmibd: fix PortXmitData, PortRcvData divided by 4

* Thu Apr 26 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx19
- implemented a new format of the counter

* Fri Apr 20 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx18
- nmibd: new implementation based on libibmad
- minor fixes in spec files

* Mon Apr 9 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx17
- nmond: fix verification version of the package configuration

* Thu Apr 5 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx16
- nmond: added verification version of the package configuration
- nmond, hybmond: fix default runlevel in init script
- minor changes in makefiles

* Fri Mar 23 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx15
- nmond: rewrited interaction with modules
- nmiod: improved logging
- fix spec file
- fix init-script for CentOS

* Tue Mar 13 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx14
- nmibd: added reopen files in sysfs

* Wed Mar 7 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx13
- added to init-scripts for CentOS and openSUSE

* Fri Mar 2 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx12
- Changed the build system. Static build with dietlibc now optional.

* Wed Feb 29 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx11
- nmhopsad: implemented change the owner of listen socket
- multiple small fixes

* Mon Feb 27 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx10
- added init scripts for nmond and knmonctld
- start checking the configuration utility moved into nmond/knmonctld

* Fri Feb 17 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx9
- nmhopsad: extended communication protocol, implemented client timeout

* Thu Feb 10 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx8
- nmhopsad fix incorrect len value in tlv
- fix nmhopsad install

* Thu Feb 9 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx7
- created nmhopsad module
- nmond added load average sensors

* Wed Dec 14 2011 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.1-cx6
- nmond added VM event sensors

* Wed Oct 19 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx5
- nmibd added (userspace IB monitor)
- raw values in nmsmartd added

* Wed Oct 19 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx4
- nmiod added (userspace IO monitor)

* Tue Sep 5 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx3
- bind error fix

* Thu Aug 4 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx2
- Split Chassis fans and System fans on TB2MM

* Fri Jul 15 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx1
- Make BAD_CNF_INFO persistent

* Mon May 23 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-cx0
- nmipmid: Possible memory leak fix
- nmipmid: optimized CPU utilization (kipmid_max_busy_us)
- nmond: CPU data fix

* Fri Jan 14 2011 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc37
- Typo fix

* Tue Dec 29 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc36
- Fixed race condition in nmond

* Tue Dec 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc35
- nmsmartd: added smart percheck to avoid race condition

* Wed Dec 15 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc34
- Some logging improvements

* Mon Dec 13 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc33
- Added syslog() logging

* Thu Nov 11 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc32
- nmond,knmonctld: added KNMONCTLD_OOM_ADJ environment variable reaction

* Tue Nov 2 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc31
- nmipmid: TB2MM: MON_SYS_FAN -> MON_CHASSIS_FAN

* Mon Nov 1 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc30
- nmond, knmonctld: now random delay calls before module loading

* Thu Oct 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc29
- nmond: removed conflict to knmonctld. knmonctld added as provides

* Thu Oct 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc28
- nmond: symlink to knmonctld added
- nmond, knmonctld: rpm conflicts added

* Tue Sep 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc27
- knmonctld: kernel modules unloading, hwstatus passed to knmhwsd

* Mon Sep 13 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc26
- nmond: added CPU frequences sensor

* Wed Sep 8 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc25
- nmsensd: i2c xfer error handling fix

* Fri Sep 3 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc24
- hybmond: lustre buffer length fix

* Thu Sep 2 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc23
- hybmond: Added init scipt
- nmond/knmonctld: Added kernel modules loading

* Mon Aug 16 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc22
- nmond: cmdline params processing fix

* Fri Aug 13 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc21
- Updated requires

* Mon Aug 9 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc20
- nmipmid, nmsensd: misc platform-dependent fixes

* Thu Aug 5 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc19
- nmipmid: X8* platforms idf update

* Wed Aug 4 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc18
- nmipmid: optimization

* Thu Jul 29 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc17
- hybmond: protocol  fixes

* Mon Jul 26 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc16
- nmipmid: build w/ dietlibc

* Tue Jul 20 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc15
- Sysnfo structure changes
- nmipmid segfault fix

* Tue Jul 20 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc14
- Added: support for different platforms

* Wed Jul 14 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc13
- Added: nmipmid module

* Mon Jun 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc12
- Fix: CPU sensors
- Added MCE counter

* Wed Jun 23 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc11
- HW status sensors moved to the end of packet
- Interface counters fix

* Tue Jun 22 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc10
- Commented fan sensors on X8DTT* platforms
- nmsensd: Tach functions possible division by zero fix
- Config reply packets send to src address
- knmonctld: ioctl calls only if tgt specified

* Fri May 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc9
- TB2 fans removed

* Fri May 28 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc8
- CPU endianess fix

* Wed May 26 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc7
- Added Supermicro X8DTT platform (currently X8DTT-IBX clone)

* Sat May 22 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc6
- New kernel monitoring interface

* Fri Apr 16 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc5
- Fix: TB2 (TYAN S7029) ident fix

* Thu Mar 25 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc4
- Fix: Tyan 7029MM hdf fix

* Tue Mar 23 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc3
- Fix: Tyan 7029MM ident fix
- Fix: Tyan S5382 sensors fix

* Fri Mar 19 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc2
- Fix: nmsensd: X8DTT-IBX platform sensors
- Fix: nmsensd: build with dietlibc

* Tue Feb 23 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc1
- Fix: nmsensd: hdf file names
- Fix: nmsensd: SEGSEGV in hdf postinit
- Fix: nmond/knmonctld: local address initialization
- Fix: nmond/knmonctld: default route interface detection

* Tue Feb 16 2010 Mykola Oleksienko <nickolasbug@gmail.com> 0.1-tmc0
- First release of nodemonframework

