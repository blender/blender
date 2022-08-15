# SPDX-License-Identifier: GPL-2.0-or-later

set(ZLIB_VERSION 1.2.12)
set(ZLIB_URI https://zlib.net/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 5fc414a9726be31427b440b434d05f78)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)

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

set(JPEG_VERSION 2.1.3)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 627b980fad0573e08e4c3b80b290fc91)
set(JPEG_HASH_TYPE MD5)
set(JPEG_FILE libjpeg-turbo-${JPEG_VERSION}.tar.gz)

set(BOOST_VERSION 1.78.0)
set(BOOST_VERSION_SHORT 1.78)
set(BOOST_VERSION_NODOTS 1_78_0)
set(BOOST_VERSION_NODOTS_SHORT 1_78)
set(BOOST_URI https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_HASH c2f6428ac52b0e5a3c9b2e1d8cc832b5)
set(BOOST_HASH_TYPE MD5)
set(BOOST_FILE boost_${BOOST_VERSION_NODOTS}.tar.gz)

set(BLOSC_VERSION 1.21.1)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.tar.gz)
set(BLOSC_HASH 134b55813b1dca57019d2a2dc1f7a923)
set(BLOSC_HASH_TYPE MD5)
set(BLOSC_FILE blosc-${BLOSC_VERSION}.tar.gz)

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
    set(OPENEXR_VERSION_POSTFIX _s)
    set(OPENEXR_VERSION_BUILD_POSTFIX _s)
  else()
    set(OPENEXR_VERSION_POSTFIX _s_d)
    set(OPENEXR_VERSION_BUILD_POSTFIX _s)
  endif()
else()
  set(OPENEXR_VERSION_BUILD_POSTFIX)
  set(OPENEXR_VERSION_POSTFIX)
endif()

set(FREETYPE_VERSION 2.11.1)
set(FREETYPE_URI http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH bd4e3b007474319909a6b79d50908e85)
set(FREETYPE_HASH_TYPE MD5)
set(FREETYPE_FILE freetype-${FREETYPE_VERSION}.tar.gz)

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

set(OPENSUBDIV_VERSION v3_4_4)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_HASH 39ecc5caf0abebc943d1ce131855e76e)
set(OPENSUBDIV_HASH_TYPE MD5)
set(OPENSUBDIV_FILE opensubdiv-${OPENSUBDIV_VERSION}.tar.gz)

set(SDL_VERSION 2.0.20)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH a53acc02e1cca98c4123229069b67c9e)
set(SDL_HASH_TYPE MD5)
set(SDL_FILE SDL2-${SDL_VERSION}.tar.gz)

set(OPENCOLLADA_VERSION v1.6.68)
set(OPENCOLLADA_URI https://github.com/KhronosGroup/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH ee7dae874019fea7be11613d07567493)
set(OPENCOLLADA_HASH_TYPE MD5)
set(OPENCOLLADA_FILE opencollada-${OPENCOLLADA_VERSION}.tar.gz)

set(OPENCOLORIO_VERSION 2.1.1)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH 604f562e073f23d88ce89ed4f7f709ba)
set(OPENCOLORIO_HASH_TYPE MD5)
set(OPENCOLORIO_FILE OpenColorIO-${OPENCOLORIO_VERSION}.tar.gz)

set(LLVM_VERSION 12.0.0)
set(LLVM_URI https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH 5a4fab4d7fc84aefffb118ac2c8a4fc0)
set(LLVM_HASH_TYPE MD5)
set(LLVM_FILE llvm-project-${LLVM_VERSION}.src.tar.xz)

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

set(OPENIMAGEIO_VERSION v2.3.13.0)
set(OPENIMAGEIO_URI https://github.com/OpenImageIO/oiio/archive/refs/tags/${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH de45fb38501c4581062b522b53b6141c)
set(OPENIMAGEIO_HASH_TYPE MD5)
set(OPENIMAGEIO_FILE OpenImageIO-${OPENIMAGEIO_VERSION}.tar.gz)

# 8.0.0 is currently oiio's preferred version although never versions may be available.
# the preferred version can be found in oiio's externalpackages.cmake
set(FMT_VERSION 8.0.0)
set(FMT_URI https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.tar.gz)
set(FMT_HASH 7bce0e9e022e586b178b150002e7c2339994e3c2bbe44027e9abb0d60f9cce83)
set(FMT_HASH_TYPE SHA256)
set(FMT_FILE fmt-${FMT_VERSION}.tar.gz)

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

set(OSL_VERSION 1.11.17.0)
set(OSL_URI https://github.com/imageworks/OpenShadingLanguage/archive/Release-${OSL_VERSION}.tar.gz)
set(OSL_HASH 63265472ce14548839ace2e21e401544)
set(OSL_HASH_TYPE MD5)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)

set(PYTHON_VERSION 3.10.2)
set(PYTHON_SHORT_VERSION 3.10)
set(PYTHON_SHORT_VERSION_NO_DOTS 310)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH 14e8c22458ed7779a1957b26cde01db9)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)

