%define _unpackaged_files_terminate_build 0
Name:           hamclock-next
Version:        0.9.0
Release:        1%{?dist}
Summary:        Amateur Radio Clock and Solar/Space Weather Dashboard

License:        MIT
URL:            https://github.com/k4drw/hamclock-next
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
%if !0%{?suse_version}
BuildRequires:  libstdc++-static
%endif
BuildRequires:  libX11-devel
BuildRequires:  libXext-devel
BuildRequires:  libXcursor-devel
BuildRequires:  libXi-devel
BuildRequires:  libXrandr-devel
%if 0%{?suse_version}
BuildRequires:  libXss-devel
%else
BuildRequires:  libXScrnSaver-devel
%endif
BuildRequires:  libXxf86vm-devel
BuildRequires:  libXinerama-devel
BuildRequires:  libcurl-devel
BuildRequires:  openssl-devel
BuildRequires:  libdrm-devel
%if 0%{?suse_version}
BuildRequires:  libgbm-devel
BuildRequires:  Mesa-libEGL-devel
BuildRequires:  Mesa-libGLESv2-devel
BuildRequires:  alsa-devel
BuildRequires:  update-desktop-files
%else
BuildRequires:  mesa-libgbm-devel
BuildRequires:  mesa-libEGL-devel
BuildRequires:  mesa-libGLES-devel
BuildRequires:  alsa-lib-devel
%endif
BuildRequires:  desktop-file-utils

%description
HamClock-Next is a modernized rewrite of the classic ESP8266 HamClock, designed 
for Linux/Raspberry Pi/macOS. It provides real-time space weather, propagation 
models, satellite tracking, and more.

%prep
%setup -q -c

%build
%cmake \
    -DENABLE_DEBUG_API=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DCURL_DISABLE_INSTALL=ON \
    -DSDL_STATIC=ON \
    -DSDL_SHARED=OFF \
    -DSDL_X11=ON \
    -DSDL_WAYLAND=OFF \
    -DSDL_KMSDRM=ON \
    -DSDL_OPENGL=OFF \
    -DSDL_GLES=ON \
    -DSDL2IMAGE_VENDORED=ON \
    -DSDL2IMAGE_SAMPLES=OFF \
    -DSDL2IMAGE_WEBP=OFF \
    -DSDL2IMAGE_TIF=OFF \
    -DSDL2IMAGE_JXL=OFF \
    -DSDL2IMAGE_AVIF=OFF

%cmake_build

%install
%cmake_install
# Install desktop file
install -D -m 0644 packaging/linux/rpm/hamclock-next.desktop %{buildroot}%{_datadir}/applications/hamclock-next.desktop
%if 0%{?suse_version}
%suse_update_desktop_file hamclock-next
%endif
# Install icon
install -D -m 0644 packaging/icon.png %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/hamclock-next.png

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/hamclock-next.desktop

%files
%license LICENSE.md
%doc README.md
%{_bindir}/hamclock-next
%{_datadir}/applications/hamclock-next.desktop
%{_datadir}/icons/hicolor/256x256/apps/hamclock-next.png

%changelog
* Thu Feb 19 2026 Dave <dave@example.com> - 0.8.0-1
- Initial RPM release with desktop integration
