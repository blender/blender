# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SQLITE_CONFIGURE_ENV echo .)
set(SQLITE_CONFIGURATION_ARGS "")

if(WIN32)
  # Python will build this with its preferred build options.
  # We only need to unpack sqlite.
  ExternalProject_Add(external_sqlite
    URL file://${PACKAGE_DIR}/${SQLITE_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${SQLITE_HASH_TYPE}=${SQLITE_HASH}
    PREFIX ${BUILD_DIR}/sqlite
    CONFIGURE_COMMAND echo "."
    BUILD_COMMAND echo "."
    INSTALL_COMMAND echo "."
    INSTALL_DIR ${LIBDIR}/sqlite
  )
endif()

if(UNIX)
  if(NOT APPLE)
    set(SQLITE_LDFLAGS -Wl,--as-needed)
  endif()
  set(SQLITE_CFLAGS "\
-DSQLITE_SECURE_DELETE \
-DSQLITE_ENABLE_COLUMN_METADATA \
-DSQLITE_ENABLE_FTS3 \
-DSQLITE_ENABLE_FTS3_PARENTHESIS \
-DSQLITE_SOUNDEX \
-DSQLITE_ENABLE_UNLOCK_NOTIFY \
-DSQLITE_OMIT_LOOKASIDE=1 \
-DSQLITE_ENABLE_DBSTAT_VTAB \
-DSQLITE_ENABLE_UPDATE_DELETE_LIMIT=1 \
-DSQLITE_ENABLE_LOAD_EXTENSION \
-DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
-DSQLITE_MAX_SCHEMA_RETRY=25 \
-DSQLITE_ENABLE_PREUPDATE_HOOK \
-DSQLITE_ENABLE_SESSION \
-DSQLITE_ENABLE_STMTVTAB \
-DSQLITE_MAX_VARIABLE_NUMBER=250000 \
-fPIC"
  )
  set(SQLITE_CONFIGURE_ENV
    ${SQLITE_CONFIGURE_ENV} &&
    export LDFLAGS=${SQLITE_LDFLAGS} &&
    export CFLAGS=${SQLITE_CFLAGS}
  )
  set(SQLITE_CONFIGURATION_ARGS
    ${SQLITE_CONFIGURATION_ARGS}
    --enable-rtree
    --enable-geopoly
    --enable-threadsafe
    --enable-load-extension
    --enable-fts4
    --enable-fts5
    --disable-shared
  )

  if(ANDROID)
    # Sqlite's configure doesn't support passing a compiler + a `--target` argument as CC/CXX, work around this by
    # using the clang target-prefixed entry-point scripts shipped with the Android NDK.
    string(REPLACE "-none-" "-" _android_triple ${ANDROID_LLVM_TRIPLE})
    set(ANDROID_CC ${ANDROID_TOOLCHAIN_ROOT}/bin/${_android_triple}-clang)
    set(ANDROID_CXX ${ANDROID_TOOLCHAIN_ROOT}/bin/${_android_triple}-clang++)

    # Override toolchain from CMake env set by the Android CMake toolchain file.
    set(SQLITE_CONFIGURE_ENV
      ${SQLITE_CONFIGURE_ENV} &&
      export CC=${ANDROID_CC} &&
      export CXX=${ANDROID_CXX} &&
      export AS=${CMAKE_ASM_COMPILER} &&
      export AR=${CMAKE_AR} &&
      export LD=${CMAKE_C_COMPILER_LINKER} &&
      export RANLIB=${CMAKE_RANLIB} &&
      export STRIP=${CMAKE_STRIP}
    )

    set(SQLITE_CONFIGURATION_ARGS
      ${SQLITE_CONFIGURATION_ARGS}
      --host=${ANDROID_LLVM_TRIPLE}
    )
  endif()

  ExternalProject_Add(external_sqlite
    URL file://${PACKAGE_DIR}/${SQLITE_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${SQLITE_HASH_TYPE}=${SQLITE_HASH}
    PREFIX ${BUILD_DIR}/sqlite

    CONFIGURE_COMMAND ${SQLITE_CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/sqlite/src/external_sqlite/ &&
      ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/sqlite ${SQLITE_CONFIGURATION_ARGS}

    BUILD_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/sqlite/src/external_sqlite/ &&
      make -j${MAKE_THREADS}

    INSTALL_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/sqlite/src/external_sqlite/ &&
      make install

    INSTALL_DIR ${LIBDIR}/sqlite
  )
endif()
