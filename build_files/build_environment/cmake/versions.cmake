# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Compilers
#
# Version used for precompiled library builds used for official releases.
# For anyone making their own library build, matching these exactly is not
# needed but it can be a useful reference.

set(RELEASE_GCC_VERSION 11.2)
set(RELEASE_CUDA_VERSION 12.8)
set(RELEASE_HIP_VERSION 6.4)

# Libraries
#
# CPE's are used to identify dependencies, for more information on what they
# are please see https://nvd.nist.gov/products/cpe
#
# We use them in combination with cve-bin-tool to scan for known security issues.
#
# Not all of our dependencies are currently in the nvd database so not all
# dependencies have one assigned.
#
# -----------------------------------------------------------------------------
#
# The following fields are used for license generation (see `make license`):
#
#  * NAME: Human readable project name.
#  * LICENSE: License following SPDX standard.
#  * HOMEPAGE: Main project page.
#  * COPYRIGHT: Single-line copyright holders, semi-colon separated.
#      Some licenses may not require a copyright.
#  * DEPSBUILDTIMEONLY: Indicate that the library is not included nor linked with
#      Blender, thus can be excluded from the final license. Use to indicate the
#      reason why this is not being included with Blender.
#
#  Note that multi-line strings *must* use [=[...]=] bounds.


