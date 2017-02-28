Name: libnmib
Version: 1.0.0
Release: cx1
Summary: Node monitoring daemon InfiniBand library

Group: System/Libraries
License: GPL/BSD
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{?buildroot:%{buildroot}}%{!?buildroot:%{_tmppath}/%{name}-%{version}-%{release}.%{_arch}}

BuildRequires: libibmad-devel

%description
Node monitoring daemon InfiniBand library


%package devel
Summary: Development files for the libnmib library
Group: System/Libraries
Requires: %{name} = %{version}-%{release}
Requires: libibmad-devel

%description devel
Development files for the libnmib library



%prep
%setup -q


%build
make %{?old_ibmad_compat: OLD_IBMAD_COMPAT=1} VERSION=%{version} %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot} VERSION=%{version} LIBDIR=%{_libdir} INCDIR=%{_includedir}


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%{_libdir}/libnmib.so.*


%files devel
%defattr(-,root,root,-)
%{_libdir}/libnmib.so
%{_includedir}/nmond/*.h

%changelog
* Tue Jul 3 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 1.0.0-cx1
- Changed API
- Fixed incorrect work with older versions of libibmad
- Optimized for reading counters

* Mon Jun 11 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.0.3-cx1
- Added compatibility with older versions of libibmad

* Fri Apr 20 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.0.2-cx0
- Fix build in multiarch environment

* Fri Apr 20 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.0.1-cx1
- Delete ldconfig from post section
- Fix Requires

* Thu Apr 19 2012 Sergey Gorenko <sergey.gorenko@massivesolutions.co.uk> 0.0.1-cx0
- First release of libnmib
