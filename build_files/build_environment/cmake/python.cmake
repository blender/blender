# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
  set(ZLIB_SOURCE_FOLDER ${BUILD_DIR}/zlib/src/external_zlib)
  set(SSL_SOURCE_FOLDER ${BUILD_DIR}/ssl/src/external_ssl)
  set(DOWNLOADS_EXTERNALS_FOLDER ${DOWNLOAD_DIR}/externals)

  cmake_to_dos_path(${PYTHON_EXTERNALS_FOLDER} PYTHON_EXTERNALS_FOLDER_DOS)
  cmake_to_dos_path(${ZLIB_SOURCE_FOLDER} ZLIB_SOURCE_FOLDER_DOS)
  cmake_to_dos_path(${SSL_SOURCE_FOLDER} SSL_SOURCE_FOLDER_DOS)
  cmake_to_dos_path(${DOWNLOADS_EXTERNALS_FOLDER} DOWNLOADS_EXTERNALS_FOLDER_DOS)

  ExternalProject_Add(external_python
    URL file://${PACKAGE_DIR}/${PYTHON_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${PYTHON_HASH_TYPE}=${PYTHON_HASH}
    PREFIX ${BUILD_DIR}/python
    # Python will download its own deps and there's very little we can do about
    # that beyond placing some code in their externals dir before it tries.
    # the foldernames *HAVE* to match the ones inside pythons get_externals.cmd.
    # regardless of the version actually in there.
    PATCH_COMMAND mkdir ${PYTHON_EXTERNALS_FOLDER_DOS} &&
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\zlib-1.2.13 ${ZLIB_SOURCE_FOLDER_DOS} &&
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\openssl-1.1.1t ${SSL_SOURCE_FOLDER_DOS} &&
      ${CMAKE_COMMAND} -E copy ${ZLIB_SOURCE_FOLDER}/../external_zlib-build/zconf.h ${PYTHON_EXTERNALS_FOLDER}/zlib-1.2.13/zconf.h &&
      ${PATCH_CMD} --verbose -p1 -d ${BUILD_DIR}/python/src/external_python < ${PATCH_DIR}/python_windows.diff
    CONFIGURE_COMMAND echo "."
    BUILD_COMMAND ${CONFIGURE_ENV_MSVC} && cd ${BUILD_DIR}/python/src/external_python/pcbuild/ && set IncludeTkinter=false && set LDFLAGS=/DEBUG && call prepare_ssl.bat && call build.bat -e -p x64 -c ${BUILD_MODE}
    INSTALL_COMMAND ${PYTHON_BINARY_INTERNAL} ${PYTHON_SRC}/PC/layout/main.py -b ${PYTHON_SRC}/PCbuild/amd64 -s ${PYTHON_SRC} -t ${PYTHON_SRC}/tmp/ --include-stable --include-pip --include-dev --include-launchers  --include-venv --include-symbols ${PYTHON_EXTRA_INSTLAL_FLAGS} --copy ${LIBDIR}/python
  )
  add_dependencies(
    external_python
    external_zlib
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
  else()
    set(PYTHON_CONFIGURE_ENV ${CONFIGURE_ENV})
  endif()
  set(PYTHON_BINARY ${LIBDIR}/python/bin/python${PYTHON_SHORT_VERSION})
  # Link against zlib statically (Unix). Avoid rpath issues (macOS).
  set(PYTHON_PATCH ${PATCH_CMD} --verbose -p1 -d ${BUILD_DIR}/python/src/external_python < ${PATCH_DIR}/python_unix.diff)
  set(PYTHON_CONFIGURE_EXTRA_ARGS "--with-openssl=${LIBDIR}/ssl")
  set(PYTHON_CFLAGS "-I${LIBDIR}/sqlite/include -I${LIBDIR}/bzip2/include -I${LIBDIR}/lzma/include -I${LIBDIR}/zlib/include ${PLATFORM_CFLAGS}")
  set(PYTHON_LDFLAGS "-L${LIBDIR}/ffi/lib -L${LIBDIR}/sqlite/lib -L${LIBDIR}/bzip2/lib -L${LIBDIR}/lzma/lib -L${LIBDIR}/zlib/lib ${PLATFORM_LDFLAGS}")
  set(PYTHON_CONFIGURE_EXTRA_ENV
    export CFLAGS=${PYTHON_CFLAGS} &&
    export CPPFLAGS=${PYTHON_CFLAGS} &&
    export LDFLAGS=${PYTHON_LDFLAGS} &&
    export PKG_CONFIG_PATH=${LIBDIR}/ffi/lib/pkgconfig)

  # NOTE: untested on APPLE so far.
  if(NOT APPLE)
    set(PYTHON_CONFIGURE_EXTRA_ARGS
      ${PYTHON_CONFIGURE_EXTRA_ARGS}
      # Used on most release Linux builds (Fedora for e.g.),
      # increases build times noticeably with the benefit of a modest speedup at runtime.
      --enable-optimizations
      # While LTO is OK when building on the same system, it's incompatible across GCC versions,
      # making it impractical for developers to build against, so keep it disabled.
      # `--with-lto`
    )
  endif()

  ExternalProject_Add(external_python
    URL file://${PACKAGE_DIR}/${PYTHON_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${PYTHON_HASH_TYPE}=${PYTHON_HASH}
    PREFIX ${BUILD_DIR}/python
    PATCH_COMMAND ${PYTHON_PATCH}
    CONFIGURE_COMMAND ${PYTHON_CONFIGURE_ENV} && ${PYTHON_CONFIGURE_EXTRA_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/python ${PYTHON_CONFIGURE_EXTRA_ARGS}
    BUILD_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make -j${MAKE_THREADS}
    INSTALL_COMMAND ${PYTHON_CONFIGURE_ENV} && cd ${BUILD_DIR}/python/src/external_python/ && make install
    INSTALL_DIR ${LIBDIR}/python)
endif()

add_dependencies(
  external_python
  external_ssl
  external_zlib
)
if(UNIX)
  add_dependencies(
    external_python
    external_bzip2
    external_ffi
    external_lzma
    external_sqlite
  )
endif()

if(WIN32)
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_python after_install
      # Boost can't keep it self from linking release python
      # in a debug configuration even if all options are set
      # correctly to instruct it to use the debug version
      # of python. So just copy the debug imports file over
      # and call it a day...
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib
      DEPENDEES install
    )
  endif()
endif()
