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

if(WIN32)
  option(ENABLE_MINGW64 "Enable building of ffmpeg/iconv/libsndfile/fftw3 by installing mingw64" ON)
endif()
option(WITH_WEBP "Enable building of oiio with webp support" OFF)
option(WITH_BOOST_PYTHON "Enable building of boost with python support" OFF)
set(MAKE_THREADS 1 CACHE STRING "Number of threads to run make with")

if(NOT BUILD_MODE)
  set(BUILD_MODE "Release")
  message(STATUS "Build type not specified: defaulting to a release build.")
endif()
message("BuildMode = ${BUILD_MODE}")

if(BUILD_MODE STREQUAL "Debug")
  set(LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/Debug)
else(BUILD_MODE STREQUAL "Debug")
  set(LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/Release)
endif()

set(DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/downloads" CACHE STRING "Path for downloaded files")
# look in blenders source folder for packages directory, if that exists
# it will our package folder, otherwise it will be in the build folder
if(EXISTS "${CMAKE_SOURCE_DIR}/../../packages")
  set(PACKAGE_DIR_DEFAULT "${CMAKE_SOURCE_DIR}/../../packages")
else()
  set(PACKAGE_DIR_DEFAULT "${CMAKE_CURRENT_BINARY_DIR}/packages")
endif()
set(PACKAGE_DIR ${PACKAGE_DIR_DEFAULT} CACHE STRING "Path for downloaded source files")
option(PACKAGE_USE_UPSTREAM_SOURCES "Use soures upstream to download the package sources, when OFF the blender mirror will be used" ON)

file(TO_CMAKE_PATH ${DOWNLOAD_DIR} DOWNLOAD_DIR)
file(TO_CMAKE_PATH ${PACKAGE_DIR} PACKAGE_DIR)
set(PATCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/patches)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)

message("LIBDIR = ${LIBDIR}")
message("DOWNLOAD_DIR = ${DOWNLOAD_DIR}")
message("PACKAGE_DIR = ${PACKAGE_DIR}")
message("PATCH_DIR = ${PATCH_DIR}")
message("BUILD_DIR = ${BUILD_DIR}")

