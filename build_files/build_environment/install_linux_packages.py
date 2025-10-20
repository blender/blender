#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import logging
import os
import re
import subprocess
import sys
import time


DISTRO_ID_DEBIAN = "debian"
DISTRO_ID_FEDORA = "fedora"
DISTRO_ID_SUSE = "suse"
DISTRO_ID_ARCH = "arch"


MAYSUDO = subprocess.run("command -v sudo || command -v doas",
                         shell=True,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         universal_newlines=True).stdout.rstrip('\n')


class LoggingColoredFormatter(logging.Formatter):
    """
    Logging colored formatter,.
    Based on https://alexandra-zaharia.github.io/posts/make-your-own-custom-color-formatter-with-python-logging/
    """
    GREY = '\x1b[38;21m'
    BLUE = '\x1b[38;5;39m'
    YELLOW = '\x1b[38;5;226m'
    RED = '\x1b[38;5;196m'
    BOLD_RED = '\x1b[31;1m'
    RESET = '\x1b[0m'

    def __init__(self, fmt=None):
        super().__init__(fmt=fmt)
        self.FORMATS = {
            logging.DEBUG: self.GREY + "DEBUG:    " + self.RESET + self._fmt,
            logging.INFO: self.BLUE + "INFO:     " + self.RESET + self._fmt,
            logging.WARNING: self.YELLOW + "WARNING: " + self.RESET + self._fmt,
            logging.ERROR: self.RED + "ERROR:    " + self.RESET + self._fmt,
            logging.CRITICAL: self.BOLD_RED + "CRITICAL: " + self.RESET + self._fmt,
        }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


class Package:
    __slots__ = (
        # User-friendly name for the package.
        "name",
        # This is a fake package used to bulk-install a group of packages.
        # There is no version check performed here, and a single missing package will fail the whole thing.
        # Used for the basic sets of build packages and dependencies that can be assumed always available,
        # with stable enough API that the version does not matter (to some extent, it is expected to work with
        # any recent distribution version at least).
        "is_group",
        # Whether Blender can build without this package or not.
        # Note: In case of group packages, all sub-packages inherit from the value of the root group package.
        "is_mandatory",
        # Exact version currently used for pre-built libraries and build-bot builds.
        "version",
        # Ideal version of the package (if possible, prioritize a package of that version), `version` should match it.
        "version_short",
        # Minimal (included)/maximal (excluded) assumed supported version range.
        # Package outside of that range won't be installed.
        "version_min", "version_mex",
        # Actual installed package version.
        "version_installed",
        # Other Packages that depend/are only installed if the 'parent' one is valid.
        "sub_packages",
        # A mapping from distribution name key to distribution package name value.
        # Value may either be:
        #   - A package name string.
        #   - A callback taking the Package and an iterable of its parents as parameters, and returning a string.
        #   - None to indicate that there is no known package for that distribution.
        #   - ... to indicate that this package can be skipped for that distribution
        #     (typically, because it is included in a parent package already).
        "distro_package_names",
    )

    def __init__(self, name, is_group=False, is_mandatory=False,
                 version=None, version_short=None, version_min=None, version_mex=None,
                 sub_packages=(), distro_package_names={}):
        self.name = name
        self.is_group = is_group
        self.is_mandatory = is_mandatory
        self.version = version
        self.version_short = version_short
        self.version_min = version_min
        self.version_mex = version_mex
        self.version_installed = ...
        self.sub_packages = sub_packages
        self.distro_package_names = distro_package_names

    def __repr__(self):
        is_mandatory_repr = "[mandatory]" if self.is_mandatory else ""
        is_group_repr = "[group]" if self.is_group else ""
        return (
            f"{self.name} ({self.version_short}) {is_mandatory_repr}{is_group_repr}:\n"
            f"\t{self.version} ({self.version_min} ... {self.version_mex}) ==> {self.version_installed}"
        )


# Absolute minimal required tools to build Blender.
BUILD_MANDATORY_SUBPACKAGES = (
    Package(name="Build Essentials", is_group=True,
            sub_packages=(
                Package(name="GCC",
                        distro_package_names={DISTRO_ID_DEBIAN: ...,
                                              DISTRO_ID_FEDORA: "gcc",
                                              DISTRO_ID_SUSE: "gcc",
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
                Package(name="GCC-C++",
                        distro_package_names={DISTRO_ID_DEBIAN: ...,
                                              DISTRO_ID_FEDORA: "gcc-c++",
                                              DISTRO_ID_SUSE: "gcc-c++",
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
                Package(name="make",
                        distro_package_names={DISTRO_ID_DEBIAN: ...,
                                              DISTRO_ID_FEDORA: "make",
                                              DISTRO_ID_SUSE: "make",
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
                Package(name="glibc",
                        distro_package_names={DISTRO_ID_DEBIAN: ...,
                                              DISTRO_ID_FEDORA: "glibc-devel",
                                              DISTRO_ID_SUSE: "glibc-devel",
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "build-essential",
                                  DISTRO_ID_FEDORA: ...,
                                  DISTRO_ID_SUSE: ...,
                                  DISTRO_ID_ARCH: "base-devel",
                                  },
            ),
    Package(name="Git", is_group=True,
            sub_packages=(
                Package(name="Git LFS",
                        distro_package_names={DISTRO_ID_DEBIAN: "git-lfs",
                                              DISTRO_ID_FEDORA: "git-lfs",
                                              DISTRO_ID_SUSE: "git-lfs",
                                              DISTRO_ID_ARCH: "git-lfs",
                                              },
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "git",
                                  DISTRO_ID_FEDORA: "git",
                                  DISTRO_ID_SUSE: "git",
                                  DISTRO_ID_ARCH: "git",
                                  },
            ),
    Package(name="CMake",
            distro_package_names={DISTRO_ID_DEBIAN: "cmake",
                                  DISTRO_ID_FEDORA: "cmake",
                                  DISTRO_ID_SUSE: "cmake",
                                  DISTRO_ID_ARCH: "cmake",
                                  },
            ),
)


# Fairly common additional tools useful to build Blender.
BUILD_OPTIONAL_SUBPACKAGES = (
    Package(name="Ninja Builder",
            distro_package_names={DISTRO_ID_DEBIAN: "ninja-build",
                                  DISTRO_ID_FEDORA: "ninja-build",
                                  DISTRO_ID_SUSE: "ninja",
                                  DISTRO_ID_ARCH: "ninja",
                                  },
            ),
    Package(name="CMake commandline GUI",
            distro_package_names={DISTRO_ID_DEBIAN: "cmake-curses-gui",
                                  DISTRO_ID_FEDORA: None,
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: None,
                                  },
            ),
    Package(name="CMake GUI",
            distro_package_names={DISTRO_ID_DEBIAN: "cmake-gui",
                                  DISTRO_ID_FEDORA: "cmake-gui",
                                  DISTRO_ID_SUSE: "cmake-gui",
                                  DISTRO_ID_ARCH: None,
                                  },
            ),
    Package(name="Patch",
            distro_package_names={DISTRO_ID_DEBIAN: "patch",
                                  DISTRO_ID_FEDORA: "patch",
                                  DISTRO_ID_SUSE: "patch",
                                  DISTRO_ID_ARCH: "patch",
                                  },
            ),
)


# Library dependencies that are not provided by precompiled libraries.
DEPS_CRITICAL_SUBPACKAGES = (
    Package(name="X11 library",
            distro_package_names={DISTRO_ID_DEBIAN: "libx11-dev",
                                  DISTRO_ID_FEDORA: "libX11-devel",
                                  DISTRO_ID_SUSE: "libX11-devel",
                                  DISTRO_ID_ARCH: "libx11",
                                  },
            ),
    Package(name="Xxf86vm Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxxf86vm-dev",
                                  DISTRO_ID_FEDORA: "libXxf86vm-devel",
                                  DISTRO_ID_SUSE: "libXxf86vm-devel",
                                  DISTRO_ID_ARCH: "libxxf86vm",
                                  },
            ),
    Package(name="XCursor Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxcursor-dev",
                                  DISTRO_ID_FEDORA: "libXcursor-devel",
                                  DISTRO_ID_SUSE: "libXcursor-devel",
                                  DISTRO_ID_ARCH: "libxcursor",
                                  },
            ),
    Package(name="Xi Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxi-dev",
                                  DISTRO_ID_FEDORA: "libXi-devel",
                                  DISTRO_ID_SUSE: "libXi-devel",
                                  DISTRO_ID_ARCH: "libxi",
                                  },
            ),
    Package(name="XRandr Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxrandr-dev",
                                  DISTRO_ID_FEDORA: "libXrandr-devel",
                                  DISTRO_ID_SUSE: "libXrandr-devel",
                                  DISTRO_ID_ARCH: "libxrandr",
                                  },
            ),
    Package(name="Xinerama Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxinerama-dev",
                                  DISTRO_ID_FEDORA: "libXinerama-devel",
                                  DISTRO_ID_SUSE: "libXinerama-devel",
                                  DISTRO_ID_ARCH: "libxinerama",
                                  },
            ),
    Package(name="XKbCommon Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxkbcommon-dev",
                                  DISTRO_ID_FEDORA: "libxkbcommon-devel",
                                  DISTRO_ID_SUSE: "libxkbcommon-devel",
                                  DISTRO_ID_ARCH: "libxkbcommon",
                                  },
            ),
    Package(name="Wayland Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libwayland-dev",
                                  DISTRO_ID_FEDORA: "wayland-devel",
                                  DISTRO_ID_SUSE: "wayland-devel",
                                  DISTRO_ID_ARCH: "wayland",
                                  },
            ),
    Package(name="Decor Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libdecor-0-dev",
                                  DISTRO_ID_FEDORA: "libdecor-devel",
                                  DISTRO_ID_SUSE: "libdecor-devel",
                                  DISTRO_ID_ARCH: "libdecor",
                                  },
            ),
    Package(name="Wayland Protocols",
            distro_package_names={DISTRO_ID_DEBIAN: "wayland-protocols",
                                  DISTRO_ID_FEDORA: "wayland-protocols-devel",
                                  DISTRO_ID_SUSE: "wayland-protocols-devel",
                                  DISTRO_ID_ARCH: "wayland-protocols",
                                  },
            ),
    Package(name="DBus Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libdbus-1-dev",
                                  DISTRO_ID_FEDORA: "dbus-devel",
                                  DISTRO_ID_SUSE: "dbus-1-devel",
                                  DISTRO_ID_ARCH: "dbus",
                                  },
            ),
    Package(name="OpenGL Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libgl-dev",
                                  DISTRO_ID_FEDORA: "mesa-libGL-devel",
                                  DISTRO_ID_SUSE: "Mesa-libGL-devel",
                                  DISTRO_ID_ARCH: "libglvnd",
                                  },
            ),
    Package(name="EGL Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libegl-dev",
                                  DISTRO_ID_FEDORA: "mesa-libEGL-devel",
                                  DISTRO_ID_SUSE: "Mesa-libEGL-devel",
                                  DISTRO_ID_ARCH: None,  # Included in `libglvnd`.
                                  },
            ),
)


