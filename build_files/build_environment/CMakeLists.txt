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

####################################################################################################
#
# This is a build system used by platform maintainers to build library dependencies on
# Windows, macOS and Linux.
#
# For users building Blender, we recommend using the precompiled libraries from lib/ on
# Windows and macOS, and install_deps.sh on Linux.
#
# WINDOWS USAGE:
#   Don't call this cmake file yourself, use build_deps.cmd
#   build_deps 2013 x64 / build_deps 2013 x86
#   build_deps 2015 x64 / build_deps 2015 x86
#
# MAC OS X USAGE:
#   Install with homebrew: brew install cmake autoconf automake libtool yasm nasm
#   Run "make deps" from main Blender directory
#
# LINUX USAGE:
#   Install compiler cmake autoconf automake libtool yasm nasm tcl
#   Run "make deps" from main Blender directory
#
####################################################################################################

project("BlenderDependencies")
cmake_minimum_required(VERSION 3.5)

include(ExternalProject)
include(cmake/check_software.cmake)
include(cmake/options.cmake)
include(cmake/versions.cmake)

if(ENABLE_MINGW64)
  if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
    include(cmake/setup_mingw64.cmake)
  else()
    include(cmake/setup_mingw32.cmake)
  endif()
else()
  set(mingw_LIBDIR ${LIBDIR})
endif()

include(cmake/zlib.cmake)
include(cmake/openal.cmake)
include(cmake/png.cmake)
include(cmake/jpeg.cmake)
include(cmake/boost.cmake)
include(cmake/blosc.cmake)
include(cmake/pthreads.cmake)
include(cmake/openexr.cmake)
include(cmake/freetype.cmake)
include(cmake/freeglut.cmake)
include(cmake/glew.cmake)
include(cmake/alembic.cmake)
include(cmake/glfw.cmake)
include(cmake/clew.cmake)
include(cmake/cuew.cmake)
include(cmake/opensubdiv.cmake)
include(cmake/sdl.cmake)
include(cmake/opencollada.cmake)
include(cmake/opencolorio.cmake)
include(cmake/llvm.cmake)
include(cmake/clang.cmake)
if(APPLE)
  include(cmake/openmp.cmake)
endif()
include(cmake/openimageio.cmake)
include(cmake/tiff.cmake)
include(cmake/flexbison.cmake)
include(cmake/osl.cmake)
include(cmake/tbb.cmake)
include(cmake/openvdb.cmake)
include(cmake/python.cmake)
include(cmake/python_site_packages.cmake)
include(cmake/package_python.cmake)
include(cmake/numpy.cmake)
include(cmake/usd.cmake)
if(UNIX)
  # Rely on PugiXML compiled with OpenImageIO
else()
  include(cmake/pugixml.cmake)
endif()
include(cmake/ispc.cmake)
include(cmake/openimagedenoise.cmake)
include(cmake/embree.cmake)
include(cmake/xr_openxr.cmake)

if(WITH_WEBP)
  include(cmake/webp.cmake)
endif()

if(WIN32)
  # HMD branch deps
  include(cmake/hidapi.cmake)
  # OCIO deps
  include(cmake/tinyxml.cmake)
  include(cmake/yamlcpp.cmake)
  # LCMS is an OCIO dep, but only if you build the apps, leaving it here for convenience
  # include(cmake/lcms.cmake)

endif()

if(NOT WIN32 OR ENABLE_MINGW64)
  include(cmake/openjpeg.cmake)
  if(NOT WIN32 OR BUILD_MODE STREQUAL Release)
    if(WIN32)
      include(cmake/zlib_mingw.cmake)
    endif()
    include(cmake/lame.cmake)
    include(cmake/ogg.cmake)
    include(cmake/vorbis.cmake)
    include(cmake/theora.cmake)
    include(cmake/opus.cmake)
    include(cmake/vpx.cmake)
    include(cmake/x264.cmake)
    include(cmake/xvidcore.cmake)
    include(cmake/ffmpeg.cmake)
    include(cmake/fftw.cmake)
    include(cmake/sndfile.cmake)
    if(WIN32)
      include(cmake/iconv.cmake)
    endif()
    if(UNIX)
      include(cmake/flac.cmake)
      include(cmake/xml2.cmake)
      if(NOT APPLE)
        include(cmake/spnav.cmake)
        include(cmake/jemalloc.cmake)
      endif()
    endif()
  endif()
endif()

if(UNIX)
  include(cmake/bzip2.cmake)
  include(cmake/ffi.cmake)
  include(cmake/lzma.cmake)
  include(cmake/ssl.cmake)
  include(cmake/sqlite.cmake)
endif()

if(UNIX AND NOT APPLE)
  include(cmake/libglu.cmake)
  include(cmake/mesa.cmake)
endif()

include(cmake/harvest.cmake)
