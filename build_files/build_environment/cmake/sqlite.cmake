# SPDX-License-Identifier: GPL-2.0-or-later

set(SQLITE_CONFIGURE_ENV echo .)
set(SQLITE_CONFIGURATION_ARGS)

if(UNIX)
  if(NOT APPLE)
    set(SQLITE_LDFLAGS -Wl,--as-needed)
  endif()
  set(SQLITE_CFLAGS
    "-DSQLITE_SECURE_DELETE -DSQLITE_ENABLE_COLUMN_METADATA \
    -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS \
    -DSQLITE_ENABLE_RTREE=1 -DSQLITE_SOUNDEX=1 \
    -DSQLITE_ENABLE_UNLOCK_NOTIFY \
    -DSQLITE_OMIT_LOOKASIDE=1 -DSQLITE_ENABLE_DBSTAT_VTAB \
    -DSQLITE_ENABLE_UPDATE_DELETE_LIMIT=1 \
    -DSQLITE_ENABLE_LOAD_EXTENSION \
    -DSQLITE_ENABLE_JSON1 \
    -DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
    -DSQLITE_THREADSAFE=1 \
    -DSQLITE_ENABLE_FTS3_TOKENIZER=1 \
    -DSQLITE_MAX_SCHEMA_RETRY=25 \
    -DSQLITE_ENABLE_PREUPDATE_HOOK \
    -DSQLITE_ENABLE_SESSION \
    -DSQLITE_ENABLE_STMTVTAB \
    -DSQLITE_MAX_VARIABLE_NUMBER=250000 \
    -fPIC")
  set(SQLITE_CONFIGURE_ENV ${SQLITE_CONFIGURE_ENV} && export LDFLAGS=${SQLITE_LDFLAGS} && export CFLAGS=${SQLITE_CFLAGS})
  set(SQLITE_CONFIGURATION_ARGS
    ${SQLITE_CONFIGURATION_ARGS}
    --enable-threadsafe
    --enable-load-extension
    --enable-json1
    --enable-fts4
    --enable-fts5
    # While building `tcl` is harmless, it causes problems when the install step
    # tries to copy the files into the system path.
    # Since this isn't required by Python or Blender this can be disabled.
    # Note that Debian (for example), splits this off into a separate package,
    # so it's safe to turn off.
    --disable-tcl
    --enable-shared=no
  )
endif()

ExternalProject_Add(external_sqlite
  URL file://${PACKAGE_DIR}/${SQLITE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${SQLITE_HASH_TYPE}=${SQLITE_HASH}
  PREFIX ${BUILD_DIR}/sqlite
  CONFIGURE_COMMAND ${SQLITE_CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/sqlite ${SQLITE_CONFIGURATION_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && make install
  INSTALL_DIR ${LIBDIR}/sqlite
)
