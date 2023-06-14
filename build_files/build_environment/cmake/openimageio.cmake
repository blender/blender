# SPDX-License-Identifier: GPL-2.0-or-later

if(BUILD_MODE STREQUAL Release)
  set(OIIO_TOOLS ON)
else()
  set(OIIO_TOOLS OFF)
endif()

if(UNIX AND NOT APPLE)
  # This causes linking to static pthread libraries which gives link errors.
  # Since we manually specify library paths it should static link other libs.
  set(OPENIMAGEIO_LINKSTATIC -DLINKSTATIC=OFF)
else()
  set(OPENIMAGEIO_LINKSTATIC -DLINKSTATIC=ON)
endif()

if(WIN32)
  set(OIIO_SIMD_FLAGS -DUSE_SIMD=sse2)
  set(OPENJPEG_POSTFIX _msvc)
  if(BUILD_MODE STREQUAL Debug)
    set(TIFF_POSTFIX d)
    set(PNG_POSTFIX d)
  else()
    set(TIFF_POSTFIX)
    set(PNG_POSTFIX)
  endif()
  set(PNG_LIBNAME libpng16_static${PNG_POSTFIX}${LIBEXT})
else()
  set(PNG_LIBNAME libpng${LIBEXT})
  set(OIIO_SIMD_FLAGS)
  set(TIFF_POSTFIX)
endif()

if(MSVC)
  set(OPENJPEG_FLAGS
    -DOpenJPEG_ROOT=${LIBDIR}/openjpeg_msvc
  )
else()
  set(OPENJPEG_FLAGS
    -DOpenJPEG_ROOT=${LIBDIR}/openjpeg
  )
endif()

set(OPENIMAGEIO_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=ON
  ${OPENIMAGEIO_LINKSTATIC}
  ${DEFAULT_BOOST_FLAGS}
  -DUSE_LIBSQUISH=OFF
  -DUSE_QT5=OFF
  -DUSE_NUKE=OFF
  -DUSE_OPENVDB=OFF
  -DUSE_BZIP2=OFF
  -DUSE_FREETYPE=OFF
  -DUSE_DCMTK=OFF
  -DUSE_LIBHEIF=OFF
  -DUSE_OPENGL=OFF
  -DUSE_TBB=ON
  -DUSE_QT=OFF
  -DUSE_PYTHON=ON
  -DUSE_GIF=OFF
  -DUSE_OPENCV=OFF
  -DUSE_OPENJPEG=ON
  -DUSE_FFMPEG=OFF
  -DUSE_PTEX=OFF
  -DUSE_FREETYPE=OFF
  -DUSE_LIBRAW=OFF
  -DUSE_OPENCOLORIO=OFF
  -DUSE_WEBP=ON
  -DOIIO_BUILD_TOOLS=${OIIO_TOOLS}
  -DOIIO_BUILD_TESTS=OFF
  -DBUILD_TESTING=OFF
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include
  -DPNG_LIBRARY=${LIBDIR}/png/lib/${PNG_LIBNAME}
  -DPNG_PNG_INCLUDE_DIR=${LIBDIR}/png/include
  -DTIFF_LIBRARY=${LIBDIR}/tiff/lib/${LIBPREFIX}tiff${TIFF_POSTFIX}${LIBEXT}
  -DTIFF_INCLUDE_DIR=${LIBDIR}/tiff/include
  -DJPEG_LIBRARY=${LIBDIR}/jpeg/lib/${JPEG_LIBRARY}
  -DJPEG_INCLUDE_DIR=${LIBDIR}/jpeg/include
  ${OPENJPEG_FLAGS}
  -DOPENEXR_ILMTHREAD_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmThread${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DOPENEXR_IEX_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Iex${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DOPENEXR_ILMIMF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DSTOP_ON_WARNING=OFF
  -DUSE_EXTERNAL_PUGIXML=ON
  -DPUGIXML_LIBRARY=${LIBDIR}/pugixml/lib/${LIBPREFIX}pugixml${LIBEXT}
  -DPUGIXML_INCLUDE_DIR=${LIBDIR}/pugixml/include/
  -Dpugixml_DIR=${LIBDIR}/pugixml/lib/cmake/pugixml
  -DBUILD_MISSING_ROBINMAP=OFF
  -DBUILD_MISSING_FMT=OFF
  -DFMT_INCLUDE_DIR=${LIBDIR}/fmt/include/
  -DRobinmap_ROOT=${LIBDIR}/robinmap
  -DWebP_ROOT=${LIBDIR}/webp
  ${OIIO_SIMD_FLAGS}
  -DOpenEXR_ROOT=${LIBDIR}/openexr
  -DImath_ROOT=${LIBDIR}/imath
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -DTBB_ROOT=${LIBDIR}/tbb
)

ExternalProject_Add(external_openimageio
  URL file://${PACKAGE_DIR}/${OPENIMAGEIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENIMAGEIO_HASH_TYPE}=${OPENIMAGEIO_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openimageio
  PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/openimageio/src/external_openimageio/ < ${PATCH_DIR}/openimageio.diff &&
                ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/openimageio/src/external_openimageio/ < ${PATCH_DIR}/oiio_3832.diff &&
                ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/openimageio/src/external_openimageio/ < ${PATCH_DIR}/oiio_deadlock.diff &&
                ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/openimageio/src/external_openimageio/ < ${PATCH_DIR}/oiio_psd_8da473e254.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openimageio ${DEFAULT_CMAKE_FLAGS} ${OPENIMAGEIO_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/openimageio
)

add_dependencies(
  external_openimageio
  external_png
  external_zlib
  external_openexr
  external_imath
  external_jpeg
  external_boost
  external_tiff
  external_pugixml
  external_fmt
  external_robinmap
  external_openjpeg${OPENJPEG_POSTFIX}
  external_webp
  external_python
  external_pybind11
  external_tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openimageio after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/include ${HARVEST_TARGET}/OpenImageIO/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/lib ${HARVEST_TARGET}/OpenImageIO/lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/idiff.exe ${HARVEST_TARGET}/OpenImageIO/bin/idiff.exe
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/OpenImageIO.dll ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/OpenImageIO_Util.dll ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_Util.dll
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_openimageio after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO_d.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO_Util_d.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_Util_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/OpenImageIO_d.dll ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/OpenImageIO_Util_d.dll ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_Util_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/lib/python${PYTHON_SHORT_VERSION}/ ${HARVEST_TARGET}/OpenImageIO/lib/python${PYTHON_SHORT_VERSION}_debug/
      DEPENDEES install
    )
  endif()
endif()
