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

set(PYTHON_POSTFIX)
if(BUILD_MODE STREQUAL Debug)
  set(PYTHON_POSTFIX _d)
  set(PYTHON_EXTRA_INSTLAL_FLAGS -d)
endif()

if(WIN32)
  set(PYTHON_BINARY_INTERNAL ${BUILD_DIR}/python/src/external_python/PCBuild/amd64/python${PYTHON_POSTFIX}.exe)
  set(PYTHON_BINARY ${LIBDIR}/python/python${PYTHON_POSTFIX}.exe)
  set(PYTHON_SRC ${BUILD_DIR}/python/src/external_python/)
  macro(cmake_to_dos_path MsysPath ResultingPath)
    string(REPLACE "/" "\\" ${ResultingPath} "${MsysPath}")
  endmacro()

  set(PYTHON_EXTERNALS_FOLDER ${BUILD_DIR}/python/src/external_python/externals)
  set(DOWNLOADS_EXTERNALS_FOLDER ${DOWNLOAD_DIR}/externals)

  cmake_to_dos_path(${PYTHON_EXTERNALS_FOLDER} PYTHON_EXTERNALS_FOLDER_DOS)
  cmake_to_dos_path(${DOWNLOADS_EXTERNALS_FOLDER} DOWNLOADS_EXTERNALS_FOLDER_DOS)

  ExternalProject_Add(external_python
    URL ${PYTHON_URI}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH MD5=${PYTHON_HASH}
    PREFIX ${BUILD_DIR}/python
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cd ${BUILD_DIR}/python/src/external_python/pcbuild/ && set IncludeTkinter=false && call build.bat -e -p x64 -c ${BUILD_MODE}
    INSTALL_COMMAND ${PYTHON_BINARY_INTERNAL} ${PYTHON_SRC}/PC/layout/main.py -b ${PYTHON_SRC}/PCbuild/amd64 -s ${PYTHON_SRC} -t ${PYTHON_SRC}/tmp/  --include-underpth --include-stable --include-pip --include-dev --include-launchers  --include-venv --include-symbols ${PYTHON_EXTRA_INSTLAL_FLAGS} --copy ${LIBDIR}/python
  )

else()
  if(APPLE)
    # Disable functions that can be in 10.13 sdk but aren't available on 10.9 target.
    #
    # Disable libintl (gettext library) as it might come from Homebrew, which makes
    # it so test program compiles, but the Python does not. This is because for Python
    # we use isysroot, which seems to forbid using libintl.h.
    # The gettext functionality seems to come from CoreFoundation, so should be all fine.
    set(PYTHON_FUNC_CONFIGS
      export ac_cv_func_futimens=no &&
      export ac_cv_func_utimensat=no &&
      export ac_cv_func_basename_r=no &&
      export ac_cv_func_clock_getres=no &&
      export ac_cv_func_clock_gettime=no &&
      export ac_cv_func_clock_settime=no &&
      export ac_cv_func_dirname_r=no &&
      export ac_cv_func_getentropy=no &&
      export ac_cv_func_mkostemp=no &&
      export ac_cv_func_mkostemps=no &&
      export ac_cv_func_timingsafe_bcmp=no &&
      export ac_cv_header_libintl_h=no &&
      export ac_cv_lib_intl_textdomain=no
    )
    if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
      set(PYTHON_FUNC_CONFIGS ${PYTHON_FUNC_CONFIGS} && export PYTHON_DECIMAL_WITH_MACHINE=ansi64)
    endif()
    set(PYTHON_CONFIGURE_ENV ${CONFIGURE_ENV} && ${PYTHON_FUNC_CONFIGS})
    set(PYTHON_BINARY ${BUILD_DIR}/python/src/external_python/python.exe)
    set(PYTHON_PATCH ${PATCH_CMD} --verbose -p1 -d ${BUILD_DIR}/python/src/external_python < ${PATCH_DIR}/python_macos.diff)
  else()
    set(PYTHON_CONFIGURE_ENV ${CONFIGURE_ENV})
    set(PYTHON_BINARY ${BUILD_DIR}/python/src/external_python/python)
    set(PYTHON_PATCH ${PATCH_CMD} --verbose -p1 -d ${BUILD_DIR}/python/src/external_python < ${PATCH_DIR}/python_linux.diff)
 endif()

  set(PYTHON_CONFIGURE_EXTRA_ARGS "--with-openssl=${LIBDIR}/ssl")
  set(PYTHON_CFLAGS "-I${LIBDIR}/sqlite/include -I${LIBDIR}/bzip2/include -I${LIBDIR}/lzma/include -I${LIBDIR}/zlib/include")
  set(PYTHON_LDFLAGS "-L${LIBDIR}/ffi/lib -L${LIBDIR}/sqlite/lib -L${LIBDIR}/bzip2/lib -L${LIBDIR}/lzma/lib -L${LIBDIR}/zlib/lib")
  set(PYTHON_CONFIGURE_EXTRA_ENV
    export CFLAGS=${PYTHON_CFLAGS} &&
    export CPPFLAGS=${PYTHON_CFLAGS} &&
    export LDFLAGS=${PYTHON_LDFLAGS} &&
    export PKG_CONFIG_PATH=${LIBDIR}/ffi/lib/pkgconfig)

  ExternalProject_Add(external_python
    URL ${PYTHON_URI}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH MD5=${PYTHON_HASH}
    PREFIX ${BUILD_DIR}/python
    PATCH_COMMAND ${PYTHON_PATCH}
    CONFIGURE_COMMAND ${PYTHON_CONFIGURE_ENV} && ${PYTHON_CONFIGURE_EXTRA_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/python ${PYTHON_CONFIGURE_EXTRA_ARGS}
    BUILD_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make install
    INSTALL_DIR ${LIBDIR}/python)
endif()

if(UNIX)
  add_dependencies(
    external_python
    external_bzip2
    external_ffi
    external_lzma
    external_ssl
    external_sqlite
    external_zlib
  )
endif()
