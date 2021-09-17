# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


###########################################################################
# Helper macros
###########################################################################

macro(_set_default variable value)
  if(NOT ${variable})
    set(${variable} ${value})
  endif()
endmacro()

###########################################################################
# Precompiled libraries detection
#
# Use precompiled libraries from Blender repository
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(APPLE)
    set(_cycles_lib_dir "${CMAKE_SOURCE_DIR}/../lib/darwin")
  elseif(WIN32)
    if(CMAKE_CL_64)
      set(_cycles_lib_dir "${CMAKE_SOURCE_DIR}/../lib/win64_vc15")
    else()
      message(FATAL_ERROR "Unsupported Visual Studio Version")
    endif()
  else()
    # Path to a locally compiled libraries.
    set(LIBDIR_NAME ${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR})
    string(TOLOWER ${LIBDIR_NAME} LIBDIR_NAME)
    set(LIBDIR_NATIVE_ABI ${CMAKE_SOURCE_DIR}/../lib/${LIBDIR_NAME})

    # Path to precompiled libraries with known CentOS 7 ABI.
    set(LIBDIR_CENTOS7_ABI ${CMAKE_SOURCE_DIR}/../lib/linux_centos7_x86_64)

    # Choose the best suitable libraries.
    if(EXISTS ${LIBDIR_NATIVE_ABI})
      set(_cycles_lib_dir ${LIBDIR_NATIVE_ABI})
    elseif(EXISTS ${LIBDIR_CENTOS7_ABI})
      set(_cycles_lib_dir ${LIBDIR_CENTOS7_ABI})
      set(WITH_CXX11_ABI OFF)

      if(CMAKE_COMPILER_IS_GNUCC AND
         CMAKE_C_COMPILER_VERSION VERSION_LESS 9.3)
        message(FATAL_ERROR "GCC version must be at least 9.3 for precompiled libraries, found ${CMAKE_C_COMPILER_VERSION}")
      endif()
    endif()

    # Avoid namespace pollustion.
    unset(LIBDIR_NATIVE_ABI)
    unset(LIBDIR_CENTOS7_ABI)
  endif()

  if(EXISTS ${_cycles_lib_dir})
    _set_default(BOOST_ROOT "${_cycles_lib_dir}/boost")
    _set_default(BLOSC_ROOT_DIR "${_cycles_lib_dir}/blosc")
    _set_default(EMBREE_ROOT_DIR "${_cycles_lib_dir}/embree")
    _set_default(GLEW_ROOT_DIR "${_cycles_lib_dir}/glew")
    _set_default(JPEG_ROOT "${_cycles_lib_dir}/jpeg")
    _set_default(LLVM_ROOT_DIR "${_cycles_lib_dir}/llvm")
    _set_default(OPENCOLORIO_ROOT_DIR "${_cycles_lib_dir}/opencolorio")
    _set_default(OPENEXR_ROOT_DIR "${_cycles_lib_dir}/openexr")
    _set_default(OPENIMAGEDENOISE_ROOT_DIR "${_cycles_lib_dir}/openimagedenoise")
    _set_default(OPENIMAGEIO_ROOT_DIR "${_cycles_lib_dir}/openimageio")
    _set_default(OPENJPEG_ROOT_DIR "${_cycles_lib_dir}/openjpeg")
    _set_default(OPENSUBDIV_ROOT_DIR "${_cycles_lib_dir}/opensubdiv")
    _set_default(OPENVDB_ROOT_DIR "${_cycles_lib_dir}/openvdb")
    _set_default(OSL_ROOT_DIR "${_cycles_lib_dir}/osl")
    _set_default(PNG_ROOT "${_cycles_lib_dir}/png")
    _set_default(TBB_ROOT_DIR "${_cycles_lib_dir}/tbb")
    _set_default(TIFF_ROOT "${_cycles_lib_dir}/tiff")
    _set_default(ZLIB_ROOT "${_cycles_lib_dir}/zlib")

    # Ignore system libraries
    set(CMAKE_IGNORE_PATH "${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES};${CMAKE_SYSTEM_INCLUDE_PATH};${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES};${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")
  else()
    unset(_cycles_lib_dir)
  endif()
endif()

