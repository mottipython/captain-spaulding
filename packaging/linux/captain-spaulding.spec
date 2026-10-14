Name:           captain-spaulding
Version:        %{version}
Release:        1%{?dist}
Summary:        Hooray for Captain Spaulding, a paper-doll screensaver
License:        MIT
Source0:        captain-spaulding-%{version}.tar.gz
BuildRequires:  gcc, make, pkgconfig(sdl2)
Requires:       swayidle

%description
Captain Spaulding, the African explorer of the Marx Brothers' Animal Crackers
(1930), as a jointed paper doll cut from the film's publicity photograph, with
brass split pins at his neck, hips and knees. On a Victorian toy-theatre stage
(Pollock's scenery, about 1850) he marches on, marks time, keeps making for
the wings, and dances, kicking his legs right round their pins, to the song
from the film.

On Wayland desktops it starts by itself after five minutes idle.
captain-spaulding-screensaver changes the delay or turns it off.

%prep
%setup -q

%build
make %{?_smp_mflags} CFLAGS="%{optflags}" LDFLAGS="%{?__global_ldflags}"

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix} SYSCONFDIR=%{_sysconfdir}
rm -rf %{buildroot}%{_datadir}/doc/captain-spaulding

%files
%license LICENSE
%doc README.md
%{_bindir}/captain-spaulding
%{_bindir}/captain-spaulding-screensaver
%{_datadir}/applications/captain-spaulding.desktop
%dir %{_datadir}/screensaver-service
%dir %{_datadir}/screensaver-service/savers
%{_datadir}/screensaver-service/savers/captain-spaulding.desktop
%{_datadir}/icons/hicolor/256x256/apps/captain-spaulding.png
%config(noreplace) %{_sysconfdir}/xdg/autostart/captain-spaulding-screensaver.desktop
