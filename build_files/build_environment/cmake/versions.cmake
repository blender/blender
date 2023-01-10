# SPDX-License-Identifier: GPL-2.0-or-later

# CPE's are used to identify dependencies, for more information on what they
# are please see https://nvd.nist.gov/products/cpe
#
# We use them in combination with cve-bin-tool to scan for known security issues.
#
# Not all of our dependencies are currently in the nvd database so not all
# dependencies have one assigned.

set(ZLIB_VERSION 1.2.13)
set(ZLIB_URI https://zlib.net/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 9b8aa094c4e5765dabf4da391f00d15c)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_CPE "cpe:2.3:a:zlib:zlib:${ZLIB_VERSION}:*:*:*:*:*:*:*")

set(OPENAL_VERSION 1.21.1)
set(OPENAL_URI http://openal-soft.org/openal-releases/openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HASH a936806ebd8de417b0ffd8cf3f48f456)
set(OPENAL_HASH_TYPE MD5)
set(OPENAL_FILE openal-soft-${OPENAL_VERSION}.tar.bz2)

set(PNG_VERSION 1.6.37)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca)
set(PNG_HASH_TYPE SHA256)
set(PNG_FILE libpng-${PNG_VERSION}.tar.xz)
set(PNG_CPE "cpe:2.3:a:libpng:libpng:${PNG_VERSION}:*:*:*:*:*:*:*")

set(JPEG_VERSION 2.1.3)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 627b980fad0573e08e4c3b80b290fc91)
set(JPEG_HASH_TYPE MD5)
set(JPEG_FILE libjpeg-turbo-${JPEG_VERSION}.tar.gz)
set(JPEG_CPE "cpe:2.3:a:d.r.commander:libjpeg-turbo:${JPEG_VERSION}:*:*:*:*:*:*:*")

set(BOOST_VERSION 1.80.0)
set(BOOST_VERSION_SHORT 1.80)
set(BOOST_VERSION_NODOTS 1_80_0)
set(BOOST_VERSION_NODOTS_SHORT 1_80)
set(BOOST_URI https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_HASH 077f074743ea7b0cb49c6ed43953ae95)
set(BOOST_HASH_TYPE MD5)
set(BOOST_FILE boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_CPE "cpe:2.3:a:boost:boost:${BOOST_VERSION}:*:*:*:*:*:*:*")

set(BLOSC_VERSION 1.21.1)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.tar.gz)
set(BLOSC_HASH 134b55813b1dca57019d2a2dc1f7a923)
set(BLOSC_HASH_TYPE MD5)
set(BLOSC_FILE blosc-${BLOSC_VERSION}.tar.gz)
set(BLOSC_CPE "cpe:2.3:a:c-blosc2_project:c-blosc2:${BLOSC_VERSION}:*:*:*:*:*:*:*")

