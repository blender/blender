# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Compilers
#
# Version used for precompiled library builds used for official releases.
# For anyone making their own library build, matching these exactly is not
# needed but it can be a useful reference.

set(RELEASE_GCC_VERSION 11.2.*)
set(RELEASE_CUDA_VERSION 12.3.*)
set(RELEASE_HIP_VERSION 5.7.*)

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


set(ZLIB_VERSION 1.2.13)
set(ZLIB_URI https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 9b8aa094c4e5765dabf4da391f00d15c)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_CPE "cpe:2.3:a:zlib:zlib:${ZLIB_VERSION}:*:*:*:*:*:*:*")
set(ZLIB_HOMEPAGE https://zlib.net)
set(ZLIB_LICENSE SPDX:Zlib)
set(ZLIB_COPYRIGHT "Copyright (C) 1995-2017 Jean-loup Gailly")

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

set(PNG_VERSION 1.6.43)
set(PNG_NAME libpng)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c)
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

set(BOOST_VERSION 1.82.0)
set(BOOST_VERSION_SHORT 1.82)
set(BOOST_VERSION_NODOTS 1_82_0)
set(BOOST_VERSION_NODOTS_SHORT 1_82)
set(BOOST_URI https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_HASH f7050f554a65f6a42ece221eaeec1660)
set(BOOST_HASH_TYPE MD5)
set(BOOST_FILE boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_CPE "cpe:2.3:a:boost:boost:${BOOST_VERSION}:*:*:*:*:*:*:*")
set(BOOST_HOMEPAGE https://www.boost.org/)
set(BOOST_NAME Boost)
set(BOOST_LICENSE SPDX:BSL-1.0)
set(BOOST_COPYRIGHT "The Boost license encourages both commercial and non-commercial use and does not require attribution for binary use.")

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
Copyright 1998 John E. Bossom.
Copyright 1999-2018, Pthreads4w contributors.
]=])

