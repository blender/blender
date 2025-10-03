# SPDX-FileCopyrightText: 2014-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Turn everything ON that's expected for an official release builds.
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_release.cmake  ../blender
#
# NOTE: the built-bot supports configuration overrides for some of these settings.
# This means the daily-builds may not match this configuration *exactly*,
# see: `build_files/buildbot/config/*.cmake`.

set(WITH_ALEMBIC             ON  CACHE BOOL "" FORCE)
set(WITH_ASSERT_ABORT        OFF CACHE BOOL "" FORCE)
set(WITH_AUDASPACE           ON  CACHE BOOL "" FORCE)
set(WITH_BUILDINFO           ON  CACHE BOOL "" FORCE)
set(WITH_BULLET              ON  CACHE BOOL "" FORCE)
set(WITH_CODEC_FFMPEG        ON  CACHE BOOL "" FORCE)
set(WITH_CODEC_SNDFILE       ON  CACHE BOOL "" FORCE)
set(WITH_CYCLES              ON  CACHE BOOL "" FORCE)
set(WITH_CYCLES_EMBREE       ON  CACHE BOOL "" FORCE)
set(WITH_CYCLES_OSL          ON  CACHE BOOL "" FORCE)
set(WITH_CYCLES_PATH_GUIDING ON  CACHE BOOL "" FORCE)
set(WITH_DRACO               ON  CACHE BOOL "" FORCE)
set(WITH_FFTW3               ON  CACHE BOOL "" FORCE)
set(WITH_FREESTYLE           ON  CACHE BOOL "" FORCE)
set(WITH_GMP                 ON  CACHE BOOL "" FORCE)
set(WITH_HARU                ON  CACHE BOOL "" FORCE)
set(WITH_IK_ITASC            ON  CACHE BOOL "" FORCE)
set(WITH_IK_SOLVER           ON  CACHE BOOL "" FORCE)
set(WITH_IMAGE_CINEON        ON  CACHE BOOL "" FORCE)
set(WITH_IMAGE_OPENEXR       ON  CACHE BOOL "" FORCE)
set(WITH_IMAGE_OPENJPEG      ON  CACHE BOOL "" FORCE)
set(WITH_IMAGE_WEBP          ON  CACHE BOOL "" FORCE)
set(WITH_INPUT_NDOF          ON  CACHE BOOL "" FORCE)
set(WITH_INPUT_IME           ON  CACHE BOOL "" FORCE)
set(WITH_INTERNATIONAL       ON  CACHE BOOL "" FORCE)
set(WITH_LIBMV               ON  CACHE BOOL "" FORCE)
set(WITH_LIBMV_SCHUR_SPECIALIZATIONS ON CACHE BOOL "" FORCE)
set(WITH_MOD_FLUID           ON  CACHE BOOL "" FORCE)
set(WITH_MOD_OCEANSIM        ON  CACHE BOOL "" FORCE)
set(WITH_MOD_REMESH          ON  CACHE BOOL "" FORCE)
set(WITH_UV_SLIM             ON  CACHE BOOL "" FORCE)
set(WITH_NANOVDB             ON  CACHE BOOL "" FORCE)
set(WITH_OPENAL              ON  CACHE BOOL "" FORCE)
set(WITH_OPENCOLORIO         ON  CACHE BOOL "" FORCE)
set(WITH_OPENIMAGEDENOISE    ON  CACHE BOOL "" FORCE)
set(WITH_OPENSUBDIV          ON  CACHE BOOL "" FORCE)
set(WITH_OPENVDB             ON  CACHE BOOL "" FORCE)
set(WITH_OPENVDB_BLOSC       ON  CACHE BOOL "" FORCE)
set(WITH_POTRACE             ON  CACHE BOOL "" FORCE)
set(WITH_PUGIXML             ON  CACHE BOOL "" FORCE)
set(WITH_PYTHON_INSTALL      ON  CACHE BOOL "" FORCE)
set(WITH_RUBBERBAND          ON  CACHE BOOL "" FORCE)
set(WITH_QUADRIFLOW          ON  CACHE BOOL "" FORCE)
set(WITH_SDL                 OFF CACHE BOOL "" FORCE)
set(WITH_TBB                 ON  CACHE BOOL "" FORCE)
set(WITH_USD                 ON  CACHE BOOL "" FORCE)
set(WITH_MATERIALX           ON  CACHE BOOL "" FORCE)
set(WITH_HYDRA               ON  CACHE BOOL "" FORCE)

set(WITH_MEM_JEMALLOC          ON  CACHE BOOL "" FORCE)

# platform dependent options
if(APPLE)
  set(WITH_COREAUDIO           ON  CACHE BOOL "" FORCE)
  set(WITH_CYCLES_DEVICE_METAL ON  CACHE BOOL "" FORCE)
  set(WITH_BLENDER_THUMBNAILER ON  CACHE BOOL "" FORCE)
endif()
if(WIN32)
  set(WITH_WASAPI               ON  CACHE BOOL "" FORCE)
endif()
if(UNIX AND NOT APPLE)
  set(WITH_JACK                ON  CACHE BOOL "" FORCE)
  set(WITH_DOC_MANPAGE         ON  CACHE BOOL "" FORCE)
  set(WITH_GHOST_XDND          ON  CACHE BOOL "" FORCE)
  set(WITH_PULSEAUDIO          ON  CACHE BOOL "" FORCE)
  set(WITH_X11_XINPUT          ON  CACHE BOOL "" FORCE)
  set(WITH_JACK_DYNLOAD        ON  CACHE BOOL "" FORCE)
  set(WITH_PULSEAUDIO_DYNLOAD  ON  CACHE BOOL "" FORCE)
  # TODO don't disable pipewire on release builds once it has gotten more testing
  set(WITH_PIPEWIRE            OFF  CACHE BOOL "" FORCE)
  set(WITH_PIPEWIRE_DYNLOAD    ON  CACHE BOOL "" FORCE)
endif()
if(NOT APPLE)
  set(WITH_XR_OPENXR              ON  CACHE BOOL "" FORCE)

  # Can't use CMAKE_SYSTEM_PROCESSOR here as it's not set yet,
  # so fall back to checking the env for vcvarsall's VSCMD_ARG_TGT_ARCH
  if(NOT (WIN32 AND "$ENV{VSCMD_ARG_TGT_ARCH}" STREQUAL "arm64"))
    set(WITH_CYCLES_DEVICE_HIPRT    ON  CACHE BOOL "" FORCE)
    set(WITH_CYCLES_DEVICE_OPTIX    ON  CACHE BOOL "" FORCE)
    set(WITH_CYCLES_CUDA_BINARIES   ON  CACHE BOOL "" FORCE)
    set(WITH_CYCLES_HIP_BINARIES    ON  CACHE BOOL "" FORCE)
    set(WITH_CYCLES_DEVICE_ONEAPI   ON  CACHE BOOL "" FORCE)
    set(WITH_CYCLES_ONEAPI_BINARIES ON  CACHE BOOL "" FORCE)
  endif()
endif()
