# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(UNIX)
  set(OPENCOLLADA_EXTRA_ARGS
    -DLIBXML2_INCLUDE_DIR=${LIBDIR}/xml2/include/libxml2
    -DLIBXML2_LIBRARIES=${LIBDIR}/xml2/lib/libxml2.a)

  # WARNING: the patch contains mixed UNIX and DOS line endings
  # as does the OPENCOLLADA package, if this can be corrected upstream that would be better.
  # For now use `sed` to force UNIX line endings so the patch applies.
  # Needed as neither ignoring white-space or applying as a binary resolve this problem.
  if(APPLE)
    set(_dos2unix dos2unix)
  else()
    set(_dos2unix sed -i "s/\\r//")
  endif()
  set(PATCH_MAYBE_DOS2UNIX_CMD
    ${_dos2unix}
    ${PATCH_DIR}/opencollada.diff
    ${BUILD_DIR}/opencollada/src/external_opencollada/CMakeLists.txt
    ${BUILD_DIR}/opencollada/src/external_opencollada/Externals/LibXML/CMakeLists.txt &&
  )
else()
  set(OPENCOLLADA_EXTRA_ARGS
    -DCMAKE_DEBUG_POSTFIX=_d
    -DLIBXML2_INCLUDE_DIR=${LIBDIR}/xml2/include/libxml2
  )
  if(BUILD_MODE STREQUAL Release)
    list(APPEND OPENCOLLADA_EXTRA_ARGS -DLIBXML2_LIBRARIES=${LIBDIR}/xml2/lib/libxml2s.lib)
  else()
    list(APPEND OPENCOLLADA_EXTRA_ARGS -DLIBXML2_LIBRARIES=${LIBDIR}/xml2/lib/libxml2sd.lib)
  endif()
  set(PATCH_MAYBE_DOS2UNIX_CMD)
endif()

ExternalProject_Add(external_opencollada
  URL file://${PACKAGE_DIR}/${OPENCOLLADA_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENCOLLADA_HASH_TYPE}=${OPENCOLLADA_HASH}
  PREFIX ${BUILD_DIR}/opencollada

  PATCH_COMMAND
    ${PATCH_MAYBE_DOS2UNIX_CMD}
    ${PATCH_CMD} -p 1 -N -d
      ${BUILD_DIR}/opencollada/src/external_opencollada <
      ${PATCH_DIR}/opencollada.diff

  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencollada
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENCOLLADA_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/opencollada
)

unset(PATCH_MAYBE_DOS2UNIX_CMD)

add_dependencies(
  external_opencollada
  external_xml2
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_opencollada after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opencollada/
        ${HARVEST_TARGET}/opencollada/

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opencollada after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/opencollada/lib
        ${HARVEST_TARGET}/opencollada/lib

      DEPENDEES install
    )
  endif()
endif()
