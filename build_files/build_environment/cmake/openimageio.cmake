# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
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
  set(OIIO_SIMD_FLAGS -DUSE_SIMD=sse4.2)
else()
  set(OPENIMAGEIO_LINKSTATIC -DLINKSTATIC=ON)
endif()

if(WIN32)
  if(BLENDER_PLATFORM_ARM)
    set(OIIO_SIMD_FLAGS -DUSE_SIMD=0)
  else()
    set(OIIO_SIMD_FLAGS -DUSE_SIMD=sse4.2)
  endif()
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
  -DOpenImageIO_REQUIRED_DEPS=WebP$<SEMICOLON>JPEGTurbo$<SEMICOLON>TIFF$<SEMICOLON>OpenEXR$<SEMICOLON>PNG$<SEMICOLON>OpenJPEG$<SEMICOLON>fmt$<SEMICOLON>Robinmap$<SEMICOLON>ZLIB$<SEMICOLON>pugixml$<SEMICOLON>Python
  -DUSE_NUKE=OFF
  -DUSE_OPENVDB=OFF
  -DUSE_FREETYPE=OFF
  -DUSE_DCMTK=OFF
  -DUSE_LIBHEIF=OFF
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
  -DUSE_JXL=OFF
  -DUSE_OPENCOLORIO=ON
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
  -DJPEG_ROOT=${LIBDIR}/jpeg/
  -Dlibjpeg-turbo_ROOT=${LIBDIR}/jpeg/
  ${OPENJPEG_FLAGS}
  -DOPENEXR_ILMTHREAD_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmThread${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DOPENEXR_IEX_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Iex${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DOPENEXR_ILMIMF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${SHAREDLIBEXT}
  -DSTOP_ON_WARNING=OFF
  -DUSE_EXTERNAL_PUGIXML=ON
  -DPUGIXML_LIBRARY=${LIBDIR}/pugixml/lib/${LIBPREFIX}pugixml${LIBEXT}
  -DPUGIXML_INCLUDE_DIR=${LIBDIR}/pugixml/include/
  -Dpugixml_DIR=${LIBDIR}/pugixml/lib/cmake/pugixml
  -DOpenColorIO_DIR=${LIBDIR}/opencolorio/lib/cmake/OpenColorIO
  -DOpenImageIO_BUILD_MISSING_DEPS=""
  -DFMT_INCLUDE_DIR=${LIBDIR}/fmt/include/
  -DRobinmap_ROOT=${LIBDIR}/robinmap
  -DWebP_ROOT=${LIBDIR}/webp
  ${OIIO_SIMD_FLAGS}
  -DOpenEXR_ROOT=${LIBDIR}/openexr
  -DImath_ROOT=${LIBDIR}/imath
  -Dpybind11_ROOT=${LIBDIR}/pybind11
  -DPython_EXECUTABLE=${PYTHON_BINARY}
  -DPython3_EXECUTABLE=${PYTHON_BINARY}
  -DTBB_ROOT=${LIBDIR}/tbb
  -Dlibdeflate_ROOT=${LIBDIR}/deflate
  -Dfmt_ROOT=${LIBDIR}/fmt
)

if(WIN32)
  # We don't want the SOABI tags in the final filename since it gets the debug
  # tags wrong and the final .pyd won't be found by python, pybind11 will try to
  # get the tags and dump them into PYTHON_MODULE_EXTENSION every time the current
  # python interpreter doesn't match the old one, overwriting our preference.
  # To side step this behavior we set PYBIND11_PYTHON_EXECUTABLE_LAST so it'll
  # leave the PYTHON_MODULE_EXTENSION value we set alone.
  list(APPEND OPENIMAGEIO_EXTRA_ARGS -DPYBIND11_PYTHON_EXECUTABLE_LAST=${PYTHON_BINARY})
  if(BUILD_MODE STREQUAL Release)
    list(APPEND OPENIMAGEIO_EXTRA_ARGS -DPYTHON_MODULE_EXTENSION=.pyd)
  else()
    list(APPEND OPENIMAGEIO_EXTRA_ARGS -DPYTHON_MODULE_EXTENSION=_d.pyd)
  endif()
endif()

ExternalProject_Add(external_openimageio
  URL file://${PACKAGE_DIR}/${OPENIMAGEIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENIMAGEIO_HASH_TYPE}=${OPENIMAGEIO_HASH}
  CMAKE_GENERATOR ${PLATFORM_ALT_GENERATOR}
  PREFIX ${BUILD_DIR}/openimageio

  PATCH_COMMAND
    ${PATCH_CMD} -p 1 -N -d
      ${BUILD_DIR}/openimageio/src/external_openimageio/ <
      ${PATCH_DIR}/openimageio.diff &&
    ${PATCH_CMD} -p 1 -N -d
      ${BUILD_DIR}/openimageio/src/external_openimageio/ <
      ${PATCH_DIR}/openimageio_png_cicp_4746.diff
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openimageio
    ${DEFAULT_CMAKE_FLAGS}
    ${OPENIMAGEIO_EXTRA_ARGS}

  INSTALL_DIR ${LIBDIR}/openimageio
)

add_dependencies(
  external_openimageio
  external_opencolorio
  external_png
  external_zlib
  external_openexr
  external_imath
  external_jpeg
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
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/OpenImageIO/include
        ${HARVEST_TARGET}/OpenImageIO/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/OpenImageIO/lib
        ${HARVEST_TARGET}/OpenImageIO/lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/iconvert.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/iconvert.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/idiff.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/idiff.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/igrep.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/igrep.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/iinfo.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/iinfo.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/maketx.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/maketx.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/oiiotool.exe
        ${HARVEST_TARGET}/OpenImageIO/bin/oiiotool.exe
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/OpenImageIO.dll
        ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/OpenImageIO_Util.dll
        ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_Util.dll

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_openimageio after_install
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openimageio/lib/OpenImageIO_d.lib
        ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/openimageio/lib/OpenImageIO_Util_d.lib
        ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_Util_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/OpenImageIO_d.dll
        ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/OpenImageIO/bin/OpenImageIO_Util_d.dll
        ${HARVEST_TARGET}/OpenImageIO/bin/OpenImageIO_Util_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/OpenImageIO/lib/python${PYTHON_SHORT_VERSION}/
        ${HARVEST_TARGET}/OpenImageIO/lib/python${PYTHON_SHORT_VERSION}_debug/

      DEPENDEES install
    )
  endif()
else()
  harvest_rpath_bin(external_openimageio openimageio/bin openimageio/bin "idiff")
  harvest_rpath_bin(external_openimageio openimageio/bin openimageio/bin "maketx")
  harvest_rpath_bin(external_openimageio openimageio/bin openimageio/bin "oiiotool")
  harvest(external_openimageio openimageio/include openimageio/include "*")
  harvest_rpath_lib(external_openimageio openimageio/lib openimageio/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(external_openimageio
    openimageio/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
endif()
