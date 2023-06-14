# SPDX-FileCopyrightText: 2002-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(UNIX)
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV} && export HAVE_PDFLATEX=no)
else()
  set(THEORA_CONFIGURE_ENV ${CONFIGURE_ENV})
endif()

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

add_dependencies(
  external_theora
  external_vorbis
  external_ogg
)

if(MSVC)
  set_target_properties(external_theora PROPERTIES FOLDER Mingw)
endif()
