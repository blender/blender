# SPDX-License-Identifier: GPL-2.0-or-later
if(WIN32)
  set(EPOXY_LIB_TYPE shared)
else()
  set(EPOXY_LIB_TYPE static)
endif()
ExternalProject_Add(external_epoxy
  URL file://${PACKAGE_DIR}/${EPOXY_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${EPOXY_HASH_TYPE}=${EPOXY_HASH}
  PREFIX ${BUILD_DIR}/epoxy
  PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/epoxy/src/external_epoxy/ < ${PATCH_DIR}/epoxy.diff
  CONFIGURE_COMMAND ${CONFIGURE_ENV} && ${MESON} setup --prefix ${LIBDIR}/epoxy --default-library ${EPOXY_LIB_TYPE} --libdir lib ${BUILD_DIR}/epoxy/src/external_epoxy-build ${BUILD_DIR}/epoxy/src/external_epoxy -Dtests=false ${MESON_BUILD_TYPE}
  BUILD_COMMAND ninja
  INSTALL_COMMAND ninja install
)

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_epoxy after_install
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/epoxy/include ${HARVEST_TARGET}/epoxy/include
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/epoxy/bin/epoxy-0.dll ${HARVEST_TARGET}/epoxy/bin/epoxy-0.dll
    COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/epoxy/lib/epoxy.lib ${HARVEST_TARGET}/epoxy/lib/epoxy.lib
    DEPENDEES install
  )
endif()

add_dependencies(
  external_epoxy
  # Needed for `MESON`.
  external_python_site_packages
)
