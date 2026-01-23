# SPDX-FileCopyrightText: 2002-2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  if(BLENDER_PLATFORM_WINDOWS_ARM)
    set(FFI_TARGET_HOST aarch64-w64-mingw32)
    set(FFI_PYTHON_TARGET_ARCH arm64)
    set(FFI_MSVC_SH_FLAGS -marm64)
  else()
    set(FFI_TARGET_HOST x86_64-w64-mingw32)
    set(FFI_PYTHON_TARGET_ARCH amd64)
    set(FFI_MSVC_SH_FLAGS -m64)
  endif()

  set(CONFIGURE_ENV_FFI ${CONFIGURE_ENV} &&
    set CC=${BUILD_DIR}/ffi/src/external_ffi/msvcc.sh ${FFI_MSVC_SH_FLAGS} &&
    set CXX=${BUILD_DIR}/ffi/src/external_ffi/msvcc.sh ${FFI_MSVC_SH_FLAGS} && 
    set LD=link &&
    set CPP=cl -nologo -EP &&
    set CXXCPP=cl -nologo -EP &&
    set CPPFLAGS=-DFFI_BUILDING_DLL
  )
  # `make install` just refuses to do its job without erroring
  # out, "fine ,then don't". We'll pick the files we need ourselves
  # from the build folder in the after install cmake step.
  set(FFI_INSTALL echo .)
  set(FFI_EXTRA_ARGS 
      --disable-docs
      --disable-multi-os-directory
      --enable-shared=yes
      --enable-static=no
      --build=${FFI_TARGET_HOST}
      --host=${FFI_TARGET_HOST}
  )
else()
  set(CONFIGURE_ENV_FFI ${CONFIGURE_ENV})
  set(FFI_INSTALL make install)
  set(FFI_EXTRA_ARGS
      --disable-multi-os-directory
      --enable-shared=no
      --enable-static=yes
      --with-pic  
  )
endif()

ExternalProject_Add(external_ffi
  URL file://${PACKAGE_DIR}/${FFI_FILE}
  URL_HASH ${FFI_HASH_TYPE}=${FFI_HASH}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  PREFIX ${BUILD_DIR}/ffi

  CONFIGURE_COMMAND ${CONFIGURE_ENV_FFI} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/ffi
      --libdir=${LIBDIR}/ffi/lib/
      ${FFI_EXTRA_ARGS}

  BUILD_COMMAND ${CONFIGURE_ENV_FFI} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    make -j${MAKE_THREADS}

  INSTALL_COMMAND ${CONFIGURE_ENV} &&
    cd ${BUILD_DIR}/ffi/src/external_ffi/ &&
    ${FFI_INSTALL}

  INSTALL_DIR ${LIBDIR}/ffi
)

if(UNIX AND NOT APPLE)
  ExternalProject_Add_Step(external_ffi after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${LIBDIR}/ffi/lib/libffi.a
      ${LIBDIR}/ffi/lib/libffi_pic.a

    DEPENDEES install
  )
elseif(WIN32)
  # Since there are currently no other consumers of ffi
  # we populate the libdir in the way python wants to see
  # it so we don't have to do this later on in the build
  ExternalProject_Add_Step(external_ffi after_install
    COMMAND ${CMAKE_COMMAND} -E copy
      ${BUILD_DIR}/ffi/src/external_ffi/${FFI_TARGET_HOST}/.libs/libffi-8.dll
      ${LIBDIR}/ffi/${FFI_PYTHON_TARGET_ARCH}/libffi-8.dll
    COMMAND ${CMAKE_COMMAND} -E copy
      ${BUILD_DIR}/ffi/src/external_ffi/${FFI_TARGET_HOST}/.libs/libffi-8.lib
      ${LIBDIR}/ffi/${FFI_PYTHON_TARGET_ARCH}/libffi-8.lib
    COMMAND ${CMAKE_COMMAND} -E copy
      ${BUILD_DIR}/ffi/src/external_ffi/${FFI_TARGET_HOST}/include/ffi.h
      ${LIBDIR}/ffi/${FFI_PYTHON_TARGET_ARCH}/include/ffi.h
    COMMAND ${CMAKE_COMMAND} -E copy
      ${BUILD_DIR}/ffi/src/external_ffi/${FFI_TARGET_HOST}/include/ffitarget.h
      ${LIBDIR}/ffi/${FFI_PYTHON_TARGET_ARCH}/include/ffitarget.h
    COMMAND ${CMAKE_COMMAND} -E copy
      ${BUILD_DIR}/ffi/src/external_ffi/${FFI_TARGET_HOST}/fficonfig.h
      ${LIBDIR}/ffi/${FFI_PYTHON_TARGET_ARCH}/include/fficonfig.h
    DEPENDEES install
  )
endif()
