# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

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
  set(SQLITE_CONFIGURATION_ARGS ${SQLITE_CONFIGURATION_ARGS} --enable-threadsafe --enable-load-extension --enable-json1 --enable-fts4 --enable-fts5 --disable-tcl
      --enable-shared=no)
endif()

ExternalProject_Add(external_sqlite
  URL ${SQLITE_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH SHA1=${SQLITE_HASH}
  PREFIX ${BUILD_DIR}/sqlite
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/sqlite/src/external_sqlite < ${PATCH_DIR}/sqlite.diff
  CONFIGURE_COMMAND ${SQLITE_CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/sqlite ${SQLITE_CONFIGURATION_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV} && cd ${BUILD_DIR}/sqlite/src/external_sqlite/ && make install
  INSTALL_DIR ${LIBDIR}/sqlite
)