# Basic mandatory set of common libraries to build Blender, which are also available as pre-compiled libraries.
DEPS_MANDATORY_SUBPACKAGES = (
    Package(name="JPEG Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libjpeg-dev",
                                  DISTRO_ID_FEDORA: "libjpeg-turbo-devel",
                                  DISTRO_ID_SUSE: "libjpeg8-devel",
                                  DISTRO_ID_ARCH: "libjpeg-turbo",
                                  },
            ),
    Package(name="PNG Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpng-dev",
                                  DISTRO_ID_FEDORA: "libpng-devel",
                                  DISTRO_ID_SUSE: "libpng16-compat-devel",
                                  DISTRO_ID_ARCH: "libpng",
                                  },
            ),
    Package(name="FreeType Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libfreetype-dev",
                                  DISTRO_ID_FEDORA: "freetype-devel",
                                  DISTRO_ID_SUSE: "freetype2-devel",
                                  DISTRO_ID_ARCH: "freetype2",
                                  },
            ),
    Package(name="FontConfig Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libfontconfig-dev",
                                  DISTRO_ID_FEDORA: "fontconfig",
                                  DISTRO_ID_SUSE: "fontconfig",
                                  DISTRO_ID_ARCH: "fontconfig",
                                  },
            ),
    Package(name="ZStandard Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libzstd-dev",
                                  DISTRO_ID_FEDORA: "libzstd-devel",
                                  DISTRO_ID_SUSE: "libzstd-devel",
                                  DISTRO_ID_ARCH: "zstd",
                                  },
            ),
    Package(name="BZ2 Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libbz2-dev",
                                  DISTRO_ID_FEDORA: "bzip2-devel",
                                  DISTRO_ID_SUSE: "libbz2-devel",
                                  DISTRO_ID_ARCH: "bzip2",
                                  },
            ),
    Package(name="LZMA Library",
            distro_package_names={DISTRO_ID_DEBIAN: "liblzma-dev",
                                  DISTRO_ID_FEDORA: "lzma-sdk-devel",  # ???
                                  DISTRO_ID_SUSE: "lzma-sdk-devel",  # ???
                                  DISTRO_ID_ARCH: "xz",  # ???
                                  },
            ),
    Package(name="SDL2 Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libsdl2-dev",
                                  DISTRO_ID_FEDORA: "SDL2-devel",
                                  DISTRO_ID_SUSE: "SDL2-devel",
                                  DISTRO_ID_ARCH: "sdl2",
                                  },
            ),
    Package(name="ShaderC Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libshaderc-dev",
                                  DISTRO_ID_FEDORA: "libshaderc-devel",
                                  DISTRO_ID_SUSE: "shaderc-devel",
                                  DISTRO_ID_ARCH: "shaderc",
                                  },
            ),
    Package(name="Epoxy Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libepoxy-dev",
                                  DISTRO_ID_FEDORA: "libepoxy-devel",
                                  DISTRO_ID_SUSE: "libepoxy-devel",
                                  DISTRO_ID_ARCH: "libepoxy",
                                  },
            ),
    Package(name="XML2 Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libxml2-dev",
                                  DISTRO_ID_FEDORA: "libxml2-devel",
                                  DISTRO_ID_SUSE: "libxml2-devel",
                                  DISTRO_ID_ARCH: "libxml2",
                                  },
            ),
    Package(name="Haru Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libhpdf-dev",
                                  DISTRO_ID_FEDORA: "libharu-devel",
                                  DISTRO_ID_SUSE: "libharu-devel",
                                  DISTRO_ID_ARCH: "libharu",
                                  },
            ),
    Package(name="PyString Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpystring-dev",
                                  DISTRO_ID_FEDORA: "pystring-devel",
                                  DISTRO_ID_SUSE: "pystring-devel",
                                  DISTRO_ID_ARCH: "pystring",
                                  },
            ),
)


