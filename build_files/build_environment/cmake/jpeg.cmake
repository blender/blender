# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  # cmake for windows
  set(JPEG_EXTRA_ARGS -DNASM=${NASM_PATH} -DWITH_JPEG8=ON  -DCMAKE_DEBUG_POSTFIX=d -DWITH_CRT_DLL=On)

  ExternalProject_Add(external_jpeg
    URL file://${PACKAGE_DIR}/${JPEG_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${JPEG_HASH_TYPE}=${JPEG_HASH}
    PREFIX ${BUILD_DIR}/jpg
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/jpg ${DEFAULT_CMAKE_FLAGS} ${JPEG_EXTRA_ARGS}
    INSTALL_DIR ${LIBDIR}/jpg
  )

  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_jpeg after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/jpg/lib/jpegd${LIBEXT}  ${LIBDIR}/jpg/lib/jpeg${LIBEXT}
      DEPENDEES install
    )
  endif()

  if(BUILD_MODE STREQUAL Release)
    set(JPEG_LIBRARY jpeg-static${LIBEXT})
  else()
    set(JPEG_LIBRARY jpeg-staticd${LIBEXT})
  endif()
else(WIN32)
  # cmake for unix
  set(JPEG_EXTRA_ARGS
    -DWITH_JPEG8=ON
    -DENABLE_STATIC=ON
    -DENABLE_SHARED=OFF
    -DCMAKE_INSTALL_LIBDIR=${LIBDIR}/jpg/lib)

  ExternalProject_Add(external_jpeg
    URL file://${PACKAGE_DIR}/${JPEG_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${JPEG_HASH_TYPE}=${JPEG_HASH}
    PREFIX ${BUILD_DIR}/jpg
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/jpg ${DEFAULT_CMAKE_FLAGS} ${JPEG_EXTRA_ARGS}
    INSTALL_DIR ${LIBDIR}/jpg
  )

  set(JPEG_LIBRARY libjpeg${LIBEXT})
endif()