set(TBB_VERSION 2020_U3)
set(TBB_URI https://github.com/oneapi-src/oneTBB/archive/${TBB_VERSION}.tar.gz)
set(TBB_HASH 55ec8df6eae5ed6364a47f0e671e460c)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)

set(OPENVDB_VERSION 9.0.0)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 684ce40c2f74f3a0c9cac530e1c7b07e)
set(OPENVDB_HASH_TYPE MD5)
set(OPENVDB_FILE openvdb-${OPENVDB_VERSION}.tar.gz)

set(IDNA_VERSION 3.3)
set(CHARSET_NORMALIZER_VERSION 2.0.10)
set(URLLIB3_VERSION 1.26.8)
set(CERTIFI_VERSION 2021.10.8)
set(REQUESTS_VERSION 2.27.1)
set(CYTHON_VERSION 0.29.26)
# The version of the zstd library used to build the Python package should match ZSTD_VERSION defined below.
# At this time of writing, 0.17.0 was already released, but built against zstd 1.5.1, while we use 1.5.0.
set(ZSTANDARD_VERSION 0.16.0)
set(AUTOPEP8_VERSION 1.6.0)
set(PYCODESTYLE_VERSION 2.8.0)
set(TOML_VERSION 0.10.2)

set(NUMPY_VERSION 1.22.0)
set(NUMPY_SHORT_VERSION 1.22)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.zip)
set(NUMPY_HASH 252de134862a27bd66705d29622edbfe)
set(NUMPY_HASH_TYPE MD5)
set(NUMPY_FILE numpy-${NUMPY_VERSION}.zip)

set(LAME_VERSION 3.100)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 83e260acbe4389b54fe08e0bdbf7cddb)
set(LAME_HASH_TYPE MD5)
set(LAME_FILE lame-${LAME_VERSION}.tar.gz)

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

set(VPX_VERSION 1.11.0)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 965e51c91ad9851e2337aebcc0f517440c637c506f3a03948062e3d5ea129a83)
set(VPX_HASH_TYPE SHA256)
set(VPX_FILE libvpx-v${VPX_VERSION}.tar.gz)

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

set(OPENJPEG_VERSION 2.4.0)
set(OPENJPEG_SHORT_VERSION 2.4)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 8702ba68b442657f11aaeb2b338443ca8d5fb95b0d845757968a7be31ef7f16d)
set(OPENJPEG_HASH_TYPE SHA256)
set(OPENJPEG_FILE openjpeg-v${OPENJPEG_VERSION}.tar.gz)

set(FFMPEG_VERSION 5.0)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH c0130b8db2c763430fd1c6905288d61bc44ee0548ad5fcd2dfd650b88432bed9)
set(FFMPEG_HASH_TYPE SHA256)
set(FFMPEG_FILE ffmpeg-${FFMPEG_VERSION}.tar.bz2)

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

set(SNDFILE_VERSION 1.0.28)
set(SNDFILE_URI http://www.mega-nerd.com/libsndfile/files/libsndfile-${SNDFILE_VERSION}.tar.gz)
set(SNDFILE_HASH 646b5f98ce89ac60cdb060fcd398247c)
set(SNDFILE_HASH_TYPE MD5)
set(SNDFILE_FILE libsndfile-${SNDFILE_VERSION}.tar.gz)

set(WEBP_VERSION 1.2.2)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH b5e2e414a8adee4c25fe56b18dd9c549)
set(WEBP_HASH_TYPE MD5)
set(WEBP_FILE libwebp-${WEBP_VERSION}.tar.gz)