set(PTHREADS_VERSION 3.0.0)
set(PTHREADS_URI http://prdownloads.sourceforge.net/pthreads4w/pthreads4w-code-v${PTHREADS_VERSION}.zip)
set(PTHREADS_HASH f3bf81bb395840b3446197bcf4ecd653)
set(PTHREADS_HASH_TYPE MD5)
set(PTHREADS_FILE pthreads4w-code-${PTHREADS_VERSION}.zip)

set(OPENEXR_VERSION 3.1.5)
set(OPENEXR_URI https://github.com/AcademySoftwareFoundation/openexr/archive/v${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH a92f38eedd43e56c0af56d4852506886)
set(OPENEXR_HASH_TYPE MD5)
set(OPENEXR_FILE openexr-${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_CPE "cpe:2.3:a:openexr:openexr:${OPENEXR_VERSION}:*:*:*:*:*:*:*")

set(IMATH_VERSION 3.1.5)
set(IMATH_URI https://github.com/AcademySoftwareFoundation/Imath/archive/v${OPENEXR_VERSION}.tar.gz)
set(IMATH_HASH dd375574276c54872b7b3d54053baff0)
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

set(FREETYPE_VERSION 2.12.1)
set(FREETYPE_URI http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH 8bc5c9c9df7ac12c504f8918552a7cf2)
set(FREETYPE_HASH_TYPE MD5)
set(FREETYPE_FILE freetype-${FREETYPE_VERSION}.tar.gz)
SET(FREETYPE_CPE "cpe:2.3:a:freetype:freetype:${FREETYPE_VERSION}:*:*:*:*:*:*:*")

set(EPOXY_VERSION 1.5.10)
set(EPOXY_URI https://github.com/anholt/libepoxy/archive/refs/tags/${EPOXY_VERSION}.tar.gz)
set(EPOXY_HASH f0730aad115c952e77591fcc805b1dc1)
set(EPOXY_HASH_TYPE MD5)
set(EPOXY_FILE libepoxy-${EPOXY_VERSION}.tar.gz)

set(FREEGLUT_VERSION 3.0.0)
set(FREEGLUT_URI http://prdownloads.sourceforge.net/freeglut/freeglut/${FREEGLUT_VERSION}/freeglut-${FREEGLUT_VERSION}.tar.gz)
set(FREEGLUT_HASH 90c3ca4dd9d51cf32276bc5344ec9754)
set(FREEGLUT_HASH_TYPE MD5)
set(FREEGLUT_FILE freeglut-${FREEGLUT_VERSION}.tar.gz)

set(ALEMBIC_VERSION 1.8.3)
set(ALEMBIC_URI https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.tar.gz)
set(ALEMBIC_HASH 2cd8d6e5a3ac4a014e24a4b04f4fadf9)
set(ALEMBIC_HASH_TYPE MD5)
set(ALEMBIC_FILE alembic-${ALEMBIC_VERSION}.tar.gz)
SET(FREETYPE_CPE "cpe:2.3:a:freetype:freetype:${FREETYPE_VERSION}:*:*:*:*:*:*:*")

set(OPENSUBDIV_VERSION v3_5_0)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_HASH 230f5cd2911d6240e58a3773b9c6e5e4)
set(OPENSUBDIV_HASH_TYPE MD5)
set(OPENSUBDIV_FILE opensubdiv-${OPENSUBDIV_VERSION}.tar.gz)

set(SDL_VERSION 2.0.20)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH a53acc02e1cca98c4123229069b67c9e)
set(SDL_HASH_TYPE MD5)
set(SDL_FILE SDL2-${SDL_VERSION}.tar.gz)
set(SDL_CPE "cpe:2.3:a:libsdl:sdl:${SDL_VERSION}:*:*:*:*:*:*:*")

set(OPENCOLLADA_VERSION v1.6.68)
set(OPENCOLLADA_URI https://github.com/KhronosGroup/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH ee7dae874019fea7be11613d07567493)
set(OPENCOLLADA_HASH_TYPE MD5)
set(OPENCOLLADA_FILE opencollada-${OPENCOLLADA_VERSION}.tar.gz)

set(OPENCOLORIO_VERSION 2.2.0)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH d58a5980adba2d89a363100a09daa5f3)
set(OPENCOLORIO_HASH_TYPE MD5)
set(OPENCOLORIO_FILE OpenColorIO-${OPENCOLORIO_VERSION}.tar.gz)

set(MINIZIPNG_VERSION 3.0.7)
set(MINIZIPNG_URI https://github.com/zlib-ng/minizip-ng/archive/${MINIZIPNG_VERSION}.tar.gz)
set(MINIZIPNG_HASH 09dcc8a9def348e1be9659e384c2cd55)
set(MINIZIPNG_HASH_TYPE MD5)
set(MINIZIPNG_FILE minizip-ng-${MINIZIPNG_VERSION}.tar.gz)

set(LLVM_VERSION 12.0.0)
set(LLVM_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH 5a4fab4d7fc84aefffb118ac2c8a4fc0)
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
  set(OPENMP_HASH ac48ce3e4582ccb82f81ab59eb3fc9dc)
endif()
set(OPENMP_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${OPENMP_VERSION}/openmp-${OPENMP_VERSION}.src.tar.xz)
set(OPENMP_HASH_TYPE MD5)
set(OPENMP_FILE openmp-${OPENMP_VERSION}.src.tar.xz)

set(OPENIMAGEIO_VERSION v2.4.6.0)
set(OPENIMAGEIO_URI https://github.com/OpenImageIO/oiio/archive/refs/tags/${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH c7acc1b9a8fda04ef48f7de1feda4dae)
set(OPENIMAGEIO_HASH_TYPE MD5)
set(OPENIMAGEIO_FILE OpenImageIO-${OPENIMAGEIO_VERSION}.tar.gz)

# 8.0.0 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(FMT_VERSION 8.0.0)
set(FMT_URI https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.tar.gz)
set(FMT_HASH 7bce0e9e022e586b178b150002e7c2339994e3c2bbe44027e9abb0d60f9cce83)
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

set(TIFF_VERSION 4.4.0)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH 376f17f189e9d02280dfe709b2b2bbea)
set(TIFF_HASH_TYPE MD5)
set(TIFF_FILE tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_CPE "cpe:2.3:a:libtiff:libtiff:${TIFF_VERSION}:*:*:*:*:*:*:*")

# Recent commit from 1.13.0.2 under development, which includes string table
# changes that make the Cycles OptiX implementation work. Official 1.12 OSL
# releases should also build but without OptiX support.
set(OSL_VERSION 1a7670600c8b08c2443a78d03c8c27e9a1149140)
set(OSL_URI https://github.com/AcademySoftwareFoundation/OpenShadingLanguage/archive/${OSL_VERSION}.tar.gz)
set(OSL_HASH 7b6d6716b05d1addb92a8f47280bf77f)
set(OSL_HASH_TYPE MD5)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)

# NOTE: When updating the python version, it's required to check the versions of
# it wants to use in PCbuild/get_externals.bat for the following dependencies:
# BZIP2, FFI, SQLITE and change the versions in this file as well. For compliance
# reasons there can be no exceptions to this.

set(PYTHON_VERSION 3.10.9)
set(PYTHON_SHORT_VERSION 3.10)
set(PYTHON_SHORT_VERSION_NO_DOTS 310)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH dc8c0f274b28ee9e95923d20cfc364c9)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_CPE "cpe:2.3:a:python:python:${PYTHON_VERSION}:-:*:*:*:*:*:*")

set(TBB_YEAR 2020)
set(TBB_VERSION ${TBB_YEAR}_U3)
set(TBB_URI https://github.com/oneapi-src/oneTBB/archive/${TBB_VERSION}.tar.gz)
set(TBB_HASH 55ec8df6eae5ed6364a47f0e671e460c)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)
set(TBB_CPE "cpe:2.3:a:intel:threading_building_blocks:${TBB_YEAR}:*:*:*:*:*:*:*")

set(OPENVDB_VERSION 10.0.0)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 64301c737e16b26c8f3085a31e6397e9)
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

set(NUMPY_VERSION 1.23.5)
set(NUMPY_SHORT_VERSION 1.23)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_HASH 8b2692a511a3795f3af8af2cd7566a15)
set(NUMPY_HASH_TYPE MD5)
set(NUMPY_FILE numpy-${NUMPY_VERSION}.tar.gz)
set(NUMPY_CPE "cpe:2.3:a:numpy:numpy:${NUMPY_VERSION}:*:*:*:*:*:*:*")

set(LAME_VERSION 3.100)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 83e260acbe4389b54fe08e0bdbf7cddb)
set(LAME_HASH_TYPE MD5)
set(LAME_FILE lame-${LAME_VERSION}.tar.gz)
set(LAME_CPE "cpe:2.3:a:lame_project:lame:${LAME_VERSION}:*:*:*:*:*:*:*")

