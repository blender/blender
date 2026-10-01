# SPDX-FileCopyrightText: 2002-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  set(FLAC_CXX_FLAGS "-DCMAKE_C_FLAGS_RELEASE=-DFLAC__NO_DLL=ON")
endif()

set(FLAC_EXTRA_ARGS
  -DCMAKE_POLICY_DEFAULT_CMP0074=NEW
  -DBUILD_PROGRAMS=OFF
  -DBUILD_EXAMPLES=OFF
  -DBUILD_DOCS=OFF
  -DBUILD_TESTING=OFF
  -DINSTALL_MANPAGES=OFF
  -DOgg_ROOT=${LIBDIR}/ogg
  -DBUILD_SHARED_LIBS=OFF
  ${FLAC_CXX_FLAGS}
)

ExternalProject_Add(external_flac
  URL file://${PACKAGE_DIR}/${FLAC_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${FLAC_HASH_TYPE}=${FLAC_HASH}
  PREFIX ${BUILD_DIR}/flac
  CMAKE_GENERATOR "Ninja"

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/flac
    ${DEFAULT_CMAKE_FLAGS}
    ${FLAC_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/flac
)

if(NOT WIN32)
  harvest(external_flac flac/lib sndfile/lib "libFLAC.a")
endif()

add_dependencies(
  external_flac
  external_ogg
)
