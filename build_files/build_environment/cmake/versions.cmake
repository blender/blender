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

set(OPENAL_VERSION 1.17.2)
set(OPENAL_URI http://kcat.strangesoft.net/openal-releases/openal-soft-${OPENAL_VERSION}.tar.bz2)
set(OPENAL_HASH 1764e0d8fec499589b47ebc724e0913d)

set(PNG_VERSION 1.6.35)
set(PNG_URI http://prdownloads.sourceforge.net/libpng/libpng-${PNG_VERSION}.tar.xz)
set(PNG_HASH 678b7e696a62a193ed3503b04bf449d6)

set(JPEG_VERSION 1.5.3)
set(JPEG_URI https://github.com/libjpeg-turbo/libjpeg-turbo/archive/${JPEG_VERSION}.tar.gz)
set(JPEG_HASH 5b7549d440b86c98a517355c102d155e)

set(BOOST_VERSION 1.68.0)
set(BOOST_VERSION_NODOTS 1_68_0)
set(BOOST_URI https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_NODOTS}.tar.gz)
set(BOOST_HASH 5d8b4503582fffa9eefdb9045359c239)
  
set(BLOSC_VERSION 1.7.1)
set(BLOSC_URI https://github.com/Blosc/c-blosc/archive/v${BLOSC_VERSION}.zip)
set(BLOSC_HASH ff5cc729a5a25934ef714217218eed26)

set(PTHREADS_VERSION 2-9-1)
set(PTHREADS_URI ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-${PTHREADS_VERSION}-release.tar.gz)
set(PTHREADS_SHA512 9c06e85310766834370c3dceb83faafd397da18a32411ca7645c8eb6b9495fea54ca2872f4a3e8d83cb5fdc5dea7f3f0464be5bb9af3222a6534574a184bd551)

set(ILMBASE_VERSION 2.2.1)
set(ILMBASE_URI http://download.savannah.nongnu.org/releases/openexr/ilmbase-${ILMBASE_VERSION}.tar.gz)
set(ILMBASE_HASH 7b86128b04f0541b6bb33633e299cb44)

set(OPENEXR_VERSION 2.2.1)
set(OPENEXR_URI http://download.savannah.nongnu.org/releases/openexr/openexr-${OPENEXR_VERSION}.tar.gz)
set(OPENEXR_HASH 421815c32989e1b98fc4798ee754c433)

set(FREETYPE_VERSION 2.9.1)
set(FREETYPE_URI http://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VERSION}.tar.gz)
set(FREETYPE_HASH 3adb0e35d3c100c456357345ccfa8056)

set(GLEW_VERSION 1.13.0)
set(GLEW_URI http://prdownloads.sourceforge.net/glew/glew/${GLEW_VERSION}/glew-${GLEW_VERSION}.tgz)
set(GLEW_HASH 7cbada3166d2aadfc4169c4283701066)

set(FREEGLUT_VERSION 3.0.0)
set(FREEGLUT_URI http://pilotfiber.dl.sourceforge.net/project/freeglut/freeglut/${FREEGLUT_VERSION}/freeglut-${FREEGLUT_VERSION}.tar.gz)
set(FREEGLUT_HASH 90c3ca4dd9d51cf32276bc5344ec9754)

set(HDF5_VERSION 1.8.17)
set(HDF5_URI https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8/hdf5-${HDF5_VERSION}/src/hdf5-${HDF5_VERSION}.tar.gz)
set(HDF5_HASH 7d572f8f3b798a628b8245af0391a0ca)

set(ALEMBIC_VERSION 1.7.1)
set(ALEMBIC_URI https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.zip)
set(ALEMBIC_MD5 cf7705055501d5ea0cb8256866496f79)

## hash is for 3.1.2
set(GLFW_GIT_UID 30306e54705c3adae9fe082c816a3be71963485c)
set(GLFW_URI https://github.com/glfw/glfw/archive/${GLFW_GIT_UID}.zip)
set(GLFW_HASH 20cacb1613da7eeb092f3ac4f6b2b3d0)

#latest uid in git as of 2016-04-01
set(CLEW_GIT_UID 277db43f6cafe8b27c6f1055f69dc67da4aeb299)
set(CLEW_URI https://github.com/OpenCLWrangler/clew/archive/${CLEW_GIT_UID}.zip)
set(CLEW_HASH 2c699d10ed78362e71f56fae2a4c5f98)

#latest uid in git as of 2016-04-01
set(CUEW_GIT_UID 1744972026de9cf27c8a7dc39cf39cd83d5f922f)
set(CUEW_URI https://github.com/CudaWrangler/cuew/archive/${CUEW_GIT_UID}.zip)
set(CUEW_HASH 86760d62978ebfd96cd93f5aa1abaf4a)

set(OPENSUBDIV_VERSION v3_1_1)
set(OPENSUBDIV_Hash 25a9a6a94136b0eb85ce69e9c8cb6ab3)
set(OPENSUBDIV_URI https://github.com/PixarAnimationStudios/OpenSubdiv/archive/${OPENSUBDIV_VERSION}.zip)

set(SDL_VERSION 2.0.4)
set(SDL_URI https://www.libsdl.org/release/SDL2-${SDL_VERSION}.tar.gz)
set(SDL_HASH 44fc4a023349933e7f5d7a582f7b886e)

set(OPENCOLLADA_VERSION v1.6.51)
set(OPENCOLLADA_URI https://github.com/KhronosGroup/OpenCOLLADA/archive/${OPENCOLLADA_VERSION}.tar.gz)
set(OPENCOLLADA_HASH 23db5087faed4bc4cc1dfe456c0d4701)

set(OPENCOLORIO_URI https://github.com/imageworks/OpenColorIO/archive/6de971097c7f552300f669ed69ca0b6cf5a70843.zip)
set(OPENCOLORIO_HASH c9de0fd98f26ce6f2e08d617ca68b8e4)

set(LLVM_VERSION 6.0.1)
set(LLVM_URI http://releases.llvm.org/${LLVM_VERSION}/llvm-${LLVM_VERSION}.src.tar.xz)
set(LLVM_HASH c88c98709300ce2c285391f387fecce0)

set(CLANG_URI http://releases.llvm.org/${LLVM_VERSION}/cfe-${LLVM_VERSION}.src.tar.xz)
set(CLANG_HASH 4e419bd4e3b55aa06d872320f754bd85)

set(OPENMP_URI http://releases.llvm.org/${LLVM_VERSION}/openmp-${LLVM_VERSION}.src.tar.xz)
set(OPENMP_HASH 4826402ae3633c36c51ba4d0e5527d30)

set(OPENIMAGEIO_VERSION 1.7.15)
set(OPENIMAGEIO_URI https://github.com/OpenImageIO/oiio/archive/Release-${OPENIMAGEIO_VERSION}.zip)
set(OPENIMAGEIO_HASH_178 e156e3669af0e1373142ab5e8f13de66)
set(OPENIMAGEIO_HASH_179 4121cb0e0433bda6a7ef32c8628a149f)
set(OPENIMAGEIO_HASH_1713 42a662775b834161ba88c6abdb299360)
set(OPENIMAGEIO_HASH_1715 e2ece0f62c013d64c478f82265988b0b)
set(OPENIMAGEIO_HASH ${OPENIMAGEIO_HASH_1715})


set(TIFF_VERSION 4.0.9)
set(TIFF_URI http://download.osgeo.org/libtiff/tiff-${TIFF_VERSION}.tar.gz)
set(TIFF_HASH 54bad211279cc93eb4fca31ba9bfdc79)

set(FLEXBISON_VERSION 2.5.5)
set(FLEXBISON_URI http://prdownloads.sourceforge.net/winflexbison//win_flex_bison-2.5.5.zip)
set(FLEXBISON_HASH d87a3938194520d904013abef3df10ce)

set(OSL_VERSION 1.7.5)
set(OSL_URI https://github.com/imageworks/OpenShadingLanguage/archive/Release-${OSL_VERSION}.zip)
set(OSL_HASH 6924dd5d453159e7b6eb106a08c358cf)

set(PYTHON_VERSION 3.7.0)
set(PYTHON_SHORT_VERSION 3.7)
set(PYTHON_SHORT_VERSION_NO_DOTS 37)
set(PYTHON_URI https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tar.xz)
set(PYTHON_HASH eb8c2a6b1447d50813c02714af4681f3)

if(UNIX AND NOT APPLE)
	# Needed to be compatible with GCC 7, other platforms can upgrade later
	set(TBB_VERSION 2017_U7)
	set(TBB_URI https://github.com/01org/tbb/archive/${TBB_VERSION}.tar.gz)
	set(TBB_HASH 364f2a4b80e978f38a69cbf7c466b898)
else()
	set(TBB_VERSION 44_20160128)
	set(TBB_URI https://www.threadingbuildingblocks.org/sites/default/files/software_releases/source/tbb${TBB_VERSION}oss_src_0.tgz)
	set(TBB_HASH 9d8a4cdf43496f1b3f7c473a5248e5cc)
endif()

set(OPENVDB_VERSION 3.1.0)
set(OPENVDB_URI https://github.com/dreamworksanimation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz)
set(OPENVDB_HASH 30a7e9571a03ab7bcf1a39fb62aa436f)

set(IDNA_VERSION 2.6)
set(CHARDET_VERSION 3.0.2)
set(URLLIB3_VERSION 1.22)
set(CERTIFI_VERSION 2017.7.27.1)
set(REQUESTS_VERSION 2.18.4)

set(NUMPY_VERSION v1.15.0)
set(NUMPY_SHORT_VERSION 1.15)
set(NUMPY_URI https://files.pythonhosted.org/packages/3a/20/c81632328b1a4e1db65f45c0a1350a9c5341fd4bbb8ea66cdd98da56fe2e/numpy-1.15.0.zip)
set(NUMPY_HASH 20e13185089011116a98e11c9bf8aa07)

set(LAME_VERSION 3.99.5)
set(LAME_URI http://downloads.sourceforge.net/project/lame/lame/3.99/lame-${LAME_VERSION}.tar.gz)
set(LAME_HASH 84835b313d4a8b68f5349816d33e07ce)

set(OGG_VERSION 1.3.2)
set(OGG_URI http://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.gz)
set(OGG_HASH e19ee34711d7af328cb26287f4137e70630e7261b17cbe3cd41011d73a654692)

set(VORBIS_VERSION 1.3.5)
set(VORBIS_URI http://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.gz)
set(VORBIS_HASH 6efbcecdd3e5dfbf090341b485da9d176eb250d893e3eb378c428a2db38301ce)

set(THEORA_VERSION 1.1.1)
set(THEORA_URI http://downloads.xiph.org/releases/theora/libtheora-${THEORA_VERSION}.tar.bz2)
set(THEORA_HASH b6ae1ee2fa3d42ac489287d3ec34c5885730b1296f0801ae577a35193d3affbc)

set(FLAC_VERSION 1.3.1)
set(FLAC_URI http://downloads.xiph.org/releases/flac/flac-${FLAC_VERSION}.tar.xz)
set(FLAC_HASH 4773c0099dba767d963fd92143263be338c48702172e8754b9bc5103efe1c56c)

set(VPX_VERSION 1.5.0)
set(VPX_URI http://storage.googleapis.com/downloads.webmproject.org/releases/webm/libvpx-${VPX_VERSION}.tar.bz2)
set(VPX_HASH 306d67908625675f8e188d37a81fbfafdf5068b09d9aa52702b6fbe601c76797)

set(X264_URI http://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-20160401-2245-stable.tar.bz2)
set(X264_HASH 1e9a7b835e80313aade53a9b6ff353e099de3856bf5f30a4d8dfc91281f786f5)

set(XVIDCORE_VERSION 1.3.4)
set(XVIDCORE_URI http://downloads.xvid.org/downloads/xvidcore-${XVIDCORE_VERSION}.tar.gz)
set(XVIDCORE_HASH 4e9fd62728885855bc5007fe1be58df42e5e274497591fec37249e1052ae316f)

#this has to be in sync with the version in blenders /extern folder
set(OPENJPEG_VERSION 2.3.0)
set(OPENJPEG_SHORT_VERSION 2.3)
# Use slightly newer commit after release which includes a cmake fix
set(OPENJPEG_URI https://github.com/uclouvain/openjpeg/archive/66297f07a43.zip)
set(OPENJPEG_HASH 8242b18d908c7c42174e4231a741cfa7ce7c26b6ed5c9644feb9df7b3054310b)

set(FAAD_VERSION 2-2.7)
set(FAAD_URI http://downloads.sourceforge.net/faac/faad${FAAD_VERSION}.tar.bz2)
set(FAAD_HASH 4c332fa23febc0e4648064685a3d4332)

set(FFMPEG_VERSION 3.4.1)
set(FFMPEG_URI http://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2)
set(FFMPEG_HASH bbf3fcded80c33968c91bf323a744286)

set(FFTW_VERSION 3.3.4)
set(FFTW_URI http://www.fftw.org/fftw-${FFTW_VERSION}.tar.gz)
set(FFTW_HASH 2edab8c06b24feeb3b82bbb3ebf3e7b3)

set(ICONV_VERSION 1.14)
set(ICONV_URI http://ftp.gnu.org/pub/gnu/libiconv/libiconv-${ICONV_VERSION}.tar.gz)
set(ICONV_HASH e34509b1623cec449dfeb73d7ce9c6c6)

set(LAPACK_VERSION 3.6.0)
set(LAPACK_URI http://www.netlib.org/lapack/lapack-${LAPACK_VERSION}.tgz)
set(LAPACK_HASH f2f6c67134e851fe189bb3ca1fbb5101)

set(SNDFILE_VERSION 1.0.28)
set(SNDFILE_URI http://www.mega-nerd.com/libsndfile/files/libsndfile-${SNDFILE_VERSION}.tar.gz)
set(SNDFILE_HASH 646b5f98ce89ac60cdb060fcd398247c)

#set(HIDAPI_VERSION 0.8.0-rc1)
#set(HIDAPI_URI https://github.com/signal11/hidapi/archive/hidapi-${HIDAPI_VERSION}.tar.gz)
#set(HIDAPI_HASH 069f9dd746edc37b6b6d0e3656f47199)

set(HIDAPI_UID 89a6c75dc6f45ecabd4ddfbd2bf5ba6ad8ba38b5)
set(HIDAPI_URI https://github.com/TheOnlyJoey/hidapi/archive/${HIDAPI_UID}.zip)
set(HIDAPI_HASH b6e22f6b514f8bcf594989f20ffc46fb)

set(WEBP_VERSION 0.6.1)
set(WEBP_URI https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${WEBP_VERSION}.tar.gz)
set(WEBP_HASH b49ce9c3e3e9acae4d91bca44bb85a72)

set(SPNAV_VERSION 0.2.3)
set(SPNAV_URI http://downloads.sourceforge.net/project/spacenav/spacenav%20library%20%28SDK%29/libspnav%20${SPNAV_VERSION}/libspnav-${SPNAV_VERSION}.tar.gz)
set(SPNAV_HASH 44d840540d53326d4a119c0f1aa7bf0a)

set(JEMALLOC_VERSION 5.0.1)
set(JEMALLOC_URI https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2)
set(JEMALLOC_HASH 507f7b6b882d868730d644510491d18f)

set(XML2_VERSION 2.9.4)
set(XML2_URI ftp://xmlsoft.org/libxml2/libxml2-${XML2_VERSION}.tar.gz)
set(XML2_HASH ae249165c173b1ff386ee8ad676815f5)
