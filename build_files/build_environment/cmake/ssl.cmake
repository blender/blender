# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SSL_CONFIGURE_COMMAND ./Configure)

if(WIN32)
  # Python will build this with its preferred build options and patches. We only need to unpack openssl
  ExternalProject_Add(external_ssl
    URL file://${PACKAGE_DIR}/${SSL_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${SSL_HASH_TYPE}=${SSL_HASH}
    PREFIX ${BUILD_DIR}/ssl
    CONFIGURE_COMMAND echo "."
    BUILD_COMMAND echo "."
    INSTALL_COMMAND echo "."
    INSTALL_DIR ${LIBDIR}/ssl
  )
else()
  if(APPLE)
    set(SSL_OS_COMPILER "blender-darwin-${CMAKE_OSX_ARCHITECTURES}")
  else()
    if(BLENDER_PLATFORM_ARM)
      set(SSL_OS_COMPILER "blender-linux-aarch64")
    elseif("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
      set(SSL_EXTRA_ARGS enable-ec_nistp_64_gcc_128)
      set(SSL_OS_COMPILER "blender-linux-x86_64")
    else()
      set(SSL_OS_COMPILER "blender-linux-x86")
    endif()
  endif()

  ExternalProject_Add(external_ssl
    URL file://${PACKAGE_DIR}/${SSL_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${SSL_HASH_TYPE}=${SSL_HASH}
    PREFIX ${BUILD_DIR}/ssl

    CONFIGURE_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/ssl/src/external_ssl/ &&
      ${SSL_CONFIGURE_COMMAND}
        --prefix=${LIBDIR}/ssl
        --openssldir=${LIBDIR}/ssl
        # Without this: Python will use the build directories.
        # using the system directory `/etc/ssl` might seem the obvious choice,
        # there is no guarantee the version of SSL used with Blender is compatible with the systems,
        # where changes to the SSL configuration format can cause SSL not to load (see #114452).
        # So reference a directory known not to exist. Ideally Blender could distribute its own SSL
        # directory, but this isn't compatible with hard coded paths.
        # See #111132 & https://github.com/openssl/openssl/issues/20185 for details.
        -DOPENSSLDIR=\\"/dev/null\\"
        no-shared
        no-idea no-mdc2 no-rc5 no-zlib no-ssl3 enable-unit-test no-ssl3-method enable-rfc3779 enable-cms
        --config=${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl.conf
        ${SSL_OS_COMPILER}

    BUILD_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/ssl/src/external_ssl/ &&
      make -j${MAKE_THREADS}

    INSTALL_COMMAND ${CONFIGURE_ENV} &&
      cd ${BUILD_DIR}/ssl/src/external_ssl/ &&
      make install

    INSTALL_DIR ${LIBDIR}/ssl
  )
endif()
