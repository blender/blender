# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(LIBGLU_CFLAGS "-static-libgcc")
set(LIBGLU_CXXFLAGS "-static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a")
set(LIBGLU_LDFLAGS "-pthread -static-libgcc -static-libstdc++ -Bstatic -lstdc++ -Bdynamic -l:libstdc++.a")

set(LIBGLU_EXTRA_FLAGS
  CFLAGS=${LIBGLU_CFLAGS}
  CXXFLAGS=${LIBGLU_CXXFLAGS}
  LDFLAGS=${LIBGLU_LDFLAGS}
)

ExternalProject_Add(external_libglu
  URL file://${PACKAGE_DIR}/${LIBGLU_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${LIBGLU_HASH_TYPE}=${LIBGLU_HASH}
  PREFIX ${BUILD_DIR}/libglu
  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/libglu/src/external_libglu/ &&
    ${CONFIGURE_COMMAND_NO_TARGET} --prefix=${LIBDIR}/libglu ${LIBGLU_EXTRA_FLAGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/libglu/src/external_libglu/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/libglu/src/external_libglu/ && make install
  INSTALL_DIR ${LIBDIR}/libglu
)
