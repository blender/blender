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

set(ZLIB_VERSION 1.2.13)
set(ZLIB_URI https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 9b8aa094c4e5765dabf4da391f00d15c)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_CPE "cpe:2.3:a:zlib:zlib:${ZLIB_VERSION}:*:*:*:*:*:*:*")
set(ZLIB_HOMEPAGE https://zlib.net)

set(OPENAL_VERSION 1.23.1)
set(OPENAL_URI https://github.com/kcat/openal-soft/releases/download/${OPENAL_VERSION}/openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HASH 58a73698288d2787451b61f8f4431513)
set(OPENAL_HASH_TYPE MD5)
set(OPENAL_FILE openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HOMEPAGE https://openal-soft.org/)

set(PNG_VERSION 1.6.37)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca)
set(PNG_HASH_TYPE SHA256)
set(PNG_FILE libpng-${PNG_VERSION}.tar.xz)
set(PNG_CPE "cpe:2.3:a:libpng:libpng:${PNG_VERSION}:*:*:*:*:*:*:*")
set(PNG_HOMEPAGE http://www.libpng.org/pub/png/libpng.html)

set(JPEG_VERSION 2.1.3)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 627b980fad0573e08e4c3b80b290fc91)
set(JPEG_HASH_TYPE MD5)
set(JPEG_FILE libjpeg-turbo-${JPEG_VERSION}.tar.gz)
set(JPEG_CPE "cpe:2.3:a:d.r.commander:libjpeg-turbo:${JPEG_VERSION}:*:*:*:*:*:*:*")

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

set(BLOSC_VERSION 1.21.1)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.tar.gz)
set(BLOSC_HASH 134b55813b1dca57019d2a2dc1f7a923)
set(BLOSC_HASH_TYPE MD5)
set(BLOSC_FILE blosc-${BLOSC_VERSION}.tar.gz)
set(BLOSC_CPE "cpe:2.3:a:c-blosc_project:c-blosc:${BLOSC_VERSION}:*:*:*:*:*:*:*")

set(PTHREADS_VERSION 3.0.0)
set(PTHREADS_URI http://prdownloads.sourceforge.net/pthreads4w/pthreads4w-code-v${PTHREADS_VERSION}.zip)
set(PTHREADS_HASH f3bf81bb395840b3446197bcf4ecd653)
set(PTHREADS_HASH_TYPE MD5)
set(PTHREADS_FILE pthreads4w-code-${PTHREADS_VERSION}.zip)
set(PTHREADS_HOMEPAGE https://github.com/fwbuilder/pthreads4w)

set(DEFLATE_VERSION 1.18)
set(DEFLATE_URI https://github.com/ebiggers/libdeflate/archive/refs/tags/v${DEFLATE_VERSION}.tar.gz)
set(DEFLATE_HASH a29d9dd653cbe03f2d5cd83972063f9e)
set(DEFLATE_HASH_TYPE MD5)
set(DEFLATE_FILE libdeflate-v${DEFLATE_VERSION}.tar.gz)

set(OPENEXR_VERSION 3.2.1)
set(OPENEXR_URI https://github.com/AcademySoftwareFoundation/openexr/archive/v${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH 1d5bb07433ec641cf3bb1b519a27ea6f)
set(OPENEXR_HASH_TYPE MD5)
set(OPENEXR_FILE openexr-${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_CPE "cpe:2.3:a:openexr:openexr:${OPENEXR_VERSION}:*:*:*:*:*:*:*")

set(IMATH_VERSION 3.1.7)
set(IMATH_URI https://github.com/AcademySoftwareFoundation/Imath/archive/v${IMATH_VERSION}.tar.gz)
set(IMATH_HASH 5cedab446ab296c080957c3037c6d097)
set(IMATH_HASH_TYPE MD5)
set(IMATH_FILE imath-${IMATH_VERSION}.tar.gz)


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

set(EPOXY_VERSION 1.5.10)
set(EPOXY_URI https://github.com/anholt/libepoxy/archive/refs/tags/${EPOXY_VERSION}.tar.gz)
set(EPOXY_HASH f0730aad115c952e77591fcc805b1dc1)
set(EPOXY_HASH_TYPE MD5)
set(EPOXY_FILE libepoxy-${EPOXY_VERSION}.tar.gz)

set(ALEMBIC_VERSION 1.8.3)
set(ALEMBIC_URI https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.tar.gz)
set(ALEMBIC_HASH 2cd8d6e5a3ac4a014e24a4b04f4fadf9)
set(ALEMBIC_HASH_TYPE MD5)
set(ALEMBIC_FILE alembic-${ALEMBIC_VERSION}.tar.gz)

set(OPENSUBDIV_VERSION v3_6_0)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_HASH cd03aaf8890bc0b8550eef62029cabe7)
set(OPENSUBDIV_HASH_TYPE MD5)
set(OPENSUBDIV_FILE opensubdiv-${OPENSUBDIV_VERSION}.tar.gz)

set(SDL_VERSION 2.28.2)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH 06ff379c406cd8318d18f0de81ee2709)
set(SDL_HASH_TYPE MD5)
set(SDL_FILE SDL2-${SDL_VERSION}.tar.gz)
set(SDL_CPE "cpe:2.3:a:libsdl:sdl:${SDL_VERSION}:*:*:*:*:*:*:*")
set(SDL_HOMEPAGE https://www.libsdl.org)

set(OPENCOLLADA_VERSION v1.6.68)
set(OPENCOLLADA_URI https://github.com/KhronosGroup/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH ee7dae874019fea7be11613d07567493)
set(OPENCOLLADA_HASH_TYPE MD5)
set(OPENCOLLADA_FILE opencollada-${OPENCOLLADA_VERSION}.tar.gz)

set(OPENCOLORIO_VERSION 2.3.2)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH 8af74fcb8c4820ab21204463a06ba490)
set(OPENCOLORIO_HASH_TYPE MD5)
set(OPENCOLORIO_FILE OpenColorIO-${OPENCOLORIO_VERSION}.tar.gz)

set(MINIZIPNG_VERSION 3.0.7)
set(MINIZIPNG_URI https://github.com/zlib-ng/minizip-ng/archive/${MINIZIPNG_VERSION}.tar.gz)
set(MINIZIPNG_HASH 09dcc8a9def348e1be9659e384c2cd55)
set(MINIZIPNG_HASH_TYPE MD5)
set(MINIZIPNG_FILE minizip-ng-${MINIZIPNG_VERSION}.tar.gz)

set(LLVM_VERSION 17.0.6)
set(LLVM_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH 62a09d65240a5133f001ace48269dbfc)
set(LLVM_HASH_TYPE MD5)
set(LLVM_FILE llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_CPE "cpe:2.3:a:llvm:compiler:${LLVM_VERSION}:*:*:*:*:*:*:*")

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

set(OPENIMAGEIO_VERSION v2.5.6.0)
set(OPENIMAGEIO_URI https://github.com/AcademySoftwareFoundation/OpenImageIO/archive/refs/tags/${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH d02db17716a20a71a446bdc6de57bd9c)
set(OPENIMAGEIO_HASH_TYPE MD5)
set(OPENIMAGEIO_FILE OpenImageIO-${OPENIMAGEIO_VERSION}.tar.gz)

# 9.1.0 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(FMT_VERSION 9.1.0)
set(FMT_URI https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.tar.gz)
set(FMT_HASH 5dea48d1fcddc3ec571ce2058e13910a0d4a6bab4cc09a809d8b1dd1c88ae6f2)
set(FMT_HASH_TYPE SHA256)
set(FMT_FILE fmt-${FMT_VERSION}.tar.gz)
set(FMT_CPE "cpe:2.3:a:fmt:fmt:${FMT_VERSION}:*:*:*:*:*:*:*")

# 0.6.2 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(ROBINMAP_VERSION v0.6.2)
set(ROBINMAP_URI https://github.com/Tessil/robin-map/archive/refs/tags/${ROBINMAP_VERSION}.tar.gz)
set(ROBINMAP_HASH c08ec4b1bf1c85eb0d6432244a6a89862229da1cb834f3f90fba8dc35d8c8ef1)
set(ROBINMAP_HASH_TYPE SHA256)
set(ROBINMAP_FILE robinmap-${ROBINMAP_VERSION}.tar.gz)

set(TIFF_VERSION 4.5.1)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH d08c5f9eee6350fffc239e5993d92779)
set(TIFF_HASH_TYPE MD5)
set(TIFF_FILE tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_CPE "cpe:2.3:a:libtiff:libtiff:${TIFF_VERSION}:*:*:*:*:*:*:*")
set(TIFF_HOMEPAGE http://www.simplesystems.org/libtiff/)

# Recent commit from 1.13.5.0 under development, which includes string table
# changes that make the Cycles OptiX implementation work. Official 1.12 OSL
# releases should also build but without OptiX support.
set(OSL_VERSION 3d52f3906b12d38ad0f4b991a8f9ea678171bd28)
set(OSL_URI https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/${OSL_VERSION}.tar.gz)
set(OSL_HASH dfe5d69f48930badc1ad39a4e11e2e98)
set(OSL_HASH_TYPE MD5)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)

# NOTE: When updating the python version, it's required to check the versions of
# it wants to use in PCbuild/get_externals.bat for the following dependencies:
# BZIP2, FFI, SQLITE and change the versions in this file as well. For compliance
# reasons there can be no exceptions to this.

set(PYTHON_VERSION 3.11.7)
set(PYTHON_SHORT_VERSION 3.11)
set(PYTHON_SHORT_VERSION_NO_DOTS 311)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH d96c7e134c35a8c46236f8a0e566b69c)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_CPE "cpe:2.3:a:python:python:${PYTHON_VERSION}:-:*:*:*:*:*:*")
set(PYTHON_HOMEPAGE https://www.python.org/)

set(TBB_YEAR 2020)
set(TBB_VERSION ${TBB_YEAR}_U3)
set(TBB_URI https://github.com/oneapi-src/oneTBB/archive/${TBB_VERSION}.tar.gz)
set(TBB_HASH 55ec8df6eae5ed6364a47f0e671e460c)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)
set(TBB_CPE "cpe:2.3:a:intel:threading_building_blocks:${TBB_YEAR}:*:*:*:*:*:*:*")

set(OPENVDB_VERSION 11.0.0)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 025f4fc4db58419341a4991f1a16174a)
set(OPENVDB_HASH_TYPE MD5)
set(OPENVDB_FILE openvdb-${OPENVDB_VERSION}.tar.gz)

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
set(AUTOPEP8_VERSION 1.6.0)
# Needed by: `autopep8` (so the version doesn't change on rebuild).
set(PYCODESTYLE_VERSION 2.8.0)
# Needed by: `autopep8` (so the version doesn't change on rebuild).
set(TOML_VERSION 0.10.2)
# Build system for other packages (not used by Blender at run-time).
set(MESON_VERSION 0.63.0)

set(NUMPY_VERSION 1.24.3)
set(NUMPY_SHORT_VERSION 1.24)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_HASH 89e5e2e78407032290ae6acf6dcaea46)
set(NUMPY_HASH_TYPE MD5)
set(NUMPY_FILE numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_CPE "cpe:2.3:a:numpy:numpy:${NUMPY_VERSION}:*:*:*:*:*:*:*")

set(LAME_VERSION 3.100)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 83e260acbe4389b54fe08e0bdbf7cddb)
set(LAME_HASH_TYPE MD5)
set(LAME_FILE lame-${LAME_VERSION}.tar.gz)
set(LAME_CPE "cpe:2.3:a:lame_project:lame:${LAME_VERSION}:*:*:*:*:*:*:*")
set(LAME_HOMEPAGE https://lame.sourceforge.io/)

set(OGG_VERSION 1.3.5)
set(OGG_URI http://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz)
set(OGG_HASH 0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664)
set(OGG_HASH_TYPE SHA256)
set(OGG_FILE libogg-${OGG_VERSION}.tar.gz)
set(OGG_HOMEPAGE https://xiph.org/ogg/)

set(VORBIS_VERSION 1.3.7)
set(VORBIS_URI http://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_HASH 0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab)
set(VORBIS_HASH_TYPE SHA256)
set(VORBIS_FILE libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_CPE "cpe:2.3:a:xiph.org:libvorbis:${VORBIS_VERSION}:*:*:*:*:*:*:*")
set(VORBIS_HOMEPAGE https://xiph.org/vorbis/)

set(THEORA_VERSION 1.1.1)
set(THEORA_URI http://downloads.xiph.org/releases/theora/libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HASH b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc)
set(THEORA_HASH_TYPE SHA256)
set(THEORA_FILE libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HOMEPAGE https://xiph.org/theora/)

set(FLAC_VERSION 1.4.2)
set(FLAC_URI http://downloads.xiph.org/releases/flac/flac-${FLAC_VERSION}.tar.xz)
set(FLAC_HASH e322d58a1f48d23d9dd38f432672865f6f79e73a6f9cc5a5f57fcaa83eb5a8e4 )
set(FLAC_HASH_TYPE SHA256)
set(FLAC_FILE flac-${FLAC_VERSION}.tar.xz)
set(FLAC_CPE "cpe:2.3:a:flac_project:flac:${FLAC_VERSION}:*:*:*:*:*:*:*")
set(FLAC_HOMEPAGE https://xiph.org/flac/)

set(VPX_VERSION 1.14.0)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 5f21d2db27071c8a46f1725928a10227ae45c5cd1cad3727e4aafbe476e321fa)
set(VPX_HASH_TYPE SHA256)
set(VPX_FILE libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_CPE "cpe:2.3:a:webmproject:libvpx:${VPX_VERSION}:*:*:*:*:*:*:*")

set(OPUS_VERSION 1.3.1)
set(OPUS_URI https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HASH 65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d)
set(OPUS_HASH_TYPE SHA256)
set(OPUS_FILE opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HOMEPAGE https://opus-codec.org/)

set(X264_VERSION 35fe20d1ba49918ec739a5b068c208ca82f977f7)
set(X264_URI https://code.videolan.org/videolan/x264/-/archive/${X264_VERSION}/x264-${X264_VERSION}.tar.gz)
set(X264_HASH bb4f7da03936b5a030ed5827133b58eb3f701d7e5dce32cca4ba6df93797d42e)
set(X264_HASH_TYPE SHA256)
set(X264_FILE x264-${X264_VERSION}.tar.gz)
set(X264_HOMEPAGE https://www.videolan.org/developers/x264.html)

set(OPENJPEG_VERSION 2.5.0)
set(OPENJPEG_SHORT_VERSION 2.5)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 0333806d6adecc6f7a91243b2b839ff4d2053823634d4f6ed7a59bc87409122a)
set(OPENJPEG_HASH_TYPE SHA256)
set(OPENJPEG_FILE openjpeg-v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_CPE "cpe:2.3:a:uclouvain:openjpeg:${OPENJPEG_VERSION}:*:*:*:*:*:*:*")

set(FFMPEG_VERSION 6.1.1)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH 5e3133939a61ef64ac9b47ffd29a5ea6e337a4023ef0ad972094b4da844e3a20)
set(FFMPEG_HASH_TYPE SHA256)
set(FFMPEG_FILE ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_CPE "cpe:2.3:a:ffmpeg:ffmpeg:${FFMPEG_VERSION}:*:*:*:*:*:*:*")
set(FFMPEG_HOMEPAGE https://ffmpeg.org/)

set(FFTW_VERSION 3.3.10)
set(FFTW_URI http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HASH 8ccbf6a5ea78a16dbc3e1306e234cc5c)
set(FFTW_HASH_TYPE MD5)
set(FFTW_FILE fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HOMEPAGE https://www.fftw.org/)

set(ICONV_VERSION 1.16)
set(ICONV_URI http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HASH 7d2a800b952942bb2880efb00cfd524c)
set(ICONV_HASH_TYPE MD5)
set(ICONV_FILE libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HOMEPAGE https://www.gnu.org/software/libiconv/)

set(SNDFILE_VERSION 1.2.2)
set(SNDFILE_URI https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_HASH 04e2e6f726da7c5dc87f8cf72f250d04)
set(SNDFILE_HASH_TYPE MD5)
set(SNDFILE_FILE libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_CPE "cpe:2.3:a:libsndfile_project:libsndfile:${SNDFILE_VERSION}:*:*:*:*:*:*:*")

set(WEBP_VERSION 1.3.2)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH 34869086761c0e2da6361035f7b64771)
set(WEBP_HASH_TYPE MD5)
set(WEBP_FILE libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_CPE "cpe:2.3:a:webmproject:libwebp:${WEBP_VERSION}:*:*:*:*:*:*:*")
set(WEBP_HOMEPAGE https://developers.google.com/speed/webp)

set(SPNAV_VERSION 1.1)
set(SPNAV_URI https://github.com/FreeSpacenav/libspnav/releases/download/v${SPNAV_VERSION}/libspnav-${SPNAV_VERSION}.tar.gz)
set(SPNAV_HASH 7c0032034672dfba3c4bb9b49a440e70)
set(SPNAV_HASH_TYPE MD5)
set(SPNAV_FILE libspnav-${SPNAV_VERSION}.tar.gz)

set(JEMALLOC_VERSION 5.2.1)
set(JEMALLOC_URI https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2)
set(JEMALLOC_HASH 3d41fbf006e6ebffd489bdb304d009ae)
set(JEMALLOC_HASH_TYPE MD5)
set(JEMALLOC_FILE jemalloc-${JEMALLOC_VERSION}.tar.bz2)

set(XML2_VERSION 2.12.3)
set(XML2_URI https://download.gnome.org/sources/libxml2/2.12/libxml2-${XML2_VERSION}.tar.xz)
set(XML2_HASH 13871e7cf2137b4b9b9da753ffef538c)
set(XML2_HASH_TYPE MD5)
set(XML2_FILE libxml2-${XML2_VERSION}.tar.xz)
set(XML2_CPE "cpe:2.3:a:xmlsoft:libxml2:${XML2_VERSION}:*:*:*:*:*:*:*")
set(XML2_HOMEPAGE https://gitlab.gnome.org/GNOME/libxml2)

set(YAMLCPP_VERSION 0.7.0)
set(YAMLCPP_URI https://codeload.github.com/jbeder/yaml-cpp/tar.gz/yaml-cpp-${YAMLCPP_VERSION})
set(YAMLCPP_HASH 74d646a3cc1b5d519829441db96744f0)
set(YAMLCPP_HASH_TYPE MD5)
set(YAMLCPP_FILE yaml-cpp-${YAMLCPP_VERSION}.tar.gz)
set(YAMLCPP "cpe:2.3:a:yaml-cpp_project:yaml-cpp:${YAMLCPP_VERSION}:*:*:*:*:*:*:*")

set(PYSTRING_VERSION v1.1.3)
set(PYSTRING_URI https://codeload.github.com/imageworks/pystring/tar.gz/refs/tags/${PYSTRING_VERSION})
set(PYSTRING_HASH f2c68786b359f5e4e62bed53bc4fb86d)
set(PYSTRING_HASH_TYPE MD5)
set(PYSTRING_FILE pystring-${PYSTRING_VERSION}.tar.gz)

set(EXPAT_VERSION 2_5_0)
set(EXPAT_VERSION_DOTS 2.5.0)
set(EXPAT_URI https://github.com/libexpat/libexpat/archive/R_${EXPAT_VERSION}.tar.gz)
set(EXPAT_HASH d375fa3571c0abb945873f5061a8f2e2)
set(EXPAT_HASH_TYPE MD5)
set(EXPAT_FILE libexpat-${EXPAT_VERSION}.tar.gz)
set(EXPAT_CPE "cpe:2.3:a:libexpat_project:libexpat:${EXPAT_VERSION_DOTS}:*:*:*:*:*:*:*")

set(PUGIXML_VERSION 1.10)
set(PUGIXML_URI https://github.com/zeux/pugixml/archive/v${PUGIXML_VERSION}.tar.gz)
set(PUGIXML_HASH 0c208b0664c7fb822bf1b49ad035e8fd)
set(PUGIXML_HASH_TYPE MD5)
set(PUGIXML_FILE pugixml-${PUGIXML_VERSION}.tar.gz)
set(PUGIXML_CPE "cpe:2.3:a:pugixml_project:pugixml:${PUGIXML_VERSION}:*:*:*:*:*:*:*")

set(FLEXBISON_VERSION 2.5.24)
set(FLEXBISON_URI http://prdownloads.sourceforge.net/winflexbison/win_flex_bison-${FLEXBISON_VERSION}.zip)
set(FLEXBISON_HASH 6b549d43e34ece0e8ed05af92daa31c4)
set(FLEXBISON_HASH_TYPE MD5)
set(FLEXBISON_FILE win_flex_bison-${FLEXBISON_VERSION}.zip)
set(FLEXBISON_HOMEPAGE https://github.com/lexxmark/winflexbison)

set(FLEX_VERSION 2.6.4)
set(FLEX_URI https://github.com/westes/flex/releases/download/v${FLEX_VERSION}/flex-${FLEX_VERSION}.tar.gz)
set(FLEX_HASH 2882e3179748cc9f9c23ec593d6adc8d)
set(FLEX_HASH_TYPE MD5)
set(FLEX_FILE flex-${FLEX_VERSION}.tar.gz)

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

# NOTE: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat. For compliance reasons there
# can be no exceptions to this.
set(FFI_VERSION 3.4.4)
set(FFI_URI https://github.com/libffi/libffi/releases/download/v${FFI_VERSION}/libffi-${FFI_VERSION}.tar.gz)
set(FFI_HASH d66c56ad259a82cf2a9dfc408b32bf5da52371500b84745f7fb8b645712df676)
set(FFI_HASH_TYPE SHA256)
set(FFI_FILE libffi-${FFI_VERSION}.tar.gz)
set(FFI_CPE "cpe:2.3:a:libffi_project:libffi:${FFI_VERSION}:*:*:*:*:*:*:*")

set(LZMA_VERSION 5.2.5)
set(LZMA_URI https://tukaani.org/xz/xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_HASH 5117f930900b341493827d63aa910ff5e011e0b994197c3b71c08a20228a42df)
set(LZMA_HASH_TYPE SHA256)
set(LZMA_FILE xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_HOMEPAGE https://tukaani.org/lzma/)

# NOTE: Python's build has been modified to use our ssl version.
set(SSL_VERSION 3.1.5)
set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
set(SSL_HASH 6ae015467dabf0469b139ada93319327be24b98251ffaeceda0221848dc09262)
set(SSL_HASH_TYPE SHA256)
set(SSL_FILE openssl-${SSL_VERSION}.tar.gz)
set(SSL_CPE "cpe:2.3:a:openssl:openssl:${SSL_VERSION}:*:*:*:*:*:*:*")
set(SSL_HOMEPAGE https://www.openssl.org)

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

set(EMBREE_VERSION 4.1.0)
set(EMBREE_URI https://github.com/embree/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 4b525955b08e1249a700dea5b5ffc8b2)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)

set(USD_VERSION 23.11)
set(USD_URI https://github.com/PixarAnimationStudios/OpenUSD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH 77358a244f50fc607e8b40764ea4f6c6)
set(USD_HASH_TYPE MD5)
set(USD_FILE usd-v${USD_VERSION}.tar.gz)

set(MATERIALX_VERSION 1.38.8)
set(MATERIALX_URI https://github.com/AcademySoftwareFoundation/MaterialX/archive/refs/tags/v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HASH fad8f4e19305fb2ee920cbff638f3560)
set(MATERIALX_HASH_TYPE MD5)
set(MATERIALX_FILE materialx-v${MATERIALX_VERSION}.tar.gz)

set(OIDN_VERSION 2.2.0-rc2)
set(OIDN_URI https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HASH 14b261af3a719c49ab10e71583f1a61a)
set(OIDN_HASH_TYPE MD5)
set(OIDN_FILE oidn-${OIDN_VERSION}.src.tar.gz)

set(LIBGLU_VERSION 9.0.1)
set(LIBGLU_URI ftp://ftp.freedesktop.org/pub/mesa/glu/glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HASH 151aef599b8259efe9acd599c96ea2a3)
set(LIBGLU_HASH_TYPE MD5)
set(LIBGLU_FILE glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HOMEPAGE https://gitlab.freedesktop.org/mesa/glu)

set(MESA_VERSION 23.3.0)
set(MESA_URI ftp://ftp.freedesktop.org/pub/mesa/mesa-${MESA_VERSION}.tar.xz)
set(MESA_HASH 50f729dd60ed6335b989095baad81ef5edf7cfdd4b4b48b9b955917cb07d69c5)
set(MESA_HASH_TYPE SHA256)
set(MESA_FILE mesa-${MESA_VERSION}.tar.xz)
set(MESA_CPE "cpe:2.3:a:mesa3d:mesa:${MESA_VERSION}:*:*:*:*:*:*:*")
set(MESA_HOMEPAGE https://www.mesa3d.org/)

set(NASM_VERSION 2.15.02)
set(NASM_URI https://github.com/netwide-assembler/nasm/archive/nasm-${NASM_VERSION}.tar.gz)
set(NASM_HASH aded8b796c996a486a56e0515c83e414116decc3b184d88043480b32eb0a8589)
set(NASM_HASH_TYPE SHA256)
set(NASM_FILE nasm-${NASM_VERSION}.tar.gz)
set(NASM_PCE "cpe:2.3:a:nasm:nasm:${NASM_VERSION}:*:*:*:*:*:*:*")

set(XR_OPENXR_SDK_VERSION 1.0.22)
set(XR_OPENXR_SDK_URI https://github.com/KhronosGroup/OpenXR-SDK/archive/release-${XR_OPENXR_SDK_VERSION}.tar.gz)
set(XR_OPENXR_SDK_HASH a2623ebab3d0b340bc16311b14f02075)
set(XR_OPENXR_SDK_HASH_TYPE MD5)
set(XR_OPENXR_SDK_FILE OpenXR-SDK-${XR_OPENXR_SDK_VERSION}.tar.gz)

set(WL_PROTOCOLS_VERSION 1.32)
set(WL_PROTOCOLS_FILE wayland-protocols-${WL_PROTOCOLS_VERSION}.tar.xz)
set(WL_PROTOCOLS_URI https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/${WL_PROTOCOLS_VERSION}/downloads/${WL_PROTOCOLS_FILE})
set(WL_PROTOCOLS_HASH 00c2cedb0d2df714a0965a00c19385c6)
set(WL_PROTOCOLS_HASH_TYPE MD5)
set(WL_PROTOCOLS_HOMEPAGE https://gitlab.freedesktop.org/wayland/wayland-protocols)

set(WAYLAND_VERSION 1.22.0)
set(WAYLAND_FILE wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_URI https://gitlab.freedesktop.org/wayland/wayland/-/releases/${WAYLAND_VERSION}/downloads/wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_HASH 7410ab549e3928fce9381455b17b0803)
set(WAYLAND_HASH_TYPE MD5)
set(WAYLAND_HOMEPAGE https://gitlab.freedesktop.org/wayland/wayland)

set(WAYLAND_LIBDECOR_VERSION 0.1.0)
set(WAYLAND_LIBDECOR_FILE libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz)
set(WAYLAND_LIBDECOR_URI https://gitlab.gnome.org/jadahl/libdecor/uploads/81adf91d27620e20bcc5f6b9b312d768/libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz )
set(WAYLAND_LIBDECOR_HASH 47b59eba76faa3787f0878bf8700e912)
set(WAYLAND_LIBDECOR_HASH_TYPE MD5)
set(WAYLAND_LIBDECOR_HOMEPAGE https://gitlab.freedesktop.org/libdecor/libdecor)

set(WAYLAND_WESTON_VERSION 12.0.92)
set(WAYLAND_WESTON_FILE weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_URI https://gitlab.freedesktop.org/wayland/weston/-/releases/${WAYLAND_WESTON_VERSION}/downloads/weston-${WAYLAND_WESTON_VERSION}.tar.xz)
set(WAYLAND_WESTON_HASH 44542b60bf9b9fe3add904af11bbad98)
set(WAYLAND_WESTON_HASH_TYPE MD5)
set(WAYLAND_WESTON_HOMEPAGE https://wayland.freedesktop.org)

set(ISPC_VERSION v1.21.1)
set(ISPC_URI https://github.com/ispc/ispc/archive/${ISPC_VERSION}.tar.gz)
set(ISPC_HASH edd16b016aabc07819d14fd86a1fb5d0)
set(ISPC_HASH_TYPE MD5)
set(ISPC_FILE ispc-${ISPC_VERSION}.tar.gz)

set(GMP_VERSION 6.2.1)
set(GMP_URI https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.xz)
set(GMP_HASH 0b82665c4a92fd2ade7440c13fcaa42b)
set(GMP_HASH_TYPE MD5)
set(GMP_FILE gmp-${GMP_VERSION}.tar.xz)
set(GMP_CPE "cpe:2.3:a:gmplib:gmp:${GMP_VERSION}:*:*:*:*:*:*:*")
set(GMP_HOMEPAGE https://gmplib.org/)

set(POTRACE_VERSION 1.16)
set(POTRACE_URI http://potrace.sourceforge.net/download/${POTRACE_VERSION}/potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_HASH 5f0bd87ddd9a620b0c4e65652ef93d69)
set(POTRACE_HASH_TYPE MD5)
set(POTRACE_FILE potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_CPE "cpe:2.3:a:icoasoft:potrace:${POTRACE_VERSION}:*:*:*:*:*:*:*")
set(POTRACE_HOMEPAGE https://potrace.sourceforge.net/)

set(HARU_VERSION 2_3_0)
set(HARU_URI https://github.com/libharu/libharu/archive/RELEASE_${HARU_VERSION}.tar.gz)
set(HARU_HASH 4f916aa49c3069b3a10850013c507460)
set(HARU_HASH_TYPE MD5)
set(HARU_FILE libharu-${HARU_VERSION}.tar.gz)

set(ZSTD_VERSION 1.5.0)
set(ZSTD_URI https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_HASH 5194fbfa781fcf45b98c5e849651aa7b3b0a008c6b72d4a0db760f3002291e94)
set(ZSTD_HASH_TYPE SHA256)
set(ZSTD_FILE zstd-${ZSTD_VERSION}.tar.gz)
set(ZSTD_CPE "cpe:2.3:a:facebook:zstandard:${ZSTD_VERSION}:*:*:*:*:*:*:*")

set(SSE2NEON_VERSION cfaa59fc04fecb117c0a0f3fe9c82dece6f359ad)
set(SSE2NEON_URI https://github.com/DLTcollab/sse2neon/archive/${SSE2NEON_VERSION}.tar.gz)
set(SSE2NEON_HASH 5491c5038a301a6b0ba0531e516568bb50d165d206360f03d8d56558a2490669)
set(SSE2NEON_HASH_TYPE SHA256)
set(SSE2NEON_FILE sse2neon-${SSE2NEON_VERSION}.tar.gz)

set(BROTLI_VERSION 1.0.9)
set(BROTLI_URI https://github.com/google/brotli/archive/refs/tags/v${BROTLI_VERSION}.tar.gz)
set(BROTLI_HASH f9e8d81d0405ba66d181529af42a3354f838c939095ff99930da6aa9cdf6fe46)
set(BROTLI_HASH_TYPE SHA256)
set(BROTLI_FILE brotli-v${BROTLI_VERSION}.tar.gz)
set(BROTLI_CPE "cpe:2.3:a:google:brotli:${BROTLI_VERSION}:*:*:*:*:*:*:*")

set(OPENPGL_VERSION v0.6.0)
set(OPENPGL_SHORT_VERSION 0.6.0)
set(OPENPGL_URI https://github.com/OpenPathGuidingLibrary/openpgl/archive/refs/tags/${OPENPGL_VERSION}.tar.gz)
set(OPENPGL_HASH 4192a4096ee3e3d31878cd013f8de23418c8037c576537551f946c4811931c5e)
set(OPENPGL_HASH_TYPE SHA256)
set(OPENPGL_FILE openpgl-${OPENPGL_VERSION}.tar.gz)

set(LEVEL_ZERO_VERSION 1.15.8)
set(LEVEL_ZERO_URI https://codeload.github.com/oneapi-src/level-zero/tar.gz/refs/tags/v${LEVEL_ZERO_VERSION})
set(LEVEL_ZERO_HASH 80663dbd4d01d9519185c6e568f2e836bfea7484363f4da8cf5cf77c3bf58602)
set(LEVEL_ZERO_HASH_TYPE SHA256)
set(LEVEL_ZERO_FILE level-zero-${LEVEL_ZERO_VERSION}.tar.gz)

set(DPCPP_VERSION 2022-12)
set(DPCPP_URI https://github.com/intel/llvm/archive/refs/tags/${DPCPP_VERSION}.tar.gz)
set(DPCPP_HASH 13151d5ae79f7c9c4a9b072a0c486ae7b3c4993e301bb1268c92214451025790)
set(DPCPP_HASH_TYPE SHA256)
set(DPCPP_FILE DPCPP-${DPCPP_VERSION}.tar.gz)

########################
### DPCPP DEPS BEGIN ###
########################
# The following deps are build time requirements for dpcpp, when possible
# the source in the dpcpp source tree for the version chosen is documented
# by each dep, these will only have to be downloaded and unpacked, dpcpp
# will take care of building them, unpack is being done in dpcpp_deps.cmake

# Source llvm/lib/SYCLLowerIR/CMakeLists.txt
set(VCINTRINSICS_VERSION 782fbf7301dc73acaa049a4324c976ad94f587f7)
set(VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HASH f4c0ccad8c1f77760364c551c65e8e1cf194d058889fa46d3b1b2d19ec4dc33f)
set(VCINTRINSICS_HASH_TYPE SHA256)
set(VCINTRINSICS_FILE vc-intrinsics-${VCINTRINSICS_VERSION}.tar.gz)

# Source opencl/CMakeLists.txt
set(OPENCLHEADERS_VERSION dcd5bede6859d26833cd85f0d6bbcee7382dc9b3)
set(OPENCLHEADERS_URI https://github.com/KhronosGroup/OpenCL-Headers/archive/${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_HASH ca8090359654e94f2c41e946b7e9d826253d795ae809ce7c83a7d3c859624693)
set(OPENCLHEADERS_HASH_TYPE SHA256)
set(OPENCLHEADERS_FILE opencl_headers-${OPENCLHEADERS_VERSION}.tar.gz)

# Source opencl/CMakeLists.txt
set(ICDLOADER_VERSION 792682ad3d877ab38573b997808bab3b43902b70)
set(ICDLOADER_URI https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HASH b33a0320d94bf300efa1da97931ded506d27813bd1148da6858fe79d412d1ea2)
set(ICDLOADER_HASH_TYPE SHA256)
set(ICDLOADER_FILE icdloader-${ICDLOADER_VERSION}.tar.gz)

# Source sycl/cmake/modules/AddBoostMp11Headers.cmake
# Using external MP11 here, getting AddBoostMp11Headers.cmake to recognize
# our copy in boost directly was more trouble than it was worth.
set(MP11_VERSION 7bc4e1ae9b36ec8ee635c3629b59ec525bbe82b9)
set(MP11_URI https://github.com/boostorg/mp11/archive/${MP11_VERSION}.tar.gz)
set(MP11_HASH 071ee2bd3952ec89882edb3af25dd1816f6b61723f66e42eea32f4d02ceef426)
set(MP11_HASH_TYPE SHA256)
set(MP11_FILE mp11-${MP11_VERSION}.tar.gz)

# Source llvm-spirv/CMakeLists.txt (repo)
# Source llvm-spirv/spirv-headers-tag.conf (hash)
set(SPIRV_HEADERS_VERSION 5a121866927a16ab9d49bed4788b532c7fcea766)
set(SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SPIRV_HEADERS_VERSION}.tar.gz)
set(SPIRV_HEADERS_HASH ec8ecb471a62672697846c436501638ab25447ae9d4a6761e0bfe8a9a839502a)
set(SPIRV_HEADERS_HASH_TYPE SHA256)
set(SPIRV_HEADERS_FILE SPIR-V-Headers-${SPIRV_HEADERS_VERSION}.tar.gz)

# Source llvm/sycl/plugins/unified_runtime/CMakeLists.txt
set(UNIFIED_RUNTIME_VERSION fd711c920acc4434cb52ff18b078c082d9d7f44d)
set(UNIFIED_RUNTIME_URI https://github.com/oneapi-src/unified-runtime/archive/${UNIFIED_RUNTIME_VERSION}.tar.gz)
set(UNIFIED_RUNTIME_HASH 535ca2ee78f68c5e7e62b10f1bbabd909179488885566e6d9b1fc50e8a1be65f)
set(UNIFIED_RUNTIME_HASH_TYPE SHA256)
set(UNIFIED_RUNTIME_FILE unified-runtime-${UNIFIED_RUNTIME_VERSION}.tar.gz)

######################
### DPCPP DEPS END ###
######################

##########################################
### Intel Graphics Compiler DEPS BEGIN ###
##########################################
# The following deps are build time requirements for the intel graphics
# compiler, the versions used are taken from the following location
# https://github.com/intel/intel-graphics-compiler/releases

set(IGC_VERSION 1.0.15468.25)
set(IGC_URI https://github.com/intel/intel-graphics-compiler/archive/refs/tags/igc-${IGC_VERSION}.tar.gz)
set(IGC_HASH c2c36af98ead4f4f6975633eaa53f45b84cb96ce48d9bfa879bebfaf12367b79)
set(IGC_HASH_TYPE SHA256)
set(IGC_FILE igc-${IGC_VERSION}.tar.gz)

set(IGC_LLVM_VERSION llvmorg-14.0.5)
set(IGC_LLVM_URI https://github.com/llvm/llvm-project/archive/refs/tags/${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HASH a4a57f029cb81f04618e05853f05fc2d21b64353c760977d8e7799bf7218a23a)
set(IGC_LLVM_HASH_TYPE SHA256)
set(IGC_LLVM_FILE ${IGC_LLVM_VERSION}.tar.gz)

# WARNING WARNING WARNING
#
# IGC_OPENCL_CLANG contains patches for some of its dependencies.
#
# Whenever IGC_OPENCL_CLANG_VERSION changes, one *MUST* inspect
# IGC_OPENCL_CLANG's patches folder and update igc.cmake to account for
# any added or removed patches.
#
# WARNING WARNING WARNING

set(IGC_OPENCL_CLANG_VERSION cf95b338d14685e4f3402ab1828bef31d48f1fd6)
set(IGC_OPENCL_CLANG_URI https://github.com/intel/opencl-clang/archive/${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HASH e6191148c87ac7fdc2806b04feeb008c217344ee4dd1308b87e4c6cf3112d4bc)
set(IGC_OPENCL_CLANG_HASH_TYPE SHA256)
set(IGC_OPENCL_CLANG_FILE opencl-clang-${IGC_OPENCL_CLANG_VERSION}.tar.gz)

set(IGC_VCINTRINSICS_VERSION v0.13.0)
set(IGC_VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/refs/tags/${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_HASH f98e265b38312cceaa3276b9800e0c5b1f167e5807d50abd9585268c7025e9b7)
set(IGC_VCINTRINSICS_HASH_TYPE SHA256)
set(IGC_VCINTRINSICS_FILE vc-intrinsics-${IGC_VCINTRINSICS_VERSION}.tar.gz)

set(IGC_SPIRV_HEADERS_VERSION sdk-1.3.239.0)
set(IGC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_HASH fdaf6670e311cd1c08ae90bf813e89dd31630205bc60030ffd25fb0af39b51fe)
set(IGC_SPIRV_HEADERS_HASH_TYPE SHA256)
set(IGC_SPIRV_HEADERS_FILE SPIR-V-Headers-${IGC_SPIRV_HEADERS_VERSION}.tar.gz)

set(IGC_SPIRV_TOOLS_VERSION sdk-1.3.239.0)
set(IGC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_HASH 327b2dba4515646eee28c1a5fe1332891e81c8b6ff289363f52877f3e67c2d81)
set(IGC_SPIRV_TOOLS_HASH_TYPE SHA256)
set(IGC_SPIRV_TOOLS_FILE SPIR-V-Tools-${IGC_SPIRV_TOOLS_VERSION}.tar.gz)

set(IGC_SPIRV_TRANSLATOR_VERSION 7e332d0acc8ee57462d9fbedefaf411fc193fdd0)
set(IGC_SPIRV_TRANSLATOR_URI https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_HASH 29aadf5fd4e64ff1d4f86446eacd6a7439efeb280478988c36314c4441072c36)
set(IGC_SPIRV_TRANSLATOR_HASH_TYPE SHA256)
set(IGC_SPIRV_TRANSLATOR_FILE SPIR-V-Translator-${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)

########################################
### Intel Graphics Compiler DEPS END ###
########################################

set(GMMLIB_VERSION intel-gmmlib-22.3.11)
set(GMMLIB_URI https://github.com/intel/gmmlib/archive/refs/tags/${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HASH b97f4e501c1e902a559cbd6597c008a700f4ab8c495680bf1968db99c6547afe)
set(GMMLIB_HASH_TYPE SHA256)
set(GMMLIB_FILE ${GMMLIB_VERSION}.tar.gz)

set(OCLOC_VERSION 23.43.27642.40)
set(OCLOC_URI https://github.com/intel/compute-runtime/archive/refs/tags/${OCLOC_VERSION}.tar.gz)
set(OCLOC_HASH 67d0c6f3103ff12408a628e14f7170da3e0220313e10799693d576cea7821fe2)
set(OCLOC_HASH_TYPE SHA256)
set(OCLOC_FILE ocloc-${OCLOC_VERSION}.tar.gz)

set(AOM_VERSION 3.4.0)
set(AOM_URI https://storage.googleapis.com/aom-releases/libaom-${AOM_VERSION}.tar.gz)
set(AOM_HASH bd754b58c3fa69f3ffd29da77de591bd9c26970e3b18537951336d6c0252e354)
set(AOM_HASH_TYPE SHA256)
set(AOM_FILE libaom-${AOM_VERSION}.tar.gz)
set(AOM_HOMEPAGE https://aomedia.googlesource.com/aom/)

set(FRIBIDI_VERSION v1.0.12)
set(FRIBIDI_URI https://github.com/fribidi/fribidi/archive/refs/tags/${FRIBIDI_VERSION}.tar.gz)
set(FRIBIDI_HASH 2e9e859876571f03567ac91e5ed3b5308791f31cda083408c2b60fa1fe00a39d)
set(FRIBIDI_HASH_TYPE SHA256)
set(FRIBIDI_FILE fribidi-${FRIBIDI_VERSION}.tar.gz)

set(HARFBUZZ_VERSION 5.1.0)
set(HARFBUZZ_URI https://github.com/harfbuzz/harfbuzz/archive/refs/tags/${HARFBUZZ_VERSION}.tar.gz)
set(HARFBUZZ_HASH 5352ff2eec538ea9a63a485cf01ad8332a3f63aa79921c5a2e301cef185caea1)
set(HARFBUZZ_HASH_TYPE SHA256)
set(HARFBUZZ_FILE harfbuzz-${HARFBUZZ_VERSION}.tar.gz)

set(SHADERC_VERSION v2022.3)
set(SHADERC_URI https://github.com/google/shaderc/archive/${SHADERC_VERSION}.tar.gz)
set(SHADERC_HASH 5cb762af57637caf997d5f46baa4e8a4)
set(SHADERC_HASH_TYPE MD5)
set(SHADERC_FILE shaderc-${SHADERC_VERSION}.tar.gz)

# The versions of shaderc's dependencies can be found in the root of shaderc's
# source in a file called DEPS.

set(SHADERC_SPIRV_TOOLS_VERSION eb0a36633d2acf4de82588504f951ad0f2cecacb)
set(SHADERC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)
set(SHADERC_SPIRV_TOOLS_HASH a4bdb8161f0e959c75d0d82d367c24f2)
set(SHADERC_SPIRV_TOOLS_HASH_TYPE MD5)
set(SHADERC_SPIRV_TOOLS_FILE SPIRV-Tools-${SHADERC_SPIRV_TOOLS_VERSION}.tar.gz)

set(SHADERC_SPIRV_HEADERS_VERSION 85a1ed200d50660786c1a88d9166e871123cce39)
set(SHADERC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)
set(SHADERC_SPIRV_HEADERS_HASH 10d5e8160f39344a641523810b075568)
set(SHADERC_SPIRV_HEADERS_HASH_TYPE MD5)
set(SHADERC_SPIRV_HEADERS_FILE SPIRV-Headers-${SHADERC_SPIRV_HEADERS_VERSION}.tar.gz)

set(SHADERC_GLSLANG_VERSION 89db4e1caa273a057ea46deba709c6e50001b314)
set(SHADERC_GLSLANG_URI https://github.com/KhronosGroup/glslang/archive/${SHADERC_GLSLANG_VERSION}.tar.gz)
set(SHADERC_GLSLANG_HASH 3b3c79ad8e9132ffcb8b63cc29c532e2)
set(SHADERC_GLSLANG_HASH_TYPE MD5)
set(SHADERC_GLSLANG_FILE glslang-${SHADERC_GLSLANG_VERSION}.tar.gz)

set(VULKAN_VERSION v1.3.270)

set(VULKAN_HEADERS_VERSION ${VULKAN_VERSION})
set(VULKAN_HEADERS_URI https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${VULKAN_HEADERS_VERSION}.tar.gz)
set(VULKAN_HEADERS_HASH 805bde4c23197b86334cee5c2cf69d8e)
set(VULKAN_HEADERS_HASH_TYPE MD5)
set(VULKAN_HEADERS_FILE Vulkan-Headers-${VULKAN_HEADERS_VERSION}.tar.gz)

set(VULKAN_LOADER_VERSION ${VULKAN_VERSION})
set(VULKAN_LOADER_URI https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${VULKAN_LOADER_VERSION}.tar.gz)
set(VULKAN_LOADER_HASH 6903f9d285afcd1a167ec7c46cbabd49)
set(VULKAN_LOADER_HASH_TYPE MD5)
set(VULKAN_LOADER_FILE Vulkan-Loader-${VULKAN_LOADER_VERSION}.tar.gz)

set(PYBIND11_VERSION 2.10.1)
set(PYBIND11_URI https://github.com/pybind/pybind11/archive/refs/tags/v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HASH ce07bfd5089245da7807b3faf6cbc878)
set(PYBIND11_HASH_TYPE MD5)
set(PYBIND11_FILE pybind-v${PYBIND11_VERSION}.tar.gz)
