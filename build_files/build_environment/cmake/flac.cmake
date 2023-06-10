# SPDX-License-Identifier: GPL-2.0-or-later

if(NOT WIN32)
  ExternalProject_Add(external_flac
    URL file://${PACKAGE_DIR}/${FLAC_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${FLAC_HASH_TYPE}=${FLAC_HASH}
    PREFIX ${BUILD_DIR}/flac
    CONFIGURE_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/flac --disable-shared --enable-static
    BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/flac/src/external_flac/ && make install
    INSTALL_DIR ${LIBDIR}/flac
  )
else()
  set(FLAC_CXX_FLAGS "-DFLAC__NO_DLL=ON")

  set(FLAC_EXTRA_ARGS
    -DCMAKE_POLICY_DEFAULT_CMP0074=NEW
    -DBUILD_PROGRAMS=OFF
    -DBUILD_EXAMPLES=OFF
    -DBUILD_DOCS=OFF
    -DBUILD_TESTING=OFF
    -DINSTALL_MANPAGES=OFF
    -DOgg_ROOT=${LIBDIR}/ogg
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_C_FLAGS_RELEASE=${FLAC_CXX_FLAGS}
  )

  ExternalProject_Add(external_flac
    URL file://${PACKAGE_DIR}/${FLAC_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${FLAC_HASH_TYPE}=${FLAC_HASH}
    PREFIX ${BUILD_DIR}/flac
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/flac ${DEFAULT_CMAKE_FLAGS} ${FLAC_EXTRA_ARGS}
    INSTALL_DIR ${LIBDIR}/flac
  )
endif()

if(MSVC)
  add_dependencies(
    external_flac
    external_ogg
  )
endif()
