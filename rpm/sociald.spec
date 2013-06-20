Name:       sociald
Summary:    Syncs device data from social services
Version:    0.0.11
Release:    1
Group:      System/Applications
License:    TBD
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(QtDBus)
BuildRequires:  pkgconfig(QtSql)
BuildRequires:  pkgconfig(QtNetwork)
BuildRequires:  pkgconfig(QtContacts)
BuildRequires:  pkgconfig(mlite)
BuildRequires:  pkgconfig(QJson)
BuildRequires:  pkgconfig(libsignon-qt)
BuildRequires:  pkgconfig(accounts-qt)
BuildRequires:  pkgconfig(libsailfishkeyprovider)
BuildRequires:  nemo-qml-plugin-notifications-devel
BuildRequires:  eventfeed-devel
BuildRequires:  libmeegotouchevents-devel
BuildRequires:  buteo-syncfw-devel
Requires: nemo-qml-plugin-notifications
Requires: eventfeed-viewer
Requires: buteo-syncfw-msyncd

%description
A daemon process which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
/usr/lib/buteo-plugins/libsociald-client.so
%config %{_sysconfdir}/buteo/profiles/client/sociald.xml
%config %{_sysconfdir}/buteo/profiles/sync/*.xml
%{_datadir}/lipstick/notificationcategories/x-nemo.social.facebook.notification.conf
%{_datadir}/lipstick/notificationcategories/x-nemo.social.twitter.mention.conf
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