if(WIN32)
  set(PATCH_CMD ${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/patch.exe)
  set(LIBEXT ".lib")
  set(LIBPREFIX "")

  # For OIIO and OSL
  set(COMMON_DEFINES /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS)

  if(MSVC_VERSION GREATER 1909)
    set(COMMON_MSVC_FLAGS "/Wv:18") #some deps with warnings as error aren't quite ready for dealing with the new 2017 warnings.
  endif()
  string(APPEND COMMON_MSVC_FLAGS " /bigobj")
  if(WITH_OPTIMIZED_DEBUG)
    set(BLENDER_CMAKE_C_FLAGS_DEBUG "/MDd ${COMMON_MSVC_FLAGS} /O2 /Ob2 /DNDEBUG /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  else()
    set(BLENDER_CMAKE_C_FLAGS_DEBUG "/MDd ${COMMON_MSVC_FLAGS} /Zi /Ob0 /Od /RTC1 /D_DEBUG /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  endif()
  set(BLENDER_CMAKE_C_FLAGS_MINSIZEREL "/MD ${COMMON_MSVC_FLAGS} /O1 /Ob1 /D NDEBUG /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  set(BLENDER_CMAKE_C_FLAGS_RELEASE "/MD ${COMMON_MSVC_FLAGS} /O2 /Ob2 /DNDEBUG /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  set(BLENDER_CMAKE_C_FLAGS_RELWITHDEBINFO "/MD ${COMMON_MSVC_FLAGS} /Zi /O2 /Ob1 /D NDEBUG /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")

  if(WITH_OPTIMIZED_DEBUG)
    set(BLENDER_CMAKE_CXX_FLAGS_DEBUG "/MDd ${COMMON_MSVC_FLAGS} /O2 /Ob2 /D NDEBUG /D PLATFORM_WINDOWS /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  else()
    set(BLENDER_CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /D PLATFORM_WINDOWS /MTd  ${COMMON_MSVC_FLAGS} /Zi /Ob0 /Od /RTC1 /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  endif()
  set(BLENDER_CMAKE_CXX_FLAGS_MINSIZEREL "/MD /${COMMON_MSVC_FLAGS} /O1 /Ob1 /D NDEBUG  /D PLATFORM_WINDOWS /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  set(BLENDER_CMAKE_CXX_FLAGS_RELEASE "/MD ${COMMON_MSVC_FLAGS} /O2 /Ob2 /D NDEBUG /D PLATFORM_WINDOWS /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")
  set(BLENDER_CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MD ${COMMON_MSVC_FLAGS} /Zi /O2 /Ob1 /D NDEBUG /D PLATFORM_WINDOWS /DPSAPI_VERSION=1 /DOIIO_STATIC_BUILD /DTINYFORMAT_ALLOW_WCHAR_STRINGS")

  set(PLATFORM_FLAGS)
  set(PLATFORM_CXX_FLAGS)
  set(PLATFORM_CMAKE_FLAGS)

  set(MINGW_PATH ${DOWNLOAD_DIR}/mingw/mingw64)
  set(MINGW_SHELL ming64sh.cmd)
  set(PERL_SHELL ${DOWNLOAD_DIR}/perl/portableshell.bat)
  set(MINGW_HOST x86_64-w64-mingw32)

  set(CONFIGURE_ENV
    cd ${MINGW_PATH} &&
    call ${PERL_SHELL} &&
    call ${MINGW_SHELL} &&
    set path &&
    set CFLAGS=-g &&
    set LDFLAGS=-Wl,--as-needed -static-libgcc
  )

  set(CONFIGURE_ENV_NO_PERL
    cd ${MINGW_PATH} &&
    call ${MINGW_SHELL} &&
    set path &&
    set CFLAGS=-g &&
    set LDFLAGS=-Wl,--as-needed -static-libgcc
  )

  set(CONFIGURE_COMMAND sh ./configure)
  set(CONFIGURE_COMMAND_NO_TARGET ${CONFIGURE_COMMAND})
else()
  set(PATCH_CMD patch)
  set(LIBEXT ".a")
  set(LIBPREFIX "lib")

  if(APPLE)
    # Let's get the current Xcode dir, to support xcode-select
    execute_process(
      COMMAND xcode-select --print-path
      OUTPUT_VARIABLE XCODE_DEV_PATH OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND xcodebuild -version -sdk macosx SDKVersion
      OUTPUT_VARIABLE MACOSX_SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(NOT CMAKE_OSX_ARCHITECTURES)
      execute_process(COMMAND uname -m OUTPUT_VARIABLE ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
      message(STATUS "Detected native architecture ${ARCHITECTURE}.")
      set(CMAKE_OSX_ARCHITECTURES "${ARCHITECTURE}")
    endif()
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
      set(OSX_DEPLOYMENT_TARGET 10.13)
    else()
      set(OSX_DEPLOYMENT_TARGET 11.00)
    endif()
    set(OSX_SYSROOT ${XCODE_DEV_PATH}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    set(PLATFORM_CFLAGS "-isysroot ${OSX_SYSROOT} -mmacosx-version-min=${OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
    set(PLATFORM_CXXFLAGS "-isysroot ${OSX_SYSROOT} -mmacosx-version-min=${OSX_DEPLOYMENT_TARGET} -std=c++11 -stdlib=libc++ -arch ${CMAKE_OSX_ARCHITECTURES}")
    set(PLATFORM_LDFLAGS "-isysroot ${OSX_SYSROOT} -mmacosx-version-min=${OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
      set(PLATFORM_BUILD_TARGET --build=x86_64-apple-darwin17.0.0) # OS X 10.13
    else()
      set(PLATFORM_BUILD_TARGET --build=aarch64-apple-darwin20.0.0) # macOS 11.00
    endif()
    set(PLATFORM_CMAKE_FLAGS
      -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_SYSROOT:PATH=${OSX_SYSROOT}
    )
  else()
    set(PLATFORM_CFLAGS "-fPIC")
    set(PLATFORM_CXXFLAGS "-std=c++11 -fPIC")
    set(PLATFORM_LDFLAGS)
    set(PLATFORM_BUILD_TARGET)
    set(PLATFORM_CMAKE_FLAGS -DCMAKE_INSTALL_LIBDIR=lib)
  endif()

  if(WITH_OPTIMIZED_DEBUG)
    set(BLENDER_CMAKE_C_FLAGS_DEBUG "-O2 -DNDEBUG ${PLATFORM_CFLAGS}")
  else()
    set(BLENDER_CMAKE_C_FLAGS_DEBUG "-g ${PLATFORM_CFLAGS}")
  endif()
  set(BLENDER_CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG ${PLATFORM_CFLAGS}")
  set(BLENDER_CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG ${PLATFORM_CFLAGS}")
  set(BLENDER_CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG ${PLATFORM_CFLAGS}")

  if(WITH_OPTIMIZED_DEBUG)
    set(BLENDER_CMAKE_CXX_FLAGS_DEBUG "-O2 -DNDEBUG ${PLATFORM_CXXFLAGS}")
  else()
    set(BLENDER_CMAKE_CXX_FLAGS_DEBUG "-g ${PLATFORM_CXXFLAGS}")
  endif()

  set(BLENDER_CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG ${PLATFORM_CXXFLAGS}")
  set(BLENDER_CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG ${PLATFORM_CXXFLAGS}")
  set(BLENDER_CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG ${PLATFORM_CXXFLAGS}")

  set(CONFIGURE_ENV
    export MACOSX_DEPLOYMENT_TARGET=${OSX_DEPLOYMENT_TARGET} &&
    export MACOSX_SDK_VERSION=${OSX_DEPLOYMENT_TARGET} &&
    export CFLAGS=${PLATFORM_CFLAGS} &&
    export CXXFLAGS=${PLATFORM_CXXFLAGS} &&
    export LDFLAGS=${PLATFORM_LDFLAGS}
  )
  set(CONFIGURE_ENV_NO_PERL ${CONFIGURE_ENV})
  set(CONFIGURE_COMMAND ./configure ${PLATFORM_BUILD_TARGET})
  set(CONFIGURE_COMMAND_NO_TARGET ./configure)
endif()

set(DEFAULT_CMAKE_FLAGS
  -DCMAKE_BUILD_TYPE=${BUILD_MODE}
  -DCMAKE_C_FLAGS_DEBUG=${BLENDER_CMAKE_C_FLAGS_DEBUG}
  -DCMAKE_C_FLAGS_MINSIZEREL=${BLENDER_CMAKE_C_FLAGS_MINSIZEREL}
  -DCMAKE_C_FLAGS_RELEASE=${BLENDER_CMAKE_C_FLAGS_RELEASE}
  -DCMAKE_C_FLAGS_RELWITHDEBINFO=${BLENDER_CMAKE_C_FLAGS_RELWITHDEBINFO}
  -DCMAKE_CXX_FLAGS_DEBUG=${BLENDER_CMAKE_CXX_FLAGS_DEBUG}
  -DCMAKE_CXX_FLAGS_MINSIZEREL=${BLENDER_CMAKE_CXX_FLAGS_MINSIZEREL}
  -DCMAKE_CXX_FLAGS_RELEASE=${BLENDER_CMAKE_CXX_FLAGS_RELEASE}
  -DCMAKE_CXX_FLAGS_RELWITHDEBINFO=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}
  ${PLATFORM_CMAKE_FLAGS}
)

if(WIN32)
  if(BUILD_MODE STREQUAL Debug)
    set(ZLIB_LIBRARY zlibstaticd${LIBEXT})
  else()
    set(ZLIB_LIBRARY zlibstatic${LIBEXT})
  endif()
else()
  set(ZLIB_LIBRARY libz${LIBEXT})
endif()

if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(CMAKE_INSTALL_MESSAGE LAZY)
