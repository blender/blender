# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(PYTHON_POSTFIX)
if(BUILD_MODE STREQUAL Debug)
  set(PYTHON_POSTFIX _d)
  set(PYTHON_EXTRA_INSTLAL_FLAGS -d)
endif()

if(WIN32)
  set(PYTHON_BINARY ${LIBDIR}/python/python${PYTHON_POSTFIX}.exe)
  set(PYTHON_SRC ${BUILD_DIR}/python/src/external_python/)
  macro(cmake_to_dos_path MsysPath ResultingPath)
    string(REPLACE "/" "\\" ${ResultingPath} "${MsysPath}")
  endmacro()

  if(BLENDER_PLATFORM_ARM)
    set(PYTHON_BINARY_INTERNAL ${BUILD_DIR}/python/src/external_python/PCBuild/arm64/python${PYTHON_POSTFIX}.exe)
    set(PYTHON_BAT_ARCH arm64)
    set(PYTHON_INSTALL_ARCH_FOLDER ${PYTHON_SRC}/PCbuild/arm64)
    set(PYTHON_PATCH_FILE python_windows_arm64.diff)
  else()
    set(PYTHON_BINARY_INTERNAL ${BUILD_DIR}/python/src/external_python/PCBuild/amd64/python${PYTHON_POSTFIX}.exe)
    set(PYTHON_BAT_ARCH x64)
    set(PYTHON_INSTALL_ARCH_FOLDER ${PYTHON_SRC}/PCbuild/amd64)
    set(PYTHON_PATCH_FILE python_windows_x64.diff)
  endif()

  set(PYTHON_EXTERNALS_FOLDER ${BUILD_DIR}/python/src/external_python/externals)
  set(ZLIB_SOURCE_FOLDER ${BUILD_DIR}/zlib/src/external_zlib)
  set(SSL_SOURCE_FOLDER ${BUILD_DIR}/ssl/src/external_ssl)
  set(FFI_SOURCE_FOLDER ${LIBDIR}/ffi)
  set(SQLITE_SOURCE_FOLDER ${BUILD_DIR}/sqlite/src/external_sqlite)
  set(DOWNLOADS_EXTERNALS_FOLDER ${DOWNLOAD_DIR}/externals)

  cmake_to_dos_path(${PYTHON_EXTERNALS_FOLDER} PYTHON_EXTERNALS_FOLDER_DOS)
  cmake_to_dos_path(${ZLIB_SOURCE_FOLDER} ZLIB_SOURCE_FOLDER_DOS)
  cmake_to_dos_path(${FFI_SOURCE_FOLDER} FFI_SOURCE_FOLDER_DOS)
  cmake_to_dos_path(${SSL_SOURCE_FOLDER} SSL_SOURCE_FOLDER_DOS)
  cmake_to_dos_path(${SQLITE_SOURCE_FOLDER} SQLITE_SOURCE_FOLDER_DOS)
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
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\libffi-3.4.4 ${FFI_SOURCE_FOLDER_DOS} &&
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\zlib-1.3.1 ${ZLIB_SOURCE_FOLDER_DOS} &&
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\openssl-3.0.18 ${SSL_SOURCE_FOLDER_DOS} &&
      mklink /J ${PYTHON_EXTERNALS_FOLDER_DOS}\\sqlite-3.50.4.0 ${SQLITE_SOURCE_FOLDER_DOS} &&
      ${CMAKE_COMMAND} -E copy
        ${ZLIB_SOURCE_FOLDER}/../external_zlib-build/zconf.h
        ${PYTHON_EXTERNALS_FOLDER}/zlib-1.3.1/zconf.h &&
      ${PATCH_CMD} --verbose -p1 -d
        ${BUILD_DIR}/python/src/external_python <
        ${PATCH_DIR}/${PYTHON_PATCH_FILE}

    CONFIGURE_COMMAND echo "."

    BUILD_COMMAND ${CONFIGURE_ENV_MSVC} &&
      cd ${BUILD_DIR}/python/src/external_python/pcbuild/ &&
      set IncludeTkinter=false &&
      set LDFLAGS=/DEBUG &&
      call prepare_ssl.bat &&
      call build.bat -e -p ${PYTHON_BAT_ARCH} -c ${BUILD_MODE}

    INSTALL_COMMAND ${PYTHON_BINARY_INTERNAL} ${PYTHON_SRC}/PC/layout/main.py
      -b ${PYTHON_INSTALL_ARCH_FOLDER}
      -s ${PYTHON_SRC}
      -t ${PYTHON_SRC}/tmp/
      --include-stable
      --include-pip
      --include-dev
      --include-launchers
      --include-venv
      --include-symbols
      ${PYTHON_EXTRA_INSTLAL_FLAGS}
      --copy
      ${LIBDIR}/python
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

  set(PYTHON_CFLAGS "${PLATFORM_CFLAGS} ")
  # We need to add the zlib static lib path here as even if python itself links the static zlib correctly,
  # the "_sqlite" cpython library needs to know where to get it from.
  set(PYTHON_LDFLAGS "-L${LIBDIR}/zlib/lib ${PLATFORM_LDFLAGS} ")

  set(PYTHON_CONFIGURE_EXTRA_ARGS
    # Using pkg-config is supported for most libs besides bzip2, so make sure it is on.
    --with-pkg-config=yes
    --enable-loadable-sqlite-extensions
    # Don't build or ship the python test suite
    --disable-test-modules
  )

  set(PYTHON_CONFIGURE_PKG_CONFIG_PATH "\
${LIBDIR}/ffi/lib/pkgconfig:${LIBDIR}/sqlite/lib/pkgconfig:${LIBDIR}/ssl/lib/pkgconfig:\
${LIBDIR}/ssl/lib64/pkgconfig:${LIBDIR}/lzma/lib/pkgconfig:${LIBDIR}/zlib/share/pkgconfig")

  set(PYTHON_CONFIGURE_EXTRA_ENV
    export CFLAGS=${PYTHON_CFLAGS} &&
    export CPPFLAGS=${PYTHON_CFLAGS} &&
    export LDFLAGS=${PYTHON_LDFLAGS} &&

    # Use pkg-config for libraries that support it, and ensure that it used static libraries.
    export PKG_CONFIG=pkg-config\ --static
    export PKG_CONFIG_PATH=${PYTHON_CONFIGURE_PKG_CONFIG_PATH}

    # Use flags documented by ./configure for other libs.
    export BZIP2_CFLAGS=-I${LIBDIR}/bzip2/include
    export BZIP2_LIBS=${LIBDIR}/bzip2/lib/${LIBPREFIX}bz2${LIBEXT}
  )

  if(APPLE)
    # Prevent linking against Homebrew's libmpdec if it exists.
    set(PYTHON_CONFIGURE_EXTRA_ARGS
      ${PYTHON_CONFIGURE_EXTRA_ARGS}
      --without-system-libmpdec
    )

    # Override library paths for SQLite and zlib on macOS (which are normally provided by pkg-config).
    # Redefining these prevents Python from wrongly trying to dynamically link zlib in SQLite and various built-in modules.
    set(PYTHON_CONFIGURE_EXTRA_ENV
      ${PYTHON_CONFIGURE_EXTRA_ENV}
      export LIBSQLITE3_CFLAGS=-I${LIBDIR}/sqlite/include
      export LIBSQLITE3_LIBS=${LIBDIR}/sqlite/lib/${LIBPREFIX}sqlite3${LIBEXT}
      export ZLIB_CFLAGS=-I${LIBDIR}/zlib/include
      export ZLIB_LIBS=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
    )
  endif()

  # NOTE: untested on APPLE so far.
  if(NOT APPLE)
    set(PYTHON_CONFIGURE_EXTRA_ARGS
      ${PYTHON_CONFIGURE_EXTRA_ARGS}
      # We disable optimizations as this flag turns on PGO which leads to non-reproducible builds.
      --disable-optimizations
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

    CONFIGURE_COMMAND ${PYTHON_CONFIGURE_ENV} &&
      ${PYTHON_CONFIGURE_EXTRA_ENV} &&
      cd ${BUILD_DIR}/python/src/external_python/ &&
      ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/python ${PYTHON_CONFIGURE_EXTRA_ARGS}

    BUILD_COMMAND ${PYTHON_CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/python/src/external_python/ &&
      make -j${MAKE_THREADS}

    INSTALL_COMMAND ${PYTHON_CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/python/src/external_python/ &&
      make install

    INSTALL_DIR ${LIBDIR}/python)
endif()

add_dependencies(
  external_python
  external_ssl
  external_zlib
  external_sqlite
  external_ffi
)
if(UNIX)
  add_dependencies(
    external_python
    external_bzip2
    external_lzma
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
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib
        ${LIBDIR}/python/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib

      DEPENDEES install
    )
  endif()
else()
  harvest(external_python python/bin python/bin "python${PYTHON_SHORT_VERSION}")
  harvest(external_python python/include python/include "*h")
  harvest(external_python python/lib python/lib "*")
endif()
