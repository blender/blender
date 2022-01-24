# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

set(ZLIB_VERSION 1.2.11)
set(ZLIB_URI https://zlib.net/zlib-${ZLIB_VERSION}.tar.gz)
set(ZLIB_HASH 1c9f62f0778697a09d36121ead88e08e)
set(ZLIB_HASH_TYPE MD5)
set(ZLIB_FILE zlib-${ZLIB_VERSION}.tar.gz)

set(OPENAL_VERSION 1.20.1)
set(OPENAL_URI http://openal-soft.org/openal-releases/openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HASH 556695068ce8375b89986083d810fd35)
set(OPENAL_HASH_TYPE MD5)
set(OPENAL_FILE openal-soft-${OPENAL_VERSION}.tar.bz2)

set(PNG_VERSION 1.6.37)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca)
set(PNG_HASH_TYPE SHA256)
set(PNG_FILE libpng-${PNG_VERSION}.tar.xz)

set(JPEG_VERSION 2.0.4)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 44c43e4a9fb352f47090804529317c88)
set(JPEG_HASH_TYPE MD5)
set(JPEG_FILE libjpeg-turbo-${JPEG_VERSION}.tar.gz)

set(BOOST_VERSION 1.73.0)
set(BOOST_VERSION_NODOTS 1_73_0)
set(BOOST_VERSION_NODOTS_SHORT 1_73)
set(BOOST_URI https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_HASH 4036cd27ef7548b8d29c30ea10956196)
set(BOOST_HASH_TYPE MD5)
set(BOOST_FILE boost_${BOOST_VERSION_NODOTS}.tar.gz)

# Using old version as recommended by OpenVDB build documentation.
set(BLOSC_VERSION 1.5.0)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.tar.gz)
set(BLOSC_HASH 6e4a49c8c06f05aa543f3312cfce3d55)
set(BLOSC_HASH_TYPE MD5)
set(BLOSC_FILE blosc-${BLOSC_VERSION}.tar.gz)

