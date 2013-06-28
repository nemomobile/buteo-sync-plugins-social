Name:       sociald
Summary:    Syncs device data from social services
Version:    0.0.11
Release:    1
Group:      System/Applications
License:    TBD
URL:        https://bitbucket.org/jolla/base-sociald
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
# Looks like that sailfish-components-accounts pc file is missing
# Qt5Gui requires -> hence adding over here.
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(mlite5)
BuildRequires:  pkgconfig(libsignon-qt5)
BuildRequires:  pkgconfig(libsailfishkeyprovider)
BuildRequires:  pkgconfig(sailfishaccounts)
BuildRequires:  nemo-qml-plugin-notifications-qt5-devel
BuildRequires:  eventfeed-devel
BuildRequires:  libmeegotouchevents-qt5-devel
BuildRequires:  buteo-syncfw-qt5-devel
Requires: nemo-qml-plugin-notifications-qt5
Requires: eventfeed-viewer
Requires: buteo-syncfw-qt5-msyncd

%description
A daemon process which provides data synchronization with various social services.

%files
%defattr(-,root,root,-)
/usr/lib/buteo-plugins-qt5/libsociald-client.so
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
%qmake5
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake5_install
