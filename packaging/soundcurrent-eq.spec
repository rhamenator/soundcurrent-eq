Name:           soundcurrent-eq
%global debug_package %{nil}
Version:        1.1.0
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

%check
ctest --test-dir build --output-on-failure --no-tests=error

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE
%doc README.md
%{_bindir}/soundcurrent-eq
%{_libdir}/soundcurrent-eq
%{_datadir}/doc/soundcurrent-eq/LGPL-2.1
%{_datadir}/applications/io.github.rhamenator.SoundCurrentEQ.desktop
%{_datadir}/icons/hicolor/scalable/apps/io.github.rhamenator.SoundCurrentEQ.svg
%{_datadir}/doc/soundcurrent-eq/copyright
%{_datadir}/doc/soundcurrent-eq/LICENSE
%{_datadir}/doc/soundcurrent-eq/THIRD-PARTY-NOTICES.md
%{_datadir}/doc/soundcurrent-eq/speakers
%{_datadir}/doc/soundcurrent-eq/equipment
%{_datadir}/doc/soundcurrent-eq/equipment-profiles.md

%changelog
* Mon Oct 05 2026 rhamenator <rhamenator@gmail.com> - 0.7.5-1
- Add listening enhancements and controls, with advanced Studio parameters

* Mon Oct 05 2026 rhamenator <rhamenator@gmail.com> - 0.7.1-1
- Unify Linux and Windows feature parity on the default branch
- Fit native window frames and defer initial device startup until visible
* Mon Oct 05 2026 rhamenator <rhamenator@gmail.com> - 0.7.0-1
- Add measured speaker profiles and amplifier imports with source attribution
- Hide disconnected microphone jacks and reject clipped calibration capture
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.5.1-1
- Add persistent EQ lock, grouped Undo, safe wheel scrolling, and mic disconnect status
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.5.0-1
- Add speaker and room measurement with 20 Hz to 25 kHz sweep and EQ preview
- Fit the desktop window to the available display area
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.4.0-1
- Add automatic microphone routing, natural voice EQ, and editable input controls
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.7-1
- Apply EQ controls immediately and avoid a second system volume reduction
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.6-1
- Add post-gain slider, stereo balance, and overall output level indicator
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.5-1
- Allow precise 1-100 ms level refresh with faster audio monitor chunks
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.4-1
- Add adjustable live level refresh and optional peak hold markers
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.3-1
- Add output gain, live level indicators, Loudness preset, and GPLv3 licensing
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.2-1
- Make the preset list scrollable, clarify group separators, and default to Flat
* Sun Oct 04 2026 rhamenator <rhamenator@gmail.com> - 0.3.1-1
- Add Fedora and RHEL-compatible RPM packaging
