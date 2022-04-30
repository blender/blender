# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)
  # OIIO and OSL are statically linked for us, but USD doesn't know
  set(USD_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DOIIO_STATIC_DEFINE /DOSL_STATIC_DEFINE")
  if(BUILD_MODE STREQUAL Debug)
    # USD does not look for debug libs, nor does it link them
    # when building static, so this is just to keep find_package happy
    # if we ever link dynamically on windows util will need to be linked as well.
    set(USD_OIIO_CMAKE_DEFINES "-DOIIO_LIBRARIES=${LIBDIR}/openimageio/lib/OpenImageIO_d${LIBEXT}")
  endif()
  set(USD_PLATFORM_FLAGS
    ${USD_OIIO_CMAKE_DEFINES}
    -DCMAKE_CXX_FLAGS=${USD_CXX_FLAGS}
  )
endif()

set(USD_EXTRA_ARGS
  ${DEFAULT_BOOST_FLAGS}
  ${USD_PLATFORM_FLAGS}
  # This is a preventative measure that avoids possible conflicts when add-ons
  # try to load another USD library into the same process space.
  -DPXR_SET_INTERNAL_NAMESPACE=usdBlender
  -DOPENSUBDIV_ROOT_DIR=${LIBDIR}/opensubdiv
  -DOpenImageIO_ROOT=${LIBDIR}/openimageio
  -DOPENEXR_LIBRARIES=${LIBDIR}/imath/lib/imath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_INCLUDE_DIR=${LIBDIR}/imath/include
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF
  -DPXR_BUILD_IMAGING=ON
  -DPXR_BUILD_TESTS=OFF
  -DPXR_BUILD_EXAMPLES=OFF
  -DPXR_BUILD_TUTORIALS=OFF
  -DPXR_ENABLE_HDF5_SUPPORT=OFF
  -DPXR_ENABLE_MATERIALX_SUPPORT=OFF
  -DPXR_ENABLE_OPENVDB_SUPPORT=OFF
  -DPYTHON_EXECUTABLE=${PYTHON_BINARY}
  -DPXR_BUILD_MONOLITHIC=ON
  # OSL is an optional dependency of the Imaging module. However, since that
  # module was included for its support for converting primitive shapes (sphere,
  # cube, etc.) to geometry, it's not necessary. Disabling it will make it
  # simpler to build Blender; currently only Cycles uses OSL.
  -DPXR_ENABLE_OSL_SUPPORT=OFF
  # GL support on Linux also links to X11 libraries. Enabling it would break
  # headless or Wayland-only builds. OpenGL support would be useful if someone
  # wants to work on a Hydra viewport in Blender; when that's actually being
  # worked on, we could patch in a new PXR_ENABLE_X11_SUPPORT option (to
  # separate OpenGL from X11) and contribute it upstream.
  -DPXR_ENABLE_GL_SUPPORT=OFF
  # Disable Metal since USD fails to build this when OpenGL is disabled.
  -DPXR_ENABLE_METAL_SUPPORT=OFF
  # OIIO is used for loading image textures in Hydra Storm / Embree renderers,
  # which we don't use.
  -DPXR_BUILD_OPENIMAGEIO_PLUGIN=OFF
  # USD 22.03 does not support OCIO 2.x
  # Tracking ticket https://github.com/PixarAnimationStudios/USD/issues/1386
  -DPXR_BUILD_OPENCOLORIO_PLUGIN=OFF
  -DPXR_ENABLE_PTEX_SUPPORT=OFF
  -DPXR_BUILD_USD_TOOLS=OFF
  -DCMAKE_DEBUG_POSTFIX=_d
  -DBUILD_SHARED_LIBS=Off
  # USD is hellbound on making a shared lib, unless you point this variable to a valid cmake file
  # doesn't have to make sense, but as long as it points somewhere valid it will skip the shared lib.
  -DPXR_MONOLITHIC_IMPORT=${BUILD_DIR}/usd/src/external_usd/cmake/defaults/Version.cmake
  -DTBB_INCLUDE_DIRS=${LIBDIR}/tbb/include
  -DTBB_LIBRARIES=${LIBDIR}/tbb/lib/${LIBPREFIX}${TBB_LIBRARY}${LIBEXT}
  -DTbb_TBB_LIBRARY=${LIBDIR}/tbb/lib/${LIBPREFIX}${TBB_LIBRARY}${LIBEXT}
  # USD wants the tbb debug lib set even when you are doing a release build
  # Otherwise it will error out during the cmake configure phase.
  -DTBB_LIBRARIES_DEBUG=${LIBDIR}/tbb/lib/${LIBPREFIX}${TBB_LIBRARY}${LIBEXT}
)

ExternalProject_Add(external_usd
  URL file://${PACKAGE_DIR}/${USD_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${USD_HASH_TYPE}=${USD_HASH}
  PREFIX ${BUILD_DIR}/usd
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/usd/src/external_usd < ${PATCH_DIR}/usd.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/usd -Wno-dev ${DEFAULT_CMAKE_FLAGS} ${USD_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/usd
)

add_dependencies(
  external_usd
  external_tbb
  external_boost
  external_opensubdiv
)

# Since USD 21.11 the libraries are prefixed with "usd_", i.e. "libusd_m.a" became "libusd_usd_m.a".
# See https://github.com/PixarAnimationStudios/USD/blob/release/CHANGELOG.md#2111---2021-11-01
if(NOT WIN32)
  if (USD_VERSION VERSION_LESS 21.11)
    set(PXR_LIB_PREFIX "")
  else()
    set(PXR_LIB_PREFIX "usd_")
  endif()
endif()

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_usd after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/usd/ ${HARVEST_TARGET}/usd
      COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/usd/src/external_usd-build/pxr/Release/usd_usd_m.lib ${HARVEST_TARGET}/usd/lib/usd_usd_m.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_usd after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/usd/lib ${HARVEST_TARGET}/usd/lib
      COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/usd/src/external_usd-build/pxr/Debug/usd_usd_m_d.lib ${HARVEST_TARGET}/usd/lib/usd_usd_m_d.lib
      DEPENDEES install
    )
  endif()
else()
  # USD has two build options. The default build creates lots of small libraries,
  # whereas the 'monolithic' build produces only a single library. The latter
  # makes linking simpler, so that's what we use in Blender. However, running
  # 'make install' in the USD sources doesn't install the static library in that
  # case (only the shared library). As a result, we need to grab the `libusd_m.a`
  # file from the build directory instead of from the install directory.
  ExternalProject_Add_Step(external_usd after_install
    COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/usd/src/external_usd-build/pxr/lib${PXR_LIB_PREFIX}usd_m.a ${HARVEST_TARGET}/usd/lib/lib${PXR_LIB_PREFIX}usd_m.a
    DEPENDEES install
  )
endif()
