# SPDX-License-Identifier: GPL-2.0-or-later

if(NOT WIN32)
  ExternalProject_Add(external_vorbis
    URL file://${PACKAGE_DIR}/${VORBIS_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${VORBIS_HASH_TYPE}=${VORBIS_HASH}
    PREFIX ${BUILD_DIR}/vorbis
    CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vorbis/src/external_vorbis/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/vorbis
      --disable-shared
      --enable-static
      --with-pic
      --with-ogg=${LIBDIR}/ogg
    BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vorbis/src/external_vorbis/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/vorbis/src/external_vorbis/ && make install
    INSTALL_DIR ${LIBDIR}/vorbis
  )
else()
ExternalProject_Add(external_vorbis
    URL file://${PACKAGE_DIR}/${VORBIS_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${VORBIS_HASH_TYPE}=${VORBIS_HASH}
    PREFIX ${BUILD_DIR}/vorbis
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/vorbis -DOGG_ROOT=${LIBDIR}/ogg ${DEFAULT_CMAKE_FLAGS}
    INSTALL_DIR ${LIBDIR}/vorbis
  )
endif()

add_dependencies(
  external_vorbis
  external_ogg
)