# Basic optional set of common libraries to build Blender, which are also available as pre-compiled libraries.
DEPS_OPTIONAL_SUBPACKAGES = (
    Package(name="OpenJPG Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libopenjp2-7-dev",
                                  DISTRO_ID_FEDORA: "openjpeg2-devel",
                                  DISTRO_ID_SUSE: "openjpeg2-devel",
                                  DISTRO_ID_ARCH: "openjpeg2",
                                  },
            ),
    Package(name="TIFF Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libtiff-dev",
                                  DISTRO_ID_FEDORA: "libtiff-devel",
                                  DISTRO_ID_SUSE: "libtiff-devel",
                                  DISTRO_ID_ARCH: "libtiff",
                                  },
            ),
    Package(name="Jack2 Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libjack-jackd2-dev",
                                  DISTRO_ID_FEDORA: "jack-audio-connection-kit-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "jack2",
                                  },
            ),
    Package(name="Pulse Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpulse-dev",
                                  DISTRO_ID_FEDORA: "pulseaudio-libs-devel",
                                  DISTRO_ID_SUSE: "libpulse-devel",
                                  DISTRO_ID_ARCH: "libpulse",
                                  },
            ),
    Package(name="Pipewire Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpipewire-0.3-dev",
                                  DISTRO_ID_FEDORA: "pipewire-devel",
                                  DISTRO_ID_SUSE: "pipewire-devel",
                                  DISTRO_ID_ARCH: "pipewire",
                                  },
            ),
    Package(name="OpenAL Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libopenal-dev",
                                  DISTRO_ID_FEDORA: "openal-soft-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "openal",
                                  },
            ),
    Package(name="SndFile Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libsndfile1-dev",
                                  DISTRO_ID_FEDORA: "libsndfile-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "libsndfile",
                                  },
            ),
    Package(name="JEMalloc Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libjemalloc-dev",
                                  DISTRO_ID_FEDORA: "jemalloc-devel",
                                  DISTRO_ID_SUSE: "jemalloc-devel",
                                  DISTRO_ID_ARCH: "jemalloc",
                                  },
            ),
    Package(name="Vulkan Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libvulkan-dev",
                                  DISTRO_ID_FEDORA: ...,
                                  DISTRO_ID_SUSE: ...,
                                  DISTRO_ID_ARCH: ...,
                                  },
            ),
    Package(name="Vulkan Headers",
            distro_package_names={DISTRO_ID_DEBIAN: ...,
                                  DISTRO_ID_FEDORA: "vulkan-headers",
                                  DISTRO_ID_SUSE: "vulkan-headers",
                                  DISTRO_ID_ARCH: "vulkan-headers",
                                  },
            ),
    Package(name="Vulkan ICD Loader",
            distro_package_names={DISTRO_ID_DEBIAN: ...,
                                  DISTRO_ID_FEDORA: "vulkan-loader-devel",
                                  DISTRO_ID_SUSE: ...,
                                  DISTRO_ID_ARCH: "vulkan-icd-loader",
                                  },
            ),
    Package(name="GMP Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libgmp-dev",
                                  DISTRO_ID_FEDORA: "gmp-devel",
                                  DISTRO_ID_SUSE: "gmp-devel",
                                  DISTRO_ID_ARCH: "gmp",
                                  },
            ),
    Package(name="PugiXML Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpugixml-dev",
                                  DISTRO_ID_FEDORA: "pugixml-devel",
                                  DISTRO_ID_SUSE: "pugixml-devel",
                                  DISTRO_ID_ARCH: "pugixml",
                                  },
            ),
    Package(name="FFTW3 Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libfftw3-dev",
                                  DISTRO_ID_FEDORA: "fftw-devel",
                                  DISTRO_ID_SUSE: "fftw-devel",
                                  DISTRO_ID_ARCH: "fftw",
                                  },
            ),
    Package(name="POTrace Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libpotrace-dev",
                                  DISTRO_ID_FEDORA: "potrace-devel",
                                  DISTRO_ID_SUSE: "potrace-devel",
                                  DISTRO_ID_ARCH: "potrace",
                                  },
            ),
    Package(name="Yaml CPP Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libyaml-cpp-dev",
                                  DISTRO_ID_FEDORA: "yaml-cpp-devel",
                                  DISTRO_ID_SUSE: "yaml-cpp-devel",
                                  DISTRO_ID_ARCH: "yaml-cpp",
                                  },
            ),
    Package(name="Deflate Library",
            distro_package_names={DISTRO_ID_DEBIAN: "libdeflate-dev",
                                  DISTRO_ID_FEDORA: "libdeflate-devel",
                                  DISTRO_ID_SUSE: "libdeflate-devel",
                                  DISTRO_ID_ARCH: "libdeflate",
                                  },
            ),
)


# Python packages that should be available for Blender Pythons-scripts.
# SUSE uses names like `python310-Cython` for its python module packages...
def suse_pypackages_name_gen(name):
    def _gen(package, parent_packages):
        pp = parent_packages[-1]
        if pp is not None and pp.version_installed is not ...:
            v = "".join(str(i) for i in PackageInstaller.version_tokenize(pp.version_installed)[0][:2])
            return "python" + v + "-" + name
    return _gen


PYTHON_SUBPACKAGES = (
    Package(name="Cython",
            version="3.0.11", version_short="3.0", version_min="3.0", version_mex="4.0",
            distro_package_names={DISTRO_ID_DEBIAN: "cython3",
                                  DISTRO_ID_FEDORA: "python3-Cython",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("Cython"),
                                  DISTRO_ID_ARCH: "cython",
                                  },
            ),
    Package(name="IDNA",
            version="3.10", version_short="3.10", version_min="2.0", version_mex="4.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-idna",
                                  DISTRO_ID_FEDORA: "python3-idna",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("idna"),
                                  DISTRO_ID_ARCH: "python-idna",
                                  },
            ),
    Package(name="Charset Normalizer",
            version="3.4.1", version_short="3.4", version_min="2.0.6", version_mex="4.0.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-charset-normalizer",
                                  DISTRO_ID_FEDORA: "python3-charset-normalizer",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("charset-normalizer"),
                                  DISTRO_ID_ARCH: "python-charset-normalizer",
                                  },
            ),
    Package(name="URLLib",
            version="2.4.0", version_short="2.4", version_min="1.0", version_mex="3.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-urllib3",
                                  DISTRO_ID_FEDORA: "python3-urllib3",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("urllib3"),
                                  DISTRO_ID_ARCH: "python-urllib3",
                                  },
            ),
    Package(name="Certifi",
            version="2025.4.26", version_short="2025.4", version_min="2021.0", version_mex="2026.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-certifi",
                                  DISTRO_ID_FEDORA: "python3-certifi",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("certifi"),
                                  DISTRO_ID_ARCH: "python-certifi",
                                  },
            ),
    Package(name="Requests",
            version="2.32.2", version_short="2.32", version_min="2.0", version_mex="3.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-requests",
                                  DISTRO_ID_FEDORA: "python3-requests",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("requests"),
                                  DISTRO_ID_ARCH: "python-requests",
                                  },
            ),
    Package(name="ZStandard",
            version="0.23.0", version_short="0.23", version_min="0.15.2", version_mex="1.0.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-zstandard",
                                  DISTRO_ID_FEDORA: "python3-zstandard",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("zstandard"),
                                  DISTRO_ID_ARCH: "python-zstandard",
                                  },
            ),
    Package(name="NumPy",
            version="1.26.4", version_short="1.26", version_min="1.14", version_mex="2.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python3-numpy",
                                  DISTRO_ID_FEDORA: "python3-numpy",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("numpy"),
                                  DISTRO_ID_ARCH: "python-numpy",
                                  },
            ),
    Package(name="NumPy Devel",
            version="1.26.4", version_short="1.26", version_min="1.14", version_mex="2.0",
            distro_package_names={DISTRO_ID_DEBIAN: ...,
                                  DISTRO_ID_FEDORA: ...,
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("numpy-devel"),
                                  DISTRO_ID_ARCH: ...,
                                  },
            ),
    Package(name="fastjsonschema",
            version="2.21.1", version_short="2.21", version_min="2.21", version_mex="3.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python-fastjsonschema",
                                  DISTRO_ID_FEDORA: "python-fastjsonschema",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("fastjsonschema"),
                                  DISTRO_ID_ARCH: "python-fastjsonschema",
                                  },
            ),
    Package(name="cattrs",
            version="25.1.1", version_short="25.1", version_min="25.1", version_mex="26.0",
            distro_package_names={DISTRO_ID_DEBIAN: "python-cattrs",
                                  DISTRO_ID_FEDORA: "python-cattrs",
                                  DISTRO_ID_SUSE: suse_pypackages_name_gen("cattrs"),
                                  DISTRO_ID_ARCH: "python-cattrs",
                                  },
            ),
)


# Packages required to build Blender, which are not included in the precompiled libraries.
PACKAGES_BASICS_BUILD = (
    Package(name="Basics Mandatory Build", is_group=True, is_mandatory=True, sub_packages=BUILD_MANDATORY_SUBPACKAGES),
    Package(name="Basics Optional Build", is_group=True, is_mandatory=False, sub_packages=BUILD_OPTIONAL_SUBPACKAGES),
    Package(name="Basic Critical Deps", is_group=True, is_mandatory=True, sub_packages=DEPS_CRITICAL_SUBPACKAGES),
)