set(OGG_VERSION 1.3.5)
set(OGG_URI http://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz)
set(OGG_HASH 0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664)
set(OGG_HASH_TYPE SHA256)
set(OGG_FILE libogg-${OGG_VERSION}.tar.gz)

set(VORBIS_VERSION 1.3.7)
set(VORBIS_URI http://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_HASH 0e982409a9c3fc82ee06e08205b1355e5c6aa4c36bca58146ef399621b0ce5ab)
set(VORBIS_HASH_TYPE SHA256)
set(VORBIS_FILE libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_CPE "cpe:2.3:a:xiph.org:libvorbis:${VORBIS_VERSION}:*:*:*:*:*:*:*")

set(THEORA_VERSION 1.1.1)
set(THEORA_URI http://downloads.xiph.org/releases/theora/libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HASH b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc)
set(THEORA_HASH_TYPE SHA256)
set(THEORA_FILE libtheora-${THEORA_VERSION}.tar.bz2)

set(FLAC_VERSION 1.3.4)
set(FLAC_URI http://downloads.xiph.org/releases/flac/flac-${FLAC_VERSION}.tar.xz)
set(FLAC_HASH 8ff0607e75a322dd7cd6ec48f4f225471404ae2730d0ea945127b1355155e737 )
set(FLAC_HASH_TYPE SHA256)
set(FLAC_FILE flac-${FLAC_VERSION}.tar.xz)
set(FLAC_CPE "cpe:2.3:a:flac_project:flac:${FLAC_VERSION}:*:*:*:*:*:*:*")

set(VPX_VERSION 1.11.0)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 965e51c91ad9851e2337aebcc0f517440c637c506f3a03948062e3d5ea129a83)
set(VPX_HASH_TYPE SHA256)
set(VPX_FILE libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_CPE "cpe:2.3:a:webmproject:libvpx:${VPX_VERSION}:*:*:*:*:*:*:*")

set(OPUS_VERSION 1.3.1)
set(OPUS_URI https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HASH 65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d)
set(OPUS_HASH_TYPE SHA256)
set(OPUS_FILE opus-${OPUS_VERSION}.tar.gz)

set(X264_VERSION 35fe20d1ba49918ec739a5b068c208ca82f977f7)
set(X264_URI https://code.videolan.org/videolan/x264/-/archive/${X264_VERSION}/x264-${X264_VERSION}.tar.gz)
set(X264_HASH bb4f7da03936b5a030ed5827133b58eb3f701d7e5dce32cca4ba6df93797d42e)
set(X264_HASH_TYPE SHA256)
set(X264_FILE x264-${X264_VERSION}.tar.gz)

set(XVIDCORE_VERSION 1.3.7)
set(XVIDCORE_URI https://downloads.xvid.com/downloads/xvidcore-${XVIDCORE_VERSION}.tar.gz)
set(XVIDCORE_HASH abbdcbd39555691dd1c9b4d08f0a031376a3b211652c0d8b3b8aa9be1303ce2d)
set(XVIDCORE_HASH_TYPE SHA256)
set(XVIDCORE_FILE xvidcore-${XVIDCORE_VERSION}.tar.gz)

set(OPENJPEG_VERSION 2.5.0)
set(OPENJPEG_SHORT_VERSION 2.5)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 0333806d6adecc6f7a91243b2b839ff4d2053823634d4f6ed7a59bc87409122a)
set(OPENJPEG_HASH_TYPE SHA256)
set(OPENJPEG_FILE openjpeg-v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_CPE "cpe:2.3:a:uclouvain:openjpeg:${OPENJPEG_VERSION}:*:*:*:*:*:*:*")

set(FFMPEG_VERSION 5.1.2)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH 39a0bcc8d98549f16c570624678246a6ac736c066cebdb409f9502e915b22f2b)
set(FFMPEG_HASH_TYPE SHA256)
set(FFMPEG_FILE ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_CPE "cpe:2.3:a:ffmpeg:ffmpeg:${FFMPEG_VERSION}:*:*:*:*:*:*:*")

set(FFTW_VERSION 3.3.10)
set(FFTW_URI http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HASH 8ccbf6a5ea78a16dbc3e1306e234cc5c)
set(FFTW_HASH_TYPE MD5)
set(FFTW_FILE fftw-${FFTW_VERSION}.tar.gz)

set(ICONV_VERSION 1.16)
set(ICONV_URI http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HASH 7d2a800b952942bb2880efb00cfd524c)
set(ICONV_HASH_TYPE MD5)
set(ICONV_FILE libiconv-${ICONV_VERSION}.tar.gz)

set(SNDFILE_VERSION 1.1.0)
set(SNDFILE_URI https://github.com/libsndfile/libsndfile/releases/download/1.1.0/libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_HASH e63dead2b4f0aaf323687619d007ee6a)
set(SNDFILE_HASH_TYPE MD5)
set(SNDFILE_FILE libsndfile-${SNDFILE_VERSION}.tar.xz)
set(SNDFILE_CPE "cpe:2.3:a:libsndfile_project:libsndfile:${SNDFILE_VERSION}:*:*:*:*:*:*:*")

set(WEBP_VERSION 1.2.2)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH b5e2e414a8adee4c25fe56b18dd9c549)
set(WEBP_HASH_TYPE MD5)
set(WEBP_FILE libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_CPE "cpe:2.3:a:webmproject:libwebp:${WEBP_VERSION}:*:*:*:*:*:*:*")

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

set(XML2_VERSION 2.10.3)
set(XML2_URI https://download.gnome.org/sources/libxml2/2.10/libxml2-${XML2_VERSION}.tar.xz)
set(XML2_HASH f9edac7fac232b3657a003fd9a5bbe42)
set(XML2_HASH_TYPE MD5)
set(XML2_FILE libxml2-${XML2_VERSION}.tar.xz)
set(XML2_CPE "cpe:2.3:a:xmlsoft:libxml2:${XML2_VERSION}:*:*:*:*:*:*:*")

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

# NOTE: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat. For compliance reasons there
# can be no exceptions to this.
set(FFI_VERSION 3.3)
set(FFI_URI https://sourceware.org/pub/libffi/libffi-${FFI_VERSION}.tar.gz)
set(FFI_HASH 72fba7922703ddfa7a028d513ac15a85c8d54c8d67f55fa5a4802885dc652056)
set(FFI_HASH_TYPE SHA256)
set(FFI_FILE libffi-${FFI_VERSION}.tar.gz)
set(FFI_CPE "cpe:2.3:a:libffi_project:libffi:${FFI_VERSION}:*:*:*:*:*:*:*")

set(LZMA_VERSION 5.2.5)
set(LZMA_URI https://tukaani.org/xz/xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_HASH 5117f930900b341493827d63aa910ff5e011e0b994197c3b71c08a20228a42df)
set(LZMA_HASH_TYPE SHA256)
set(LZMA_FILE xz-${LZMA_VERSION}.tar.bz2)

# NOTE: Python's build has been modified to use our ssl version.
set(SSL_VERSION 1.1.1q)
set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
set(SSL_HASH d7939ce614029cdff0b6c20f0e2e5703158a489a72b2507b8bd51bf8c8fd10ca)
set(SSL_HASH_TYPE SHA256)
set(SSL_FILE openssl-${SSL_VERSION}.tar.gz)
set(SSL_CPE "cpe:2.3:a:openssl:openssl:${SSL_VERSION}:*:*:*:*:*:*:*")

# Note: This will *HAVE* to match the version python ships on windows which
# is hardcoded in pythons PCbuild/get_externals.bat for compliance reasons there
# can be no exceptions to this.
set(SQLITE_VERSION 3.39.4)
set(SQLLITE_LONG_VERSION 3390400)
set(SQLITE_URI https://www.sqlite.org/2022/sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_HASH c4c5c39269d1b9bb1487cff580c1f583608229b2)
set(SQLITE_HASH_TYPE SHA1)
set(SQLITE_FILE sqlite-autoconf-${SQLLITE_LONG_VERSION}.tar.gz)
set(SQLITE_CPE "cpe:2.3:a:sqlite:sqlite:${SQLITE_VERSION}:*:*:*:*:*:*:*")

set(EMBREE_VERSION 3.13.4)
set(EMBREE_URI https://github.com/embree/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 52d0be294d6c88ba7a6c9e046796e7be)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)

set(USD_VERSION 22.11)
set(USD_URI https://github.com/PixarAnimationStudios/USD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH 8c89459e48a2ef0e7ae9e7e490377507)
set(USD_HASH_TYPE MD5)
set(USD_FILE usd-v${USD_VERSION}.tar.gz)

set(MATERIALX_VERSION 1.38.6)
set(MATERIALX_URI https://github.com/AcademySoftwareFoundation/MaterialX/archive/refs/tags/v${MATERIALX_VERSION}.tar.gz)
set(MATERIALX_HASH d49c9fdef34b5702fc60058d3e1864f2)
set(MATERIALX_HASH_TYPE MD5)
set(MATERIALX_FILE materialx-v${MATERIALX_VERSION}.tar.gz)

set(OIDN_VERSION 1.4.3)
set(OIDN_URI https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HASH 027093eaf5e8b4e45835b991137b38e6)
set(OIDN_HASH_TYPE MD5)
set(OIDN_FILE oidn-${OIDN_VERSION}.src.tar.gz)

set(LIBGLU_VERSION 9.0.1)
set(LIBGLU_URI ftp://ftp.freedesktop.org/pub/mesa/glu/glu-${LIBGLU_VERSION}.tar.xz)
set(LIBGLU_HASH 151aef599b8259efe9acd599c96ea2a3)
set(LIBGLU_HASH_TYPE MD5)
set(LIBGLU_FILE glu-${LIBGLU_VERSION}.tar.xz)

set(MESA_VERSION 21.1.5)
set(MESA_URI ftp://ftp.freedesktop.org/pub/mesa/mesa-${MESA_VERSION}.tar.xz)
set(MESA_HASH 022c7293074aeeced2278c872db4fa693147c70f8595b076cf3f1ef81520766d)
set(MESA_HASH_TYPE SHA256)
set(MESA_FILE mesa-${MESA_VERSION}.tar.xz)
set(MESA_CPE "cpe:2.3:a:mesa3d:mesa:${MESA_VERSION}:*:*:*:*:*:*:*")

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

set(WL_PROTOCOLS_VERSION 1.21)
set(WL_PROTOCOLS_FILE wayland-protocols-${WL_PROTOCOLS_VERSION}.tar.gz)
set(WL_PROTOCOLS_URI https://gitlab.freedesktop.org/wayland/wayland-protocols/-/archive/${WL_PROTOCOLS_VERSION}/${WL_PROTOCOLS_FILE})
set(WL_PROTOCOLS_HASH af5ca07e13517cdbab33504492cef54a)
set(WL_PROTOCOLS_HASH_TYPE MD5)

set(WAYLAND_VERSION 1.21.0)
set(WAYLAND_FILE wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_URI https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.21.0/downloads/wayland-${WAYLAND_VERSION}.tar.xz)
set(WAYLAND_HASH f2653a2293bcd882d756c6a83d278903)
set(WAYLAND_HASH_TYPE MD5)

set(WAYLAND_LIBDECOR_VERSION 0.1.0)
set(WAYLAND_LIBDECOR_FILE libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz)
set(WAYLAND_LIBDECOR_URI https://gitlab.gnome.org/jadahl/libdecor/uploads/81adf91d27620e20bcc5f6b9b312d768/libdecor-${WAYLAND_LIBDECOR_VERSION}.tar.xz )
set(WAYLAND_LIBDECOR_HASH 47b59eba76faa3787f0878bf8700e912)
set(WAYLAND_LIBDECOR_HASH_TYPE MD5)

set(ISPC_VERSION v1.17.0)
set(ISPC_URI https://github.com/ispc/ispc/archive/${ISPC_VERSION}.tar.gz)
set(ISPC_HASH 4f476a3109332a77fe839a9014c60ca9)
set(ISPC_HASH_TYPE MD5)
set(ISPC_FILE ispc-${ISPC_VERSION}.tar.gz)

set(GMP_VERSION 6.2.1)
set(GMP_URI https://gmplib.org/download/gmp/gmp-${GMP_VERSION}.tar.xz)
set(GMP_HASH 0b82665c4a92fd2ade7440c13fcaa42b)
set(GMP_HASH_TYPE MD5)
set(GMP_FILE gmp-${GMP_VERSION}.tar.xz)
set(GMP_CPE "cpe:2.3:a:gmplib:gmp:${GMP_VERSION}:*:*:*:*:*:*:*")

set(POTRACE_VERSION 1.16)
set(POTRACE_URI http://potrace.sourceforge.net/download/${POTRACE_VERSION}/potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_HASH 5f0bd87ddd9a620b0c4e65652ef93d69)
set(POTRACE_HASH_TYPE MD5)
set(POTRACE_FILE potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_CPE "cpe:2.3:a:icoasoft:potrace:${POTRACE_VERSION}:*:*:*:*:*:*:*")

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

set(SSE2NEON_VERSION fe5ff00bb8d19b327714a3c290f3e2ce81ba3525)
set(SSE2NEON_URI https://github.com/DLTcollab/sse2neon/archive/${SSE2NEON_VERSION}.tar.gz)
set(SSE2NEON_HASH 0780253525d299c31775ef95853698d03db9c7739942af8570000f4a25a5d605)
set(SSE2NEON_HASH_TYPE SHA256)
set(SSE2NEON_FILE sse2neon-${SSE2NEON_VERSION}.tar.gz)

set(BROTLI_VERSION 1.0.9)
set(BROTLI_URI https://github.com/google/brotli/archive/refs/tags/v${BROTLI_VERSION}.tar.gz)
set(BROTLI_HASH f9e8d81d0405ba66d181529af42a3354f838c939095ff99930da6aa9cdf6fe46)
set(BROTLI_HASH_TYPE SHA256)
set(BROTLI_FILE brotli-v${BROTLI_VERSION}.tar.gz)
set(BROTLI_CPE "cpe:2.3:a:google:brotli:${BROTLI_VERSION}:*:*:*:*:*:*:*")

set(OPENPGL_VERSION v0.4.1-beta)
set(OPENPGL_SHORT_VERSION 0.4.1)
set(OPENPGL_URI https://github.com/OpenPathGuidingLibrary/openpgl/archive/refs/tags/${OPENPGL_VERSION}.tar.gz)
set(OPENPGL_HASH db63f5dac5cfa8c110ede241f0c413f00db0c4748697381c4fa23e0f9e82a754)
set(OPENPGL_HASH_TYPE SHA256)
set(OPENPGL_FILE openpgl-${OPENPGL_VERSION}.tar.gz)

set(LEVEL_ZERO_VERSION v1.8.5)
set(LEVEL_ZERO_URI https://github.com/oneapi-src/level-zero/archive/refs/tags/${LEVEL_ZERO_VERSION}.tar.gz)
set(LEVEL_ZERO_HASH b6e9663bbcc53c148d32376998298bec6f7c434ef2218c61fa708963e3a09394)
set(LEVEL_ZERO_HASH_TYPE SHA256)
set(LEVEL_ZERO_FILE level-zero-${LEVEL_ZERO_VERSION}.tar.gz)

set(DPCPP_VERSION 20221019)
set(DPCPP_URI https://github.com/intel/llvm/archive/refs/tags/sycl-nightly/${DPCPP_VERSION}.tar.gz)
set(DPCPP_HASH 2f533946e91ce3829431758ea17b0b834b960c1a796e9e4563c86e03eb9603a2)
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
set(VCINTRINSICS_VERSION abce9184b7a3a7fe1b02289b9285610d9dc45465)
set(VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HASH 3e9fd471246b87633b26f7e15e17ab7733d357458c53d5c5881c03929d6c551f)
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

######################
### DPCPP DEPS END ###
######################

##########################################
### Intel Graphics Compiler DEPS BEGIN ###
##########################################
# The following deps are build time requirements for the intel graphics
# compiler, the versions used are taken from the following location
# https://github.com/intel/intel-graphics-compiler/releases

set(IGC_VERSION 1.0.12149.1)
set(IGC_URI https://github.com/intel/intel-graphics-compiler/archive/refs/tags/igc-${IGC_VERSION}.tar.gz)
set(IGC_HASH 44f67f24e3bc5130f9f062533abf8154782a9d0a992bc19b498639a8521ae836)
set(IGC_HASH_TYPE SHA256)
set(IGC_FILE igc-${IGC_VERSION}.tar.gz)

set(IGC_LLVM_VERSION llvmorg-11.1.0)
set(IGC_LLVM_URI https://github.com/llvm/llvm-project/archive/refs/tags/${IGC_LLVM_VERSION}.tar.gz)
set(IGC_LLVM_HASH 53a0719f3f4b0388013cfffd7b10c7d5682eece1929a9553c722348d1f866e79)
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

set(IGC_OPENCL_CLANG_VERSION 363a5262d8c7cff3fb28f3bdb5d85c8d7e91c1bb)
set(IGC_OPENCL_CLANG_URI https://github.com/intel/opencl-clang/archive/${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HASH aa8cf72bb239722ce8ce44f79413c6887ecc8ca18477dd520aa5c4809756da9a)
set(IGC_OPENCL_CLANG_HASH_TYPE SHA256)
set(IGC_OPENCL_CLANG_FILE opencl-clang-${IGC_OPENCL_CLANG_VERSION}.tar.gz)

set(IGC_VCINTRINSICS_VERSION v0.5.0)
set(IGC_VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/refs/tags/${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_HASH 70bb47c5e32173cf61514941e83ae7c7eb4485e6d2fca60cfa1f50d4f42c41f2)
set(IGC_VCINTRINSICS_HASH_TYPE SHA256)
set(IGC_VCINTRINSICS_FILE vc-intrinsics-${IGC_VCINTRINSICS_VERSION}.tar.gz)

set(IGC_SPIRV_HEADERS_VERSION sdk-1.3.204.1)
set(IGC_SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/${IGC_SPIRV_HEADERS_VERSION}.tar.gz)
set(IGC_SPIRV_HEADERS_HASH 262864053968c217d45b24b89044a7736a32361894743dd6cfe788df258c746c)
set(IGC_SPIRV_HEADERS_HASH_TYPE SHA256)
set(IGC_SPIRV_HEADERS_FILE SPIR-V-Headers-${IGC_SPIRV_HEADERS_VERSION}.tar.gz)

set(IGC_SPIRV_TOOLS_VERSION sdk-1.3.204.1)
set(IGC_SPIRV_TOOLS_URI https://github.com/KhronosGroup/SPIRV-Tools/archive/refs/tags/${IGC_SPIRV_TOOLS_VERSION}.tar.gz)
set(IGC_SPIRV_TOOLS_HASH 6e19900e948944243024aedd0a201baf3854b377b9cc7a386553bc103b087335)
set(IGC_SPIRV_TOOLS_HASH_TYPE SHA256)
set(IGC_SPIRV_TOOLS_FILE SPIR-V-Tools-${IGC_SPIRV_TOOLS_VERSION}.tar.gz)

set(IGC_SPIRV_TRANSLATOR_VERSION a31ffaeef77e23d500b3ea3d35e0c42ff5648ad9)
set(IGC_SPIRV_TRANSLATOR_URI https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_HASH 9e26c96a45341b8f8af521bacea20e752623346340addd02af95d669f6e89252)
set(IGC_SPIRV_TRANSLATOR_HASH_TYPE SHA256)
set(IGC_SPIRV_TRANSLATOR_FILE SPIR-V-Translator-${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)

########################################
### Intel Graphics Compiler DEPS END ###
########################################

set(GMMLIB_VERSION intel-gmmlib-22.1.8)
set(GMMLIB_URI https://github.com/intel/gmmlib/archive/refs/tags/${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HASH bf23e9a3742b4fb98c7666c9e9b29f3219e4b2fb4d831aaf4eed71f5e2d17368)
set(GMMLIB_HASH_TYPE SHA256)
set(GMMLIB_FILE ${GMMLIB_VERSION}.tar.gz)

set(OCLOC_VERSION 22.38.24278)
set(OCLOC_URI https://github.com/intel/compute-runtime/archive/refs/tags/${OCLOC_VERSION}.tar.gz)
set(OCLOC_HASH db0c542fccd651e6404b15a74d46027f1ce0eda8dc9e25a40cbb6c0faef257ee)
set(OCLOC_HASH_TYPE SHA256)
set(OCLOC_FILE ocloc-${OCLOC_VERSION}.tar.gz)

set(AOM_VERSION 3.4.0)
set(AOM_URI https://storage.googleapis.com/aom-releases/libaom-${AOM_VERSION}.tar.gz)
set(AOM_HASH bd754b58c3fa69f3ffd29da77de591bd9c26970e3b18537951336d6c0252e354)
set(AOM_HASH_TYPE SHA256)
set(AOM_FILE libaom-${AOM_VERSION}.tar.gz)

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

set(VULKAN_VERSION v1.2.198)

set(VULKAN_HEADERS_URI https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/${VULKAN_VERSION}.tar.gz)
set(VULKAN_HEADERS_HASH 64fe73e887c963ad546bfc7f9505fa1d)
set(VULKAN_HEADERS_HASH_TYPE MD5)
set(VULKAN_HEADERS_FILE Vulkan-Headers-${VULKAN_VERSION}.tar.gz)

set(VULKAN_LOADER_URI https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/${VULKAN_VERSION}.tar.gz)
set(VULKAN_LOADER_HASH 015170a74f648fd2b41e209b6bf1ebc4)
set(VULKAN_LOADER_HASH_TYPE MD5)
set(VULKAN_LOADER_FILE Vulkan-Loader-${VULKAN_VERSION}.tar.gz)

set(PYBIND11_VERSION 2.10.1)
set(PYBIND11_URI https://github.com/pybind/pybind11/archive/refs/tags/v${PYBIND11_VERSION}.tar.gz)
set(PYBIND11_HASH ce07bfd5089245da7807b3faf6cbc878)
set(PYBIND11_HASH_TYPE MD5)
set(PYBIND11_FILE pybind-v${PYBIND11_VERSION}.tar.gz)
