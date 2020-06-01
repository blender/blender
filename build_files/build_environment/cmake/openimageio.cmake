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
  set(PNG_LIBNAME libpng16_static${LIBEXT})
  set(OIIO_SIMD_FLAGS -DUSE_SIMD=sse2)
  set(OPENJPEG_POSTFIX _msvc)
else()
  set(PNG_LIBNAME libpng${LIBEXT})
  set(OIIO_SIMD_FLAGS)
endif()

if(WITH_WEBP)
  set(WEBP_ARGS
    -DWEBP_INCLUDE_DIR=${LIBDIR}/webp/include
    -DWEBP_LIBRARY=${LIBDIR}/webp/lib/${LIBPREFIX}webp${LIBEXT}
  )
  set(WEBP_DEP external_webp)
endif()

if(MSVC)
  set(OPENJPEG_FLAGS
    -DOpenJpeg_ROOT=${LIBDIR}/openjpeg_msvc
  )
else()
  set(OPENJPEG_FLAGS
    -DOpenJpeg_ROOT=${LIBDIR}/openjpeg
  )
endif()

set(OPENIMAGEIO_EXTRA_ARGS
  -DBUILD_SHARED_LIBS=OFF
  ${OPENIMAGEIO_LINKSTATIC}
  -DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
  -DBoost_USE_MULTITHREADED=ON
  -DBoost_USE_STATIC_LIBS=ON
  -DBoost_USE_STATIC_RUNTIME=OFF
  -DBOOST_ROOT=${LIBDIR}/boost
  -DBOOST_LIBRARYDIR=${LIBDIR}/boost/lib/
  -DBoost_NO_SYSTEM_PATHS=ON
  -OIIO_BUILD_CPP11=ON
  -DUSE_LIBSQUISH=OFF
  -DUSE_QT5=OFF
  -DUSE_NUKE=OFF
  -DUSE_OPENVDB=OFF
  -DUSE_BZIP2=OFF
  -DUSE_FREETYPE=OFF
  -DUSE_DCMTK=OFF
  -DUSE_LIBHEIF=OFF
  -DUSE_OPENGL=OFF
  -DUSE_TBB=OFF
  -DUSE_FIELD3D=OFF
  -DUSE_QT=OFF
  -DUSE_PYTHON=OFF
  -DUSE_GIF=OFF
  -DUSE_OPENCV=OFF
  -DUSE_OPENJPEG=ON
  -DUSE_FFMPEG=OFF
  -DUSE_PTEX=OFF
  -DUSE_FREETYPE=OFF
  -DUSE_LIBRAW=OFF
  -DUSE_OPENCOLORIO=OFF
  -DUSE_WEBP=${WITH_WEBP}
  -DOIIO_BUILD_TOOLS=${OIIO_TOOLS}
  -DOIIO_BUILD_TESTS=OFF
  -DBUILD_TESTING=OFF
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include
  -DPNG_LIBRARY=${LIBDIR}/png/lib/${PNG_LIBNAME}
  -DPNG_PNG_INCLUDE_DIR=${LIBDIR}/png/include
  -DTIFF_LIBRARY=${LIBDIR}/tiff/lib/${LIBPREFIX}tiff${LIBEXT}
  -DTIFF_INCLUDE_DIR=${LIBDIR}/tiff/include
  -DJPEG_LIBRARY=${LIBDIR}/jpg/lib/${JPEG_LIBRARY}
  -DJPEG_INCLUDE_DIR=${LIBDIR}/jpg/include
  ${OPENJPEG_FLAGS}
  -DOpenEXR_USE_STATIC_LIBS=On
  -DILMBASE_INCLUDE_DIR=${LIBDIR}/openexr/include/
  -DOPENEXR_INCLUDE_DIR=${LIBDIR}/openexr/include/
  -DOPENEXR_HALF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Half${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_IMATH_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Imath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_ILMTHREAD_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmThread${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_IEX_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Iex${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_ILMIMF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DSTOP_ON_WARNING=OFF
  ${WEBP_FLAGS}
  ${OIIO_SIMD_FLAGS}
)

ExternalProject_Add(external_openimageio
  URL ${OPENIMAGEIO_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${OPENIMAGEIO_HASH}
  PREFIX ${BUILD_DIR}/openimageio
  PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/openimageio/src/external_openimageio/ < ${PATCH_DIR}/openimageio.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/openimageio ${DEFAULT_CMAKE_FLAGS} ${OPENIMAGEIO_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/openimageio
)

add_dependencies(
  external_openimageio
  external_png
  external_zlib
  external_openexr
  external_jpeg
  external_boost
  external_tiff
  external_openjpeg${OPENJPEG_POSTFIX}
  ${WEBP_DEP}
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_openimageio after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/include ${HARVEST_TARGET}/OpenImageIO/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/lib ${HARVEST_TARGET}/OpenImageIO/lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/idiff.exe ${HARVEST_TARGET}/OpenImageIO/bin/idiff.exe

      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_openimageio after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO_Util.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_Util_d.lib
      DEPENDEES install
    )
  endif()
endif()
