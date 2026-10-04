Name:           soundcurrent-eq
%global debug_package %{nil}
Version:        0.3.3
Release:        1%{?dist}
Summary:        Adjustable desktop equalizer for PipeWire
License:        GPL-3.0-only
URL:            https://github.com/rhamenator/soundcurrent-eq
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.20
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.4
Requires:       pipewire-pulseaudio
Requires:       pipewire-utils
Requires:       pulseaudio-utils
Requires:       wireplumber

%description
SoundCurrent EQ is a C++ desktop equalizer with adjustable bands, listening
presets, output device selection, and background tray controls for PipeWire.

%prep
%setup -q

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%{_prefix}
cmake --build build --parallel %{_smp_build_ncpus}

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE
%doc README.md
%{_bindir}/soundcurrent-eq
%{_datadir}/applications/io.github.rhamenator.SoundCurrentEQ.desktop
%{_datadir}/icons/hicolor/scalable/apps/io.github.rhamenator.SoundCurrentEQ.svg
%{_datadir}/doc/soundcurrent-eq/copyright
%{_datadir}/doc/soundcurrent-eq/LICENSE

%changelog
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.3-1
- Add output gain, live level indicators, Loudness preset, and GPLv3 licensing
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.2-1
- Make the preset list scrollable, clarify group separators, and default to Flat
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.1-1
- Add Fedora and RHEL-compatible RPM packaging