set(SPNAV_VERSION 0.2.3)
set(SPNAV_URI http://downloads.sourceforge.net/project/spacenav/spacenav%20library%20%28SDK%29/libspnav%20${SPNAV_VERSION}/libspnav-${SPNAV_VERSION}.tar.gz)
set(SPNAV_HASH 44d840540d53326d4a119c0f1aa7bf0a)
set(SPNAV_HASH_TYPE MD5)
set(SPNAV_FILE libspnav-${SPNAV_VERSION}.tar.gz)

set(JEMALLOC_VERSION 5.2.1)
set(JEMALLOC_URI https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2)
set(JEMALLOC_HASH 3d41fbf006e6ebffd489bdb304d009ae)
set(JEMALLOC_HASH_TYPE MD5)
set(JEMALLOC_FILE jemalloc-${JEMALLOC_VERSION}.tar.bz2)

set(XML2_VERSION 2.9.10)
set(XML2_URI http://xmlsoft.org/sources/libxml2-${XML2_VERSION}.tar.gz)
set(XML2_HASH 10942a1dc23137a8aa07f0639cbfece5)
set(XML2_HASH_TYPE MD5)
set(XML2_FILE libxml2-${XML2_VERSION}.tar.gz)

set(TINYXML_VERSION 2_6_2)
set(TINYXML_VERSION_DOTS 2.6.2)
set(TINYXML_URI https://nchc.dl.sourceforge.net/project/tinyxml/tinyxml/${TINYXML_VERSION_DOTS}/tinyxml_${TINYXML_VERSION}.tar.gz)
set(TINYXML_HASH c1b864c96804a10526540c664ade67f0)
set(TINYXML_HASH_TYPE MD5)
set(TINYXML_FILE tinyxml_${TINYXML_VERSION}.tar.gz)

set(YAMLCPP_VERSION 0.6.3)
set(YAMLCPP_URI https://codeload.github.com/jbeder/yaml-cpp/tar.gz/yaml-cpp-${YAMLCPP_VERSION})
set(YAMLCPP_HASH b45bf1089a382e81f6b661062c10d0c2)
set(YAMLCPP_HASH_TYPE MD5)
set(YAMLCPP_FILE yaml-cpp-${YAMLCPP_VERSION}.tar.gz)

set(PYSTRING_VERSION v1.1.3)
set(PYSTRING_URI https://codeload.github.com/imageworks/pystring/tar.gz/refs/tags/${PYSTRING_VERSION})
set(PYSTRING_HASH f2c68786b359f5e4e62bed53bc4fb86d)
set(PYSTRING_HASH_TYPE MD5)
set(PYSTRING_FILE pystring-${PYSTRING_VERSION}.tar.gz)

set(EXPAT_VERSION 2_4_4)
set(EXPAT_URI https://github.com/libexpat/libexpat/archive/R_${EXPAT_VERSION}.tar.gz)
set(EXPAT_HASH 2d3e81dee94b452369dc6394ff0f8f98)
set(EXPAT_HASH_TYPE MD5)
set(EXPAT_FILE libexpat-${EXPAT_VERSION}.tar.gz)

set(PUGIXML_VERSION 1.10)
set(PUGIXML_URI https://github.com/zeux/pugixml/archive/v${PUGIXML_VERSION}.tar.gz)
set(PUGIXML_HASH 0c208b0664c7fb822bf1b49ad035e8fd)
set(PUGIXML_HASH_TYPE MD5)
set(PUGIXML_FILE pugixml-${PUGIXML_VERSION}.tar.gz)

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
set(BZIP2_VERSION 1.0.8)
set(BZIP2_URI http://http.debian.net/debian/pool/main/b/bzip2/bzip2_${BZIP2_VERSION}.orig.tar.gz)
set(BZIP2_HASH ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269)
set(BZIP2_HASH_TYPE SHA256)
set(BZIP2_FILE bzip2_${BZIP2_VERSION}.orig.tar.gz)

set(FFI_VERSION 3.3)
set(FFI_URI https://sourceware.org/pub/libffi/libffi-${FFI_VERSION}.tar.gz)
set(FFI_HASH 72fba7922703ddfa7a028d513ac15a85c8d54c8d67f55fa5a4802885dc652056)
set(FFI_HASH_TYPE SHA256)
set(FFI_FILE libffi-${FFI_VERSION}.tar.gz)

set(LZMA_VERSION 5.2.5)
set(LZMA_URI https://tukaani.org/xz/xz-${LZMA_VERSION}.tar.bz2)
set(LZMA_HASH 5117f930900b341493827d63aa910ff5e011e0b994197c3b71c08a20228a42df)
set(LZMA_HASH_TYPE SHA256)
set(LZMA_FILE xz-${LZMA_VERSION}.tar.bz2)

if(BLENDER_PLATFORM_ARM)
  # Need at least 1.1.1i for aarch64 support (https://github.com/openssl/openssl/pull/13218)
  set(SSL_VERSION 1.1.1i)
  set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
  set(SSL_HASH e8be6a35fe41d10603c3cc635e93289ed00bf34b79671a3a4de64fcee00d5242)
  set(SSL_HASH_TYPE SHA256)
  set(SSL_FILE openssl-${SSL_VERSION}.tar.gz)
else()
  set(SSL_VERSION 1.1.1g)
  set(SSL_URI https://www.openssl.org/source/openssl-${SSL_VERSION}.tar.gz)
  set(SSL_HASH ddb04774f1e32f0c49751e21b67216ac87852ceb056b75209af2443400636d46)
  set(SSL_HASH_TYPE SHA256)
  set(SSL_FILE openssl-${SSL_VERSION}.tar.gz)
endif()

set(SQLITE_VERSION 3.31.1)
set(SQLITE_URI https://www.sqlite.org/2018/sqlite-src-3240000.zip)
set(SQLITE_HASH fb558c49ee21a837713c4f1e7e413309aabdd9c7)
set(SQLITE_HASH_TYPE SHA1)
set(SQLITE_FILE sqlite-src-3240000.zip)

set(EMBREE_VERSION 3.13.4)
set(EMBREE_URI https://github.com/embree/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 52d0be294d6c88ba7a6c9e046796e7be)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)

set(USD_VERSION 22.03)
set(USD_URI https://github.com/PixarAnimationStudios/USD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH e0e441a05057692a83124a1195b09eed)
set(USD_HASH_TYPE MD5)
set(USD_FILE usd-v${USD_VERSION}.tar.gz)

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

set(NASM_VERSION 2.15.02)
set(NASM_URI https://github.com/netwide-assembler/nasm/archive/nasm-${NASM_VERSION}.tar.gz)
set(NASM_HASH aded8b796c996a486a56e0515c83e414116decc3b184d88043480b32eb0a8589)
set(NASM_HASH_TYPE SHA256)
set(NASM_FILE nasm-${NASM_VERSION}.tar.gz)

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

set(POTRACE_VERSION 1.16)
set(POTRACE_URI http://potrace.sourceforge.net/download/${POTRACE_VERSION}/potrace-${POTRACE_VERSION}.tar.gz)
set(POTRACE_HASH 5f0bd87ddd9a620b0c4e65652ef93d69)
set(POTRACE_HASH_TYPE MD5)
set(POTRACE_FILE potrace-${POTRACE_VERSION}.tar.gz)

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

set(SSE2NEON_GIT https://github.com/DLTcollab/sse2neon.git)
set(SSE2NEON_GIT_HASH fe5ff00bb8d19b327714a3c290f3e2ce81ba3525)

set(BROTLI_VERSION v1.0.9)
set(BROTLI_URI https://github.com/google/brotli/archive/refs/tags/${BROTLI_VERSION}.tar.gz)
set(BROTLI_HASH f9e8d81d0405ba66d181529af42a3354f838c939095ff99930da6aa9cdf6fe46)
set(BROTLI_HASH_TYPE SHA256)
set(BROTLI_FILE brotli-${BROTLI_VERSION}.tar.gz)

set(LEVEL_ZERO_VERSION v1.7.15)
set(LEVEL_ZERO_URI https://github.com/oneapi-src/level-zero/archive/refs/tags/${LEVEL_ZERO_VERSION}.tar.gz)
set(LEVEL_ZERO_HASH c39bb05a8e5898aa6c444e1704105b93d3f1888b9c333f8e7e73825ffbfb2617)
set(LEVEL_ZERO_HASH_TYPE SHA256)
set(LEVEL_ZERO_FILE level-zero-${LEVEL_ZERO_VERSION}.tar.gz)

set(DPCPP_VERSION 20220620)
set(DPCPP_URI https://github.com/intel/llvm/archive/refs/tags/sycl-nightly/${DPCPP_VERSION}.tar.gz)
set(DPCPP_HASH a5f41abd5229d28afa92cbd8a5d8d786ee698bf239f722929fd686276bad692c)
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
set(VCINTRINSICS_VERSION 984bb27baacce6ee5c716c2e64845f2a1928025b)
set(VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/${VCINTRINSICS_VERSION}.tar.gz)
set(VCINTRINSICS_HASH abea415a15a0dd11fdc94dee8fb462910f2548311b787e02f42509789e1b0d7b)
set(VCINTRINSICS_HASH_TYPE SHA256)
set(VCINTRINSICS_FILE vc-intrinsics-${VCINTRINSICS_VERSION}.tar.gz)

# Source opencl/CMakeLists.txt
set(OPENCLHEADERS_VERSION dcd5bede6859d26833cd85f0d6bbcee7382dc9b3)
set(OPENCLHEADERS_URI https://github.com/KhronosGroup/OpenCL-Headers/archive/${OPENCLHEADERS_VERSION}.tar.gz)
set(OPENCLHEADERS_HASH ca8090359654e94f2c41e946b7e9d826253d795ae809ce7c83a7d3c859624693)
set(OPENCLHEADERS_HASH_TYPE SHA256)
set(OPENCLHEADERS_FILE opencl_headers-${OPENCLHEADERS_VERSION}.tar.gz)

# Source opencl/CMakeLists.txt
set(ICDLOADER_VERSION aec3952654832211636fc4af613710f80e203b0a)
set(ICDLOADER_URI https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/${ICDLOADER_VERSION}.tar.gz)
set(ICDLOADER_HASH e1880551d67bd8dc31d13de63b94bbfd6b1f315b6145dad1ffcd159b89bda93c)
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
set(SPIRV_HEADERS_VERSION 36c0c1596225e728bd49abb7ef56a3953e7ed468)
set(SPIRV_HEADERS_URI https://github.com/KhronosGroup/SPIRV-Headers/archive/${SPIRV_HEADERS_VERSION}.tar.gz)
set(SPIRV_HEADERS_HASH 7a5c89633f8740456fe8adee052033e134476d267411d1336c0cb1e587a9229a)
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

set(IGC_VERSION 1.0.11222)
set(IGC_URI https://github.com/intel/intel-graphics-compiler/archive/refs/tags/igc-${IGC_VERSION}.tar.gz)
set(IGC_HASH d92f0608dcbb52690855685f9447282e5c09c0ba98ae35fabf114fcf8b1e9fcf)
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

set(IGC_OPENCL_CLANG_VERSION bbdd1587f577397a105c900be114b56755d1f7dc)
set(IGC_OPENCL_CLANG_URI https://github.com/intel/opencl-clang/archive/${IGC_OPENCL_CLANG_VERSION}.tar.gz)
set(IGC_OPENCL_CLANG_HASH d08315f1b0d8a6fef33de2b3e6aa7356534c324910634962c72523d970773efc)
set(IGC_OPENCL_CLANG_HASH_TYPE SHA256)
set(IGC_OPENCL_CLANG_FILE opencl-clang-${IGC_OPENCL_CLANG_VERSION}.tar.gz)

set(IGC_VCINTRINSICS_VERSION v0.4.0)
set(IGC_VCINTRINSICS_URI https://github.com/intel/vc-intrinsics/archive/refs/tags/${IGC_VCINTRINSICS_VERSION}.tar.gz)
set(IGC_VCINTRINSICS_HASH c8b92682ad5031cf9d5b82a40e7d5c0e763cd9278660adbcaa69aab988e4b589)
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

set(IGC_SPIRV_TRANSLATOR_VERSION 99420daab98998a7e36858befac9c5ed109d4920)
set(IGC_SPIRV_TRANSLATOR_URI https://github.com/KhronosGroup/SPIRV-LLVM-Translator/archive/${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)
set(IGC_SPIRV_TRANSLATOR_HASH 77dfb4ddb6bfb993535562c02ddea23f0a0d1c5a0258c1afe7e27c894ff783a8)
set(IGC_SPIRV_TRANSLATOR_HASH_TYPE SHA256)
set(IGC_SPIRV_TRANSLATOR_FILE SPIR-V-Translator-${IGC_SPIRV_TRANSLATOR_VERSION}.tar.gz)

########################################
### Intel Graphics Compiler DEPS END ###
########################################

set(GMMLIB_VERSION intel-gmmlib-22.1.2)
set(GMMLIB_URI https://github.com/intel/gmmlib/archive/refs/tags/${GMMLIB_VERSION}.tar.gz)
set(GMMLIB_HASH 3b9a6d5e7e3f5748b3d0a2fb0e980ae943907fece0980bd9c0508e71c838e334)
set(GMMLIB_HASH_TYPE SHA256)
set(GMMLIB_FILE ${GMMLIB_VERSION}.tar.gz)

set(OCLOC_VERSION 22.20.23198)
set(OCLOC_URI https://github.com/intel/compute-runtime/archive/refs/tags/${OCLOC_VERSION}.tar.gz)
set(OCLOC_HASH ab22b8bf2560a57fdd3def0e35a62ca75991406f959c0263abb00cd6cd9ae998)
set(OCLOC_HASH_TYPE SHA256)
set(OCLOC_FILE ocloc-${OCLOC_VERSION}.tar.gz)

set(AOM_VERSION 3.4.0)
set(AOM_URI https://storage.googleapis.com/aom-releases/libaom-${AOM_VERSION}.tar.gz)
set(AOM_HASH bd754b58c3fa69f3ffd29da77de591bd9c26970e3b18537951336d6c0252e354)
set(AOM_HASH_TYPE SHA256)
set(AOM_FILE libaom-${AOM_VERSION}.tar.gz)