# All packages, required or 'nice to have', to build Blender.
# Also covers (as best as possible) the dependencies provided by the precompiled libraries.
PACKAGES_ALL = (
    Package(name="Basics Mandatory Build", is_group=True, is_mandatory=True, sub_packages=BUILD_MANDATORY_SUBPACKAGES),
    Package(name="Basics Optional Build", is_group=True, is_mandatory=False, sub_packages=BUILD_OPTIONAL_SUBPACKAGES),
    Package(name="Basic Critical Deps", is_group=True, is_mandatory=True, sub_packages=DEPS_CRITICAL_SUBPACKAGES),
    Package(name="Basic Mandatory Deps", is_group=True, is_mandatory=True, sub_packages=DEPS_MANDATORY_SUBPACKAGES),
    Package(name="Basic Optional Deps", is_group=True, is_mandatory=False, sub_packages=DEPS_OPTIONAL_SUBPACKAGES),

    Package(name="Clang Format",
            version="10.0", version_short="10.0", version_min="6.0", version_mex="15.0",
            distro_package_names={DISTRO_ID_DEBIAN: "clang-format",
                                  DISTRO_ID_FEDORA: "clang",  # clang-format is part of the main clang package.
                                  DISTRO_ID_SUSE: "clang",  # clang-format is part of the main clang package.
                                  DISTRO_ID_ARCH: "clang",  # clang-format is part of the main clang package.
                                  },
            ),
    Package(name="Python", is_mandatory=True,
            version="3.11.11", version_short="3.11", version_min="3.11", version_mex="3.14",
            sub_packages=PYTHON_SUBPACKAGES,
            distro_package_names={DISTRO_ID_DEBIAN: "python3-dev",
                                  DISTRO_ID_FEDORA: "python3-devel",
                                  DISTRO_ID_SUSE: "python3-devel",
                                  DISTRO_ID_ARCH: "python",
                                  },
            ),
    Package(name="TBB Library", is_mandatory=True,
            version="2021.13.0", version_short="2021", version_min="2021.0.0", version_mex="2023.0.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libtbb-dev",
                                  DISTRO_ID_FEDORA: "tbb-devel",
                                  DISTRO_ID_SUSE: "tbb-devel",
                                  DISTRO_ID_ARCH: "intel-oneapi-tbb",
                                  },
            ),
    Package(name="OpenColorIO Library", is_mandatory=False,
            version="2.4.1", version_short="2.4", version_min="2.0", version_mex="3.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libopencolorio-dev",
                                  DISTRO_ID_FEDORA: "OpenColorIO-devel",
                                  DISTRO_ID_SUSE: "OpenColorIO-devel",
                                  DISTRO_ID_ARCH: "opencolorio",
                                  },
            ),
    Package(name="IMath Library", is_mandatory=False,
            version="3.2.1", version_short="3.2", version_min="3.0", version_mex="4.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libimath-dev",
                                  DISTRO_ID_FEDORA: "imath-devel",
                                  DISTRO_ID_SUSE: "Imath-devel",
                                  DISTRO_ID_ARCH: "imath",
                                  },
            ),
    Package(name="OpenEXR Library", is_mandatory=False,
            version="3.3.2", version_short="3.3", version_min="3.0", version_mex="4.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libopenexr-dev",
                                  DISTRO_ID_FEDORA: "openexr-devel",
                                  DISTRO_ID_SUSE: "openexr-devel",
                                  DISTRO_ID_ARCH: "openexr",
                                  },
            ),
    Package(name="OpenImageIO Library", is_mandatory=True,
            version="3.0.3.1", version_short="3.0", version_min="2.5.0", version_mex="3.1.0",
            sub_packages=(
                Package(name="OpenImageIO Tools", is_mandatory=False,
                        distro_package_names={DISTRO_ID_DEBIAN: "openimageio-tools",
                                              DISTRO_ID_FEDORA: "OpenImageIO-utils",
                                              DISTRO_ID_SUSE: "OpenImageIO",  # ???
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "libopenimageio-dev",
                                  DISTRO_ID_FEDORA: "OpenImageIO-devel",
                                  DISTRO_ID_SUSE: "OpenImageIO-devel",
                                  DISTRO_ID_ARCH: "openimageio",
                                  },
            ),
    Package(name="LLVM Library", is_mandatory=False,
            version="17.0.6", version_short="17.0", version_min="15.0", version_mex="18.0",
            sub_packages=(
                Package(name="Clang Compiler", is_mandatory=False,
                        distro_package_names={DISTRO_ID_DEBIAN: "clang",
                                              DISTRO_ID_FEDORA: "clang-devel",
                                              DISTRO_ID_SUSE: "clang-devel",
                                              DISTRO_ID_ARCH: "clang",
                                              },
                        ),
                Package(name="Clang Library", is_mandatory=False,
                        distro_package_names={DISTRO_ID_DEBIAN: "libclang-dev",
                                              DISTRO_ID_FEDORA: ...,
                                              DISTRO_ID_SUSE: ...,
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "llvm-dev",
                                  DISTRO_ID_FEDORA: "llvm-devel",
                                  DISTRO_ID_SUSE: "llvm-devel",
                                  DISTRO_ID_ARCH: "llvm",
                                  },
            ),
    Package(name="OpenShadingLanguage Library", is_mandatory=False,
            version="1.14.3.0", version_short="1.14", version_min="1.11", version_mex="2.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,  # No package currently.
                                  DISTRO_ID_FEDORA: "openshadinglanguage-devel",
                                  DISTRO_ID_SUSE: "OpenShadingLanguage-devel",
                                  DISTRO_ID_ARCH: "openshadinglanguage",
                                  },
            ),
    Package(name="OpenSubDiv Library", is_mandatory=False,
            version="3.6.0", version_short="3.6", version_min="3.5", version_mex="4.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libosd-dev",
                                  DISTRO_ID_FEDORA: "opensubdiv-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "opensubdiv",
                                  },
            ),
    Package(name="OpenVDB Library", is_mandatory=False,
            version="12.0.0", version_short="12.0", version_min="11.0", version_mex="13.0",
            sub_packages=(
                # Assume packaged versions of the dependencies are compatible with OpenVDB package.
                Package(name="OpenVDB Dependencies", is_mandatory=False, is_group=True,
                        sub_packages=(
                            Package(name="Blosc Library", is_mandatory=False,
                                    distro_package_names={DISTRO_ID_DEBIAN: "libblosc-dev",
                                                          DISTRO_ID_FEDORA: "blosc-devel",
                                                          DISTRO_ID_SUSE: "blosc-devel",
                                                          DISTRO_ID_ARCH: "blosc",
                                                          },
                                    ),
                            Package(name="NanoVDB Library", is_mandatory=False,
                                    distro_package_names={DISTRO_ID_DEBIAN: "libnanovdb-dev",
                                                          DISTRO_ID_FEDORA: ...,  # Part of OpenVDB package.
                                                          DISTRO_ID_SUSE: None,
                                                          DISTRO_ID_ARCH: ...,   # Part of OpenVDB package.
                                                          },
                                    ),
                        ),
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "libopenvdb-dev",
                                  DISTRO_ID_FEDORA: "openvdb-devel",
                                  DISTRO_ID_SUSE: None,  # No known package yet.
                                  DISTRO_ID_ARCH: "openvdb",
                                  },
            ),
    Package(name="Alembic Library", is_mandatory=False,
            version="1.8.3", version_short="1.8", version_min="1.7", version_mex="2.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: "alembic-devel",
                                  DISTRO_ID_SUSE: "alembic-devel",
                                  DISTRO_ID_ARCH: "alembic",
                                  },
            ),
    Package(name="MaterialX Library", is_mandatory=False,
            version="1.39.10", version_short="1.39", version_min="1.38", version_mex="1.40",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: None,
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "materialx-git",
                                  },
            ),
    Package(name="USD Library", is_mandatory=False,
            version="25.02", version_short="25.02", version_min="24.05", version_mex="26.00",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: "usd-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "usd",  # No official package, in AUR only currently.
                                  },
            ),
    Package(name="Embree Library", is_mandatory=False,
            version="4.4.0", version_short="4.4", version_min="4.3", version_mex="5.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libembree-dev",
                                  DISTRO_ID_FEDORA: "embree-devel",
                                  DISTRO_ID_SUSE: "embree-devel",
                                  DISTRO_ID_ARCH: "embree",
                                  },
            ),
    Package(name="OpenImageDenoiser Library", is_mandatory=False,
            version="2.3.3", version_short="2.3", version_min="2.0.0", version_mex="3.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: "oidn-devel",
                                  DISTRO_ID_SUSE: "OpenImageDenoise-devel",
                                  DISTRO_ID_ARCH: "openimagedenoise",
                                  },
            ),
    Package(name="Level Zero Library", is_mandatory=False,
            version="1.19.2", version_short="1.19", version_min="1.7", version_mex="2.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libze-dev",
                                  DISTRO_ID_FEDORA: "oneapi-level-zero-devel",
                                  DISTRO_ID_SUSE: "level-zero-devel",
                                  DISTRO_ID_ARCH: "level-zero-headers",  # ???
                                  },
            ),
    Package(name="OpenPGL Library", is_mandatory=False,
            version="0.6.0", version_short="0.6", version_min="0.5.0", version_mex="0.7",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: "openpgl-devel",
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "openpgl",
                                  },
            ),
    Package(name="XROpenXR Library", is_mandatory=False,
            version="1.0.22", version_short="1.0", version_min="1.0.8", version_mex="2.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libopenxr-dev",
                                  DISTRO_ID_FEDORA: None,
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: "openxr",
                                  },
            ),
    Package(name="FFMPEG Library", is_mandatory=False,
            version="7.1.1", version_short="7.1", version_min="4.0", version_mex="8.0",
            sub_packages=(
                Package(name="AVDevice FFMPEG Library", is_mandatory=False,
                        distro_package_names={DISTRO_ID_DEBIAN: "libavdevice-dev",
                                              DISTRO_ID_FEDORA: ...,
                                              DISTRO_ID_SUSE: ...,
                                              DISTRO_ID_ARCH: ...,
                                              },
                        ),
            ),
            distro_package_names={DISTRO_ID_DEBIAN: "ffmpeg",
                                  DISTRO_ID_FEDORA: "ffmpeg-free-devel",
                                  DISTRO_ID_SUSE: "ffmpeg-devel",
                                  DISTRO_ID_ARCH: "ffmpeg",
                                  },
            ),
    Package(name="harfbuzz", is_mandatory=False,
            version="10.0.1", version_short="10", version_min="5.1.0", version_mex="20.0.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: "libharfbuzz-dev",
                                  DISTRO_ID_FEDORA: "harfbuzz-devel",
                                  DISTRO_ID_SUSE: "harfbuzz-devel",
                                  DISTRO_ID_ARCH: "harfbuzz",
                                  },
            ),
    Package(name="manifold", is_mandatory=False,
            version="3.1.0", version_short="3.1", version_min="3.1.0", version_mex="4.0.0",
            sub_packages=(),
            distro_package_names={DISTRO_ID_DEBIAN: None,
                                  DISTRO_ID_FEDORA: None,
                                  DISTRO_ID_SUSE: None,
                                  DISTRO_ID_ARCH: None,
                                  },
            ),
)


