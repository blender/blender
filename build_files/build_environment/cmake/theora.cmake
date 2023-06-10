# SPDX-License-Identifier: GPL-2.0-or-later

if(UNIX)
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV} && export HAVE_PDFLATEX=no)
else()
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV})
endif()

if(NOT WIN32)
  ExternalProject_Add(external_theora
    URL file://${PACKAGE_DIR}/${THEORA_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${THEORA_HASH_TYPE}=${THEORA_HASH}
    PREFIX ${BUILD_DIR}/theora
    PATCH_COMMAND ${PATCH_CMD} -p 0 -d ${BUILD_DIR}/theora/src/external_theora < ${PATCH_DIR}/theora.diff
    CONFIGURE_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/theora
      --disable-shared
      --enable-static
      --with-pic
      --with-ogg=${LIBDIR}/ogg
      --with-vorbis=${LIBDIR}/vorbis
      --disable-examples
    BUILD_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${THEORA_CONFIGURE_ENV} && cd ${BUILD_DIR}/theora/src/external_theora/ && make install
    INSTALL_DIR ${LIBDIR}/theora
  )
else()
  # We are kind of naughty here and steal vorbis' FindOgg.cmake, but given it's a dependency anyway...
  ExternalProject_Add(external_theora
    URL file://${PACKAGE_DIR}/${THEORA_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${THEORA_HASH_TYPE}=${THEORA_HASH}
    PREFIX ${BUILD_DIR}/theora
    PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_theora.txt ${BUILD_DIR}/theora/src/external_theora/CMakeLists.txt &&
                          ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/libtheora.def ${BUILD_DIR}/theora/src/external_theora/libtheora.def &&
                          ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/vorbis/src/external_vorbis/cmake/FindOgg.cmake ${BUILD_DIR}/theora/src/external_theora/FindOgg.cmake
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/theora -DOGG_ROOT=${LIBDIR}/ogg ${DEFAULT_CMAKE_FLAGS} -DLIBDIR=${LIBDIR}
    INSTALL_DIR ${LIBDIR}/theora
  )
endif()

add_dependencies(
  external_theora
  external_vorbis
  external_ogg
)