set(DEFLATE_VERSION 1.18)
set(DEFLATE_URI https://github.com/ebiggers/libdeflate/archive/refs/tags/v${DEFLATE_VERSION}.tar.gz)
set(DEFLATE_HASH a29d9dd653cbe03f2d5cd83972063f9e)
set(DEFLATE_HASH_TYPE MD5)
set(DEFLATE_FILE libdeflate-v${DEFLATE_VERSION}.tar.gz)
set(DEFLATE_HOMEPAGE https://github.com/ebiggers/libdeflate)
set(DEFLATE_LICENSE SPDX:MIT)
set(DEFLATE_COPYRIGHT "Copyright 2016 Eric Biggers")

set(OPENEXR_VERSION 3.2.4)
set(OPENEXR_URI https://github.com/AcademySoftwareFoundation/openexr/archive/v${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH 83b23b937b3a76fd37680422f41b81b7)
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

set(FREETYPE_VERSION 2.13.0)
set(FREETYPE_URI http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH 98bc3cf234fe88ef3cf24569251fe0a4)
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

set(OPENCOLLADA_VERSION dfc341ab0b3b23ee307ab8660c0213e64da1eac6)
set(OPENCOLLADA_URI https://github.com/aras-p/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH 2120c8c02aab840e81cb87e625a608f7)
set(OPENCOLLADA_HASH_TYPE MD5)
set(OPENCOLLADA_FILE opencollada-${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_LICENSE SPDX:MIT)
set(OPENCOLLADA_COPYRIGHT "Copyright (c) 2008-2009 NetAllied Systems GmbH")

set(OPENCOLORIO_VERSION 2.3.2)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH 8af74fcb8c4820ab21204463a06ba490)
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

set(LLVM_VERSION 17.0.6)
set(LLVM_NAME LLVM)
set(LLVM_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH 62a09d65240a5133f001ace48269dbfc)
set(LLVM_HASH_TYPE MD5)
set(LLVM_FILE llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_CPE "cpe:2.3:a:llvm:compiler:${LLVM_VERSION}:*:*:*:*:*:*:*")
set(LLVM_HOMEPAGE https://github.com/llvm/llvm-project/)
set(LLVM_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(LLVM_COPYRIGHT "Copyright (c) 2003-2019 University of Illinois at Urbana-Champaign. All rights reserved.")

if(APPLE)
  # Cloth physics test is crashing due to this bug:
  # https://bugs.llvm.org/show_bug.cgi?id=50579
  set(OPENMP_VERSION 9.0.1)
  set(OPENMP_HASH 6eade16057edbdecb3c4eef9daa2bfcf)
else()
  set(OPENMP_VERSION ${LLVM_VERSION})
  set(OPENMP_HASH 5cc01d151821c546bb4ec6fb03d86c29)
endif()
set(OPENMP_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${OPENMP_VERSION}/openmp-${OPENMP_VERSION}.src.tar.xz)
set(OPENMP_HASH_TYPE MD5)
set(OPENMP_FILE openmp-${OPENMP_VERSION}.src.tar.xz)

set(OPENIMAGEIO_VERSION v2.5.11.0)
set(OPENIMAGEIO_NAME OpenImageIO)
set(OPENIMAGEIO_URI https://github.com/AcademySoftwareFoundation/OpenImageIO/archive/refs/tags/${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH 691e9364d25e2878e042d48980fad593)
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
set(ROBINMAP_VERSION v0.6.2)
set(ROBINMAP_URI https://github.com/Tessil/robin-map/archive/refs/tags/${ROBINMAP_VERSION}.tar.gz)
set(ROBINMAP_HASH c08ec4b1bf1c85eb0d6432244a6a89862229da1cb834f3f90fba8dc35d8c8ef1)
set(ROBINMAP_HASH_TYPE SHA256)
set(ROBINMAP_FILE robinmap-${ROBINMAP_VERSION}.tar.gz)
set(ROBINMAP_HOMEPAGE https://github.com/Tessil/robin-map)
set(ROBINMAP_LICENSE SPDX:MIT)
set(ROBINMAP_COPYRIGHT "Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>")

set(TIFF_VERSION 4.6.0)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH fc7d49a9348b890b29f91a4ecadd5b49)
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

# Recent commit from 1.13.5.0 under development, which includes string table
# changes that make the Cycles OptiX implementation work. Official 1.12 OSL
# releases should also build but without OptiX support.
set(OSL_VERSION 1.13.7.0)
set(OSL_NAME "Open Shading Language")
set(OSL_URI https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/refs/tags/v${OSL_VERSION}.tar.gz)
set(OSL_HASH 769ae444a7df0e6561b3e745fd2eb50d)
set(OSL_HASH_TYPE MD5)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)
set(OSL_HOMEPAGE https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/)
set(OSL_LICENSE SPDX:BSD-3-Clause)
set(OSL_COPYRIGHT "Copyright Contributors to the Open Shading Language project.")

# NOTE: When updating the python version, it's required to check the versions of
# it wants to use in PCbuild/get_externals.bat for the following dependencies:
# BZIP2, FFI, SQLITE and change the versions in this file as well. For compliance
# reasons there can be no exceptions to this.
# Additionally, keep the PYTHON_PIP_VERSION in sync with the pip version bundled
# into Python.

set(PYTHON_VERSION 3.11.9)
set(PYTHON_SHORT_VERSION 3.11)
set(PYTHON_SHORT_VERSION_NO_DOTS 311)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH 22ea467e7d915477152e99d5da856ddc)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_CPE "cpe:2.3:a:python:python:${PYTHON_VERSION}:-:*:*:*:*:*:*")
set(PYTHON_HOMEPAGE https://www.python.org/)
set(PYTHON_NAME Python)
set(PYTHON_LICENSE SPDX:Python-2.0)
set(PYTHON_COPYRIGHT "Copyright (c) 2001-2021 Python Software Foundation. All rights reserved.")

# Python bundles pip wheel, and does not track CVEs from it. Add an explicit CPE
# identifier for pip, so that cve_check can detect vulnerabilities in it.
# The version needs to be kept in symc with the version bundled in Python.
# Currently it is done manually by tracking _PIP_VERSION variable in the
# `Lib/ensurepip/__init__.py`. For example,
#   https://github.com/python/cpython/tree/v3.11.9/Lib/ensurepip/__init__.py
set(PYTHON_PIP_VERSION 24.0)
set(PYTHON_PIP_CPE "cpe:2.3:a:pypa:pip:${PYTHON_PIP_VERSION}:*:*:*:*:*:*:*")

set(TBB_YEAR 2020)
set(TBB_NAME oneTBB)
set(TBB_VERSION ${TBB_YEAR}_U3)
set(TBB_URI https://github.com/oneapi-src/oneTBB/archive/${TBB_VERSION}.tar.gz)
set(TBB_HASH 55ec8df6eae5ed6364a47f0e671e460c)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)
set(TBB_CPE "cpe:2.3:a:intel:threading_building_blocks:${TBB_YEAR}:*:*:*:*:*:*:*")
set(TBB_HOMEPAGE https://software.intel.com/en-us/oneapi/onetbb)
set(TBB_LICENSE SPDX:Apache-2.0)
set(TBB_COPYRIGHT "Copyright (c) 2005-2020 Intel Corporation")

set(OPENVDB_VERSION 11.0.0)
set(OPENVDB_NAME OpenVDB)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 025f4fc4db58419341a4991f1a16174a)
set(OPENVDB_HASH_TYPE MD5)
set(OPENVDB_FILE openvdb-${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HOMEPAGE http://www.openvdb.org/)
set(OPENVDB_LICENSE SPDX:MPL-2.0)
set(OPENVDB_COPYRIGHT "Copyright Contributors to the OpenVDB Project")

# ------------------------------------------------------------------------------
# Python Modules

# Needed by: `requests` module (so the version doesn't change on rebuild).
set(IDNA_VERSION 3.3)
# Needed by: `requests` module (so the version doesn't change on rebuild).
set(CHARSET_NORMALIZER_VERSION 2.0.10)
# Needed by: `requests` module (so the version doesn't change on rebuild).
set(URLLIB3_VERSION 1.26.8)
set(URLLIB3_CPE "cpe:2.3:a:urllib3:urllib3:${URLLIB3_VERSION}:*:*:*:*:*:*:*")
# Needed by: Python's `requests` module (so add-ons can authenticate against trusted certificates).
set(CERTIFI_VERSION 2021.10.8)
# Needed by: Some of Blender's add-ons (to support convenient interaction with online services).
set(REQUESTS_VERSION 2.27.1)
# Needed by: Python's `numpy` module (used by some add-ons).
set(CYTHON_VERSION 0.29.30)
# Needed by: Python scripts that read `.blend` files, as files may use Z-standard compression.
# The version of the ZSTD library used to build the Python package should match ZSTD_VERSION
# defined below. At this time of writing, 0.17.0 was already released,
# but built against ZSTD 1.5.1, while we use 1.5.0.
set(ZSTANDARD_VERSION 0.16.0)
# Auto-format Python source (developer tool, not used by Blender at run-time).
set(AUTOPEP8_VERSION 2.3.1)
# Needed by: `autopep8` (so the version doesn't change on rebuild).
set(PYCODESTYLE_VERSION 2.12.1)
# Build system for other packages (not used by Blender at run-time).
set(MESON_VERSION 0.63.0)

set(NUMPY_VERSION 1.24.3)
set(NUMPY_SHORT_VERSION 1.24)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_HASH 89e5e2e78407032290ae6acf6dcaea46)
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

set(VPX_VERSION 1.14.0)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 5f21d2db27071c8a46f1725928a10227ae45c5cd1cad3727e4aafbe476e321fa)
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

set(OPENJPEG_VERSION 2.5.0)
set(OPENJPEG_SHORT_VERSION 2.5)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 0333806d6adecc6f7a91243b2b839ff4d2053823634d4f6ed7a59bc87409122a)
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

set(FFMPEG_VERSION 6.1.1)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH 5e3133939a61ef64ac9b47ffd29a5ea6e337a4023ef0ad972094b4da844e3a20)
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

set(XML2_VERSION 2.12.3)
set(XML2_URI https://download.gnome.org/sources/libxml2/2.12/libxml2-${XML2_VERSION}.tar.xz)
set(XML2_HASH 13871e7cf2137b4b9b9da753ffef538c)
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

set(EXPAT_VERSION 2_5_0)
set(EXPAT_VERSION_DOTS 2.5.0)
set(EXPAT_URI https://github.com/libexpat/libexpat/archive/R_${EXPAT_VERSION}.tar.gz)
set(EXPAT_HASH d375fa3571c0abb945873f5061a8f2e2)
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
set(SSL_VERSION 3.1.5)
set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
set(SSL_HASH 6ae015467dabf0469b139ada93319327be24b98251ffaeceda0221848dc09262)
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

# Note: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat for compliance reasons there
# can be no exceptions to this.
set(SQLITE_VERSION 3.45.1)
set(SQLLITE_LONG_VERSION 3450100)
set(SQLITE_URI https://www.sqlite.org/2024/sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_HASH 650305e234add12fc1e6bef0b365d86a087b3d38)
set(SQLITE_HASH_TYPE SHA1)
set(SQLITE_FILE sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_CPE "cpe:2.3:a:sqlite:sqlite:${SQLITE_VERSION}:*:*:*:*:*:*:*")
set(SQLITE_HOMEPAGE https://www.sqlite.org)
set(SQLITE_LICENSE Public Domain)

set(EMBREE_VERSION 4.3.2-blender)
set(EMBREE_URI https://github.com/embree/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 91bd65e59c6cf4d9ff0e4d628aa28d6a)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)
set(EMBREE_HOMEPAGE https://github.com/embree/embree)
set(EMBREE_LICENSE SPDX:Apache-2.0)
set(EMBREE_COPYRIGHT "Copyright 2009-2020 Intel Corporation")

set(USD_VERSION 24.05)
set(USD_NAME USD)
set(USD_URI https://github.com/PixarAnimationStudios/OpenUSD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH 44a5b976a76588b485a652f08a55e91f)
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

set(MATERIALX_VERSION 1.38.8)
set(MATERIALX_NAME MaterialX)
set(MATERIALX_URI https://github.com/AcademySoftwareFoundation/MaterialX/archive/refs/tags/v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HASH fad8f4e19305fb2ee920cbff638f3560)
set(MATERIALX_HASH_TYPE MD5)
set(MATERIALX_FILE materialx-v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HOMEPAGE https://github.com/AcademySoftwareFoundation/MaterialX)
set(MATERIALX_LICENSE SPDX:Apache-2.0)
set(MATERIALX_COPYRIGHT "Copyright Contributors to the MaterialX Project")

set(OIDN_VERSION 2.3.0)
set(OIDN_NAME OpenImageDenoise)
set(OIDN_URI https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HASH 31a3d8b9168966a2fa93daa6becad586)
set(OIDN_HASH_TYPE MD5)
set(OIDN_FILE oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HOMEPAGE https://www.openimagedenoise.org/)
set(OIDN_LICENSE SPDX:Apache-2.0)
set(OIDN_COPYRIGHT "Copyright 2009-2020 Intel Corporation")

set(LIBGLU_VERSION 9.0.1)
set(LIBGLU_URI ftp://ftp.freedesktop.org/pub/mesa/glu/glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HASH 151aef599b8259efe9acd599c96ea2a3)
set(LIBGLU_HASH_TYPE MD5)
set(LIBGLU_FILE glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HOMEPAGE https://gitlab.freedesktop.org/mesa/glu)
set(LIBGLU_LICENSE SPDX:SGI-B-2.0)
set(LIBGLU_COPYRIGHT "Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.")

set(MESA_VERSION 23.3.0)
set(MESA_URI ftp://ftp.freedesktop.org/pub/mesa/mesa-${MESA_VERSION}.tar.xz)
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

set(WL_PROTOCOLS_VERSION 1.36)
set(WL_PROTOCOLS_NAME Wayland-Protocols)
set(WL_PROTOCOLS_FILE wayland-protocols-${WL_PROTOCOLS_VERSION}.tar.xz)
set(WL_PROTOCOLS_URI https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/${WL_PROTOCOLS_VERSION}/downloads/${WL_PROTOCOLS_FILE})
set(WL_PROTOCOLS_HASH d733380202a75ca837744e65b4dbadc5)
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

set(WAYLAND_VERSION 1.23.0)
set(WAYLAND_FILE wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_URI https://gitlab.freedesktop.org/wayland/wayland/-/releases/${WAYLAND_VERSION}/downloads/wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_HASH 23ad991e776ec8cf7e58b34cbd2efa75)
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

set(WAYLAND_WESTON_VERSION 13.0.3)
set(WAYLAND_WESTON_FILE weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_URI https://gitlab.freedesktop.org/wayland/weston/-/releases/${WAYLAND_WESTON_VERSION}/downloads/weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_HASH 9e10833f807214b4b060a1a8db1e3057)
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

set(ISPC_VERSION v1.21.1)
set(ISPC_URI https://github.com/ispc/ispc/archive/${ISPC_VERSION}.tar.gz)
set(ISPC_HASH edd16b016aabc07819d14fd86a1fb5d0)
set(ISPC_HASH_TYPE MD5)
set(ISPC_FILE ispc-${ISPC_VERSION}.tar.gz)
set(ISPC_DEPSBUILDTIMEONLY "Blender ships the produced artifact, but doesn't ship/link with any binary")

set(GMP_VERSION 6.2.1)
set(GMP_URI https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.xz)
set(GMP_HASH 0b82665c4a92fd2ade7440c13fcaa42b)
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


set(HARU_VERSION 2_3_0)
set(HARU_URI https://github.com/libharu/libharu/archive/RELEASE_${HARU_VERSION}.tar.gz)
set(HARU_HASH 4f916aa49c3069b3a10850013c507460)
set(HARU_HASH_TYPE MD5)
set(HARU_FILE libharu-${HARU_VERSION}.tar.gz)
set(HARU_HOMEPAGE http://libharu.org/)
set(HARU_LICENSE SPDX:Zlib)
set(HARU_COPYRIGHT [=[
Copyright (C) 1999-2006 Takeshi Kanno.
Copyright (C) 2007-2009 Antony Dovgal.
]=])

set(ZSTD_VERSION 1.5.0)
set(ZSTD_URI https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_HASH 5194fbfa781fcf45b98c5e849651aa7b3b0a008c6b72d4a0db760f3002291e94)
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

set(LEVEL_ZERO_VERSION 1.16.1)
set(LEVEL_ZERO_NAME "oneAPI Level Zero")
set(LEVEL_ZERO_URI https://codeload.github.com/oneapi-src/level-zero/tar.gz/refs/tags/v${LEVEL_ZERO_VERSION})
set(LEVEL_ZERO_HASH f341dd6355d8da6ee9c29031642b8e8e4259f91c13c72d318c81663af048817e)
set(LEVEL_ZERO_HASH_TYPE SHA256)
set(LEVEL_ZERO_FILE level-zero-${LEVEL_ZERO_VERSION}.tar.gz)
set(LEVEL_ZERO_HOMEPAGE https://github.com/oneapi-src/level-zero)
set(LEVEL_ZERO_LICENSE SPDX:MIT)
set(LEVEL_ZERO_COPYRIGHT "Copyright (C) 2019-2021 Intel Corporation")

set(DPCPP_VERSION d2817d6d317db1143bb227168e85c409d5ab7c82) # tip of sycl-rel_5_2_0 as of 2024.05.24
set(DPCPP_URI https://github.com/intel/llvm/archive/${DPCPP_VERSION}.tar.gz)
set(DPCPP_HASH 86cbff157b79e29a6ebb96ba79c96f64b4296c33fcd896f60a5579955fca5724)
set(DPCPP_HASH_TYPE SHA256)
set(DPCPP_FILE DPCPP-${DPCPP_VERSION}.tar.gz)
set(DPCPP_NAME DPC++)
set(DPCPP_HOMEPAGE "https://github.com/intel/llvm#oneapi-dpc-compiler")
set(DPCPP_LICENSE SPDX:Apache-2.0)
set(DPCPP_COPYRIGHT "Copyright (C) 2021 Intel Corporation")

########################
### DPCPP DEPS BEGIN ###
########################
# The following deps are build time requirements for dpcpp, when possible
# the source in the dpcpp source tree for the version chosen is documented
# by each dep, these will only have to be downloaded and unpacked, dpcpp
# will take care of building them, unpack is being done in dpcpp_deps.cmake

# Source llvm/lib/SYCLLowerIR/CMakeLists.txt
set(VCINTRINSICS_VERSION da892e1982b6c25b9a133f85b4ac97142d8a3def)
set(VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HASH 06b85bd988059939770eb6e6e6194562d17c5f5a5df9947af18696b3b1fe92f3)
set(VCINTRINSICS_HASH_TYPE SHA256)
set(VCINTRINSICS_FILE vc-intrinsics-${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HOMEPAGE https://github.com/intel/vc-intrinsics)
set(VCINTRINSICS_LICENSE SPDX:MIT)
set(VCINTRINSICS_COPYRIGHT "Copyright (c) 2019 Intel Corporation")

# Source opencl/CMakeLists.txt
set(OPENCLHEADERS_VERSION 9ddb236e6eb3cf844f9e2f81677e1045f9bf838e)
set(OPENCLHEADERS_URI https://github.com/KhronosGroup/OpenCL-Headers/archive/${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_HASH 9db682a1b0037ef54c50ba8fa3fa73182e552fc1ad0119a771bebf68e43ea739)
set(OPENCLHEADERS_HASH_TYPE SHA256)
set(OPENCLHEADERS_FILE opencl_headers-${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_NAME OpenCL-Headers)
set(OPENCLHEADERS_HOMEPAGE https://github.com/KhronosGroup/OpenCL-Headers)
set(OPENCLHEADERS_LICENSE SPDX:Apache-2.0)
set(OPENCLHEADERS_COPYRIGHT "Copyright (c) 2023 The Khronos Group Inc.")

# Source opencl/CMakeLists.txt
set(ICDLOADER_VERSION 9a3e962f16f5097d2054233ad8b6dad51b6f41b7)
set(ICDLOADER_URI https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HASH 5e2979be5692caf11a4afc2fd7995a54c94a03d0f7ee2959f03f98f7689b677b)
set(ICDLOADER_HASH_TYPE SHA256)
set(ICDLOADER_FILE icdloader-${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HOMEPAGE https://github.com/KhronosGroup/OpenCL-ICD-Loader)
set(ICDLOADER_LICENSE SPDX:Apache-2.0)
set(ICDLOADER_COPYRIGHT " Copyright (c) 2020 The Khronos Group Inc.")

# Source sycl/cmake/modules/AddBoostMp11Headers.cmake
# Using external MP11 here, getting AddBoostMp11Headers.cmake to recognize
# our copy in boost directly was more trouble than it was worth.
set(MP11_VERSION ef7608b463298b881bc82eae4f45a4385ed74fca)
set(MP11_URI https://github.com/boostorg/mp11/archive/${MP11_VERSION}.tar.gz)
set(MP11_HASH ec2d68858dd4d04f9a1e3960fc94a58440715e1b3e746cc495438116715343e2)
set(MP11_HASH_TYPE SHA256)
set(MP11_FILE mp11-${MP11_VERSION}.tar.gz)
set(MP11_HOMEPAGE https://github.com/boostorg/mp11)
set(MP11_LICENSE SPDX:BSL-1.0)

# Source llvm-spirv/CMakeLists.txt (repo)
# Source llvm-spirv/spirv-headers-tag.conf (hash)
set(SPIRV_HEADERS_VERSION b73e168ca5e123dcf3dea8a34b19a5130f421ae1)
set(SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SPIRV_HEADERS_VERSION}.tar.gz)
set(SPIRV_HEADERS_HASH 11d835c60297b26532c05c3f3b581ba7a2787b5ae7399e94f72c392169216f11)
set(SPIRV_HEADERS_HASH_TYPE SHA256)
set(SPIRV_HEADERS_FILE SPIR-V-Headers-${SPIRV_HEADERS_VERSION}.tar.gz)
set(SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

# Source sycl/plugins/unified_runtime/CMakeLists.txt
set(UNIFIED_RUNTIME_VERSION ec634ff05b067d7922ec45059dda94665e5dcd9b)
set(UNIFIED_RUNTIME_URI https://github.com/oneapi-src/unified-runtime/archive/${UNIFIED_RUNTIME_VERSION}.tar.gz)
set(UNIFIED_RUNTIME_HASH ff15574aba6225d0c8a32f71866126551dee1aaacfa7894b8fdcc5e52e0f5da9)
set(UNIFIED_RUNTIME_HASH_TYPE SHA256)
set(UNIFIED_RUNTIME_FILE unified-runtime-${UNIFIED_RUNTIME_VERSION}.tar.gz)
set(UNIFIED_RUNTIME_HOMEPAGE https://github.com/oneapi-src/unified-runtime)
set(UNIFIED_RUNTIME_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(UNIFIED_RUNTIME_COPYRIGHT "Copyright (C) 2019-2024 Intel Corporation")

# Source unified-runtime/source/common/CMakeList.txt
set(UNIFIED_MEMORY_FRAMEWORK_VERSION 9bf7a0dc4dff76844e10edbb5c6e9d917536ef6d)
set(UNIFIED_MEMORY_FRAMEWORK_URI https://github.com/oneapi-src/unified-memory-framework/archive/${UNIFIED_MEMORY_FRAMEWORK_VERSION}.tar.gz)
set(UNIFIED_MEMORY_FRAMEWORK_HASH 7ff7d0be7be6e59693d238eab02b5a9741c820d3d995446781dcd7a2adaa28e9)
set(UNIFIED_MEMORY_FRAMEWORK_HASH_TYPE SHA256)
set(UNIFIED_MEMORY_FRAMEWORK_FILE unified-memory-framework-${UNIFIED_MEMORY_FRAMEWORK_VERSION}.tar.gz)
set(UNIFIED_MEMORY_FRAMEWORK_HOMEPAGE https://github.com/oneapi-src/unified-memory-framework)
set(UNIFIED_MEMORY_FRAMEWORK_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(UNIFIED_MEMORY_FRAMEWORK_COPYRIGHT "Copyright (C) 2023-2024 Intel Corporation")

######################
### DPCPP DEPS END ###
######################

##########################################
### Intel Graphics Compiler DEPS BEGIN ###
##########################################
# The following deps are build time requirements for the intel graphics
# compiler, the versions used are taken from the following location
# https://github.com/intel/intel-graphics-compiler/releases

set(IGC_VERSION 1.0.17384.29)
set(IGC_URI https://github.com/intel/intel-graphics-compiler/archive/refs/tags/igc-${IGC_VERSION}.tar.gz)
set(IGC_HASH de7b1ba9cb1369f9aa26343bb4ad1ac7e5cb5f9f4517071f25d853e46cae6195)
set(IGC_HASH_TYPE SHA256)
set(IGC_FILE igc-${IGC_VERSION}.tar.gz)
set(IGC_NAME IGC)
set(IGC_HOMEPAGE https://github.com/intel/intel-graphics-compiler)
set(IGC_LICENSE SPDX:MIT)
set(IGC_COPYRIGHT "Copyright (C) 2019-2021 Intel Corporation")

set(IGC_LLVM_VERSION llvmorg-14.0.5)
set(IGC_LLVM_URI https://github.com/llvm/llvm-project/archive/refs/tags/${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HASH a4a57f029cb81f04618e05853f05fc2d21b64353c760977d8e7799bf7218a23a)
set(IGC_LLVM_HASH_TYPE SHA256)
set(IGC_LLVM_FILE ${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HOMEPAGE https://github.com/llvm/llvm-project/)
set(IGC_LLVM_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(IGC_LLVM_COPYRIGHT "Copyright (c) 2003-2019 University of Illinois at Urbana-Champaign. All rights reserved.")

# WARNING WARNING WARNING
#
# IGC_OPENCL_CLANG contains patches for some of its dependencies.
#
# Whenever IGC_OPENCL_CLANG_VERSION changes, one *MUST* inspect
# IGC_OPENCL_CLANG's patches folder and update igc.cmake to account for
# any added or removed patches.
#
# WARNING WARNING WARNING

set(IGC_OPENCL_CLANG_VERSION 470cf0018e1ef6fc92eda1356f5f31f7da452abc)
set(IGC_OPENCL_CLANG_URI https://github.com/intel/opencl-clang/archive/${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HASH fa410e0b4cc5b3fc3262e3b6aaace3543207a20ecd004f48dfec9a970f1fe4e2)
set(IGC_OPENCL_CLANG_HASH_TYPE SHA256)
set(IGC_OPENCL_CLANG_FILE opencl-clang-${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HOMEPAGE https://github.com/intel/opencl-clang/)
set(IGC_OPENCL_CLANG_LICENSE SPDX:Apache-2.0 WITH LLVM-exception)
set(IGC_OPENCL_CLANG_COPYRIGHT "Copyright (c) Intel Corporation (2009-2017).")

set(IGC_VCINTRINSICS_VERSION v0.19.0)
set(IGC_VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/refs/tags/${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_HASH b708df2fddc9fcb2cac5d6f26870f2e105f8395c0208ecd8acc38cbf175aee52)
set(IGC_VCINTRINSICS_HASH_TYPE SHA256)
set(IGC_VCINTRINSICS_FILE vc-intrinsics-${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_NAME "VC Intrinsics")
set(IGC_VCINTRINSICS_HOMEPAGE https://github.com/intel/vc-intrinsics)
set(IGC_VCINTRINSICS_LICENSE SPDX:MIT)
set(IGC_VCINTRINSICS_COPYRIGHT "Copyright (c) 2019 Intel Corporation")

set(IGC_SPIRV_HEADERS_VERSION vulkan-sdk-1.3.275.0)
set(IGC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_HASH d46b261f1fbc5e85022cb2fada9a6facb5b0c9932b45007a77fe05639a605bd1)
set(IGC_SPIRV_HEADERS_HASH_TYPE SHA256)
set(IGC_SPIRV_HEADERS_FILE SPIR-V-Headers-${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_NAME "SPIR-V Headers")
set(IGC_SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(IGC_SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(IGC_SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

set(IGC_SPIRV_TOOLS_VERSION v2023.6.rc1)
set(IGC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_HASH 750e4bfcaccd636fb04dd912b668a8a6d29940f8f83b7d9a266170b1023a1a89)
set(IGC_SPIRV_TOOLS_HASH_TYPE SHA256)
set(IGC_SPIRV_TOOLS_FILE SPIR-V-Tools-${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_NAME "SPIR-V Tools")
set(IGC_SPIRV_TOOLS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Tools/)
set(IGC_SPIRV_TOOLS_LICENSE SPDX:Apache-2.0)
set(IGC_SPIRV_TOOLS_COPYRIGHT "Copyright (c) 2015-2016 The Khronos Group Inc.")

set(IGC_SPIRV_TRANSLATOR_VERSION 2823e7052b7999c10fff63bc8089e5aa205716f4)
set(IGC_SPIRV_TRANSLATOR_URI https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_HASH 39eb3e033a0a1f5c69622d6a0b87e296b4e090d2f613f1014ee6fedcc2d3ca83)
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

set(GMMLIB_VERSION intel-gmmlib-22.4.1)
set(GMMLIB_URI https://github.com/intel/gmmlib/archive/refs/tags/${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HASH 451fbe2eac26533a896ca0da0356354ecc38680f273fce7d121c6a22251ed21e)
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

set(OCLOC_VERSION 24.31.30508.9)
set(OCLOC_URI https://github.com/intel/compute-runtime/archive/refs/tags/${OCLOC_VERSION}.tar.gz)
set(OCLOC_HASH 7c2b5708e996fc9e61997f1821d9be1e0fd43c9f29cfe3fea383a01d9aa92868)
set(OCLOC_HASH_TYPE SHA256)
set(OCLOC_FILE ocloc-${OCLOC_VERSION}.tar.gz)
set(OCLOC_HOMEPAGE https://github.com/intel/compute-runtime)
set(OCLOC_LICENSE SPDX:MIT)
set(OCLOC_COPYRIGHT "Copyright (C) 2021 Intel Corporation")

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

set(SHADERC_VERSION v2022.3)
set(SHADERC_URI https://github.com/google/shaderc/archive/${SHADERC_VERSION}.tar.gz)
set(SHADERC_HASH 5cb762af57637caf997d5f46baa4e8a4)
set(SHADERC_HASH_TYPE MD5)
set(SHADERC_FILE shaderc-${SHADERC_VERSION}.tar.gz)
set(SHADERC_NAME ShaderC)
set(SHADERC_HOMEPAGE https://github.com/google/shaderc)
set(SHADERC_LICENSE SPDX:Apache-2.0)
set(SHADERC_COPYRIGHT "Copyright 2015 The Shaderc Authors. All rights reserved.")

# The versions of shaderc's dependencies can be found in the root of shaderc's
# source in a file called DEPS.

set(SHADERC_SPIRV_TOOLS_VERSION eb0a36633d2acf4de82588504f951ad0f2cecacb)
set(SHADERC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)
set(SHADERC_SPIRV_TOOLS_HASH a4bdb8161f0e959c75d0d82d367c24f2)
set(SHADERC_SPIRV_TOOLS_HASH_TYPE MD5)
set(SHADERC_SPIRV_TOOLS_FILE SPIRV-Tools-${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)
set(SHADERC_SPIRV_TOOLS_NAME SPIR-V Tools)
set(SHADERC_SPIRV_TOOLS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Tools/)
set(SHADERC_SPIRV_TOOLS_LICENSE SPDX:Apache-2.0)
set(SHADERC_SPIRV_TOOLS_COPYRIGHT "Copyright (c) 2015-2016 The Khronos Group Inc.")

set(SHADERC_SPIRV_HEADERS_VERSION 85a1ed200d50660786c1a88d9166e871123cce39)
set(SHADERC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)
set(SHADERC_SPIRV_HEADERS_HASH 10d5e8160f39344a641523810b075568)
set(SHADERC_SPIRV_HEADERS_HASH_TYPE MD5)
set(SHADERC_SPIRV_HEADERS_FILE SPIRV-Headers-${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)
set(SHADERC_SPIRV_HEADERS_HOMEPAGE https://github.com/KhronosGroup/SPIRV-Headers)
set(SHADERC_SPIRV_HEADERS_LICENSE SPDX:MIT-Khronos-old)
set(SHADERC_SPIRV_HEADERS_COPYRIGHT "Copyright (c) 2015-2024 The Khronos Group Inc.")

set(SHADERC_GLSLANG_VERSION 89db4e1caa273a057ea46deba709c6e50001b314)
set(SHADERC_GLSLANG_URI https://github.com/KhronosGroup/glslang/archive/${SHADERC_GLSLANG_VERSION}.tar.gz)
set(SHADERC_GLSLANG_HASH 3b3c79ad8e9132ffcb8b63cc29c532e2)
set(SHADERC_GLSLANG_HASH_TYPE MD5)
set(SHADERC_GLSLANG_FILE glslang-${SHADERC_GLSLANG_VERSION}.tar.gz)
set(SHADERC_GLSLANG_HOMEPAGE https://github.com/KhronosGroup/glslang)
set(SHADERC_GLSLANG_LICENSE SPDX:Apache-2.0)
set(SHADERC_GLSLANG_COPYRIGHT [=[
Copyright 2020 The Khronos Group Inc.
Copyright (C) 2015-2018 Google, Inc.
]=])

set(VULKAN_VERSION v1.3.270)

set(VULKAN_HEADERS_VERSION ${VULKAN_VERSION})
set(VULKAN_HEADERS_NAME Vulkan-Headers)
set(VULKAN_HEADERS_URI https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${VULKAN_HEADERS_VERSION}.tar.gz)
set(VULKAN_HEADERS_HASH 805bde4c23197b86334cee5c2cf69d8e)
set(VULKAN_HEADERS_HASH_TYPE MD5)
set(VULKAN_HEADERS_FILE Vulkan-Headers-${VULKAN_HEADERS_VERSION}.tar.gz)
set(VULKAN_HEADERS_HOMEPAGE https://github.com/KhronosGroup/Vulkan-Headers)
set(VULKAN_HEADERS_LICENSE SPDX:Apache-2.0)
set(VULKAN_HEADERS_COPYRIGHT "Copyright 2015-2023 The Khronos Group Inc.")

set(VULKAN_LOADER_VERSION ${VULKAN_VERSION})
set(VULKAN_LOADER_NAME Vulkan-Loader)
set(VULKAN_LOADER_URI https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${VULKAN_LOADER_VERSION}.tar.gz)
set(VULKAN_LOADER_HASH 6903f9d285afcd1a167ec7c46cbabd49)
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

set(PYBIND11_VERSION 2.10.1)
set(PYBIND11_URI https://github.com/pybind/pybind11/archive/refs/tags/v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HASH ce07bfd5089245da7807b3faf6cbc878)
set(PYBIND11_HASH_TYPE MD5)
set(PYBIND11_FILE pybind-v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HOMEPAGE https://github.com/pybind/pybind11)
set(PYBIND11_LICENSE SPDX:BSD-2-Clause)
set(PYBIND11_COPYRIGHT "Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>, All rights reserved.")

set(HIPRT_VERSION 83e18cc9c3de8f2f9c48b663cf3189361e891054)
set(HIPRT_LIBRARY_VERSION 02003)
set(HIPRT_URI https://github.com/GPUOpen-LibrariesAndSDKs/HIPRT/archive/${HIPRT_VERSION}.tar.gz)
set(HIPRT_HASH b5639fa06bea45eff98bea2929516f7c)
set(HIPRT_HASH_TYPE MD5)
set(HIPRT_FILE hiprt-${HIPRT_VERSION}.tar.gz)
set(HIPRT_HOMEPAGE https://github.com/GPUOpen-LibrariesAndSDKs/HIPRT)
set(HIPRT_LICENSE SPDX:MIT)
set(HIPRT_COPYRIGHT "Copyright (C) 2024 Advanced Micro Devices, Inc. All Rights Reserved. ")