set(ZLIB_VERSION 1.3.1)
set(ZLIB_URI https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 9855b6d802d7fe5b7bd5b196a2271655)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_CPE "cpe:2.3:a:zlib:zlib:${ZLIB_VERSION}:*:*:*:*:*:*:*")
set(ZLIB_HOMEPAGE https://zlib.net)
set(ZLIB_LICENSE SPDX:Zlib)
set(ZLIB_COPYRIGHT "Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler")

set(OPENAL_VERSION 1.23.1)
set(OPENAL_URI https://github.com/kcat/openal-soft/releases/download/${OPENAL_VERSION}/openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HASH 58a73698288d2787451b61f8f4431513)
set(OPENAL_HASH_TYPE MD5)
set(OPENAL_FILE openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_NAME OpenAL)
set(OPENAL_HOMEPAGE https://openal-soft.org/)
set(OPENAL_LICENSE SPDX:LGPL-2.1-or-later)
set(OPENAL_COPYRIGHT [=[
Copyright (c) 2015, Archontis Politis.
Copyright (c) 2019, Christopher Robinson.
]=])

set(PNG_VERSION 1.6.50)
set(PNG_NAME libpng)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 4df396518620a7aa3651443e87d1b2862e4e88cad135a8b93423e01706232307)
set(PNG_HASH_TYPE SHA256)
set(PNG_FILE libpng-${PNG_VERSION}.tar.xz)
set(PNG_CPE "cpe:2.3:a:libpng:libpng:${PNG_VERSION}:*:*:*:*:*:*:*")
set(PNG_HOMEPAGE http://www.libpng.org/pub/png/libpng.html)
set(PNG_LICENSE SPDX:libpng-2.0)
set(PNG_COPYRIGHT [=[
Copyright (c) 1995-2019 The PNG Reference Library Authors.
Copyright (c) 2018-2019 Cosmin Truta.
Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson.
Copyright (c) 1996-1997 Andreas Dilger.
Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.
]=])

set(JPEG_VERSION 2.1.3)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 627b980fad0573e08e4c3b80b290fc91)
set(JPEG_HASH_TYPE MD5)
set(JPEG_NAME libjpeg-turbo)
set(JPEG_FILE libjpeg-turbo-${JPEG_VERSION}.tar.gz)
set(JPEG_CPE "cpe:2.3:a:d.r.commander:libjpeg-turbo:${JPEG_VERSION}:*:*:*:*:*:*:*")
set(JPEG_HOMEPAGE https://github.com/libjpeg-turbo/libjpeg-turbo/)
set(JPEG_LICENSE SPDX:BSD-3-Clause)
set(JPEG_COPYRIGHT [=[
Copyright (C)2009-2020 D. R. Commander. All Rights Reserved.
Copyright (C)2015 Viktor Szathmáry. All Rights Reserved.
]=])

set(BLOSC_VERSION 1.21.1)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.tar.gz)
set(BLOSC_HASH 134b55813b1dca57019d2a2dc1f7a923)
set(BLOSC_HASH_TYPE MD5)
set(BLOSC_FILE blosc-${BLOSC_VERSION}.tar.gz)
set(BLOSC_CPE "cpe:2.3:a:c-blosc_project:c-blosc:${BLOSC_VERSION}:*:*:*:*:*:*:*")
set(BLOSC_HOMEPAGE https://www.blosc.org/)
set(BLOSC_LICENSE SPDX:BSD-3-Clause)
set(BLOSC_COPYRIGHT [=[
Copyright (C) 2009-2018 Francesc Alted <francesc@blosc.org>.
Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>.
Copyright (C) 2006 by Rob Landley <rob@landley.net>.
]=])

set(PTHREADS_VERSION 3.0.0)
set(PTHREADS_URI http://prdownloads.sourceforge.net/pthreads4w/pthreads4w-code-v${PTHREADS_VERSION}.zip)
set(PTHREADS_HASH f3bf81bb395840b3446197bcf4ecd653)
set(PTHREADS_HASH_TYPE MD5)
set(PTHREADS_FILE pthreads4w-code-${PTHREADS_VERSION}.zip)
set(PTHREADS_HOMEPAGE https://github.com/fwbuilder/pthreads4w)
set(PTHREADS_LICENSE SPDX:Apache-2.0)
set(PTHREADS_COPYRIGHT [=[
Copyright 1998 John E. Bossom
Copyright 1999-2018, Pthreads4w contributors

This product includes software developed through the colaborative
effort of several individuals, each of whom is listed in the file
CONTRIBUTORS included with this software.

The following files are not covered under the Copyrights
listed above:

    [1] tests/rwlock7.c
    [1] tests/rwlock7_1.c
    [1] tests/rwlock8.c
    [1] tests/rwlock8_1.c
    [2] tests/threestage.c

[1] The file tests/rwlock7.c and those similarly named are derived from
code written by Dave Butenhof for his book 'Programming With POSIX(R)
Threads'. The original code was obtained by free download from his
website http://home.earthlink.net/~anneart/family/Threads/source.html

[2] The file tests/threestage.c is taken directly from examples in the
book "Windows System Programming, Edition 4" by Johnson (John) Hart
Session 6, Chapter 10. ThreeStage.c
Several required additional header and source files from the
book examples have been included inline to simplify compilation.
The only modification to the code has been to provide default
values when run without arguments.
]=])

set(DEFLATE_VERSION 1.18)
set(DEFLATE_URI https://github.com/ebiggers/libdeflate/archive/refs/tags/v${DEFLATE_VERSION}.tar.gz)
set(DEFLATE_HASH a29d9dd653cbe03f2d5cd83972063f9e)
set(DEFLATE_HASH_TYPE MD5)
set(DEFLATE_FILE libdeflate-v${DEFLATE_VERSION}.tar.gz)
set(DEFLATE_HOMEPAGE https://github.com/ebiggers/libdeflate)
set(DEFLATE_LICENSE SPDX:MIT)
set(DEFLATE_COPYRIGHT "Copyright 2016 Eric Biggers")

set(OPENEXR_VERSION 3.3.5)
set(OPENEXR_URI https://github.com/AcademySoftwareFoundation/openexr/archive/v${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH fd5404d49331e24c26f4a263b13a35d4)
set(OPENEXR_HASH_TYPE MD5)
set(OPENEXR_FILE openexr-${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_CPE "cpe:2.3:a:openexr:openexr:${OPENEXR_VERSION}:*:*:*:*:*:*:*")
set(OPENEXR_HOMEPAGE https://github.com/AcademySoftwareFoundation/openexr)
set(OPENEXR_LICENSE SPDX:BSD-3-Clause)
set(OPENEXR_COPYRIGHT "Copyright Contributors to the OpenEXR Project. All rights reserved.")

set(IMATH_VERSION 3.1.7)
set(IMATH_URI https://github.com/AcademySoftwareFoundation/Imath/archive/v${IMATH_VERSION}.tar.gz)
set(IMATH_HASH 5cedab446ab296c080957c3037c6d097)
set(IMATH_HASH_TYPE MD5)
set(IMATH_FILE imath-${IMATH_VERSION}.tar.gz)
set(IMATH_HOMEPAGE https://github.com/AcademySoftwareFoundation/Imath)
set(IMATH_LICENSE SPDX:BSD-3-Clause)
set(IMATH_COPYRIGHT "Copyright Contributors to the OpenEXR Project. All rights reserved.")


if(WIN32)
  # Openexr started appending _d on its own so now
  # we need to tell the build the postfix is _s while
  # telling all other deps the postfix is _s_d
  if(BUILD_MODE STREQUAL Release)
    set(OPENEXR_VERSION_POSTFIX )
    set(OPENEXR_VERSION_BUILD_POSTFIX )
  else()
    set(OPENEXR_VERSION_POSTFIX _d)
    set(OPENEXR_VERSION_BUILD_POSTFIX )
  endif()
else()
  set(OPENEXR_VERSION_BUILD_POSTFIX)
  set(OPENEXR_VERSION_POSTFIX)
endif()

set(FREETYPE_VERSION 2.13.3)
set(FREETYPE_URI http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH ac1f0b517f62bd40d50bc995faa5741d)
set(FREETYPE_HASH_TYPE MD5)
set(FREETYPE_FILE freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_CPE "cpe:2.3:a:freetype:freetype:${FREETYPE_VERSION}:*:*:*:*:*:*:*")
set(FREETYPE_HOMEPAGE https://freetype.org/)
set(FREETYPE_NAME FreeType)
set(FREETYPE_LICENSE SPDX:FTL)
set(FREETYPE_COPYRIGHT "Copyright 1996-2002, 2006 by David Turner, Robert Wilhelm, and Werner Lemberg")

set(EPOXY_VERSION 1.5.10)
set(EPOXY_URI https://github.com/anholt/libepoxy/archive/refs/tags/${EPOXY_VERSION}.tar.gz)
set(EPOXY_HASH f0730aad115c952e77591fcc805b1dc1)
set(EPOXY_HASH_TYPE MD5)
set(EPOXY_FILE libepoxy-${EPOXY_VERSION}.tar.gz)
set(EPOXY_HOMEPAGE https://github.com/anholt/libepoxy)
set(EPOXY_LICENSE SPDX:MIT)
set(EPOXY_COPYRIGHT [=[
Copyright © 2013-2014 Intel Corporation.
Copyright © 2013 The Khronos Group Inc.
]=])

set(ALEMBIC_VERSION 1.8.3)
set(ALEMBIC_URI https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.tar.gz)
set(ALEMBIC_HASH 2cd8d6e5a3ac4a014e24a4b04f4fadf9)
set(ALEMBIC_HASH_TYPE MD5)
set(ALEMBIC_FILE alembic-${ALEMBIC_VERSION}.tar.gz)
set(ALEMBIC_HOMEPAGE https://www.alembic.io/)
set(ALEMBIC_LICENSE SPDX:BSD-3-Clause)
set(ALEMBIC_COPYRIGHT [=[
TM & © 2009-2015 Lucasfilm Entertainment Company Ltd. or Lucasfilm Ltd.
All rights reserved.
Industrial Light & Magic, ILM and the Bulb and Gear design logo are all
registered trademarks or service marks of Lucasfilm Ltd.
© 2009-2015 Sony Pictures Imageworks Inc.  All rights reserved.
]=])

set(OPENSUBDIV_VERSION v3_6_0)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_HASH cd03aaf8890bc0b8550eef62029cabe7)
set(OPENSUBDIV_HASH_TYPE MD5)
set(OPENSUBDIV_FILE opensubdiv-${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_NAME OpenSubdiv)
set(OPENSUBDIV_HOMEPAGE https://graphics.pixar.com/opensubdiv/docs/intro.html)
set(OPENSUBDIV_LICENSE TOST-1.0)
set(OPENSUBDIV_COPYRIGHT [=[
OpenSubdiv
Copyright 2013 Pixar
All rights reserved.
This product includes software developed at:
Pixar (http://www.pixar.com/).
Dreamworks Animation (http://www.dreamworksanimation.com/).
Autodesk, Inc. (http://www.autodesk.com/).
Google, Inc. (http://www.google.com/).
DigitalFish (http://digitalfish.com/).
]=])

set(SDL_VERSION 2.28.2)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH 06ff379c406cd8318d18f0de81ee2709)
set(SDL_HASH_TYPE MD5)
set(SDL_FILE SDL2-${SDL_VERSION}.tar.gz)
set(SDL_CPE "cpe:2.3:a:libsdl:sdl:${SDL_VERSION}:*:*:*:*:*:*:*")
set(SDL_NAME SDL)
set(SDL_HOMEPAGE https://www.libsdl.org)
set(SDL_LICENSE SPDX:Zlib)
set(SDL_COPYRIGHT "Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>")

set(OPENCOLORIO_VERSION 2.4.2)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH db64ac0b4a67fd8e6d6d8111dccade29)
set(OPENCOLORIO_HASH_TYPE MD5)
set(OPENCOLORIO_FILE OpenColorIO-${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_NAME OpenColorIO)
set(OPENCOLORIO_HOMEPAGE https://github.com/AcademySoftwareFoundation/OpenColorIO)
set(OPENCOLORIO_LICENSE SPDX:BSD-3-Clause)
set(OPENCOLORIO_COPYRIGHT "Copyright Contributors to the OpenColorIO Project.")

set(MINIZIPNG_VERSION 3.0.7)
set(MINIZIPNG_NAME minizip-ng)
set(MINIZIPNG_URI https://github.com/zlib-ng/minizip-ng/archive/${MINIZIPNG_VERSION}.tar.gz)
set(MINIZIPNG_HASH 09dcc8a9def348e1be9659e384c2cd55)
set(MINIZIPNG_HASH_TYPE MD5)
set(MINIZIPNG_FILE minizip-ng-${MINIZIPNG_VERSION}.tar.gz)
set(MINIZIPNG_HOMEPAGE https://github.com/zlib-ng/minizip-ng)
set(MINIZIPNG_LICENSE SPDX:Zlib)
set(MINIZIPNG_COPYRIGHT [=[
Copyright (C) Nathan Moinvaziri https://github.com/zlib-ng/minizip-ng.
Copyright (C) 1998-2010 Gilles Vollant https://www.winimage.com/zLibDll/minizip.html.
]=])

set(LLVM_VERSION 20.1.8)
set(LLVM_NAME LLVM)
set(LLVM_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH 915e251a657450a2ba8e4c106e4f9555)
set(LLVM_HASH_TYPE MD5)
set(LLVM_FILE llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_CPE "cpe:2.3:a:llvm:compiler:${LLVM_VERSION}:*:*:*:*:*:*:*")
set(LLVM_HOMEPAGE https://github.com/llvm/llvm-project/)
set(LLVM_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(LLVM_COPYRIGHT "Copyright (c) 2003-2019 University of Illinois at Urbana-Champaign. All rights reserved.")

set(OPENIMAGEIO_VERSION v3.0.9.1)
set(OPENIMAGEIO_NAME OpenImageIO)
set(OPENIMAGEIO_URI https://github.com/AcademySoftwareFoundation/OpenImageIO/archive/refs/tags/${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH 5a3490d405615f48d7340ba4af41380d)
set(OPENIMAGEIO_HASH_TYPE MD5)
set(OPENIMAGEIO_FILE OpenImageIO-${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HOMEPAGE https://github.com/AcademySoftwareFoundation/OpenImageIO)
set(OPENIMAGEIO_LICENSE SPDX:Apache-2.0)
set(OPENIMAGEIO_COPYRIGHT "Copyright Contributors to the OpenImageIO project.")

# 9.1.0 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(FMT_VERSION 9.1.0)
set(FMT_URI https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.tar.gz)
set(FMT_HASH 5dea48d1fcddc3ec571ce2058e13910a0d4a6bab4cc09a809d8b1dd1c88ae6f2)
set(FMT_HASH_TYPE SHA256)
set(FMT_FILE fmt-${FMT_VERSION}.tar.gz)
set(FMT_CPE "cpe:2.3:a:fmt:fmt:${FMT_VERSION}:*:*:*:*:*:*:*")
set(FMT_HOMEPAGE https://github.com/fmtlib/fmt)
set(FMT_LICENSE SPDX:MIT)
set(FMT_COPYRIGHT "Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors")

# 0.6.2 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(ROBINMAP_VERSION v1.3.0)
set(ROBINMAP_URI https://github.com/Tessil/robin-map/archive/refs/tags/${ROBINMAP_VERSION}.tar.gz)
set(ROBINMAP_HASH a8424ad3b0affd4c57ed26f0f3d8a29604f0e1f2ef2089f497f614b1c94c7236)
set(ROBINMAP_HASH_TYPE SHA256)
set(ROBINMAP_FILE robinmap-${ROBINMAP_VERSION}.tar.gz)
set(ROBINMAP_HOMEPAGE https://github.com/Tessil/robin-map)
set(ROBINMAP_LICENSE SPDX:MIT)
set(ROBINMAP_COPYRIGHT "Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>")

set(TIFF_VERSION 4.7.0)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH 3a0fa4a270a4a192b08913f88d0cfbdd)
set(TIFF_HASH_TYPE MD5)
set(TIFF_FILE tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_CPE "cpe:2.3:a:libtiff:libtiff:${TIFF_VERSION}:*:*:*:*:*:*:*")
set(TIFF_HOMEPAGE http://www.simplesystems.org/libtiff/)
set(TIFF_NAME LibTIFF)
set(TIFF_LICENSE SPDX:libtiff)
set(TIFF_COPYRIGHT [=[
Copyright © 1988-1997 Sam Leffler.
Copyright © 1991-1997 Silicon Graphics, Inc.
]=])

set(OSL_VERSION 1.14.7.0)
set(OSL_NAME "Open Shading Language")
set(OSL_URI https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/releases/download/v${OSL_VERSION}/OSL-${OSL_VERSION}.tar.gz)
set(OSL_HASH 45b2b9de1bae6854fb8af0d332c3266898871d95741bdb6abe8178cd6d01aef2)
set(OSL_HASH_TYPE SHA256)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)
set(OSL_HOMEPAGE https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/)
set(OSL_LICENSE SPDX:BSD-3-Clause)
set(OSL_COPYRIGHT "Copyright Contributors to the Open Shading Language project.")

set(MANIFOLD_VERSION v3.2.1)
set(MANIFOLD_NAME "Manifold")
set(MANIFOLD_URI https://github.com/elalish/manifold/archive/refs/tags/${MANIFOLD_VERSION}.tar.gz)
set(MANIFOLD_HASH 5618cc9685b15a0b7ba4783589c1ae7c)
set(MANIFOLD_HASH_TYPE MD5)
set(MANIFOLD_FILE Manifold-${MANIFOLD_VERSION}.tar.gz)
set(MANIFOLD_HOMEPAGE https://github.com/elalish/manifold)
set(MANIFOLD_LICENSE SPDX:Apache-2.0)
set(MANIFOLD_COPYRIGHT "Copyright 2021 The Manifold Authors.")

set(RUBBERBAND_VERSION 4.0.0)
set(RUBBERBAND_NAME "Rubber Band Library")
set(RUBBERBAND_URI https://breakfastquay.com/files/releases/rubberband-${RUBBERBAND_VERSION}.tar.bz2)
set(RUBBERBAND_HASH 93bf3159eb91048e76eba35cf1bf766f)
set(RUBBERBAND_HASH_TYPE MD5)
set(RUBBERBAND_FILE rubberband-${RUBBERBAND_VERSION}.tar.bz2)
set(RUBBERBAND_HOMEPAGE https://breakfastquay.com/rubberband/)
set(RUBBERBAND_LICENSE SPDX:GPL-2.0-or-later)
set(RUBBERBAND_COPYRIGHT "Copyright (c) 2025 Particular Programs Ltd")

# NOTE: When updating the python version, it's required to check the versions of
# it wants to use in PCbuild/get_externals.bat for the following dependencies:
# BZIP2, FFI and change the versions in this file as well. For compliance
# reasons there can be no exceptions to this.
# Additionally, keep the PYTHON_PIP_VERSION in sync with the pip version bundled
# into Python.

set(PYTHON_VERSION 3.11.13)
set(PYTHON_SHORT_VERSION 3.11)
set(PYTHON_SHORT_VERSION_NO_DOTS 311)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH ec39a8018b9eedf6f0edeb44533bd279)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_CPE "cpe:2.3:a:python:python:${PYTHON_VERSION}:-:*:*:*:*:*:*")
set(PYTHON_HOMEPAGE https://www.python.org/)
set(PYTHON_NAME Python)
set(PYTHON_LICENSE SPDX:Python-2.0)
set(PYTHON_COPYRIGHT "Copyright (c) 2001-2023 Python Software Foundation. All rights reserved.")

# Python bundles pip wheel, and does not track CVEs from it. Add an explicit CPE
# identifier for pip, so that cve_check can detect vulnerabilities in it.
# The version needs to be kept in symc with the version bundled in Python.
# Currently it is done manually by tracking _PIP_VERSION variable in the
# `Lib/ensurepip/__init__.py`. For example,
#   https://github.com/python/cpython/tree/v3.11.9/Lib/ensurepip/__init__.py
set(PYTHON_PIP_VERSION 24.0)
set(PYTHON_PIP_CPE "cpe:2.3:a:pypa:pip:${PYTHON_PIP_VERSION}:*:*:*:*:*:*:*")

set(TBB_YEAR 2021)
set(TBB_NAME oneTBB)
set(TBB_VERSION v2021.13.0)
set(TBB_URI https://github.com/uxlfoundation/oneTBB/archive/refs/tags/${TBB_VERSION}.tar.gz)
set(TBB_HASH f287cd007240a838286ff13e7deaee12)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)
set(TBB_CPE "cpe:2.3:a:intel:threading_building_blocks:${TBB_YEAR}:*:*:*:*:*:*:*")
set(TBB_HOMEPAGE https://software.intel.com/en-us/oneapi/onetbb)
set(TBB_LICENSE SPDX:Apache-2.0)
set(TBB_COPYRIGHT "Copyright (c) 2005-2020 Intel Corporation")

set(NANOBIND_VERSION v2.1.0)
set(NANOBIND_NAME NanoBind)
set(NANOBIND_URI https://github.com/wjakob/nanobind/archive/refs/tags/${NANOBIND_VERSION}.tar.gz)
set(NANOBIND_HASH 363e96957741869bb16ff983c042e72f)
set(NANOBIND_HASH_TYPE MD5)
set(NANOBIND_FILE nanobind-${NANOBIND_VERSION}.tar.gz)
set(NANOBIND_HOMEPAGE https://github.com/wjakob/nanobind)
set(NANOBIND_LICENSE SPDX:BSD-3-Clause)
set(NANOBIND_COPYRIGHT "2023, Wenzel Jakob")

set(OPENVDB_VERSION 12.0.0)
set(OPENVDB_NAME OpenVDB)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH c5cd16784b6b42aaa956b574042f374d)
set(OPENVDB_HASH_TYPE MD5)
set(OPENVDB_FILE openvdb-${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HOMEPAGE http://www.openvdb.org/)
set(OPENVDB_LICENSE SPDX:MPL-2.0)
set(OPENVDB_COPYRIGHT "Copyright Contributors to the OpenVDB Project")

# ------------------------------------------------------------------------------
# Python Modules
# cattrs + fastjson schema + deps as requested by #141945
set(ATTRS_VERSION 25.3.0)
set(CATTRS_VERSION 25.1.1)
set(FASTJSONSCHEMA_VERSION 2.21.1)
set(TYPING_EXTENSIONS_VERSION 4.14.1)

# Needed by: `requests` module (so the version doesn't change on rebuild).
set(IDNA_VERSION 3.10)
# Needed by: `requests` module (so the version doesn't change on rebuild).
set(CHARSET_NORMALIZER_VERSION 3.4.1)
# Needed by: `requests` module (so the version doesn't change on rebuild).
set(URLLIB3_VERSION 2.4.0)
set(URLLIB3_CPE "cpe:2.3:a:urllib3:urllib3:${URLLIB3_VERSION}:*:*:*:*:*:*:*")
# Needed by: Python's `requests` module (so add-ons can authenticate against trusted certificates).
set(CERTIFI_VERSION 2025.4.26)
# Needed by: Some of Blender's add-ons (to support convenient interaction with online services).
set(REQUESTS_VERSION 2.32.3)
# Needed by: Python's `numpy` module (used by some add-ons).
set(CYTHON_VERSION 3.0.11)
set(CYTHON_URI
https://github.com/cython/cython/releases/download/${CYTHON_VERSION}-1/cython-${CYTHON_VERSION}.tar.gz)
set(CYTHON_HASH 388b85b7c23f501320d19d991b169f5d)
set(CYTHON_HASH_TYPE MD5)
set(CYTHON_FILE cython-${CYTHON_VERSION}.tar.gz)
set(CYTHON_HOMEPAGE https://cython.org/)
set(CYTHON_LICENSE SPDX:Apache-2.0)
set(CYTHON_COPYRIGHT "Copyright Contributors to the Cython Project")
# Needed by: Python scripts that read `.blend` files, as files may use Z-standard compression. (Once we move to Python 3.14, this could be replaced with inbuilt Zstandard support, see https://peps.python.org/pep-0784/)
set(ZSTANDARD_VERSION 0.23.0)
# NOTE: the release is not yet on GITHUB.
# https://github.com/indygreg/python-zstandard/releases/download/${ZSTANDARD_VERSION}/zstandard-${ZSTANDARD_VERSION}.tar.gz)
set(ZSTANDARD_URI https://files.pythonhosted.org/packages/ed/f6/2ac0287b442160a89d726b17a9184a4c615bb5237db763791a7fd16d9df1/zstandard-0.23.0.tar.gz)
set(ZSTANDARD_HASH b2d8c62d08e7255f68f7a740bae85b3c9b8e5466baa9cbf7f57f1cde0ac6bc09)
set(ZSTANDARD_HASH_TYPE SHA256)
set(ZSTANDARD_FILE zstandard-${ZSTANDARD_VERSION}.tar.gz)
set(ZSTANDARD_HOMEPAGE https://github.com/indygreg/python-zstandard/)
set(ZSTANDARD_LICENSE SPDX:BSD-3-Clause)
set(ZSTANDARD_COPYRIGHT "Copyright (c) 2016, Gregory Szorc. All rights reserved.")
# Auto-format Python source (developer tool, not used by Blender at run-time).
set(AUTOPEP8_VERSION 2.3.1)
# Needed by: `autopep8` (so the version doesn't change on rebuild).
set(PYCODESTYLE_VERSION 2.13)
# Build system for other packages (not used by Blender at run-time).
set(MESON_VERSION 0.63.0)

# When this numpy version is bumped, please also change the limit value set for variable `install_requires`
# in build_files/utils/make_bpy_wheel.py
set(NUMPY_VERSION 1.26.4)
set(NUMPY_SHORT_VERSION 1.26)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_HASH 19550cbe7bedd96a928da9d4ad69509d)
set(NUMPY_HASH_TYPE MD5)
set(NUMPY_FILE numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_CPE "cpe:2.3:a:numpy:numpy:${NUMPY_VERSION}:*:*:*:*:*:*:*")
set(NUMPY_HOMEPAGE https://numpy.org/)
set(NUMPY_LICENSE SPDX:BSD-3-Clause)
set(NUMPY_COPYRIGHT "Copyright (c) 2005-2022, NumPy Developers. All rights reserved.")

set(LAME_VERSION 3.100)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 83e260acbe4389b54fe08e0bdbf7cddb)
set(LAME_HASH_TYPE MD5)
set(LAME_FILE lame-${LAME_VERSION}.tar.gz)
set(LAME_CPE "cpe:2.3:a:lame_project:lame:${LAME_VERSION}:*:*:*:*:*:*:*")
set(LAME_NAME LAME)
set(LAME_HOMEPAGE https://lame.sourceforge.io/)
set(LAME_LICENSE SPDX:LGPL-2.1-or-later)
set(LAME_COPYRIGHT [=[
Copyrights (c) 1999-2011 by The LAME Project.
Copyrights (c) 1999,2000,2001 by Mark Taylor.
Copyrights (c) 1998 by Michael Cheng.
Copyrights (c) 1995,1996,1997 by Michael Hipp: mpglib.
]=])

set(OGG_VERSION 1.3.5)
set(OGG_URI http://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz)
set(OGG_HASH 0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664)
set(OGG_HASH_TYPE SHA256)
set(OGG_FILE libogg-${OGG_VERSION}.tar.gz)
set(OGG_HOMEPAGE https://xiph.org/ogg/)
set(OGG_LICENSE SPDX:BSD-3-Clause)
set(OGG_COPYRIGHT "COPYRIGHT (C) 1994-2019 by the Xiph.Org Foundation https://www.xiph.org/")

set(VORBIS_VERSION 1.3.7)
set(VORBIS_URI http://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_HASH 0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab)
set(VORBIS_HASH_TYPE SHA256)
set(VORBIS_FILE libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_CPE "cpe:2.3:a:xiph.org:libvorbis:${VORBIS_VERSION}:*:*:*:*:*:*:*")
set(VORBIS_HOMEPAGE https://xiph.org/vorbis/)
set(VORBIS_LICENSE SPDX:BSD-3-Clause)
set(VORBIS_COPYRIGHT "Copyright (c) 2002-2020 Xiph.org Foundation")

set(THEORA_VERSION 1.1.1)
set(THEORA_URI http://downloads.xiph.org/releases/theora/libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HASH b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc)
set(THEORA_HASH_TYPE SHA256)
set(THEORA_FILE libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HOMEPAGE https://xiph.org/theora/)
set(THEORA_LICENSE SPDX:BSD-3-Clause)
set(THEORA_COPYRIGHT "Copyright (C) 2002-2009 Xiph.org Foundation")

set(FLAC_VERSION 1.4.2)
set(FLAC_URI http://downloads.xiph.org/releases/flac/flac-${FLAC_VERSION}.tar.xz)
set(FLAC_HASH e322d58a1f48d23d9dd38f432672865f6f79e73a6f9cc5a5f57fcaa83eb5a8e4 )
set(FLAC_HASH_TYPE SHA256)
set(FLAC_FILE flac-${FLAC_VERSION}.tar.xz)
set(FLAC_CPE "cpe:2.3:a:flac_project:flac:${FLAC_VERSION}:*:*:*:*:*:*:*")
set(FLAC_HOMEPAGE https://xiph.org/flac/)
set(FLAC_LICENSE SPDX:GPL-2.0-or-later)
set(FLAC_COPYRIGHT [=[
Copyright (C) 2001-2009  Josh Coalson.
Copyright (C) 2011-2016  Xiph.Org Foundation.
]=])

set(VPX_VERSION 1.15.2)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 26fcd3db88045dee380e581862a6ef106f49b74b6396ee95c2993a260b4636aa)
set(VPX_HASH_TYPE SHA256)
set(VPX_FILE libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_CPE "cpe:2.3:a:webmproject:libvpx:${VPX_VERSION}:*:*:*:*:*:*:*")
set(VPX_HOMEPAGE https://github.com/webmproject/libvpx)
set(VPX_LICENSE SPDX:BSD-3-Clause)
set(VPX_COPYRIGHT "Copyright (c) 2010, The WebM Project authors. All rights reserved.")

set(OPUS_VERSION 1.3.1)
set(OPUS_URI https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HASH 65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d)
set(OPUS_HASH_TYPE SHA256)
set(OPUS_FILE opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HOMEPAGE https://opus-codec.org/)
set(OPUS_LICENSE SPDX:BSD-3-Clause)
set(OPUS_COPYRIGHT [=[
Copyright 2001-2023
Xiph.Org, Skype Limited, Octasic,
Jean-Marc Valin, Timothy B. Terriberry,
CSIRO, Gregory Maxwell, Mark Borgerding,
Erik de Castro Lopo, Mozilla, Amazon
]=])

set(X264_VERSION 35fe20d1ba49918ec739a5b068c208ca82f977f7)
set(X264_URI https://code.videolan.org/videolan/x264/-/archive/${X264_VERSION}/x264-${X264_VERSION}.tar.gz)
set(X264_HASH bb4f7da03936b5a030ed5827133b58eb3f701d7e5dce32cca4ba6df93797d42e)
set(X264_HASH_TYPE SHA256)
set(X264_FILE x264-${X264_VERSION}.tar.gz)
set(X264_HOMEPAGE https://www.videolan.org/developers/x264.html)
set(X264_LICENSE SPDX:GPL-2.0-or-later)
set(X264_COPYRIGHT "Copyright (C) 2003-2021 x264 project")

set(X265_VERSION 3cf6c1e53037eb9e198860365712e1bafb22f7c6)
set(X265_URI https://bitbucket.org/multicoreware/x265_git/get/${X265_VERSION}.tar.gz)
set(X265_HASH 40d12016192cdc740132cb00dd6cc80ead094ff91a1a897181256def2011342e)
set(X265_HASH_TYPE SHA256)
set(X265_FILE x265-${X265_VERSION}.tar.gz)
set(X265_HOMEPAGE https://www.videolan.org/developers/x265.html)
set(X265_LICENSE SPDX:GPL-2.0-or-later)
set(X265_COPYRIGHT "Copyright (C) 2013-2020 MulticoreWare, Inc")

set(OPENJPEG_VERSION 2.5.3)
set(OPENJPEG_SHORT_VERSION 2.5)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 368fe0468228e767433c9ebdea82ad9d801a3ad1e4234421f352c8b06e7aa707)
set(OPENJPEG_HASH_TYPE SHA256)
set(OPENJPEG_NAME OpenJPEG)
set(OPENJPEG_HOMEPAGE https://github.com/uclouvain/openjpeg)
set(OPENJPEG_FILE openjpeg-v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_CPE "cpe:2.3:a:uclouvain:openjpeg:${OPENJPEG_VERSION}:*:*:*:*:*:*:*")
set(OPENJPEG_LICENSE SPDX:BSD-2-Clause)
set(OPENJPEG_COPYRIGHT [=[
Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium.
Copyright (c) 2002-2014, Professor Benoit Macq.
Copyright (c) 2003-2014, Antonin Descampe.
Copyright (c) 2003-2009, Francois-Olivier Devaux.
Copyright (c) 2005, Herve Drolon, FreeImage Team.
Copyright (c) 2002-2003, Yannick Verschueren.
Copyright (c) 2001-2003, David Janssens.
Copyright (c) 2011-2012, Centre National d'Etudes Spatiales (CNES), France.
Copyright (c) 2012, CS Systemes d'Information, France.
]=])

set(FFMPEG_VERSION 7.1.1)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH 0c8da2f11579a01e014fc007cbacf5bb4da1d06afd0b43c7f8097ec7c0f143ba)
set(FFMPEG_HASH_TYPE SHA256)
set(FFMPEG_FILE ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_CPE "cpe:2.3:a:ffmpeg:ffmpeg:${FFMPEG_VERSION}:*:*:*:*:*:*:*")
set(FFMPEG_NAME FFmpeg)
set(FFMPEG_HOMEPAGE https://ffmpeg.org/)
set(FFMPEG_LICENSE SPDX:LGPL-2.1-or-later)
set(FFMPEG_COPYRIGHT "The FFmpeg contributors https://github.com/FFmpeg/FFmpeg/blob/master/CREDITS")

set(FFTW_VERSION 3.3.10)
set(FFTW_NAME FFTW)
set(FFTW_URI http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HASH 8ccbf6a5ea78a16dbc3e1306e234cc5c)
set(FFTW_HASH_TYPE MD5)
set(FFTW_FILE fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HOMEPAGE https://www.fftw.org/)
set(FFTW_LICENSE SPDX:GPL-2.0-or-later)
set(FFTW_COPYRIGHT [=[
Copyright (c) 2003, 2007-14 Matteo Frigo.
Copyright (c) 2003, 2007-14 Massachusetts Institute of Technology
]=])

set(ICONV_VERSION 1.16)
set(ICONV_URI http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HASH 7d2a800b952942bb2880efb00cfd524c)
set(ICONV_HASH_TYPE MD5)
set(ICONV_FILE libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HOMEPAGE https://www.gnu.org/software/libiconv/)
set(ICONV_LICENSE SPDX:LGPL-2.1-or-later)
set(ICONV_COPYRIGHT "Copyright (C) 1998, 2022 Free Software Foundation, Inc.")

set(SNDFILE_VERSION 1.2.2)
set(SNDFILE_NAME libsndfile)
set(SNDFILE_URI https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_HASH 04e2e6f726da7c5dc87f8cf72f250d04)
set(SNDFILE_HASH_TYPE MD5)
set(SNDFILE_FILE libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_CPE "cpe:2.3:a:libsndfile_project:libsndfile:${SNDFILE_VERSION}:*:*:*:*:*:*:*")
set(SNDFILE_HOMEPAGE http://libsndfile.github.io/libsndfile/)
set(SNDFILE_LICENSE SPDX:LGPL-2.1-or-later)
set(SNDFILE_COPYRIGHT "Copyright (C) 2011-2016 Erik de Castro Lopo <erikd@mega-nerd.com>")

set(WEBP_VERSION 1.3.2)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH 34869086761c0e2da6361035f7b64771)
set(WEBP_HASH_TYPE MD5)
set(WEBP_FILE libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_CPE "cpe:2.3:a:webmproject:libwebp:${WEBP_VERSION}:*:*:*:*:*:*:*")
set(WEBP_HOMEPAGE https://developers.google.com/speed/webp)
set(WEBP_LICENSE SPDX:BSD-3-Clause)
set(WEBP_COPYRIGHT "Copyright (c) 2010, Google Inc. All rights reserved.")

set(SPNAV_VERSION 1.1)
set(SPNAV_URI https://github.com/FreeSpacenav/libspnav/releases/download/v${SPNAV_VERSION}/libspnav-${SPNAV_VERSION}.tar.gz)
set(SPNAV_HASH 7c0032034672dfba3c4bb9b49a440e70)
set(SPNAV_HASH_TYPE MD5)
set(SPNAV_NAME FreeSpacenav)
set(SPNAV_FILE libspnav-${SPNAV_VERSION}.tar.gz)
set(SPNAV_HOMEPAGE https://github.com/FreeSpacenav/libspnav)
set(SPNAV_LICENSE SPDX:BSD-3-Clause)
set(SPNAV_COPYRIGHT "Copyright (C) 2007-2022 John Tsiombikas nuclear@member.fsf.org")

set(JEMALLOC_VERSION 5.2.1)
set(JEMALLOC_URI https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2)
set(JEMALLOC_HASH 3d41fbf006e6ebffd489bdb304d009ae)
set(JEMALLOC_HASH_TYPE MD5)
set(JEMALLOC_FILE jemalloc-${JEMALLOC_VERSION}.tar.bz2)
set(JEMALLOC_HOMEPAGE https://jemalloc.net/)
set(JEMALLOC_NAME jemalloc)
set(JEMALLOC_LICENSE SPDX:BSD-2-Clause)
set(JEMALLOC_COPYRIGHT [=[
Copyright (C) 2002-2013 Jason Evans <jasone@canonware.com>. All rights reserved.
Copyright (C) 2007-2012 Mozilla Foundation.  All rights reserved.
Copyright (C) 2009-2013 Facebook, Inc.  All rights reserved.
Copyright (C) 2013 Jason Evans <jasone@canonware.com>.
]=])

set(XML2_VERSION 2.14.5)
set(XML2_URI https://download.gnome.org/sources/libxml2/2.14/libxml2-${XML2_VERSION}.tar.xz)
set(XML2_HASH 59aac4e5d1d350ba2c4bddf1f7bc5098)
set(XML2_HASH_TYPE MD5)
set(XML2_FILE libxml2-${XML2_VERSION}.tar.xz)
set(XML2_CPE "cpe:2.3:a:xmlsoft:libxml2:${XML2_VERSION}:*:*:*:*:*:*:*")
set(XML2_NAME libxml2)
set(XML2_HOMEPAGE https://gitlab.gnome.org/GNOME/libxml2)
set(XML2_LICENSE SPDX:MIT)
set(XML2_COPYRIGHT "Copyright (C) 1998-2012 Daniel Veillard. All Rights Reserved.")

set(YAMLCPP_VERSION 0.7.0)
set(YAMLCPP_URI https://codeload.github.com/jbeder/yaml-cpp/tar.gz/yaml-cpp-${YAMLCPP_VERSION})
set(YAMLCPP_HASH 74d646a3cc1b5d519829441db96744f0)
set(YAMLCPP_HASH_TYPE MD5)
set(YAMLCPP_FILE yaml-cpp-${YAMLCPP_VERSION}.tar.gz)
set(YAMLCPP "cpe:2.3:a:yaml-cpp_project:yaml-cpp:${YAMLCPP_VERSION}:*:*:*:*:*:*:*")
set(YAMLCPP_LICENSE SPDX:MIT)
set(YAMLCPP_COPYRIGHT "Copyright (c) 2008-2015 Jesse Beder")

set(PYSTRING_VERSION v1.1.3)
set(PYSTRING_URI https://codeload.github.com/imageworks/pystring/tar.gz/refs/tags/${PYSTRING_VERSION})
set(PYSTRING_HASH f2c68786b359f5e4e62bed53bc4fb86d)
set(PYSTRING_HASH_TYPE MD5)
set(PYSTRING_FILE pystring-${PYSTRING_VERSION}.tar.gz)
set(PYSTRING_HOMEPAGE https://github.com/imageworks/pystring)
set(PYSTRING_LICENSE SPDX:BSD-3-Clause)
set(PYSTRING_COPYRIGHT "Copyright (c) 2008-2010, Sony Pictures Imageworks Inc; All rights reserved.")

set(EXPAT_VERSION 2_6_4)
set(EXPAT_VERSION_DOTS 2.6.4)
set(EXPAT_URI https://github.com/libexpat/libexpat/archive/R_${EXPAT_VERSION}.tar.gz)
set(EXPAT_HASH f2f8d61e2bd71d1118e1258ed9f4c0b7)
set(EXPAT_HASH_TYPE MD5)
set(EXPAT_FILE libexpat-${EXPAT_VERSION}.tar.gz)
set(EXPAT_HOMEPAGE https://github.com/libexpat/libexpat/)
set(EXPAT_CPE "cpe:2.3:a:libexpat_project:libexpat:${EXPAT_VERSION_DOTS}:*:*:*:*:*:*:*")
set(EXPAT_LICENSE SPDX:MIT)
set(EXPAT_COPYRIGHT [=[
Copyright (c) 1998-2000 Thai Open Source Software Center Ltd and Clark Cooper.
Copyright (c) 2001-2019 Expat maintainers.
]=])

set(PUGIXML_VERSION 1.10)
set(PUGIXML_URI https://github.com/zeux/pugixml/archive/v${PUGIXML_VERSION}.tar.gz)
set(PUGIXML_HASH 0c208b0664c7fb822bf1b49ad035e8fd)
set(PUGIXML_HASH_TYPE MD5)
set(PUGIXML_FILE pugixml-${PUGIXML_VERSION}.tar.gz)
set(PUGIXML_CPE "cpe:2.3:a:pugixml_project:pugixml:${PUGIXML_VERSION}:*:*:*:*:*:*:*")
set(PUGIXML_HOMEPAGE https://pugixml.org/)
set(PUGIXML_LICENSE SPDX:MIT)
set(PUGIXML_COPYRIGHT "Copyright (c) 2006-2020 Arseny Kapoulkine")

set(FLEXBISON_VERSION 2.5.24)
set(FLEXBISON_URI http://prdownloads.sourceforge.net/winflexbison/win_flex_bison-${FLEXBISON_VERSION}.zip)
set(FLEXBISON_HASH 6b549d43e34ece0e8ed05af92daa31c4)
set(FLEXBISON_HASH_TYPE MD5)
set(FLEXBISON_FILE win_flex_bison-${FLEXBISON_VERSION}.zip)
set(FLEXBISON_HOMEPAGE https://github.com/lexxmark/winflexbison)
set(FLEXBISON_DEPSBUILDTIMEONLY "Blender ships the produced artifact, but doesn't ship/link with any binary")

set(FLEX_VERSION 2.6.4)
set(FLEX_URI https://github.com/westes/flex/releases/download/v${FLEX_VERSION}/flex-${FLEX_VERSION}.tar.gz)
set(FLEX_HASH 2882e3179748cc9f9c23ec593d6adc8d)
set(FLEX_HASH_TYPE MD5)
set(FLEX_FILE flex-${FLEX_VERSION}.tar.gz)
set(FLEX_DEPSBUILDTIMEONLY "Blender ships the produced artifact, but doesn't ship/link with any binary")

# Libraries to keep Python modules static on Linux.

# NOTE: bzip.org domain does no longer belong to BZip 2 project, so we download
# sources from Debian packaging.
#
# NOTE 2: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat. For compliance reasons there
# can be no exceptions to this.
set(BZIP2_VERSION 1.0.8)
set(BZIP2_URI http://http.debian.net/debian/pool/main/b/bzip2/bzip2_${BZIP2_VERSION}.orig.tar.gz)
set(BZIP2_HASH ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269)
set(BZIP2_HASH_TYPE SHA256)
set(BZIP2_FILE bzip2_${BZIP2_VERSION}.orig.tar.gz)
set(BZIP2_CPE "cpe:2.3:a:bzip:bzip2:${BZIP2_VERSION}:*:*:*:*:*:*:*")
set(BZIP2_HOMEPAGE https://sourceware.org/bzip2/)
set(BZIP2_LICENSE SPDX:bzip2-1.0.6)
set(BZIP2_COPYRIGHT "Copyright (C) 1996-2019 Julian R Seward. All rights reserved.")

# NOTE: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat. For compliance reasons there
# can be no exceptions to this.
set(FFI_VERSION 3.4.4)
set(FFI_NAME libffi)
set(FFI_URI https://github.com/libffi/libffi/releases/download/v${FFI_VERSION}/libffi-${FFI_VERSION}.tar.gz)
set(FFI_HASH d66c56ad259a82cf2a9dfc408b32bf5da52371500b84745f7fb8b645712df676)
set(FFI_HASH_TYPE SHA256)
set(FFI_FILE libffi-${FFI_VERSION}.tar.gz)
set(FFI_CPE "cpe:2.3:a:libffi_project:libffi:${FFI_VERSION}:*:*:*:*:*:*:*")
set(FFI_HOMEPAGE https://github.com/libffi/libffi/)
set(FFI_LICENSE SPDX:MIT)
set(FFI_COPYRIGHT "Copyright (c) 1996-2024  Anthony Green, Red Hat, Inc and others.")

set(LZMA_VERSION 5.2.5)
set(LZMA_URI https://tukaani.org/xz/xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_HASH 5117f930900b341493827d63aa910ff5e011e0b994197c3b71c08a20228a42df)
set(LZMA_HASH_TYPE SHA256)
set(LZMA_FILE xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_NAME LZMA)
set(LZMA_HOMEPAGE https://tukaani.org/lzma/)
set(LZMA_LICENSE SPDX:GPL-3.0-or-later)
set(LZMA_COPYRIGHT "Igor Pavlov, Ville Koskinen, Lasse Collin")

# NOTE: Python's build has been modified to use our ssl version.
set(SSL_VERSION 3.5.2)
set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
set(SSL_HASH c53a47e5e441c930c3928cf7bf6fb00e5d129b630e0aa873b08258656e7345ec)
set(SSL_HASH_TYPE SHA256)
set(SSL_FILE openssl-${SSL_VERSION}.tar.gz)
set(SSL_CPE "cpe:2.3:a:openssl:openssl:${SSL_VERSION}:*:*:*:*:*:*:*")
set(SSL_HOMEPAGE https://www.openssl.org)
set(SSL_NAME OpenSSL)
set(SSL_LICENSE SPDX:Apache-2.0)
set(SSL_COPYRIGHT [=[
Copyright (c) 1998-2024 The OpenSSL Project Authors.
Copyright (c) 1995-1998 Eric A. Young, Tim J. Hudson; All rights reserved.
]=])

set(SQLITE_VERSION 3.50.4)
set(SQLLITE_LONG_VERSION 3500400)
set(SQLITE_URI https://www.sqlite.org/2025/sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_HASH 145048005c777796dd8494aa1cfed304e8c34283)
set(SQLITE_HASH_TYPE SHA1)
set(SQLITE_FILE sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_CPE "cpe:2.3:a:sqlite:sqlite:${SQLITE_VERSION}:*:*:*:*:*:*:*")
set(SQLITE_HOMEPAGE https://www.sqlite.org)
set(SQLITE_LICENSE Public Domain)

set(EMBREE_VERSION 4.4.0)
set(EMBREE_URI https://github.com/RenderKit/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 56e5f414b9ae441b366be79867b8fb07)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)
set(EMBREE_HOMEPAGE https://github.com/RenderKit/embree)
set(EMBREE_LICENSE SPDX:Apache-2.0)
set(EMBREE_COPYRIGHT "Copyright 2009-2024 Intel Corporation")

set(USD_VERSION 25.08)
set(USD_NAME USD)
set(USD_URI https://github.com/PixarAnimationStudios/OpenUSD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH e107ce8d79471ddd8b636bb982c24a46)
set(USD_HASH_TYPE MD5)
set(USD_FILE usd-v${USD_VERSION}.tar.gz)
set(USD_HOMEPAGE https://openusd.org/)
set(USD_LICENSE TOST-1.0)
set(USD_COPYRIGHT [=[
Universal Scene Description
Copyright 2016 Pixar
All rights reserved.
This product includes software developed at:
Pixar (http://www.pixar.com/).
]=])

set(MATERIALX_VERSION 1.39.3)
set(MATERIALX_NAME MaterialX)
set(MATERIALX_URI https://github.com/AcademySoftwareFoundation/MaterialX/archive/refs/tags/v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HASH 43fdf32d52d6e04e40761e3c08f66476)
set(MATERIALX_HASH_TYPE MD5)
set(MATERIALX_FILE materialx-v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HOMEPAGE https://github.com/AcademySoftwareFoundation/MaterialX)
set(MATERIALX_LICENSE SPDX:Apache-2.0)
set(MATERIALX_COPYRIGHT "Copyright Contributors to the MaterialX Project")

set(OIDN_VERSION 2.3.3)
set(OIDN_NAME OpenImageDenoise)
set(OIDN_URI https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HASH f7756731235b51dfb788787e2f7c9ee0)
set(OIDN_HASH_TYPE MD5)
set(OIDN_FILE oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HOMEPAGE https://www.openimagedenoise.org/)
set(OIDN_LICENSE SPDX:Apache-2.0)
set(OIDN_COPYRIGHT "Copyright 2009-2025 Intel Corporation")

set(LIBGLU_VERSION 9.0.1)
set(LIBGLU_URI https://archive.mesa3d.org/glu/glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HASH 151aef599b8259efe9acd599c96ea2a3)
set(LIBGLU_HASH_TYPE MD5)
set(LIBGLU_FILE glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HOMEPAGE https://gitlab.freedesktop.org/mesa/glu)
set(LIBGLU_LICENSE SPDX:SGI-B-2.0)
set(LIBGLU_COPYRIGHT "Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.")

set(MESA_VERSION 23.3.0)
set(MESA_URI https://archive.mesa3d.org/mesa-${MESA_VERSION}.tar.xz)
set(MESA_HASH 50f729dd60ed6335b989095baad81ef5edf7cfdd4b4b48b9b955917cb07d69c5)
set(MESA_HASH_TYPE SHA256)
set(MESA_FILE mesa-${MESA_VERSION}.tar.xz)
set(MESA_CPE "cpe:2.3:a:mesa3d:mesa:${MESA_VERSION}:*:*:*:*:*:*:*")
set(MESA_HOMEPAGE https://www.mesa3d.org/)
set(MESA_LICENSE SPDX:MIT)
set(MESA_COPYRIGHT "Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.")

set(NASM_VERSION 2.15.02)
set(NASM_URI https://github.com/netwide-assembler/nasm/archive/nasm-${NASM_VERSION}.tar.gz)
set(NASM_HASH aded8b796c996a486a56e0515c83e414116decc3b184d88043480b32eb0a8589)
set(NASM_HASH_TYPE SHA256)
set(NASM_FILE nasm-${NASM_VERSION}.tar.gz)
set(NASM_PCE "cpe:2.3:a:nasm:nasm:${NASM_VERSION}:*:*:*:*:*:*:*")
set(NASM_DEPSBUILDTIMEONLY "Blender ships the produced artifact, but doesn't ship/link with any binary")

set(XR_OPENXR_SDK_VERSION 1.0.22)
set(XR_OPENXR_SDK_URI https://github.com/KhronosGroup/OpenXR-SDK/archive/release-${XR_OPENXR_SDK_VERSION}.tar.gz)
set(XR_OPENXR_SDK_HASH a2623ebab3d0b340bc16311b14f02075)
set(XR_OPENXR_SDK_HASH_TYPE MD5)
set(XR_OPENXR_SDK_FILE OpenXR-SDK-${XR_OPENXR_SDK_VERSION}.tar.gz)
set(XR_OPENXR_SDK_NAME OpenXR)
set(XR_OPENXR_SDK_HOMEPAGE https://khronos.org/openxr/)
set(XR_OPENXR_SDK_LICENSE SPDX:Apache-2.0)
set(XR_OPENXR_SDK_COPYRIGHT [=[
Copyright (c) 2017-2020 The Khronos Group Inc.
Copyright (c) 2017-2019 Valve Corporation.
Copyright (c) 2017-2019 LunarG, Inc.
Copyright (c) 2019 Collabora, Ltd.
]=])

set(WL_PROTOCOLS_VERSION 1.44)
set(WL_PROTOCOLS_NAME Wayland-Protocols)
set(WL_PROTOCOLS_FILE wayland-protocols-${WL_PROTOCOLS_VERSION}.tar.xz)
set(WL_PROTOCOLS_URI https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/${WL_PROTOCOLS_VERSION}/downloads/${WL_PROTOCOLS_FILE})
set(WL_PROTOCOLS_HASH bbf053c2d62cf11e253cf2cc151c2df0)
set(WL_PROTOCOLS_HASH_TYPE MD5)
set(WL_PROTOCOLS_HOMEPAGE https://gitlab.freedesktop.org/wayland/wayland-protocols)
set(WL_PROTOCOLS_LICENSE SPDX:MIT)
set(WL_PROTOCOLS_COPYRIGHT [=[
Copyright © 2008-2013 Kristian Høgsberg.
Copyright © 2010-2013 Intel Corporation.
Copyright © 2013 Rafael Antognolli.
Copyright © 2013 Jasper St. Pierre.
Copyright © 2014 Jonas Ådahl.
Copyright © 2014 Jason Ekstrand.
Copyright © 2014-2015 Collabora, Ltd.
Copyright © 2015 Red Hat Inc.
]=])

set(WAYLAND_VERSION 1.24.0)
set(WAYLAND_FILE wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_URI https://gitlab.freedesktop.org/wayland/wayland/-/releases/${WAYLAND_VERSION}/downloads/wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_HASH fda0b2a73ea2716f61d75767e02008e1)
set(WAYLAND_HASH_TYPE MD5)
set(WAYLAND_HOMEPAGE https://gitlab.freedesktop.org/wayland/wayland)
set(WAYLAND_LICENSE SPDX:MIT)
set(WAYLAND_COPYRIGHT [=[
Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
Copyright © 2011 Kristian Høgsberg.
Copyright © 2011 Benjamin Franzke.
Copyright © 2010-2012 Intel Corporation.
Copyright © 2012 Collabora, Ltd.
Copyright © 2015 Giulio Camuffo.
Copyright © 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com.
Copyright © 2012 Jason Ekstrand.
Copyright (c) 2014 Red Hat, Inc.
Copyright © 2013 Marek Chalupa.
Copyright © 2014 Jonas Ådahl.
Copyright © 2016 Yong Bakos.
Copyright © 2017 Samsung Electronics Co., Ltd.
Copyright © 2002 Keith Packard.
Copyright 1999 SuSE, Inc.
Copyright © 2012 Philipp Brüschweiler.
Copyright (c) 2020 Simon Ser.
Copyright (c) 2006, 2008 Junio C Hamano.
]=])

set(WAYLAND_LIBDECOR_VERSION 0.2.2)
set(WAYLAND_LIBDECOR_FILE libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz)
set(WAYLAND_LIBDECOR_URI https://gitlab.freedesktop.org/libdecor/libdecor/-/releases/${WAYLAND_LIBDECOR_VERSION}/downloads/libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz)
set(WAYLAND_LIBDECOR_HASH 5b7f4a10a9335b62101bccc220e2d13a)
set(WAYLAND_LIBDECOR_HASH_TYPE MD5)
set(WAYLAND_LIBDECOR_HOMEPAGE https://gitlab.freedesktop.org/libdecor/libdecor)
set(WAYLAND_LIBDECOR_LICENSE SPDX:MIT)
set(WAYLAND_LIBDECOR_COPYRIGHT [=[
Copyright © 2010 Intel Corporation.
Copyright © 2011 Benjamin Franzke.
Copyright © 2018-2021 Jonas Ådahl.
Copyright © 2019 Christian Rauch.
Copyright (c) 2006, 2008 Junio C Hamano.
Copyright © 2017-2018 Red Hat Inc.
Copyright © 2012 Collabora, Ltd.
Copyright © 2008 Kristian Høgsberg.
]=])

set(WAYLAND_WESTON_VERSION 14.0.2)
set(WAYLAND_WESTON_FILE weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_URI https://gitlab.freedesktop.org/wayland/weston/-/releases/${WAYLAND_WESTON_VERSION}/downloads/weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_HASH 4575a052e2ff3ea7819cfbf33868f8f5)
set(WAYLAND_WESTON_HASH_TYPE MD5)
set(WAYLAND_WESTON_HOMEPAGE https://gitlab.freedesktop.org/wayland/weston)
set(WAYLAND_WESTON_LICENSE SPDX:MIT)
set(WAYLAND_WESTON_COPYRIGHT [=[
Copyright © 2008-2012 Kristian Høgsberg.
Copyright © 2010-2012 Intel Corporation.
Copyright © 2010-2011 Benjamin Franzke.
Copyright © 2011-2012 Collabora, Ltd.
Copyright © 2010 Red Hat <mjg@redhat.com>.
]=])

set(ISPC_VERSION v1.28.0)
set(ISPC_URI https://github.com/ispc/ispc/archive/${ISPC_VERSION}.tar.gz)
set(ISPC_HASH 5ada5ee20badc849647213fda08e651c)
set(ISPC_HASH_TYPE MD5)
set(ISPC_FILE ispc-${ISPC_VERSION}.tar.gz)
set(ISPC_DEPSBUILDTIMEONLY "Blender ships the produced artifact, but doesn't ship/link with any binary")

set(GMP_VERSION 6.3.0)
set(GMP_URI https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.xz)
set(GMP_HASH 956dc04e864001a9c22429f761f2c283)
set(GMP_HASH_TYPE MD5)
set(GMP_FILE gmp-${GMP_VERSION}.tar.xz)
set(GMP_CPE "cpe:2.3:a:gmplib:gmp:${GMP_VERSION}:*:*:*:*:*:*:*")
set(GMP_NAME GMP)
set(GMP_HOMEPAGE https://gmplib.org/)
set(GMP_LICENSE SPDX:GPL-2.0-or-later)
set(GMP_COPYRIGHT "Copyright 1996-2020 Free Software Foundation, Inc.")

set(POTRACE_VERSION 1.16)
set(POTRACE_URI http://potrace.sourceforge.net/download/${POTRACE_VERSION}/potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_HASH 5f0bd87ddd9a620b0c4e65652ef93d69)
set(POTRACE_HASH_TYPE MD5)
set(POTRACE_FILE potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_CPE "cpe:2.3:a:icoasoft:potrace:${POTRACE_VERSION}:*:*:*:*:*:*:*")
set(POTRACE_HOMEPAGE https://potrace.sourceforge.net/)
set(POTRACE_LICENSE SPDX:GPL-2.0-or-later)
set(POTRACE_COPYRIGHT "Copyright © 2001-2019 Peter Selinger.")


set(HARU_VERSION 2.4.5)
set(HARU_URI https://github.com/libharu/libharu/archive/refs/tags/v${HARU_VERSION}.tar.gz)
set(HARU_HASH d5633fb741079a7675bb3e9e8e8a58ce)
set(HARU_HASH_TYPE MD5)
set(HARU_FILE libharu-${HARU_VERSION}.tar.gz)
set(HARU_HOMEPAGE http://libharu.org/)
set(HARU_LICENSE SPDX:Zlib)
set(HARU_COPYRIGHT [=[
Copyright (C) 1999-2006 Takeshi Kanno.
Copyright (C) 2007-2009 Antony Dovgal.
]=])

set(ZSTD_VERSION 1.5.6)
set(ZSTD_URI https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_HASH 8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1)
set(ZSTD_HASH_TYPE SHA256)
set(ZSTD_FILE zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_CPE "cpe:2.3:a:facebook:zstandard:${ZSTD_VERSION}:*:*:*:*:*:*:*")
set(ZSTD_HOMEPAGE https://github.com/facebook/zstd)
set(ZSTD_LICENSE SPDX:BSD-3-Clause)
set(ZSTD_COPYRIGHT "Copyright (c) 2016-present, Facebook, Inc. All rights reserved.")

set(SSE2NEON_VERSION 227cc413fb2d50b2a10073087be96b59d5364aea)
set(SSE2NEON_URI https://github.com/DLTcollab/sse2neon/archive/${SSE2NEON_VERSION}.tar.gz)
set(SSE2NEON_HASH 3427a495743bb6fd1b5f9f806b80f57d67b1ac7ccf39a5f44aedd487fd7e6da1)
set(SSE2NEON_HASH_TYPE SHA256)
set(SSE2NEON_FILE sse2neon-${SSE2NEON_VERSION}.tar.gz)
set(SSE2NEON_NAME sse2neon)
set(SSE2NEON_HOMEPAGE https://github.com/DLTcollab/sse2neon)
set(SSE2NEON_LICENSE SPDX:MIT)
set(SSE2NEON_COPYRIGHT "Copyright sse2neon contributors")

set(BROTLI_VERSION 1.0.9)
set(BROTLI_URI https://github.com/google/brotli/archive/refs/tags/v${BROTLI_VERSION}.tar.gz)
set(BROTLI_HASH f9e8d81d0405ba66d181529af42a3354f838c939095ff99930da6aa9cdf6fe46)
set(BROTLI_HASH_TYPE SHA256)
set(BROTLI_FILE brotli-v${BROTLI_VERSION}.tar.gz)
set(BROTLI_CPE "cpe:2.3:a:google:brotli:${BROTLI_VERSION}:*:*:*:*:*:*:*")
set(BROTLI_HOMEPAGE https://github.com/google/brotli)
set(BROTLI_LICENSE SPDX:MIT)
set(BROTLI_COPYRIGHT "Copyright (c) 2009, 2010, 2013-2016 by the Brotli Authors.")

set(OPENPGL_VERSION v0.6.0)
set(OPENPGL_SHORT_VERSION 0.6.0)
set(OPENPGL_URI https://github.com/OpenPathGuidingLibrary/openpgl/archive/refs/tags/${OPENPGL_VERSION}.tar.gz)
set(OPENPGL_HASH 4192a4096ee3e3d31878cd013f8de23418c8037c576537551f946c4811931c5e)
set(OPENPGL_HASH_TYPE SHA256)
set(OPENPGL_FILE openpgl-${OPENPGL_VERSION}.tar.gz)
set(OPENPGL_NAME Open PGL)
set(OPENPGL_HOMEPAGE http://www.openpgl.org/)
set(OPENPGL_LICENSE SPDX:Apache-2.0)
set(OPENPGL_COPYRIGHT "Copyright 2020 Intel Corporation.")

# Default version used by DPCPP: unified-runtime/cmake/FetchLevelZero.cmake
set(LEVEL_ZERO_VERSION 1.21.9)
set(LEVEL_ZERO_NAME "oneAPI Level Zero")
set(LEVEL_ZERO_URI https://codeload.github.com/oneapi-src/level-zero/tar.gz/refs/tags/v${LEVEL_ZERO_VERSION})
set(LEVEL_ZERO_HASH ba543a01adbcbd241518c3eee80b75414094d1fd3efcde9ff2693196cea4d057)
set(LEVEL_ZERO_HASH_TYPE SHA256)
set(LEVEL_ZERO_FILE level-zero-${LEVEL_ZERO_VERSION}.tar.gz)
set(LEVEL_ZERO_HOMEPAGE https://github.com/oneapi-src/level-zero)
set(LEVEL_ZERO_LICENSE SPDX:MIT)
set(LEVEL_ZERO_COPYRIGHT "Copyright (C) 2019-2024 Intel Corporation")

# Currently latest commit from sycl-rel-6_2 release branch
set(DPCPP_VERSION 05e047c0932d5043ddff5e4058a3afca8e0943aa)
set(DPCPP_URI https://github.com/intel/llvm/archive/${DPCPP_VERSION}.tar.gz)
set(DPCPP_HASH 94a51f89b212099a3d4c3530f2c2b6595b3b2a9ab40982cfab5055f92610b142)
set(DPCPP_HASH_TYPE SHA256)
set(DPCPP_FILE DPCPP-${DPCPP_VERSION}.tar.gz)
set(DPCPP_NAME DPC++)
set(DPCPP_HOMEPAGE "https://github.com/intel/llvm#oneapi-dpc-compiler")
set(DPCPP_LICENSE SPDX:Apache-2.0)
set(DPCPP_COPYRIGHT "Copyright (C) 2021-2025 Intel Corporation")

########################
### DPCPP DEPS BEGIN ###
########################
# The following deps are build time requirements for dpcpp, when possible
# the source in the dpcpp source tree for the version chosen is documented
# by each dep, these will only have to be downloaded and unpacked, dpcpp
# will take care of building them, unpack is being done in dpcpp_deps.cmake

# Source llvm/lib/SYCLLowerIR/CMakeLists.txt
set(VCINTRINSICS_VERSION 4e51b2467104a257c22788e343dafbdde72e28bb)
set(VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HASH c90fabfbcc8b3dc59ea0ed56ffd99075dca4d0cdf176e7ba41417c4f2011f117)
set(VCINTRINSICS_HASH_TYPE SHA256)
set(VCINTRINSICS_FILE vc-intrinsics-${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HOMEPAGE https://github.com/intel/vc-intrinsics)
set(VCINTRINSICS_LICENSE SPDX:MIT)
set(VCINTRINSICS_COPYRIGHT "Copyright (c) 2019-2024 Intel Corporation")

# Source opencl/CMakeLists.txt
set(OPENCLHEADERS_VERSION 542d7a8f65ecfd88b38de35d8b10aa67b36b33b2)
set(OPENCLHEADERS_URI https://github.com/KhronosGroup/OpenCL-Headers/archive/${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_HASH 026e2d39930fc9383f5472d45157254569ef712d11b644fbe4d317175250f409)
set(OPENCLHEADERS_HASH_TYPE SHA256)
set(OPENCLHEADERS_FILE opencl_headers-${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_NAME OpenCL-Headers)
set(OPENCLHEADERS_HOMEPAGE https://github.com/KhronosGroup/OpenCL-Headers)
set(OPENCLHEADERS_LICENSE SPDX:Apache-2.0)
set(OPENCLHEADERS_COPYRIGHT "Copyright (c) 2023 The Khronos Group Inc.")

# Source opencl/CMakeLists.txt
set(ICDLOADER_VERSION 804b6f040503c47148bee535230070da6b857ae4)
set(ICDLOADER_URI https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HASH 4a4b790fb67c0c37e63c9193c776b102b13f3fbb40e35ee383cfcd992552edfe)
set(ICDLOADER_HASH_TYPE SHA256)
set(ICDLOADER_FILE icdloader-${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HOMEPAGE https://github.com/KhronosGroup/OpenCL-ICD-Loader)
set(ICDLOADER_LICENSE SPDX:Apache-2.0)
set(ICDLOADER_COPYRIGHT " Copyright (c) 2020 The Khronos Group Inc.")

# Source sycl/cmake/modules/FetchEmhash.cmake
set(EMHASH_VERSION 3ba9abdfdc2e0430fcc2fd8993cad31945b6a02b)
set(EMHASH_URI https://github.com/ktprime/emhash/archive/${EMHASH_VERSION}.tar.gz)
set(EMHASH_HASH f0feaa687b5d288317526a6b0c331b51eba2e2b13528d79e015d75abef5d4dfa)
set(EMHASH_HASH_TYPE SHA256)
set(EMHASH_FILE emhash-${EMHASH_VERSION}.tar.gz)
set(EMHASH_HOMEPAGE https://github.com/ktprime/emhash)
set(EMHASH_LICENSE SPDX:MIT)
set(EMHASH_COPYRIGHT "Copyright (c) 2019 hyb")

# Source llvm-spirv/CMakeLists.txt (repo)
# Source llvm-spirv/spirv-headers-tag.conf (hash)
set(DPCPP_SPIRV_HEADERS_VERSION 2b2e05e088841c63c0b6fd4c9fb380d8688738d3)
set(DPCPP_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${DPCPP_SPIRV_HEADERS_VERSION}.tar.gz)
set(DPCPP_SPIRV_HEADERS_HASH 2e226ee953472e2e39724bf315433dce8cf119a397c451742dfda25bab7690af)
set(DPCPP_SPIRV_HEADERS_HASH_TYPE SHA256)
set(DPCPP_SPIRV_HEADERS_FILE DPCPP-SPIR-V-Headers-${DPCPP_SPIRV_HEADERS_VERSION}.tar.gz)
set(DPCPP_SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(DPCPP_SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(DPCPP_SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

# Source sycl/cmake/modules/FetchUnifiedRuntime.cmake (repo)
# Source sycl/cmake/modules/UnifiedRuntimeTag.cmake (commit/version)
set(UNIFIED_RUNTIME_VERSION d03f19a88e42cb98be9604ff24b61190d1e48727)
set(UNIFIED_RUNTIME_URI https://github.com/oneapi-src/unified-runtime/archive/${UNIFIED_RUNTIME_VERSION}.tar.gz)
set(UNIFIED_RUNTIME_HASH 761f57339bbaa104749426f7157f5f45fbc5ef3c9fce5b56bf495f34602c26e1)
set(UNIFIED_RUNTIME_HASH_TYPE SHA256)
set(UNIFIED_RUNTIME_FILE unified-runtime-${UNIFIED_RUNTIME_VERSION}.tar.gz)
set(UNIFIED_RUNTIME_HOMEPAGE https://github.com/oneapi-src/unified-runtime)
set(UNIFIED_RUNTIME_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(UNIFIED_RUNTIME_COPYRIGHT "Copyright (C) 2019-2025 Intel Corporation")

# Source unified-runtime/source/common/CMakeList.txt
set(UNIFIED_MEMORY_FRAMEWORK_VERSION v0.11.0)
set(UNIFIED_MEMORY_FRAMEWORK_URI https://github.com/oneapi-src/unified-memory-framework/archive/${UNIFIED_MEMORY_FRAMEWORK_VERSION}.tar.gz)
set(UNIFIED_MEMORY_FRAMEWORK_HASH d7a6cafe978d89628003e3001a1adef57cd733c9d9a6dfef0375c2123ec2e29c)
set(UNIFIED_MEMORY_FRAMEWORK_HASH_TYPE SHA256)
set(UNIFIED_MEMORY_FRAMEWORK_FILE unified-memory-framework-${UNIFIED_MEMORY_FRAMEWORK_VERSION}.tar.gz)
set(UNIFIED_MEMORY_FRAMEWORK_HOMEPAGE https://github.com/oneapi-src/unified-memory-framework)
set(UNIFIED_MEMORY_FRAMEWORK_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(UNIFIED_MEMORY_FRAMEWORK_COPYRIGHT "Copyright (C) 2023-2024 Intel Corporation")

# Source xptifw/src/CMakeList.txt
set(PARALLEL_HASHMAP_VERSION 8a889d3699b3c09ade435641fb034427f3fd12b6)
set(PARALLEL_HASHMAP_URI https://github.com/greg7mdp/parallel-hashmap/archive/${PARALLEL_HASHMAP_VERSION}.tar.gz)
set(PARALLEL_HASHMAP_HASH da853a4a2cee32b1563391a3661cff3cf48af5e76e320c004d5520835eb9e5f6)
set(PARALLEL_HASHMAP_HASH_TYPE SHA256)
set(PARALLEL_HASHMAP_FILE parallel-hashmap-${PARALLEL_HASHMAP_VERSION}.tar.gz)
set(PARALLEL_HASHMAP_HOMEPAGE https://github.com/greg7mdp/parallel-hashmap)
set(PARALLEL_HASHMAP_LICENSE SPDX:Apache-2.0)
set(PARALLEL_HASHMAP_COPYRIGHT "Copyright (c) 2019, Gregory Popovitch - greg7mdp@gmail.com")

######################
### DPCPP DEPS END ###
######################

##########################################
### Intel Graphics Compiler DEPS BEGIN ###
##########################################
# The following deps are build time requirements for the intel graphics
# compiler, the versions used are taken from the following location
# https://github.com/intel/intel-graphics-compiler/releases

set(IGC_VERSION 2.16.0)
set(IGC_URI https://github.com/intel/intel-graphics-compiler/archive/refs/tags/v${IGC_VERSION}.tar.gz)
set(IGC_HASH e8b74139d28e3c3be6acde94ac9e106af913dcaa495e2c687cdcb520f0a26538)
set(IGC_HASH_TYPE SHA256)
set(IGC_FILE intel-graphics-compiler-${IGC_VERSION}.tar.gz)
set(IGC_NAME IGC)
set(IGC_HOMEPAGE https://github.com/intel/intel-graphics-compiler)
set(IGC_LICENSE SPDX:MIT)
set(IGC_COPYRIGHT "Copyright (C) 2019-2024 Intel Corporation")

set(IGC_LLVM_VERSION llvmorg-15.0.7)
set(IGC_LLVM_URI https://github.com/llvm/llvm-project/archive/refs/tags/${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HASH 42a0088f148edcf6c770dfc780a7273014a9a89b66f357c761b4ca7c8dfa10ba)
set(IGC_LLVM_HASH_TYPE SHA256)
set(IGC_LLVM_FILE ${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HOMEPAGE https://github.com/llvm/llvm-project/)
set(IGC_LLVM_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(IGC_LLVM_COPYRIGHT "Copyright (c) 2003-2019 University of Illinois at Urbana-Champaign. All rights reserved.")

# ******* WARNING *******
#
# IGC_OPENCL_CLANG contains patches for some of its dependencies.
#
# Whenever IGC_OPENCL_CLANG_VERSION changes, one *MUST* inspect
# IGC_OPENCL_CLANG's patches folder and update igc.cmake to account for
# any added or removed patches.
#
# ******* WARNING *******

set(IGC_OPENCL_CLANG_VERSION v15.0.2)
set(IGC_OPENCL_CLANG_URI https://github.com/intel/opencl-clang/archive/${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HASH 82422ca51ab97e140f5bebb5833f5bbb10d87981f3f0f506223195647352ce47)
set(IGC_OPENCL_CLANG_HASH_TYPE SHA256)
set(IGC_OPENCL_CLANG_FILE opencl-clang-${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HOMEPAGE https://github.com/intel/opencl-clang/)
set(IGC_OPENCL_CLANG_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(IGC_OPENCL_CLANG_COPYRIGHT "Copyright (c) Intel Corporation (2009-2017).")

set(IGC_VCINTRINSICS_VERSION 0.23.1)
set(IGC_VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/refs/tags/v${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_HASH c312420727d6f4ca832c2f02b6a0712f7cd17b45261232e2696ec8eed2962be9)
set(IGC_VCINTRINSICS_HASH_TYPE SHA256)
set(IGC_VCINTRINSICS_FILE vc-intrinsics-${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_NAME "VC Intrinsics")
set(IGC_VCINTRINSICS_HOMEPAGE https://github.com/intel/vc-intrinsics)
set(IGC_VCINTRINSICS_LICENSE SPDX:MIT)
set(IGC_VCINTRINSICS_COPYRIGHT "Copyright (C) 2020-2021 Intel Corporation")

set(IGC_SPIRV_HEADERS_VERSION vulkan-sdk-1.4.313.0)
set(IGC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_HASH f68be549d74afb61600a1e3a7d1da1e6b7437758c8e77d664909f88f302c5ac1)
set(IGC_SPIRV_HEADERS_HASH_TYPE SHA256)
set(IGC_SPIRV_HEADERS_FILE SPIR-V-Headers-${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_NAME "SPIR-V Headers")
set(IGC_SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(IGC_SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(IGC_SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

set(IGC_SPIRV_TOOLS_VERSION v2025.1.rc1)
set(IGC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_HASH b04b1f00960664319321a58f513fd33eecca19a1460047bbdf3da8fd0c46d2f2)
set(IGC_SPIRV_TOOLS_HASH_TYPE SHA256)
set(IGC_SPIRV_TOOLS_FILE SPIR-V-Tools-${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_NAME "SPIR-V Tools")
set(IGC_SPIRV_TOOLS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Tools/)
set(IGC_SPIRV_TOOLS_LICENSE SPDX:Apache-2.0)
set(IGC_SPIRV_TOOLS_COPYRIGHT "Copyright (c) 2015-2016 The Khronos Group Inc.")

set(IGC_SPIRV_TRANSLATOR_VERSION v15.0.15)
set(IGC_SPIRV_TRANSLATOR_URI https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_HASH 52b8b8ddfd133647658dff6caaea6514fd827201984e365ad3552dd1651de321)
set(IGC_SPIRV_TRANSLATOR_HASH_TYPE SHA256)
set(IGC_SPIRV_TRANSLATOR_FILE SPIR-V-Translator-${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_NAME "LLVM/SPIR-V Bi-Directional Translator")
set(IGC_SPIRV_TRANSLATOR_HOMEPAGE https://github.com/KhronosGroup/SPIRV-LLVM-Translator)
set(IGC_SPIRV_TRANSLATOR_LICENSE SPDX:NCSA)
set(IGC_SPIRV_TRANSLATOR_COPYRIGHT [=[
Copyright (c) 2003-2014 University of Illinois at Urbana-Champaign.
All rights reserved.

Developed by:
LLVM Team
University of Illinois at Urbana-Champaign
http://llvm.org
]=])


########################################
### Intel Graphics Compiler DEPS END ###
########################################

set(GMMLIB_VERSION intel-gmmlib-22.8.1)
set(GMMLIB_URI https://github.com/intel/gmmlib/archive/refs/tags/${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HASH 9b8eac1891650021ded26b72585e7a2c702a3ba47565c968feabd14ab38d18f7)
set(GMMLIB_HASH_TYPE SHA256)
set(GMMLIB_NAME "Intel(R) Graphics Memory Management Library")
set(GMMLIB_FILE ${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HOMEPAGE https://github.com/intel/gmmlib)
set(GMMLIB_LICENSE SPDX:MIT)
set(GMMLIB_COPYRIGHT [=[
Copyright (c) 2017 Intel Corporation.
Copyright (c) 2016 Gabi Melman.
Copyright 2008, Google Inc. All rights reserved.
]=])

set(OCLOC_VERSION 25.31.34666.3)
set(OCLOC_URI https://github.com/intel/compute-runtime/archive/refs/tags/${OCLOC_VERSION}.tar.gz)
set(OCLOC_HASH c307ec9d0296bcfa0bc4f93a9f0955d9a8ca479a00a731fda5a61eee3ed76489)
set(OCLOC_HASH_TYPE SHA256)
set(OCLOC_FILE ocloc-${OCLOC_VERSION}.tar.gz)
set(OCLOC_HOMEPAGE https://github.com/intel/compute-runtime)
set(OCLOC_LICENSE SPDX:MIT)
set(OCLOC_COPYRIGHT "Copyright (C) 2018-2023 Intel Corporation")

set(AOM_VERSION 3.4.0)
set(AOM_URI https://storage.googleapis.com/aom-releases/libaom-${AOM_VERSION}.tar.gz)
set(AOM_HASH bd754b58c3fa69f3ffd29da77de591bd9c26970e3b18537951336d6c0252e354)
set(AOM_HASH_TYPE SHA256)
set(AOM_FILE libaom-${AOM_VERSION}.tar.gz)
set(AOM_HOMEPAGE https://aomedia.googlesource.com/aom/)
set(AOM_LICENSE SPDX:BSD-2-Clause)
set(AOM_COPYRIGHT "Copyright (c) 2016, Alliance for Open Media. All rights reserved.")

set(FRIBIDI_VERSION v1.0.12)
set(FRIBIDI_URI https://github.com/fribidi/fribidi/archive/refs/tags/${FRIBIDI_VERSION}.tar.gz)
set(FRIBIDI_HASH 2e9e859876571f03567ac91e5ed3b5308791f31cda083408c2b60fa1fe00a39d)
set(FRIBIDI_HASH_TYPE SHA256)
set(FRIBIDI_FILE fribidi-${FRIBIDI_VERSION}.tar.gz)
set(FRIBIDI_HOMEPAGE https://github.com/fribidi/fribidi)
set(FRIBIDI_LICENSE SPDX:LGPL-2.1-or-later)
set(FRIBIDI_COPYRIGHT [=[
Behdad Esfahbod <behdad@gnu.org>,
Dov Grobgeld <dov.grobgeld@gmail.com>,
Roozbeh Pournader <roozbeh@gnu.org>,
Khaled Hosny <khaledhosny@eglug.org>
]=])

set(HARFBUZZ_VERSION 10.0.1)
set(HARFBUZZ_URI https://github.com/harfbuzz/harfbuzz/archive/refs/tags/${HARFBUZZ_VERSION}.tar.gz)
set(HARFBUZZ_HASH e7358ea86fe10fb9261931af6f010d4358dac64f7074420ca9bc94aae2bdd542)
set(HARFBUZZ_HASH_TYPE SHA256)
set(HARFBUZZ_FILE harfbuzz-${HARFBUZZ_VERSION}.tar.gz)
set(HARFBUZZ_DEPSBUILDTIMEONLY "UI module asked for preliminary libs so they could work on integrating it")
set(HARFBUZZ_HOMEPAGE https://github.com/harfbuzz/harfbuzz)

set(SHADERC_VERSION v2025.3)
set(SHADERC_URI https://github.com/google/shaderc/archive/${SHADERC_VERSION}.tar.gz)
set(SHADERC_HASH c110401f6e36ee234008b4cabe991ee0)
set(SHADERC_HASH_TYPE MD5)
set(SHADERC_FILE shaderc-${SHADERC_VERSION}.tar.gz)
set(SHADERC_NAME ShaderC)
set(SHADERC_HOMEPAGE https://github.com/google/shaderc)
set(SHADERC_LICENSE SPDX:Apache-2.0)
set(SHADERC_COPYRIGHT "Copyright 2015 The Shaderc Authors. All rights reserved.")

# The versions of shaderc's dependencies can be found in the root of shaderc's
# source in a file called DEPS.

set(SHADERC_SPIRV_TOOLS_VERSION 33e02568181e3312f49a3cf33df470bf96ef293a)
set(SHADERC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)
set(SHADERC_SPIRV_TOOLS_HASH 7b83eee717a6c4d7e4f8d7e96a32a298)
set(SHADERC_SPIRV_TOOLS_HASH_TYPE MD5)
set(SHADERC_SPIRV_TOOLS_FILE SPIRV-Tools-${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)
set(SHADERC_SPIRV_TOOLS_NAME SPIR-V Tools)
set(SHADERC_SPIRV_TOOLS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Tools/)
set(SHADERC_SPIRV_TOOLS_LICENSE SPDX:Apache-2.0)
set(SHADERC_SPIRV_TOOLS_COPYRIGHT "Copyright (c) 2015-2016 The Khronos Group Inc.")

set(SHADERC_SPIRV_HEADERS_VERSION 2a611a970fdbc41ac2e3e328802aed9985352dca)
set(SHADERC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)
set(SHADERC_SPIRV_HEADERS_HASH 21c42d628b4262ad9defc021e24e2abe)
set(SHADERC_SPIRV_HEADERS_HASH_TYPE MD5)
set(SHADERC_SPIRV_HEADERS_FILE SPIRV-Headers-${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)
set(SHADERC_SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(SHADERC_SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(SHADERC_SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

set(SHADERC_GLSLANG_VERSION efd24d75bcbc55620e759f6bf42c45a32abac5f8)
set(SHADERC_GLSLANG_URI https://github.com/KhronosGroup/glslang/archive/${SHADERC_GLSLANG_VERSION}.tar.gz)
set(SHADERC_GLSLANG_HASH c0ff01b5791dbc6c77e0eb295ce2ffc6)
set(SHADERC_GLSLANG_HASH_TYPE MD5)
set(SHADERC_GLSLANG_FILE glslang-${SHADERC_GLSLANG_VERSION}.tar.gz)
set(SHADERC_GLSLANG_HOMEPAGE https://github.com/KhronosGroup/glslang)
set(SHADERC_GLSLANG_LICENSE SPDX:Apache-2.0)
set(SHADERC_GLSLANG_COPYRIGHT [=[
Copyright 2020 The Khronos Group Inc.
Copyright (C) 2015-2018 Google, Inc.
]=])

set(VULKAN_VERSION 1.3.296)

set(VULKAN_HEADERS_VERSION ${VULKAN_VERSION})
set(VULKAN_HEADERS_NAME Vulkan-Headers)
set(VULKAN_HEADERS_URI https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v${VULKAN_HEADERS_VERSION}.tar.gz)
set(VULKAN_HEADERS_HASH 3f4cd229d8f7c24c9b5a6ead3d674803)
set(VULKAN_HEADERS_HASH_TYPE MD5)
set(VULKAN_HEADERS_FILE Vulkan-Headers-${VULKAN_HEADERS_VERSION}.tar.gz)
set(VULKAN_HEADERS_HOMEPAGE https://github.com/KhronosGroup/Vulkan-Headers)
set(VULKAN_HEADERS_LICENSE SPDX:Apache-2.0)
set(VULKAN_HEADERS_COPYRIGHT "Copyright 2015-2023 The Khronos Group Inc.")

set(VULKAN_LOADER_VERSION ${VULKAN_VERSION})
set(VULKAN_LOADER_NAME Vulkan-Loader)
set(VULKAN_LOADER_URI https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/v${VULKAN_LOADER_VERSION}.tar.gz)
set(VULKAN_LOADER_HASH bca05ec4914888990c92fa9c3e2738ff)
set(VULKAN_LOADER_HASH_TYPE MD5)
set(VULKAN_LOADER_FILE Vulkan-Loader-${VULKAN_LOADER_VERSION}.tar.gz)
set(VULKAN_LOADER_HOMEPAGE https://github.com/KhronosGroup/Vulkan-Loader)
set(VULKAN_LOADER_LICENSE SPDX:Apache-2.0)
set(VULKAN_LOADER_COPYRIGHT [=[
Copyright (c) 2019 The Khronos Group Inc.
Copyright (c) 2019 Valve Corporation.
Copyright (c) 2019 LunarG, Inc.
Copyright (c) 2019 Google Inc.
]=])

set(VULKAN_UTILITY_LIBRARIES_VERSION ${VULKAN_VERSION})
set(VULKAN_UTILITY_LIBRARIES_URI https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/v${VULKAN_UTILITY_LIBRARIES_VERSION}.tar.gz)
set(VULKAN_UTILITY_LIBRARIES_HASH 02091bdc2b5862685da83b37a6c48821)
set(VULKAN_UTILITY_LIBRARIES_HASH_TYPE MD5)
set(VULKAN_UTILITY_LIBRARIES_FILE Vulkan-Utility-Libraries-${VULKAN_UTILITY_LIBRARIES_VERSION}.tar.gz)
set(VULKAN_UTILITY_LIBRARIES_HOMEPAGE https://github.com/KhronosGroup/Vulkan-Utility-Libraries)
set(VULKAN_UTILITY_LIBRARIES_LICENSE SPDX:Apache-2.0)
set(VULKAN_UTILITY_LIBRARIES_COPYRIGHT "Copyright 2015-2025 The Khronos Group Inc.")

set(VULKAN_MEMORY_ALLOCATOR_VERSION 3.2.1)
set(VULKAN_MEMORY_ALLOCATOR_NAME Vulkan-Memory-Allocator)
set(VULKAN_MEMORY_ALLOCATOR_URI
https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v${VULKAN_MEMORY_ALLOCATOR_VERSION}.tar.gz)
set(VULKAN_MEMORY_ALLOCATOR_HASH f32b8374858566854e5f77564ea2e16d)
set(VULKAN_MEMORY_ALLOCATOR_HASH_TYPE MD5)
set(VULKAN_MEMORY_ALLOCATOR_FILE Vulkan-Memory-Allocator-${VULKAN_MEMORY_ALLOCATOR_VERSION}.tar.gz)
set(VULKAN_MEMORY_ALLOCATOR_HOMEPAGE https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
set(VULKAN_MEMORY_ALLOCATOR_LICENSE SPDX:MIT)
set(VULKAN_MEMORY_ALLOCATOR_COPYRIGHT "Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All rights reserved.")

set(SPIRV_HEADERS_VERSION ${VULKAN_VERSION})
set(SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/vulkan-sdk-${SPIRV_HEADERS_VERSION}.0.tar.gz)
set(SPIRV_HEADERS_HASH adee3d432b63f345c8ee6d625d0c8e6c)
set(SPIRV_HEADERS_HASH_TYPE MD5)
set(SPIRV_HEADERS_FILE SPIRV-Headers-${SPIRV_HEADERS_VERSION}.tar.gz)
set(SPIRV_HEADERS_NAME SPIR-V Headers)
set(SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers/)
set(SPIRV_HEADERS_LICENSE SPDX:Apache-2.0)
set(SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2014 The Khronos Group Inc.")

set(SPIRV_REFLECT_VERSION 1.4.309.0)
set(SPIRV_REFLECT_NAME SPIRV-Reflect)
set(SPIRV_REFLECT_URI
https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-${SPIRV_REFLECT_VERSION}.tar.gz)
set(SPIRV_REFLECT_HASH 11234b7d7895f69c3955e458c7b98914)
set(SPIRV_REFLECT_HASH_TYPE MD5)
set(SPIRV_REFLECT_FILE SPIRV-Reflect-${SPIRV_REFLECT_VERSION}.tar.gz)
set(SPIRV_REFLECT_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Reflect)
set(SPIRV_REFLECT_LICENSE SPDX:Apache-2.0)
set(SPIRV_REFLECT_COPYRIGHT "Copyright 2017-2018 Google Inc.")

set(PYBIND11_VERSION 2.10.1)
set(PYBIND11_URI https://github.com/pybind/pybind11/archive/refs/tags/v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HASH ce07bfd5089245da7807b3faf6cbc878)
set(PYBIND11_HASH_TYPE MD5)
set(PYBIND11_FILE pybind-v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HOMEPAGE https://github.com/pybind/pybind11)
set(PYBIND11_LICENSE SPDX:BSD-2-Clause)
set(PYBIND11_COPYRIGHT "Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>, All rights reserved.")

set(HIPRT_VERSION c4298933fe046e3f915977b5a69537a59aac8f47)
set(HIPRT_LIBRARY_VERSION 02005)
set(HIPRT_URI https://github.com/GPUOpen-LibrariesAndSDKs/HIPRT/archive/${HIPRT_VERSION}.tar.gz)
set(HIPRT_HASH 65b8a975d23db04d908c758f3fb7bae3)
set(HIPRT_HASH_TYPE MD5)
set(HIPRT_FILE hiprt-${HIPRT_VERSION}.tar.gz)
set(HIPRT_HOMEPAGE https://github.com/GPUOpen-LibrariesAndSDKs/HIPRT)
set(HIPRT_LICENSE SPDX:MIT)
set(HIPRT_COPYRIGHT "Copyright (C) 2024 Advanced Micro Devices, Inc. All Rights Reserved. ")
