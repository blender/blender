# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(FLEX_EXTRA_ARGS "")
if (ANDROID)
  set(FLEX_EXTRA_ARGS
    ${FLEX_EXTRA_ARGS}
    --host=aarch64

    # Autoconf cannot execute tests while cross-compiling and fails the malloc and realloc tests,
    # this then causes it to redefine malloc/realloc to rpl_malloc/rpl_realloc which do not exist.
    # Override the ac cache variables to yes to skip these checks.
    ac_cv_func_malloc_0_nonnull=yes
    ac_cv_func_realloc_0_nonnull=yes
  )
endif()

# Generated configuration files use an old `aclocal-1.15` on RockyLinux8.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_autoconf_cmd_optional ./autogen.sh &&)
else()
  set(_autoconf_cmd_optional "")
endif()


ExternalProject_Add(external_flex
  URL file://${PACKAGE_DIR}/${FLEX_FILE}
  URL_HASH ${FLEX_HASH_TYPE}=${FLEX_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/flex

  PATCH_COMMAND ${PATCH_CMD} --verbose -p1 -d
    ${BUILD_DIR}/flex/src/external_flex <
    ${PATCH_DIR}/flex_gcc14.diff

  CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/flex/src/external_flex/ &&
    ${_autoconf_cmd_optional} ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/flex ${FLEX_EXTRA_ARGS}

  BUILD_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/flex/src/external_flex/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/flex/src/external_flex/ &&
    make install

  INSTALL_DIR ${LIBDIR}/flex
)

unset(_autoconf_cmd_optional)