###########################################################################
# Zlib
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(ZLIB_INCLUDE_DIRS ${_cycles_lib_dir}/zlib/include)
    set(ZLIB_LIBRARIES ${_cycles_lib_dir}/zlib/lib/libz_st.lib)
    set(ZLIB_INCLUDE_DIR ${_cycles_lib_dir}/zlib/include)
    set(ZLIB_LIBRARY ${_cycles_lib_dir}/zlib/lib/libz_st.lib)
    set(ZLIB_DIR ${_cycles_lib_dir}/zlib)
    set(ZLIB_FOUND ON)
  else()
    find_package(ZLIB REQUIRED)
  endif()
endif()

###########################################################################
# PThreads
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(PTHREADS_LIBRARIES "${_cycles_lib_dir}/pthreads/lib/pthreadVC3.lib")
    include_directories("${_cycles_lib_dir}/pthreads/include")
  else()
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads REQUIRED)
    set(PTHREADS_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
  endif()
endif()

###########################################################################
# OpenImageIO and image libraries
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    add_definitions(
      # OIIO changed the name of this define in newer versions
      # we define both, so it would work with both old and new
      # versions.
      -DOIIO_STATIC_BUILD
      -DOIIO_STATIC_DEFINE
    )

    # Special exceptions for libraries which needs explicit debug version
    set(OPENIMAGEIO_LIBRARY
      optimized ${OPENIMAGEIO_ROOT_DIR}/lib/OpenImageIO.lib
      optimized ${OPENIMAGEIO_ROOT_DIR}/lib/OpenImageIO_Util.lib
      debug ${OPENIMAGEIO_ROOT_DIR}/lib/OpenImageIO_d.lib
      debug ${OPENIMAGEIO_ROOT_DIR}/lib/OpenImageIO_Util_d.lib
    )
  endif()

  find_package(OpenImageIO REQUIRED)
  if(OPENIMAGEIO_PUGIXML_FOUND)
    set(PUGIXML_INCLUDE_DIR "${OPENIMAGEIO_INCLUDE_DIR/OpenImageIO}")
    set(PUGIXML_LIBRARIES "")
  else()
    find_package(PugiXML REQUIRED)
  endif()

  # Dependencies
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(OPENJPEG_INCLUDE_DIR ${OPENJPEG}/include/openjpeg-2.3)
    set(OPENJPEG_LIBRARY ${_cycles_lib_dir}/openjpeg/lib/openjp2${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()

  find_package(JPEG REQUIRED)
  find_package(OpenJPEG REQUIRED)
  find_package(TIFF REQUIRED)
  find_package(PNG REQUIRED)
endif()

###########################################################################
# OpenEXR
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(OPENEXR_IEX_LIBRARY
      optimized ${OPENEXR_ROOT_DIR}/lib/Iex_s.lib
      debug ${OPENEXR_ROOT_DIR}/lib/Iex_s_d.lib
    )
    set(OPENEXR_HALF_LIBRARY
      optimized ${OPENEXR_ROOT_DIR}/lib/Half_s.lib
      debug ${OPENEXR_ROOT_DIR}/lib/Half_s_d.lib
    )
    set(OPENEXR_ILMIMF_LIBRARY
      optimized ${OPENEXR_ROOT_DIR}/lib/IlmImf_s.lib
      debug ${OPENEXR_ROOT_DIR}/lib/IlmImf_s_d.lib
    )
    set(OPENEXR_IMATH_LIBRARY
      optimized ${OPENEXR_ROOT_DIR}/lib/Imath_s.lib
      debug ${OPENEXR_ROOT_DIR}/lib/Imath_s_d.lib
    )
    set(OPENEXR_ILMTHREAD_LIBRARY
      optimized ${OPENEXR_ROOT_DIR}/lib/IlmThread_s.lib
      debug ${OPENEXR_ROOT_DIR}/lib/IlmThread_s_d.lib
    )
  endif()

  find_package(OpenEXR REQUIRED)
endif()

###########################################################################
# OpenShadingLanguage & LLVM
###########################################################################

if(WITH_CYCLES_OSL)
  if(CYCLES_STANDALONE_REPOSITORY)
    if(EXISTS ${_cycles_lib_dir})
      set(LLVM_STATIC ON)
    endif()

    find_package(OSL REQUIRED)
    find_package(LLVM REQUIRED)

    if(MSVC AND EXISTS ${_cycles_lib_dir})
      # TODO(sergey): On Windows llvm-config doesn't give proper results for the
      # library names, use hardcoded libraries for now.
      file(GLOB _llvm_libs_release ${LLVM_ROOT_DIR}/lib/*.lib)
      file(GLOB _llvm_libs_debug ${LLVM_ROOT_DIR}/debug/lib/*.lib)
      set(_llvm_libs)
      foreach(_llvm_lib_path ${_llvm_libs_release})
        get_filename_component(_llvm_lib_name ${_llvm_lib_path} ABSOLUTE)
        list(APPEND _llvm_libs optimized ${_llvm_lib_name})
      endforeach()
      foreach(_llvm_lib_path ${_llvm_libs_debug})
        get_filename_component(_llvm_lib_name ${_llvm_lib_path} ABSOLUTE)
        list(APPEND _llvm_libs debug ${_llvm_lib_name})
      endforeach()
      set(LLVM_LIBRARY ${_llvm_libs})
      unset(_llvm_lib_name)
      unset(_llvm_lib_path)
      unset(_llvm_libs)
      unset(_llvm_libs_debug)
      unset(_llvm_libs_release)

      set(OSL_LIBRARIES
        optimized ${OSL_ROOT_DIR}/lib/oslcomp.lib
        optimized ${OSL_ROOT_DIR}/lib/oslexec.lib
        optimized ${OSL_ROOT_DIR}/lib/oslquery.lib
        optimized ${OSL_ROOT_DIR}/lib/pugixml.lib
        debug ${OSL_ROOT_DIR}/lib/oslcomp_d.lib
        debug ${OSL_ROOT_DIR}/lib/oslexec_d.lib
        debug ${OSL_ROOT_DIR}/lib/oslquery_d.lib
        debug ${OSL_ROOT_DIR}/lib/pugixml_d.lib
      )
    endif()
  endif()
endif()

###########################################################################
# OpenColorIO
###########################################################################

if(WITH_CYCLES_OPENCOLORIO)
  if(CYCLES_STANDALONE_REPOSITORY)
    find_package(OpenColorIO REQUIRED)
    set(WITH_OPENCOLORIO ON)

    if(MSVC AND EXISTS ${_cycles_lib_dir})
      set(OPENCOLORIO_LIBPATH ${_cycles_lib_dir}/opencolorio/lib)
      set(OPENCOLORIO_LIBRARIES
        optimized ${OPENCOLORIO_LIBPATH}/OpenColorIO.lib
        optimized ${OPENCOLORIO_LIBPATH}/tinyxml.lib
        optimized ${OPENCOLORIO_LIBPATH}/libyaml-cpp.lib
        debug ${OPENCOLORIO_LIBPATH}/OpencolorIO_d.lib
        debug ${OPENCOLORIO_LIBPATH}/tinyxml_d.lib
        debug ${OPENCOLORIO_LIBPATH}/libyaml-cpp_d.lib
      )
    endif()
  endif()
endif()

###########################################################################
# Boost
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(EXISTS ${_cycles_lib_dir})
    if(MSVC)
      set(Boost_USE_STATIC_RUNTIME OFF)
      set(Boost_USE_MULTITHREADED ON)
      set(Boost_USE_STATIC_LIBS ON)
    else()
      set(BOOST_LIBRARYDIR ${_cycles_lib_dir}/boost/lib)
      set(Boost_NO_BOOST_CMAKE ON)
      set(Boost_NO_SYSTEM_PATHS ON)
    endif()
  endif()

  set(__boost_packages filesystem regex system thread date_time)
  if(WITH_CYCLES_OSL)
    list(APPEND __boost_packages wave)
  endif()
  find_package(Boost 1.48 COMPONENTS ${__boost_packages} REQUIRED)
  if(NOT Boost_FOUND)
    # Try to find non-multithreaded if -mt not found, this flag
    # doesn't matter for us, it has nothing to do with thread
    # safety, but keep it to not disturb build setups.
    set(Boost_USE_MULTITHREADED OFF)
    find_package(Boost 1.48 COMPONENTS ${__boost_packages})
  endif()
  unset(__boost_packages)
  set(BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
  set(BOOST_LIBRARIES ${Boost_LIBRARIES})
  set(BOOST_LIBPATH ${Boost_LIBRARY_DIRS})
  set(BOOST_DEFINITIONS "-DBOOST_ALL_NO_LIB")
endif()

###########################################################################
# Embree
###########################################################################

if(WITH_CYCLES_EMBREE)
  if(CYCLES_STANDALONE_REPOSITORY)
    if(MSVC AND EXISTS ${_cycles_lib_dir})
      set(EMBREE_TASKING_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/tasking.lib
        debug  ${EMBREE_ROOT_DIR}/lib/tasking_d.lib
      )
      set(EMBREE_EMBREE3_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/embree3.lib
        debug  ${EMBREE_ROOT_DIR}/lib/embree3_d.lib
      )
      set(EMBREE_EMBREE_AVX_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/embree_avx.lib
        debug  ${EMBREE_ROOT_DIR}/lib/embree_avx_d.lib
      )
      set(EMBREE_EMBREE_AVX2_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/embree_avx2.lib
        debug  ${EMBREE_ROOT_DIR}/lib/embree_avx2_d.lib
      )
      set(EMBREE_EMBREE_SSE42_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/embree_sse42.lib
        debug  ${EMBREE_ROOT_DIR}/lib/embree_sse42_d.lib
      )
      set(EMBREE_LEXERS_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/lexers.lib
        debug  ${EMBREE_ROOT_DIR}/lib/lexers_d.lib
      )
      set(EMBREE_MATH_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/math.lib
        debug  ${EMBREE_ROOT_DIR}/lib/math_d.lib
      )
      set(EMBREE_SIMD_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/simd.lib
        debug  ${EMBREE_ROOT_DIR}/lib/simd_d.lib
      )
      set(EMBREE_SYS_LIBRARY
        optimized ${EMBREE_ROOT_DIR}/lib/sys.lib
        debug  ${EMBREE_ROOT_DIR}/lib/sys_d.lib
      )
    endif()

    find_package(Embree 3.8.0 REQUIRED)
  endif()
endif()

###########################################################################
# Logging
###########################################################################

if(WITH_CYCLES_LOGGING)
  if(CYCLES_STANDALONE_REPOSITORY)
    find_package(Glog REQUIRED)
    find_package(Gflags REQUIRED)
  endif()
endif()

###########################################################################
# OpenSubdiv
###########################################################################

if(WITH_CYCLES_OPENSUBDIV)
  if(CYCLES_STANDALONE_REPOSITORY)
    find_package(OpenSubdiv REQUIRED)
    set(WITH_OPENSUBDIV ON)

    if(MSVC AND EXISTS ${_cycles_lib_dir})
      set(OPENSUBDIV_LIBRARIES
        optimized ${OPENSUBDIV_ROOT_DIR}/lib/osdCPU.lib
        optimized ${OPENSUBDIV_ROOT_DIR}/lib/osdGPU.lib
        debug ${OPENSUBDIV_ROOT_DIR}/lib/osdCPU_d.lib
        debug ${OPENSUBDIV_ROOT_DIR}/lib/osdGPU_d.lib
      )
    endif()
  endif()
endif()

###########################################################################
# OpenVDB
###########################################################################

if(WITH_CYCLES_OPENVDB)
  if(CYCLES_STANDALONE_REPOSITORY)
    if(MSVC AND EXISTS ${_cycles_lib_dir})
      set(BLOSC_LIBRARY
          optimized ${BLOSC_ROOT_DIR}/lib/libblosc.lib
          debug ${BLOSC_ROOT_DIR}/lib/libblosc_d.lib)
    endif()

    find_package(OpenVDB REQUIRED)
    find_package(Blosc REQUIRED)
    set(WITH_OPENVDB ON)
    set(OPENVDB_DEFINITIONS -DNOMINMAX -D_USE_MATH_DEFINES)
  endif()
endif()

###########################################################################
# OpenImageDenoise
###########################################################################

if(WITH_CYCLES_OPENIMAGEDENOISE)
  if(CYCLES_STANDALONE_REPOSITORY)
    find_package(OpenImageDenoise REQUIRED)
    set(WITH_OPENIMAGEDENOISE ON)

    if(MSVC AND EXISTS ${_cycles_lib_dir})
      set(OPENIMAGEDENOISE_LIBRARIES
        optimized ${OPENIMAGEDENOISE_ROOT_DIR}/lib/OpenImageDenoise.lib
        optimized ${OPENIMAGEDENOISE_ROOT_DIR}/lib/common.lib
        optimized ${OPENIMAGEDENOISE_ROOT_DIR}/lib/dnnl.lib
        debug ${OPENIMAGEDENOISE_ROOT_DIR}/lib/OpenImageDenoise_d.lib
        debug ${OPENIMAGEDENOISE_ROOT_DIR}/lib/common_d.lib
        debug ${OPENIMAGEDENOISE_ROOT_DIR}/lib/dnnl_d.lib)
    endif()

  endif()
endif()

###########################################################################
# TBB
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(TBB_LIBRARY
      optimized ${TBB_ROOT_DIR}/lib/tbb.lib
      debug ${TBB_ROOT_DIR}/lib/debug/tbb_debug.lib
    )
  endif()

  find_package(TBB REQUIRED)
endif()

###########################################################################
# GLEW
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(MSVC AND EXISTS ${_cycles_lib_dir})
    set(GLEW_LIBRARY "${_cycles_lib_dir}/opengl/lib/glew.lib")
    set(GLEW_INCLUDE_DIR "${_cycles_lib_dir}/opengl/include")
    add_definitions(-DGLEW_STATIC)
  endif()

  find_package(GLEW REQUIRED)
else()
  # Workaround for unconventional variable name use in Blender.
  set(GLEW_INCLUDE_DIR "${GLEW_INCLUDE_PATH}")
endif()

###########################################################################
# System Libraries
###########################################################################

# Detect system libraries again
if(EXISTS ${_cycles_lib_dir})
  unset(CMAKE_IGNORE_PATH)
endif()

###########################################################################
# OpenGL
###########################################################################

if(CYCLES_STANDALONE_REPOSITORY)
  if(NOT DEFINED OpenGL_GL_PREFERENCE)
    set(OpenGL_GL_PREFERENCE "LEGACY")
  endif()

  find_package(OpenGL REQUIRED)

  set(CYCLES_GL_LIBRARIES
    ${OPENGL_gl_LIBRARY}
    ${OPENGL_glu_LIBRARY}
    ${GLEW_LIBRARY}
  )
else()
  set(CYCLES_GL_LIBRARIES
    bf_intern_glew_mx
    ${BLENDER_GL_LIBRARIES}
    ${BLENDER_GLEW_LIBRARIES})
endif()

###########################################################################
# SDL
###########################################################################

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
  # We can't use the version from the Blender precompiled libraries because
  # it does not include the video subsystem.
  find_package(SDL2)

  if(NOT SDL2_FOUND)
    set(WITH_CYCLES_STANDALONE_GUI OFF)
    message(STATUS "SDL not found, disabling Cycles standalone GUI")
  endif()

  include_directories(
    SYSTEM
    ${SDL2_INCLUDE_DIRS}
  )
endif()

###########################################################################
# CUDA
###########################################################################

if(WITH_CYCLES_CUDA_BINARIES OR NOT WITH_CUDA_DYNLOAD)
  find_package(CUDA) # Try to auto locate CUDA toolkit
  if(CUDA_FOUND)
    message(STATUS "Found CUDA ${CUDA_NVCC_EXECUTABLE} (${CUDA_VERSION})")
  else()
    message(STATUS "CUDA compiler not found, disabling WITH_CYCLES_CUDA_BINARIES")
    set(WITH_CYCLES_CUDA_BINARIES OFF)
    if(NOT WITH_CUDA_DYNLOAD)
      message(STATUS "Additionally falling back to dynamic CUDA load")
      set(WITH_CUDA_DYNLOAD ON)
    endif()
  endif()
endif()


###########################################################################
# HIP
###########################################################################

if(WITH_CYCLES_HIP_BINARIES AND WITH_CYCLES_DEVICE_HIP)
  find_package(HIP)
  if(HIP_FOUND)
    message(STATUS "Found HIP ${HIP_HIPCC_EXECUTABLE} (${HIP_VERSION})")
  else()
    message(STATUS "HIP compiler not found, disabling WITH_CYCLES_HIP_BINARIES")
    set(WITH_CYCLES_HIP_BINARIES OFF)
  endif()
endif()

if(NOT WITH_HIP_DYNLOAD)
  set(WITH_HIP_DYNLOAD ON)
endif()

###########################################################################
# Metal
###########################################################################

if(WITH_CYCLES_DEVICE_METAL)
  find_library(METAL_LIBRARY Metal)

  # This file was added in the 12.0 SDK, use it as a way to detect the version.
  if(METAL_LIBRARY AND NOT EXISTS "${METAL_LIBRARY}/Headers/MTLFunctionStitching.h")
    message(STATUS "Metal version too old, must be SDK 12.0 or newer, disabling WITH_CYCLES_DEVICE_METAL")
    set(WITH_CYCLES_DEVICE_METAL OFF)
  elseif(NOT METAL_LIBRARY)
    message(STATUS "Metal not found, disabling WITH_CYCLES_DEVICE_METAL")
    set(WITH_CYCLES_DEVICE_METAL OFF)
  else()
    message(STATUS "Found Metal: ${METAL_LIBRARY}")
  endif()
endif()

unset(_cycles_lib_dir)