set(PTHREADS_VERSION 3.0.0)
set(PTHREADS_URI http://prdownloads.sourceforge.net/pthreads4w/pthreads4w-code-v${PTHREADS_VERSION}.zip)
set(PTHREADS_HASH f3bf81bb395840b3446197bcf4ecd653)
set(PTHREADS_HASH_TYPE MD5)
set(PTHREADS_FILE pthreads4w-code-${PTHREADS_VERSION}.zip)

set(OPENEXR_VERSION 2.5.5)
set(OPENEXR_URI https://github.com/AcademySoftwareFoundation/openexr/archive/v${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH 85e8a979092c9055d10ed103062d31a0)
set(OPENEXR_HASH_TYPE MD5)
set(OPENEXR_FILE openexr-${OPENEXR_VERSION}.tar.gz)

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

set(FREETYPE_VERSION 2.11.0)
set(FREETYPE_URI http://prdownloads.sourceforge.net/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH cf09172322f6b50cf8f568bf8fe14bde)
set(FREETYPE_HASH_TYPE MD5)
set(FREETYPE_FILE freetype-${FREETYPE_VERSION}.tar.gz)

set(GLEW_VERSION 1.13.0)
set(GLEW_URI http://prdownloads.sourceforge.net/glew/glew/${GLEW_VERSION}/glew-${GLEW_VERSION}.tgz)
set(GLEW_HASH 7cbada3166d2aadfc4169c4283701066)
set(GLEW_HASH_TYPE MD5)
set(GLEW_FILE glew-${GLEW_VERSION}.tgz)

set(FREEGLUT_VERSION 3.0.0)
set(FREEGLUT_URI http://prdownloads.sourceforge.net/freeglut/freeglut/${FREEGLUT_VERSION}/freeglut-${FREEGLUT_VERSION}.tar.gz)
set(FREEGLUT_HASH 90c3ca4dd9d51cf32276bc5344ec9754)
set(FREEGLUT_HASH_TYPE MD5)
set(FREEGLUT_FILE freeglut-${FREEGLUT_VERSION}.tar.gz)

set(ALEMBIC_VERSION 1.7.16)
set(ALEMBIC_URI https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.tar.gz)
set(ALEMBIC_HASH effcc86e42fe6605588e3de57bde6677)
set(ALEMBIC_HASH_TYPE MD5)
set(ALEMBIC_FILE alembic-${ALEMBIC_VERSION}.tar.gz)

# hash is for 3.1.2
set(GLFW_GIT_UID 30306e54705c3adae9fe082c816a3be71963485c)
set(GLFW_URI https://github.com/glfw/glfw/archive/${GLFW_GIT_UID}.zip)
set(GLFW_HASH 20cacb1613da7eeb092f3ac4f6b2b3d0)
set(GLFW_HASH_TYPE MD5)
set(GLFW_FILE glfw-${GLFW_GIT_UID}.zip)

# latest uid in git as of 2016-04-01
set(CLEW_GIT_UID 277db43f6cafe8b27c6f1055f69dc67da4aeb299)
set(CLEW_URI https://github.com/OpenCLWrangler/clew/archive/${CLEW_GIT_UID}.zip)
set(CLEW_HASH 2c699d10ed78362e71f56fae2a4c5f98)
set(CLEW_HASH_TYPE MD5)
set(CLEW_FILE clew-${CLEW_GIT_UID}.zip)

# latest uid in git as of 2016-04-01
set(CUEW_GIT_UID 1744972026de9cf27c8a7dc39cf39cd83d5f922f)
set(CUEW_URI https://github.com/CudaWrangler/cuew/archive/${CUEW_GIT_UID}.zip)
set(CUEW_HASH 86760d62978ebfd96cd93f5aa1abaf4a)
set(CUEW_HASH_TYPE MD5)
set(CUEW_FILE cuew-${CUEW_GIT_UID}.zip)

set(OPENSUBDIV_VERSION v3_4_3)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.tar.gz)
set(OPENSUBDIV_HASH 7bbfa275d021fb829e521df749160edb)
set(OPENSUBDIV_HASH_TYPE MD5)
set(OPENSUBDIV_FILE opensubdiv-${OPENSUBDIV_VERSION}.tar.gz)

set(SDL_VERSION 2.0.12)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH 783b6f2df8ff02b19bb5ce492b99c8ff)
set(SDL_HASH_TYPE MD5)
set(SDL_FILE SDL2-${SDL_VERSION}.tar.gz)

set(OPENCOLLADA_VERSION v1.6.68)
set(OPENCOLLADA_URI https://github.com/KhronosGroup/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH ee7dae874019fea7be11613d07567493)
set(OPENCOLLADA_HASH_TYPE MD5)
set(OPENCOLLADA_FILE opencollada-${OPENCOLLADA_VERSION}.tar.gz)

set(OPENCOLORIO_VERSION 2.0.0)
set(OPENCOLORIO_URI https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v${OPENCOLORIO_VERSION}.tar.gz)
set(OPENCOLORIO_HASH 1a2e3478b6cd9a1549f24e1b2205e3f0)
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

set(OPENIMAGEIO_VERSION 2.2.15.1)
set(OPENIMAGEIO_URI https://github.com/OpenImageIO/oiio/archive/Release-${OPENIMAGEIO_VERSION}.tar.gz)
set(OPENIMAGEIO_HASH 3db5c5f0b3dc91597c75e5df09eb9072)
set(OPENIMAGEIO_HASH_TYPE MD5)
set(OPENIMAGEIO_FILE OpenImageIO-${OPENIMAGEIO_VERSION}.tar.gz)

set(TIFF_VERSION 4.1.0)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH 2165e7aba557463acc0664e71a3ed424)
set(TIFF_HASH_TYPE MD5)
set(TIFF_FILE tiff-${TIFF_VERSION}.tar.gz)

set(OSL_VERSION 1.11.14.1)
set(OSL_URI https://github.com/imageworks/OpenShadingLanguage/archive/Release-${OSL_VERSION}.tar.gz)
set(OSL_HASH 1abd7ce40481771a9fa937f19595d2f2)
set(OSL_HASH_TYPE MD5)
set(OSL_FILE OpenShadingLanguage-${OSL_VERSION}.tar.gz)

set(PYTHON_VERSION 3.9.7)
set(PYTHON_SHORT_VERSION 3.9)
set(PYTHON_SHORT_VERSION_NO_DOTS 39)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH fddb060b483bc01850a3f412eea1d954)
set(PYTHON_HASH_TYPE MD5)
set(PYTHON_FILE Python-${PYTHON_VERSION}.tar.xz)

set(TBB_VERSION 2020_U2)
set(TBB_URI https://github.com/oneapi-src/oneTBB/archive/${TBB_VERSION}.tar.gz)
set(TBB_HASH 1b711ae956524855088df3bbf5ec65dc)
set(TBB_HASH_TYPE MD5)
set(TBB_FILE oneTBB-${TBB_VERSION}.tar.gz)

set(OPENVDB_VERSION 8.0.1)
set(OPENVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 01b490be16cc0e15c690f9a153c21461)
set(OPENVDB_HASH_TYPE MD5)
set(OPENVDB_FILE openvdb-${OPENVDB_VERSION}.tar.gz)

set(NANOVDB_GIT_UID dc37d8a631922e7bef46712947dc19b755f3e841)
set(NANOVDB_URI https://github.com/AcademySoftwareFoundation/openvdb/archive/${NANOVDB_GIT_UID}.tar.gz)
set(NANOVDB_HASH e7b9e863ec2f3b04ead171dec2322807)
set(NANOVDB_HASH_TYPE MD5)
set(NANOVDB_FILE nano-vdb-${NANOVDB_GIT_UID}.tar.gz)

set(IDNA_VERSION 3.2)
set(CHARSET_NORMALIZER_VERSION 2.0.6)
set(URLLIB3_VERSION 1.26.7)
set(CERTIFI_VERSION 2021.10.8)
set(REQUESTS_VERSION 2.26.0)
set(CYTHON_VERSION 0.29.24)
set(ZSTANDARD_VERSION 0.15.2 )

set(NUMPY_VERSION 1.21.2)
set(NUMPY_SHORT_VERSION 1.21)
set(NUMPY_URI https://github.com/numpy/numpy/releases/download/v${NUMPY_VERSION}/numpy-${NUMPY_VERSION}.zip)
set(NUMPY_HASH 5638d5dae3ca387be562912312db842e)
set(NUMPY_HASH_TYPE MD5)
set(NUMPY_FILE numpy-${NUMPY_VERSION}.zip)

set(LAME_VERSION 3.100)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.100/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 83e260acbe4389b54fe08e0bdbf7cddb)
set(LAME_HASH_TYPE MD5)
set(LAME_FILE lame-${LAME_VERSION}.tar.gz)

set(OGG_VERSION 1.3.4)
set(OGG_URI http://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz)
set(OGG_HASH fe5670640bd49e828d64d2879c31cb4dde9758681bb664f9bdbf159a01b0c76e)
set(OGG_HASH_TYPE SHA256)
set(OGG_FILE libogg-${OGG_VERSION}.tar.gz)

set(VORBIS_VERSION 1.3.6)
set(VORBIS_URI http://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_HASH 6ed40e0241089a42c48604dc00e362beee00036af2d8b3f46338031c9e0351cb)
set(VORBIS_HASH_TYPE SHA256)
set(VORBIS_FILE libvorbis-${VORBIS_VERSION}.tar.gz)

set(THEORA_VERSION 1.1.1)
set(THEORA_URI http://downloads.xiph.org/releases/theora/libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HASH b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc)
set(THEORA_HASH_TYPE SHA256)
set(THEORA_FILE libtheora-${THEORA_VERSION}.tar.bz2)

set(FLAC_VERSION 1.3.3)
set(FLAC_URI http://downloads.xiph.org/releases/flac/flac-${FLAC_VERSION}.tar.xz)
set(FLAC_HASH 213e82bd716c9de6db2f98bcadbc4c24c7e2efe8c75939a1a84e28539c4e1748)
set(FLAC_HASH_TYPE SHA256)
set(FLAC_FILE flac-${FLAC_VERSION}.tar.xz)

set(VPX_VERSION 1.8.2)
set(VPX_URI https://github.com/webmproject/libvpx/archive/v${VPX_VERSION}/libvpx-v${VPX_VERSION}.tar.gz)
set(VPX_HASH 8735d9fcd1a781ae6917f28f239a8aa358ce4864ba113ea18af4bb2dc8b474ac)
set(VPX_HASH_TYPE SHA256)
set(VPX_FILE libvpx-v${VPX_VERSION}.tar.gz)

set(OPUS_VERSION 1.3.1)
set(OPUS_URI https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz)
set(OPUS_HASH 65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d)
set(OPUS_HASH_TYPE SHA256)
set(OPUS_FILE opus-${OPUS_VERSION}.tar.gz)

set(X264_VERSION 33f9e1474613f59392be5ab6a7e7abf60fa63622)
set(X264_URI https://code.videolan.org/videolan/x264/-/archive/${X264_VERSION}/x264-${X264_VERSION}.tar.gz)
set(X264_HASH 5456450ee1ae02cd2328be3157367a232a0ab73315e8c8f80dab80469524f525)
set(X264_HASH_TYPE SHA256)
set(X264_FILE x264-${X264_VERSION}.tar.gz)

set(XVIDCORE_VERSION 1.3.7)
set(XVIDCORE_URI https://downloads.xvid.com/downloads/xvidcore-${XVIDCORE_VERSION}.tar.gz)
set(XVIDCORE_HASH abbdcbd39555691dd1c9b4d08f0a031376a3b211652c0d8b3b8aa9be1303ce2d)
set(XVIDCORE_HASH_TYPE SHA256)
set(XVIDCORE_FILE xvidcore-${XVIDCORE_VERSION}.tar.gz)

set(OPENJPEG_VERSION 2.3.1)
set(OPENJPEG_SHORT_VERSION 2.3)
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz)
set(OPENJPEG_HASH 63f5a4713ecafc86de51bfad89cc07bb788e9bba24ebbf0c4ca637621aadb6a9)
set(OPENJPEG_HASH_TYPE SHA256)
set(OPENJPEG_FILE openjpeg-v${OPENJPEG_VERSION}.tar.gz)

set(FFMPEG_VERSION 4.4)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH 42093549751b582cf0f338a21a3664f52e0a9fbe0d238d3c992005e493607d0e)
set(FFMPEG_HASH_TYPE SHA256)
set(FFMPEG_FILE ffmpeg-${FFMPEG_VERSION}.tar.bz2)

set(FFTW_VERSION 3.3.8)
set(FFTW_URI http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HASH 8aac833c943d8e90d51b697b27d4384d)
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

set(WEBP_VERSION 0.6.1)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH b49ce9c3e3e9acae4d91bca44bb85a72)
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

set(EXPAT_VERSION 2_2_10)
set(EXPAT_URI https://github.com/libexpat/libexpat/archive/R_${EXPAT_VERSION}.tar.gz)
set(EXPAT_HASH 7ca5f09959fcb9a57618368deb627b9f)
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

set(EMBREE_VERSION 3.10.0)
set(EMBREE_URI https://github.com/embree/embree/archive/v${EMBREE_VERSION}.zip)
set(EMBREE_HASH 4bbe29e7eaa46417efc75fc5f1e8eb87)
set(EMBREE_HASH_TYPE MD5)
set(EMBREE_FILE embree-v${EMBREE_VERSION}.zip)
set(EMBREE_ARM_GIT https://github.com/brechtvl/embree.git)

set(USD_VERSION 21.02)
set(USD_URI https://github.com/PixarAnimationStudios/USD/archive/v${USD_VERSION}.tar.gz)
set(USD_HASH 1dd1e2092d085ed393c1f7c450a4155a)
set(USD_HASH_TYPE MD5)
set(USD_FILE usd-v${USD_VERSION}.tar.gz)

set(OIDN_VERSION 1.4.1)
set(OIDN_URI https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz)
set(OIDN_HASH df4007b0ab93b1c41cdf223b075d01c0)
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

set(XR_OPENXR_SDK_VERSION 1.0.17)
set(XR_OPENXR_SDK_URI https://github.com/KhronosGroup/OpenXR-SDK/archive/release-${XR_OPENXR_SDK_VERSION}.tar.gz)
set(XR_OPENXR_SDK_HASH bf0fd8828837edff01047474e90013e1)
set(XR_OPENXR_SDK_HASH_TYPE MD5)
set(XR_OPENXR_SDK_FILE OpenXR-SDK-${XR_OPENXR_SDK_VERSION}.tar.gz)

set(WL_PROTOCOLS_VERSION 1.21)
set(WL_PROTOCOLS_FILE wayland-protocols-${WL_PROTOCOLS_VERSION}.tar.gz)
set(WL_PROTOCOLS_URI https://gitlab.freedesktop.org/wayland/wayland-protocols/-/archive/${WL_PROTOCOLS_VERSION}/${WL_PROTOCOLS_FILE})
set(WL_PROTOCOLS_HASH af5ca07e13517cdbab33504492cef54a)
set(WL_PROTOCOLS_HASH_TYPE MD5)

set(ISPC_VERSION v1.16.0)
set(ISPC_URI https://github.com/ispc/ispc/archive/${ISPC_VERSION}.tar.gz)
set(ISPC_HASH 2e3abedbc0ea9aaec17d6562c632454d)
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
