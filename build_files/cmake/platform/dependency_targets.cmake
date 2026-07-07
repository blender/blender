# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Common modern targets for the blender dependencies
#
# The optional dependencies in the bf::dependencies::optional namespace
# will always exist, but will only be populated if the dep is actually
# enabled. Doing it this way, prevents us from having to sprinkle
# if(WITH_SOMEDEP) all over cmake, and you can just add
# `bf::dependencies::optional::somedep` to the LIB section without
# having to worry if it's enabled or not at the consumer site.

# -----------------------------------------------------------------------------
# Configure TBB

add_library(bf_deps_optional_tbb INTERFACE)
add_library(bf::dependencies::optional::tbb ALIAS bf_deps_optional_tbb)

if(WITH_TBB)
  target_compile_definitions(bf_deps_optional_tbb INTERFACE WITH_TBB)
  target_include_directories(bf_deps_optional_tbb SYSTEM INTERFACE ${TBB_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_tbb INTERFACE ${TBB_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Manifold

add_library(bf_deps_optional_manifold INTERFACE)
add_library(bf::dependencies::optional::manifold ALIAS bf_deps_optional_manifold)
if(WITH_MANIFOLD)
  if(WIN32)
    target_compile_definitions(bf_deps_optional_manifold INTERFACE WITH_MANIFOLD)
    target_include_directories(bf_deps_optional_manifold SYSTEM INTERFACE ${MANIFOLD_INCLUDE_DIRS})
    target_link_libraries(bf_deps_optional_manifold INTERFACE ${MANIFOLD_LIBRARIES} bf::dependencies::optional::tbb)
  else()
    if(TARGET manifold::manifold)
      target_compile_definitions(bf_deps_optional_manifold INTERFACE WITH_MANIFOLD)
      target_link_libraries(bf_deps_optional_manifold INTERFACE manifold::manifold)
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Configure OpenVDB

add_library(bf_deps_optional_openvdb INTERFACE)
add_library(bf::dependencies::optional::openvdb ALIAS bf_deps_optional_openvdb)

if(WITH_OPENVDB)
  target_compile_definitions(bf_deps_optional_openvdb INTERFACE WITH_OPENVDB)
  if(WITH_OPENVDB_BLOSC)
    target_compile_definitions(bf_deps_optional_openvdb INTERFACE WITH_OPENVDB_BLOSC)
  endif()
  if(WITH_OPENVDB_3_ABI_COMPATIBLE)
    target_compile_definitions(bf_deps_optional_openvdb INTERFACE OPENVDB_3_ABI_COMPATIBLE)
  endif()

  target_include_directories(bf_deps_optional_openvdb SYSTEM INTERFACE ${OPENVDB_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_openvdb
    INTERFACE
    ${OPENVDB_LIBRARIES}
    bf::dependencies::optional::tbb
  )
endif()

# -----------------------------------------------------------------------------
# Configure Ceres
add_library(bf_deps_optional_ceres INTERFACE)
add_library(bf::dependencies::optional::ceres ALIAS bf_deps_optional_ceres)

if(TARGET Ceres::ceres)
  target_compile_definitions(bf_deps_optional_ceres INTERFACE WITH_CERES)
  target_link_libraries(bf_deps_optional_ceres INTERFACE Ceres::ceres)
endif()

# -----------------------------------------------------------------------------
# Configure Eigen

add_library(bf_deps_eigen INTERFACE)
add_library(bf::dependencies::eigen ALIAS bf_deps_eigen)
target_link_libraries(bf_deps_eigen INTERFACE Eigen3::Eigen)

if(WITH_TBB)
  target_compile_definitions(bf_deps_eigen INTERFACE EIGEN_HAS_TBB)
  target_include_directories(bf_deps_eigen SYSTEM INTERFACE ${TBB_INCLUDE_DIRS})
  target_link_libraries(bf_deps_eigen INTERFACE ${TBB_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenColorIO

add_library(bf_deps_optional_opencolorio INTERFACE)
add_library(bf::dependencies::optional::opencolorio ALIAS bf_deps_optional_opencolorio)

if(WITH_OPENCOLORIO)
  target_compile_definitions(bf_deps_optional_opencolorio INTERFACE WITH_OPENCOLORIO)
  target_include_directories(bf_deps_optional_opencolorio SYSTEM INTERFACE ${OPENCOLORIO_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_opencolorio INTERFACE ${OPENCOLORIO_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Zlib

add_library(bf_deps_zlib INTERFACE)
add_library(bf::dependencies::zlib ALIAS bf_deps_zlib)

target_include_directories(bf_deps_zlib SYSTEM INTERFACE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(bf_deps_zlib INTERFACE ${ZLIB_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure ZSTD

add_library(bf_deps_zstd INTERFACE)
add_library(bf::dependencies::zstd ALIAS bf_deps_zstd)

target_include_directories(bf_deps_zstd SYSTEM INTERFACE ${ZSTD_INCLUDE_DIRS})
target_link_libraries(bf_deps_zstd INTERFACE ${ZSTD_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure Freetype

add_library(bf_deps_freetype INTERFACE)
add_library(bf::dependencies::freetype ALIAS bf_deps_freetype)

target_include_directories(bf_deps_freetype SYSTEM INTERFACE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(bf_deps_freetype INTERFACE ${FREETYPE_LIBRARIES} ${BROTLI_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure JPEG

add_library(bf_deps_jpeg INTERFACE)
add_library(bf::dependencies::jpeg ALIAS bf_deps_jpeg)

target_include_directories(bf_deps_jpeg SYSTEM INTERFACE ${JPEG_INCLUDE_DIR})
target_link_libraries(bf_deps_jpeg INTERFACE ${JPEG_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure PNG

add_library(bf_deps_png INTERFACE)
add_library(bf::dependencies::png ALIAS bf_deps_png)

target_include_directories(bf_deps_png SYSTEM INTERFACE ${PNG_INCLUDE_DIRS})
target_link_libraries(bf_deps_png INTERFACE ${PNG_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure OpenImageIO

add_library(bf::dependencies::openimageio ALIAS OpenImageIO::OpenImageIO)
get_target_property(OPENIMAGEIO_TOOL OpenImageIO::oiiotool LOCATION)

# -----------------------------------------------------------------------------
# Configure USD

add_library(bf_deps_optional_usd INTERFACE)
add_library(bf::dependencies::optional::usd ALIAS bf_deps_optional_usd)

if(WITH_USD)
  target_compile_definitions(bf_deps_optional_usd INTERFACE WITH_USD)
  target_include_directories(bf_deps_optional_usd SYSTEM INTERFACE ${USD_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_usd INTERFACE ${USD_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Alembic

add_library(bf_deps_optional_alembic INTERFACE)
add_library(bf::dependencies::optional::alembic ALIAS bf_deps_optional_alembic)

if(WITH_ALEMBIC)
  target_compile_definitions(bf_deps_optional_alembic INTERFACE WITH_ALEMBIC)
  target_include_directories(bf_deps_optional_alembic SYSTEM INTERFACE ${ALEMBIC_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_alembic INTERFACE ${ALEMBIC_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenSubdiv

add_library(bf_deps_optional_opensubdiv INTERFACE)
add_library(bf::dependencies::optional::opensubdiv ALIAS bf_deps_optional_opensubdiv)

if(WITH_OPENSUBDIV)
  target_compile_definitions(bf_deps_optional_opensubdiv INTERFACE WITH_OPENSUBDIV)
  target_include_directories(bf_deps_optional_opensubdiv SYSTEM INTERFACE ${OPENSUBDIV_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_opensubdiv INTERFACE ${OPENSUBDIV_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenEXR

add_library(bf_deps_optional_openexr INTERFACE)
add_library(bf::dependencies::optional::openexr ALIAS bf_deps_optional_openexr)

if(WITH_IMAGE_OPENEXR)
  target_compile_definitions(bf_deps_optional_openexr INTERFACE WITH_IMAGE_OPENEXR)
  target_link_libraries(bf_deps_optional_openexr INTERFACE OpenEXR::OpenEXR)
endif()

# -----------------------------------------------------------------------------
# Configure WebP

add_library(bf_deps_optional_webp INTERFACE)
add_library(bf::dependencies::optional::webp ALIAS bf_deps_optional_webp)

if(WITH_IMAGE_WEBP)
  target_compile_definitions(bf_deps_optional_webp INTERFACE WITH_IMAGE_WEBP)
  target_include_directories(bf_deps_optional_webp SYSTEM INTERFACE ${WEBP_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_webp INTERFACE ${WEBP_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenJPEG

add_library(bf_deps_optional_openjpeg INTERFACE)
add_library(bf::dependencies::optional::openjpeg ALIAS bf_deps_optional_openjpeg)

if(WITH_IMAGE_OPENJPEG)
  target_compile_definitions(bf_deps_optional_openjpeg INTERFACE WITH_IMAGE_OPENJPEG)
  target_compile_definitions(bf_deps_optional_openjpeg INTERFACE ${OPENJPEG_DEFINES})
  target_include_directories(bf_deps_optional_openjpeg SYSTEM INTERFACE ${OPENJPEG_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_openjpeg INTERFACE ${OPENJPEG_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure SDL

add_library(bf_deps_optional_sdl INTERFACE)
add_library(bf::dependencies::optional::sdl ALIAS bf_deps_optional_sdl)

if(WITH_SDL)
  target_compile_definitions(bf_deps_optional_sdl INTERFACE WITH_SDL)
  target_include_directories(bf_deps_optional_sdl SYSTEM INTERFACE ${SDL_INCLUDE_DIR})
  target_link_libraries(bf_deps_optional_sdl INTERFACE ${SDL_LIBRARY})
endif()

# -----------------------------------------------------------------------------
# Configure FFmpeg

add_library(bf_deps_optional_ffmpeg INTERFACE)
add_library(bf::dependencies::optional::ffmpeg ALIAS bf_deps_optional_ffmpeg)

if(WITH_CODEC_FFMPEG)
  target_compile_definitions(bf_deps_optional_ffmpeg INTERFACE WITH_FFMPEG)
  target_include_directories(bf_deps_optional_ffmpeg SYSTEM INTERFACE ${FFMPEG_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_ffmpeg INTERFACE ${FFMPEG_LIBRARIES})
  if(WITH_IMAGE_OPENJPEG)
    target_link_libraries(bf_deps_optional_ffmpeg INTERFACE ${OPENJPEG_LIBRARIES})
  endif()
endif()

# -----------------------------------------------------------------------------
# Configure Python

add_library(bf_deps_optional_python INTERFACE)
add_library(bf::dependencies::optional::python ALIAS bf_deps_optional_python)

if(WITH_PYTHON)
  target_compile_definitions(bf_deps_optional_python INTERFACE WITH_PYTHON)
  target_include_directories(bf_deps_optional_python SYSTEM INTERFACE ${PYTHON_INCLUDE_DIR})
  target_link_libraries(bf_deps_optional_python INTERFACE ${PYTHON_LINKFLAGS})
  if(WITH_PYTHON_MODULE)
    target_compile_definitions(bf_deps_optional_python INTERFACE WITH_PYTHON_MODULE)
  else()
    target_link_libraries(bf_deps_optional_python INTERFACE ${PYTHON_LIBRARIES})
  endif()
endif()

# -----------------------------------------------------------------------------
# Configure GMP

add_library(bf_deps_optional_gmp INTERFACE)
add_library(bf::dependencies::optional::gmp ALIAS bf_deps_optional_gmp)

if(WITH_GMP)
  target_compile_definitions(bf_deps_optional_gmp INTERFACE WITH_GMP)
  target_include_directories(bf_deps_optional_gmp SYSTEM INTERFACE ${GMP_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_gmp INTERFACE ${GMP_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure PugiXML

add_library(bf_deps_optional_pugixml INTERFACE)
add_library(bf::dependencies::optional::pugixml ALIAS bf_deps_optional_pugixml)

if(WITH_PUGIXML)
  target_compile_definitions(bf_deps_optional_pugixml INTERFACE WITH_PUGIXML)
  target_include_directories(bf_deps_optional_pugixml SYSTEM INTERFACE ${PUGIXML_INCLUDE_DIR})
  target_link_libraries(bf_deps_optional_pugixml INTERFACE ${PUGIXML_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Haru

add_library(bf_deps_optional_haru INTERFACE)
add_library(bf::dependencies::optional::haru ALIAS bf_deps_optional_haru)

if(WITH_HARU)
  target_compile_definitions(bf_deps_optional_haru INTERFACE WITH_HARU)
  target_include_directories(bf_deps_optional_haru SYSTEM INTERFACE ${HARU_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_haru INTERFACE ${HARU_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Vulkan

add_library(bf_deps_optional_vulkan INTERFACE)
add_library(bf::dependencies::optional::vulkan ALIAS bf_deps_optional_vulkan)

if(WITH_VULKAN_BACKEND)
  target_include_directories(bf_deps_optional_vulkan SYSTEM INTERFACE ${VULKAN_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_vulkan INTERFACE ${VULKAN_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure ShaderC

add_library(bf_deps_optional_shaderc INTERFACE)
add_library(bf::dependencies::optional::shaderc ALIAS bf_deps_optional_shaderc)

if(WITH_VULKAN_BACKEND)
  target_compile_definitions(bf_deps_optional_shaderc INTERFACE WITH_SHADERC)
  target_include_directories(bf_deps_optional_shaderc SYSTEM INTERFACE ${SHADERC_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_shaderc INTERFACE ${SHADERC_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Epoxy

add_library(bf_deps_epoxy INTERFACE)
add_library(bf::dependencies::epoxy ALIAS bf_deps_epoxy)

target_include_directories(bf_deps_epoxy SYSTEM INTERFACE ${Epoxy_INCLUDE_DIRS})
target_link_libraries(bf_deps_epoxy INTERFACE ${Epoxy_LIBRARIES})

# -----------------------------------------------------------------------------
# Configure Gflags

add_library(bf_deps_gflags INTERFACE)
add_library(bf::dependencies::gflags ALIAS bf_deps_gflags)

if(WITH_LIBMV OR WITH_GTESTS)
  target_compile_definitions(bf_deps_gflags INTERFACE ${GFLAGS_DEFINES})
  target_include_directories(bf_deps_gflags SYSTEM INTERFACE ${GFLAGS_INCLUDE_DIRS})
  target_link_libraries(bf_deps_gflags INTERFACE ${GFLAGS_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Glog

add_library(bf_deps_glog INTERFACE)
add_library(bf::dependencies::glog ALIAS bf_deps_glog)

if(WITH_LIBMV OR WITH_GTESTS)
  target_compile_definitions(bf_deps_glog INTERFACE ${GLOG_DEFINES})
  target_include_directories(bf_deps_glog SYSTEM INTERFACE ${GLOG_INCLUDE_DIRS})
  target_link_libraries(bf_deps_glog INTERFACE ${GLOG_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure OpenImageDenoise

add_library(bf_deps_optional_openimagedenoise INTERFACE)
add_library(bf::dependencies::optional::openimagedenoise ALIAS bf_deps_optional_openimagedenoise)

if(WITH_OPENIMAGEDENOISE)
  target_compile_definitions(bf_deps_optional_openimagedenoise INTERFACE WITH_OPENIMAGEDENOISE)
  target_compile_definitions(bf_deps_optional_openimagedenoise INTERFACE OIDN_STATIC_LIB)
  target_include_directories(bf_deps_optional_openimagedenoise SYSTEM INTERFACE ${OPENIMAGEDENOISE_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_openimagedenoise INTERFACE ${OPENIMAGEDENOISE_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure FFTW3

add_library(bf_deps_optional_fftw3 INTERFACE)
add_library(bf::dependencies::optional::fftw3 ALIAS bf_deps_optional_fftw3)

if(WITH_FFTW3)
  target_compile_definitions(bf_deps_optional_fftw3 INTERFACE WITH_FFTW3)
  target_include_directories(bf_deps_optional_fftw3 SYSTEM INTERFACE ${FFTW3_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_fftw3 INTERFACE ${FFTW3_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Bullet

add_library(bf_deps_optional_bullet INTERFACE)
add_library(bf::dependencies::optional::bullet ALIAS bf_deps_optional_bullet)

if(WITH_BULLET)
  target_compile_definitions(bf_deps_optional_bullet INTERFACE WITH_BULLET)
  target_include_directories(bf_deps_optional_bullet SYSTEM INTERFACE ${BULLET_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_bullet INTERFACE ${BULLET_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Audaspace

add_library(bf_deps_optional_audaspace INTERFACE)
add_library(bf::dependencies::optional::audaspace ALIAS bf_deps_optional_audaspace)

if(WITH_AUDASPACE)
  target_compile_definitions(bf_deps_optional_audaspace INTERFACE WITH_AUDASPACE)
  if(WITH_RUBBERBAND)
    target_compile_definitions(bf_deps_optional_audaspace INTERFACE WITH_RUBBERBAND)
  endif()
  target_include_directories(bf_deps_optional_audaspace SYSTEM INTERFACE ${AUDASPACE_C_INCLUDE_DIRS} ${AUDASPACE_PY_INCLUDE_DIRS})
  if(WITH_SYSTEM_AUDASPACE)
    target_link_libraries(bf_deps_optional_audaspace INTERFACE ${AUDASPACE_C_LIBRARIES} ${AUDASPACE_PY_LIBRARIES})
  else()
    target_link_libraries(bf_deps_optional_audaspace INTERFACE audaspace audaspace-py)
  endif()
endif()

# -----------------------------------------------------------------------------
# Configure Potrace

add_library(bf_deps_optional_potrace INTERFACE)
add_library(bf::dependencies::optional::potrace ALIAS bf_deps_optional_potrace)

if(WITH_POTRACE)
  target_compile_definitions(bf_deps_optional_potrace INTERFACE WITH_POTRACE)
  target_include_directories(bf_deps_optional_potrace SYSTEM INTERFACE ${POTRACE_INCLUDE_DIRS})
  target_link_libraries(bf_deps_optional_potrace INTERFACE ${POTRACE_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure Pthreads
#
add_library(bf_deps_pthreads INTERFACE)
add_library(bf::dependencies::pthreads ALIAS bf_deps_pthreads)

# Include directories on non-Windows are handled globally.
if(WIN32 AND NOT UNIX)
  target_include_directories(bf_deps_pthreads SYSTEM INTERFACE ${PTHREADS_INC})
endif()
if(DEFINED PTHREADS_LIBRARIES)
  target_link_libraries(bf_deps_pthreads INTERFACE ${PTHREADS_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Configure libfmt
#

add_library(bf::dependencies::fmt ALIAS fmt::fmt)

# -----------------------------------------------------------------------------
# Configure OSL

if(WITH_CYCLES_OSL)
  add_library(bf_deps_optional_osl INTERFACE)
  target_link_libraries(bf_deps_optional_osl INTERFACE OSL::oslcomp OSL::oslquery OSL::oslnoise)
  # Link oslexec with the -force_load flag on macOS.
  if(APPLE)
    target_link_libraries(bf_deps_optional_osl INTERFACE -force_load OSL::oslexec)
  else()
    target_link_libraries(bf_deps_optional_osl INTERFACE OSL::oslexec)
  endif()
  add_library(bf::dependencies::optional::osl ALIAS bf_deps_optional_osl)
  get_target_property(OSL_COMPILER OSL::oslc LOCATION)
else()
  add_library(bf_deps_optional_osl INTERFACE)
  add_library(bf::dependencies::optional::osl ALIAS bf_deps_optional_osl)
endif()
