Name:       sociald
Summary:    Syncs device data from social services
Version:    0.0.1
Release:    1
Group:      System/Applications
License:    TBD
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
Source1:    sociald.desktop
Source2:    sociald.service
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(QtDBus)
BuildRequires:  pkgconfig(QtSql)
BuildRequires:  pkgconfig(QtNetwork)
BuildRequires:  pkgconfig(QtContacts)
BuildRequires:  pkgconfig(mlite)
BuildRequires:  pkgconfig(systemd)
BuildRequires:  pkgconfig(QJson)
BuildRequires:  pkgconfig(libsignon-qt)
BuildRequires:  pkgconfig(accounts-qt)
BuildRequires:  eventfeed-devel
BuildRequires:  libmeegotouchevents-devel
Requires: lipstick-jolla-home

%description
A daemon process which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
%{_bindir}/sociald
%{_libdir}/systemd/user/sociald.service
%config /etc/xdg/autostart/sociald.desktop
%{_datadir}/translations/sociald_eng_en.qm


%package ts-devel
Summary:    Translation source for sociald
License:    TBD
Group:      System/Applications

%description ts-devel
Translation source for sociald

%files ts-devel
%defattr(-,root,root,-)
%{_datadir}/translations/source/sociald.ts

%prep
%setup -q -n %{name}-%{version}

%build
%qmake
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake_install
mkdir -p %{buildroot}%{_libdir}/systemd/user/
cp -a %{SOURCE2} %{buildroot}%{_libdir}/systemd/user/
install -D -m 644 %{SOURCE1} %{buildroot}/etc/xdg/autostart/sociald.desktop