class ProgressBar:
    """Very basic progress bar printing in the console."""

    def __init__(self, min_value=0, max_value=100, print_len=80, is_known_limit=True):
        self.value = 0
        self.min_value = min_value
        self.max_value = max_value
        self.print_len = print_len
        self.is_known_limit = is_known_limit
        self.print_stdout()

    def update(self, steps=1):
        self.value += steps
        self.print_stdout()

    def finish(self):
        print("\033[2K\r", end="")

    def print_stdout(self):
        print("\r", self, end="")

    def __repr__(self):
        value_print_len = self.print_len - 2
        range_value = self.max_value - self.min_value
        diff_to_min = self.value - self.min_value
        value = (self.value % range_value) / range_value * value_print_len
        if (diff_to_min // range_value) % 2 == 0:
            value_str = "*" * int(value) + " " * (value_print_len - int(value))
        else:
            value_str = " " * int(value) + "*" * (value_print_len - int(value))
        if self.is_known_limit:
            return f"[{value_str}]"
        return f">{value_str}<"


class PackageInstaller:
    """Parent class of all package installers, does nothing but printing list of packages and defining the 'interface'.
    """
    _instance = None

    def __new__(cls, settings):
        if cls._instance is None:
            cls._instance = super(PackageInstaller, cls).__new__(cls)
        cls._instance.settings = settings
        return cls._instance

    def run_command(self, command):
        """Basic wrapper around ``subprocess.Popen``, mimicking ``subprocess.run`` with a basic progress bar."""
        # First dummy call to get user password for `sudo`. Otherwise the progress bar on actual commands
        # makes it impossible for users to enter their password.
        if not self.settings.no_sudo:
            subprocess.run([MAYSUDO, "echo"], capture_output=True)

        p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        pbar = ProgressBar(is_known_limit=False)
        while p.poll() is None:
            pbar.update(steps=2)
            time.sleep(0.05)
        pbar.finish()
        return subprocess.CompletedProcess(
            args=command,
            returncode=p.returncode,
            stdout=p.stdout.read(),
            stderr=p.stderr.read())

    @property
    def can_install(self):
        return not self.settings.no_sudo and self.__class__ != PackageInstaller

    # Version utils, should not need to be redefined in each sub-class.
    # ----------
    _re_version_sanitize = re.compile(r"(?P<version>([0-9]+\.?)+([0-9]+)).*")

    @classmethod
    def version_sanitize(cls, version):
        """
        Sanitize a version string by removing 'extras' like `_RC2` in `1.2_RC2`.
        Note that the version string is expected to start with at least two numbers
        separated by a dot.
        """
        version = cls._re_version_sanitize.search(version)
        return version["version"] if version is not None else version

    @classmethod
    def version_tokenize(cls, *args):
        """
        Tokenize an iterable of sanitized version strings into tuples of integers of a same length,
        filling missing items with zero values.
        """
        versions = tuple(tuple(int(i) for i in cls.version_sanitize(v).split(".")) for v in args)
        maxlen = max(len(v) for v in versions)
        return tuple(v + (0,) * (maxlen - len(v)) for v in versions)

    @classmethod
    def version_match(cls, version, ref_version):
        """
        Return True if the ``version`` string falls into the version range covered by the ``ref_version`` string.
        ``version`` should be at least as long as ``ref_version`` (in term of version number items).
        E.g. 3.3.2:
          - matches 3.3
          - matches 3.3.2
          - does not match 3.4
          - does not match 3.3.0
          - does not match 3.3.2.5
        """
        version = cls.version_tokenize(version)[0]
        ref_version = cls.version_tokenize(ref_version)[0]
        len_ref_version = len(ref_version)
        return (len(version) >= len_ref_version) and version[:len_ref_version] == ref_version

    @classmethod
    def versions_range_gen(cls, package, versions_set):
        def do_yield(version, versions_set):
            if version not in versions_set:
                versions_set.add(version)
                yield version
        MEX_RANGE_DIFF = 5
        VERSION_FACTOR_MAX = 100
        VERSION_FACTOR_MINRANGE_MULTIPLIER = 2
        VERSION_FACTOR_STEP_DIVIDER = 10

        version = cls.version_tokenize(package.version)[0][:2]
        version_min = cls.version_tokenize(package.version_min)[0][:2]
        version_mex = cls.version_tokenize(package.version_mex)[0][:2]

        version_major = version[0]
        version_major_min = version_min[0]
        version_major_mex = version_mex[0]
        if version_major_mex - version_major_min > 1:
            yield from do_yield(str(version_major), versions_set)
            for i in range(1, MEX_RANGE_DIFF):
                if version_major + i < version_major_mex:
                    yield from do_yield(str(version_major + i), versions_set)
            for i in range(1, MEX_RANGE_DIFF):
                if version_major - i >= version_major_min:
                    yield from do_yield(str(version_major - i), versions_set)
            return
        if len(version) < 2:
            yield from do_yield(str(version_major), versions_set)
            return

        version_minor = version[1]
        version_minor_min = 0 if len(version_min) < 2 else version_min[1]
        version_minor_mex = 0 if len(version_mex) < 2 else version_mex[1]
        version_minor_fac = 1
        vfac = VERSION_FACTOR_MAX
        while vfac > 1:
            version_minor_minrange = vfac * VERSION_FACTOR_MINRANGE_MULTIPLIER
            is_vfac_in_range = (version_minor >= vfac or version_minor_min >= vfac or version_minor_mex >= vfac)
            is_version_range_big_enough = (version_major_min != version_major_mex or
                                           version_minor_mex - version_minor_min >= version_minor_minrange)
            if (is_vfac_in_range and is_version_range_big_enough and version_minor % vfac == 0):
                version_minor_fac = vfac
                break
            vfac = vfac // VERSION_FACTOR_STEP_DIVIDER
        yield from do_yield(str(version_major) + "." + str(version_minor), versions_set)
        yield from do_yield(str(version_major) + str(version_minor), versions_set)
        for i in range(1, MEX_RANGE_DIFF):
            i *= version_minor_fac
            if version_minor + i < version_minor_mex or version_major_mex > version_major:
                yield from do_yield(str(version_major) + "." + str(version_minor + i), versions_set)
                yield from do_yield(str(version_major) + str(version_minor + i), versions_set)
        for i in range(1, MEX_RANGE_DIFF):
            i *= version_minor_fac
            if version_minor - i >= version_minor_min or (version_minor - i >= 0 and version_major_min < version_major):
                yield from do_yield(str(version_major) + "." + str(version_minor - i), versions_set)
                yield from do_yield(str(version_major) + str(version_minor - i), versions_set)
        return

    # Generic package handling, should not need to be redefined in each sub-class.
    # Note that they may depend on some non-generic functions defined below though.
    # ----------

    def package_query_version_get(self, package_distro_name):
        """Return the available, potentially cached if already looked-up, version of the given package."""
        if not hasattr(self, "_package_versions_cache"):
            self._package_versions_cache = {}
        return self._package_versions_cache.setdefault(package_distro_name,
                                                       self.package_query_version_get_impl(package_distro_name))

    def package_query_version_match(self, package_distro_name, ref_version):
        """Check if given package name matches given reference version."""
        version = self.package_query_version_get(package_distro_name)
        if version is None:
            return False
        if version is ...:
            # Only from PackageInstaller base class.
            assert self.__class__ is PackageInstaller
            return True
        return self.version_match(version, ref_version)

    def package_query_version_ge_lt(self, package_distro_name, ref_version_min, ref_version_mex):
        """Check if given package name fits in between given minimal and maximal excluded versions."""
        version = self.package_query_version_get(package_distro_name)
        if version is None:
            return False
        if version is ...:
            # Only from PackageInstaller base class.
            assert self.__class__ is PackageInstaller
            return True
        version, ref_version_min, ref_version_mex = self.version_tokenize(version, ref_version_min, ref_version_mex)
        return ref_version_min <= version < ref_version_mex

    def packages_database_update(self):
        """Ensure that data-base of available packages is up-to-date."""
        if self._update_command is ...:
            # Only from PackageInstaller base class.
            assert self.__class__ is PackageInstaller
            return True

        if self.settings.no_sudo:
            self.settings.logger.debug("\t--no-sudo enabled, no update of packages info.")
            return True

        self.settings.logger.info("Trying to update packages info.")
        result = self.run_command(self._update_command)
        if result.returncode != 0:
            self.settings.logger.critical(f"\tFailed to update packages info:\n\t{repr(result)}\n")
            exit(1)
        self.settings.logger.info("Done.\n")
        self.settings.logger.debug(repr(result))
        return result.returncode == 0

    def package_find(self, package, package_distro_name):
        """
        Generic heuristics to try and find 'best matching version' for a given package.
        For most packages it just ensures given package name version matches the exact version from the ``package``,
        or at least fits within the [version_min, version_mex[ range.
        But some, like e.g. Python or LLVM, can have packages available for several versions,
        with complex naming (like ``python3.10``, ``llvm-9-dev``, etc.).
        This code attempts to find the best matching one possible, based on a set of 'possible names'
        generated by the distribution-specific ``package_name_version_gen`` generator.
        """
        # Check 'exact' version match on given name.
        if self.package_query_version_match(package_distro_name, package.version_short):
            return package_distro_name
        # Check exact version match on special 'versioned' names (like `python3.10-dev' e.g.).
        for pn in self.package_name_version_gen(package, package_distro_name):
            if self.package_query_version_match(pn, package.version_short):
                return pn
        # Check version in supported range.
        if self.package_query_version_ge_lt(package_distro_name, package.version_min, package.version_mex):
            return package_distro_name
        # Check version in supported range on special 'versioned' names (like `llvm-11-dev' e.g.).
        for pn in self.package_name_version_gen(package, package_distro_name, do_range_version_names=True):
            if self.package_query_version_ge_lt(pn, package.version_min, package.version_mex):
                return pn
        return None

    def package_distro_name(self, package, parent_packages):
        """
        Generate a collection of distro-specific package names from given package.
        Typically only one name, unless given package is a group one, in which case all its
        sub-packages' distro-specific names are returned in the list.
        """
        distro_id = self.settings.distro_id

        packages_distro_names = []
        if package.is_group:
            for p in package.sub_packages:
                p.is_mandatory = package.is_mandatory
                if distro_id is ...:
                    packages_distro_names.append(p.name)
                else:
                    package_name = p.distro_package_names[distro_id]
                    if callable(package_name):
                        package_name = package_name(p, parent_packages)
                    if package_name not in {None, ...}:
                        packages_distro_names.append(package_name)
                if p.is_group:
                    packages_distro_names += self.package_distro_name(p, parent_packages + (package,))
            return packages_distro_names

        if distro_id is ...:
            package_name = package.name
        else:
            package_name = package.distro_package_names[distro_id]
            if callable(package_name):
                package_name = package_name(package, parent_packages)
        return [package_name]

    def packages_install(self, packages, parent_packages=()):
        """
        Install all given packages and their sub-packages.
        This call is recursive, parent_packages is a tuple of the ancestors of current ``package``, in calling order
        (grand-parent, parent).
        """
        def package_info_name(package, parent_packages):
            packages = parent_packages + (package,)
            return " ".join(p.name for p in packages)

        distro_id = self.settings.distro_id
        for package in packages:
            if not package.name:
                continue
            info_name = package_info_name(package, parent_packages)

            if package.is_group:
                if self.can_install:
                    self.settings.logger.info(f"Trying to install group of packages {info_name}.")
                if not package.sub_packages:
                    self.settings.logger.critical(f"Invalid group of packages {info_name}")
                    exit(1)
                success = self.group_package_install(package, parent_packages)
                if self.can_install:
                    if not success:
                        self.settings.logger.info("Failed.\n")
                    else:
                        self.settings.logger.info("Done.\n")
                continue

            package_distro_name = self.package_distro_name(package, parent_packages)[0]
            if package_distro_name is None:
                if package.is_mandatory:
                    self.settings.logger.warning(
                        f"Mandatory package {info_name} is not defined for {distro_id} distribution, "
                        "Blender will likely not build at all without it.\n")
                else:
                    self.settings.logger.info(f"Package {info_name} is not defined for {distro_id} distribution.\n")
                continue
            if package_distro_name is ...:
                self.settings.logger.debug(f"Package {info_name} is not required for {distro_id} distribution.\n")
                continue

            # Inherit parent version info if needed and possible.
            if package.version is None:
                if not parent_packages:
                    self.settings.logger.critical(
                        f"Package {info_name} ({package_distro_name}) has no version information.")
                    exit(1)
                package.version = parent_packages[-1].version
                package.version_short = parent_packages[-1].version_short
                package.version_min = parent_packages[-1].version_min
                package.version_mex = parent_packages[-1].version_mex

            if self.can_install:
                self.settings.logger.info(f"Trying to install package {info_name} ({package_distro_name}).")
            success = self.single_package_install(package, parent_packages)
            if not success:
                if self.can_install:
                    self.settings.logger.info("Failed.\n")
                continue

            if self.can_install:
                self.settings.logger.info("Done.\n")
            if package.sub_packages:
                self.packages_install(package.sub_packages, parent_packages + (package,))

    def single_package_install(self, package, parent_packages=()):
        """Install a normal, single package."""
        package_distro_name = self.package_distro_name(package, parent_packages)[0]
        package_name = self.package_find(package, package_distro_name)
        if package_name is None:
            if package.is_mandatory:
                self.settings.logger.critical(
                    f"\tFailed to find a matching mandatory {package_distro_name} "
                    f"(within versions range [{package.version_min}, {package.version_mex}[).")
                exit(1)
            self.settings.logger.warning(
                f"\tFailed to find a matching {package_distro_name} "
                f"(within versions range [{package.version_min}, {package.version_mex}[).")
            return False

        if self._install_command is ...:
            # Only from PackageInstaller base class.
            assert self.__class__ is PackageInstaller
            self.settings.logger.info(f"\tWould install {package_distro_name}.")
            return True

        if self.settings.no_sudo:
            self.settings.logger.warning(f"\t--no-sudo enabled, impossible to run apt-get install for {package_name}.")
            return True

        package_version = self.package_query_version_get(package_name)
        self.settings.logger.info(f"\tInstalling package {package_name} ({package_version}).")
        cmd = self._install_command + [package_name]
        result = self.run_command(cmd)
        if result.returncode != 0:
            self.settings.logger.critical(f"\tFailed to install {package_name}:\n\t{repr(result)}")
            exit(1)

        package_version_installed = self.package_installed_version_get(package_name)
        if package_version_installed != package_version:
            self.settings.logger.critical(f"\tInstalled version of {package_name} does not match expected value "
                                          f"({package_version_installed} vs {package_version})")
            exit(1)
        self.settings.logger.debug(repr(result))
        package.version_installed = package_version_installed
        return result.returncode == 0

    def group_package_install(self, package, parent_packages=()):
        """Install a group package and all of its sub-packages."""
        packages_distro_names = self.package_distro_name(package, parent_packages)
        if self._install_command is ...:
            # Only from PackageInstaller base class.
            assert self.__class__ is PackageInstaller
            packages_info_names = ',\n\t\t\t'.join(packages_distro_names)
            self.settings.logger.info(
                f"\tWould install group of packages {package.name}:\n\t\t\t{packages_info_names}.")
            return True

        if self.settings.no_sudo:
            self.settings.logger.warning(
                f"\t--no-sudo enabled, impossible to run apt-get install for {packages_distro_names}.")
            return True

        if not packages_distro_names:
            return True

        self.settings.logger.info(f"\tInstalling packages {', '.join(packages_distro_names)}.")
        cmd = self._install_command + [*packages_distro_names]
        result = self.run_command(cmd)
        if result.returncode != 0:
            if package.is_mandatory:
                self.settings.logger.critical(f"\tFailed to install packages:\n\t{repr(result)}")
                exit(1)
            else:
                self.settings.logger.warning(
                    f"\tFailed to find install all of {packages_distro_names}:\n\t{repr(result)}")
        self.settings.logger.debug(repr(result))
        return result.returncode == 0

    # Implementation-specific, will most likely need to be re-defined in sub-classes.
    # ----------

    # Command and options to pass to install packages in specific distro (as a list, for `subprocess.run`).
    # Will be appended with package or list of packages to install.
    _install_command = ...

    # Command and options to pass to update packages data-base in specific distro (as a list, for `subprocess.run`).
    _update_command = ...

    def package_installed_version_get(self, package_distro_name):
        """Return the installed version of the given package."""
        return ...

    def package_query_version_get_impl(self, package_distro_name):
        """Return the available version of the given package."""
        return ...

    def package_name_version_gen(self, package, package_distro_name, version, suffix="", do_range_version_names=False):
        """Generator for all potential names for a given package 'base name'."""
        yield package_distro_name


class PackageInstallerDebian(PackageInstaller):
    """Debian-like package installer, using apt and dpkg-query."""
    _instance = None

    def __new__(cls, settings):
        if cls._instance is None:
            cls._instance = super(PackageInstallerDebian, cls).__new__(cls, settings)
        return cls._instance

    _version_regex_base_pattern = r"(?:[0-9]+:)?(?P<version>([0-9]+\.?)+([0-9]+)).*"
    _re_version = re.compile(_version_regex_base_pattern)
    _re_version_candidate = re.compile(r"Candidate:\s*" + _version_regex_base_pattern)

    _install_command = [MAYSUDO, "apt", "install", "-y"]
    _update_command = [MAYSUDO, "apt", "update"]

    def package_installed_version_get(self, package_distro_name):
        cmd = ["dpkg-query", "-W", "-f", "${Version}", package_distro_name]
        result = self.run_command(cmd)
        version = self._re_version.search(str(result.stdout))
        return version["version"] if version is not None else None

    def package_query_version_get_impl(self, package_distro_name):
        # `apt-cache policy` will do partial matching (so e.g. `python3.11` will also match `libpython3.11-stdlib`).
        # Use `apt show` first to ensure exact package name is available (stdout will be empty if no package of
        # requested name is known).
        cmd = ["apt", "show", package_distro_name]
        result = self.run_command(cmd)
        if not result.stdout:
            return None
        cmd = ["apt-cache", "policy", package_distro_name]
        result = self.run_command(cmd)
        version = self._re_version_candidate.search(str(result.stdout))
        return version["version"] if version is not None else None

    def package_name_version_gen(
            self,
            package,
            package_distro_name,
            version=...,
            suffix="",
            do_range_version_names=False):
        if version is ...:
            version = package.version_short
        # Generate versions variants with version between main name and '-dev' suffix, if any.
        tmp_package_name = package_distro_name.removesuffix("-dev")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix="-dev" + suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Strip any 'version-like' numbers at the end of the package name (already stripped of '-dev' suffix)
        # and generate versions variants out of it.
        tmp_package_name = tmp_package_name.rstrip("0123456789.-")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix=suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Generate version variants from given package name.
        versions = [version]
        if do_range_version_names:
            # Also search for major version numbers around the target one, within to allowed range.
            # Necessary for packages like llvm e.g.
            versions += [*self.versions_range_gen(package, set(versions))]
        for v in versions:
            yield package_distro_name + v + suffix
            yield package_distro_name + "-" + v + suffix


class PackageInstallerFedora(PackageInstaller):
    """Fedora-like package installer, using dnf."""
    _instance = None

    def __new__(cls, settings):
        if cls._instance is None:
            cls._instance = super(PackageInstallerFedora, cls).__new__(cls, settings)
        return cls._instance

    _re_version = re.compile(r"Version\s*:\s*(?:[0-9]+:)?(?P<version>([0-9]+\.?)+([0-9]+)).*")

    _install_command = [MAYSUDO, "dnf", "install", "-y"]
    _update_command = [MAYSUDO, "dnf", "check-update"]

    def package_version_get(self, command):
        result = self.run_command(command)
        version = self._re_version.search(str(result.stdout))
        return version["version"] if version is not None else None

    def package_installed_version_get(self, package_distro_name):
        return self.package_version_get([MAYSUDO, "dnf", "info", "--installed", package_distro_name])

    def package_query_version_get_impl(self, package_distro_name):
        return self.package_version_get([MAYSUDO, "dnf", "info", package_distro_name])

    def package_name_version_gen(
            self,
            package,
            package_distro_name,
            version=...,
            suffix="",
            do_range_version_names=False):
        if version is ...:
            version = package.version_short
        # Generate versions variants with version between main name and '-devel' suffix, if any.
        tmp_package_name = package_distro_name.removesuffix("-devel")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix="-devel" + suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Strip any 'version-like' numbers at the end of the package name (already stripped of '-devel' suffix)
        # and generate versions variants out of it.
        tmp_package_name = tmp_package_name.rstrip("0123456789.-")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix=suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Generate version variants from given package name.
        versions = [version]
        if do_range_version_names:
            # Also search for major version numbers around the target one, within to allowed range.
            # Necessary for packages like llvm e.g.
            versions += [*self.versions_range_gen(package, set(versions))]
        for v in versions:
            yield package_distro_name + v + suffix
            yield package_distro_name + "-" + v + suffix


class PackageInstallerSuse(PackageInstaller):
    """Suse-like package installer, using zypper."""
    _instance = None

    def __new__(cls, settings):
        if cls._instance is None:
            cls._instance = super(PackageInstallerSuse, cls).__new__(cls, settings)
        return cls._instance

    _re_version = re.compile(r"Version\s*:\s*(?:[0-9]+:)?(?P<version>([0-9]+\.?)+([0-9]+)).*")
    _re_installed = re.compile(r"Installed\s*:\s*Yes")

    _install_command = [MAYSUDO, "zypper", "--non-interactive", "install"]
    _update_command = [MAYSUDO, "zypper", "refresh"]

    def package_version_get(self, command_result):
        version = self._re_version.search(str(command_result.stdout))
        return version["version"] if version is not None else None

    def package_installed_version_get(self, package_distro_name):
        result = self.run_command([MAYSUDO, "zypper", "info", package_distro_name])
        is_installed = self._re_installed.search(str(result.stdout))
        return self.package_version_get(result) if is_installed is not None else None

    def package_query_version_get_impl(self, package_distro_name):
        result = self.run_command([MAYSUDO, "zypper", "info", package_distro_name])
        return self.package_version_get(result)

    def package_name_version_gen(
            self,
            package,
            package_distro_name,
            version=...,
            suffix="",
            do_range_version_names=False):
        if version is ...:
            version = package.version_short
        # Generate versions variants with version between main name and '-devel' suffix, if any.
        tmp_package_name = package_distro_name.removesuffix("-devel")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix="-devel" + suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Strip any 'version-like' numbers at the end of the package name (already stripped of '-devel' suffix)
        # and generate versions variants out of it.
        tmp_package_name = tmp_package_name.rstrip("0123456789.-")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix=suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Generate version variants from given package name.
        versions = [version]
        if do_range_version_names:
            # Also search for major version numbers around the target one, within to allowed range.
            # Necessary for packages like llvm e.g.
            versions += [*self.versions_range_gen(package, set(versions))]
        for v in versions:
            yield package_distro_name + v + suffix
            yield package_distro_name + "-" + v + suffix


class PackageInstallerArch(PackageInstaller):
    """Arch-like package installer, using pacman."""
    _instance = None

    def __new__(cls, settings):
        if cls._instance is None:
            cls._instance = super(PackageInstallerArch, cls).__new__(cls, settings)
        return cls._instance

    _re_version = re.compile(r"Version\s*:\s*(?:[0-9]+:)?(?P<version>([0-9]+\.?)+([0-9]+)).*")

    _install_command = [MAYSUDO, "pacman", "-S", "--needed", "--noconfirm"]
    _update_command = [MAYSUDO, "pacman", "-Sy"]

    def package_version_get(self, command):
        result = self.run_command(command)
        version = self._re_version.search(str(result.stdout))
        return version["version"] if version is not None else None

    def package_installed_version_get(self, package_distro_name):
        return self.package_version_get(["pacman", "-Qi", package_distro_name])

    def package_query_version_get_impl(self, package_distro_name):
        return self.package_version_get(["pacman", "-Si", package_distro_name])

    def package_name_version_gen(
            self,
            package,
            package_distro_name,
            version=...,
            suffix="",
            do_range_version_names=False):
        if version is ...:
            version = package.version_short
        # Generate versions variants with version after the main name.
        tmp_package_name = package_distro_name
        # Strip any 'version-like' numbers at the end of the package name (already stripped of '-dev' suffix)
        # and generate versions variants out of it.
        tmp_package_name = tmp_package_name.rstrip("0123456789.-")
        if tmp_package_name != package_distro_name:
            for pn in self.package_name_version_gen(
                    package,
                    tmp_package_name,
                    version,
                    suffix=suffix,
                    do_range_version_names=do_range_version_names):
                yield pn
        # Generate version variants from given package name.
        versions = [version]
        if do_range_version_names:
            # Also search for major version numbers around the target one, within to allowed range.
            # Necessary for packages like llvm e.g.
            versions += [*self.versions_range_gen(package, set(versions))]
        for v in versions:
            yield package_distro_name + v + suffix
            yield package_distro_name + "-" + v + suffix


DISTRO_IDS_INSTALLERS = {
    ...: PackageInstaller,
    DISTRO_ID_DEBIAN: PackageInstallerDebian,
    DISTRO_ID_FEDORA: PackageInstallerFedora,
    DISTRO_ID_SUSE: PackageInstallerSuse,
    DISTRO_ID_ARCH: PackageInstallerArch,
}


def get_distro(settings):
    if settings.distro_id is not ...:
        settings.logger.info(f"Distribution identifier forced by user to {settings.distro_id}.")
        return settings.distro_id
    import platform
    if hasattr(platform, "freedesktop_os_release"):
        info = platform.freedesktop_os_release()
        ids = [info["ID"]]
        if "ID_LIKE" in info:
            # ids are space separated and ordered by precedence.
            ids.extend(info["ID_LIKE"].split())
        for distro_id in ids:
            if distro_id in DISTRO_IDS_INSTALLERS:
                settings.distro_id = distro_id
                return distro_id
        settings.logger.warning(f"Distribution IDs do not match any supported one by this script ({ids})")

    settings.logger.warning("A valid distribution ID could not be found using `platform.freedesktop_os_release`, "
                            "now trying a lower-level check for specific files")
    if os.path.exists("/etc/debian_version"):
        distro_id = DISTRO_ID_DEBIAN
    elif os.path.exists("/etc/redhat-release"):
        distro_id = DISTRO_ID_FEDORA
    elif os.path.exists("/etc/SuSE-release"):
        distro_id = DISTRO_ID_SUSE
    elif os.path.exists("/etc/arch-release"):
        distro_id = DISTRO_ID_ARCH
    if distro_id in DISTRO_IDS_INSTALLERS:
        settings.distro_id = distro_id
        return distro_id

    settings.distro_id = ...
    return ...


def get_distro_package_installer(settings):
    distro_id = get_distro(settings)
    if distro_id is ...:
        settings.logger.warning("No valid distribution ID found, please try to set it using the `--distro-id` option")
    else:
        settings.logger.info(f"Distribution identified as '{distro_id}'")
    return DISTRO_IDS_INSTALLERS[distro_id](settings)


def argparse_create():
    import argparse

    # When --help or no args are given, print this help
    usage_text = (
        "Attempt to install dependencies to build Blender from current linux distribution's packages only.\n"
        "\n"
        "By default, only installs critical tools and dependencies to build Blender, excluding any library provided\n"
        "by the precompiled git-lfs repository.\n"
        "`make update` should then be ran after this script to download all precompiled libraries.\n"
        "\n"
        "When ran with the `--all` option, this tool will try to install all mandatory and optional dependencies\n"
        "from the distribution packages.\n"
        "\n"
        "NOTE: Many distributions do not provide packages for all libraries used by Blender, or have no\n"
        "version-compatible packages. In some cases, mandatory dependencies cannot be satisfied, and Blender\n"
        "won't be able to build at all.\n"
        "\n"
        "NOTE: To build with system package libraries instead of the precompiled ones when both are available,\n"
        "the `WITH_LIBS_PRECOMPILED` option must be disabled in CMake.\n"
        "\n"
        "See https://developer.blender.org/docs/handbook/building_blender/ for more details.\n"
        "\n"
    )

    parser = argparse.ArgumentParser(description=usage_text, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument(
        "--show-deps",
        dest="show_deps",
        action='store_true',
        help="Show main dependencies of Blender (including officially supported versions) and exit.",
    )
    parser.add_argument(
        "--no-sudo",
        dest="no_sudo",
        action='store_true',
        help=(
            "Disable use of `sudo` or `doas` "
            "(this script won't be able to do much then, will just print needed packages)."
        ),
    )
    parser.add_argument(
        "--all",
        dest="all",
        action='store_true',
        help="Install all dependencies from the distribution packages, including these also provided as "
             "precompiled libraries.",
    )
    parser.add_argument(
        "--distro-id",
        dest="distro_id",
        default=...,
        choices=set(DISTRO_IDS_INSTALLERS.keys()) - set((...,)),
        help="Force the linux distribution identifier to a specific value instead of relying on automatic detection.",
    )
    parser.add_argument(
        "--debug",
        dest="debug",
        action='store_true',
        help="Enable all debug info messages.",
    )

    return parser


def main():
    settings = argparse_create().parse_args()

    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG if settings.debug else logging.INFO)
    stdout_handler = logging.StreamHandler(stream=sys.stdout)
    stdout_handler.setFormatter(LoggingColoredFormatter())
    logger.addHandler(stdout_handler)
    settings.logger = logger

    if not settings.no_sudo and len(MAYSUDO) == 0:
        logger.critical("`sudo` or `doas` commands are needed to escalate privileges,"
                        " but they were not found.")
        exit(42)

    distro_package_installer = (PackageInstaller(settings) if settings.show_deps
                                else get_distro_package_installer(settings))
    distro_package_installer.packages_database_update()

    if settings.all:
        distro_package_installer.packages_install(PACKAGES_ALL)
    else:
        distro_package_installer.packages_install(PACKAGES_BASICS_BUILD)


if __name__ == "__main__":
    main()
