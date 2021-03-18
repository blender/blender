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

if(WIN32)
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES "kernel32${LIBEXT} user32${LIBEXT} gdi32${LIBEXT} winspool${LIBEXT} shell32${LIBEXT} ole32${LIBEXT} oleaut32${LIBEXT} uuid${LIBEXT} comdlg32${LIBEXT} advapi32${LIBEXT} psapi${LIBEXT}")
  set(OSL_FLEX_BISON -DFLEX_EXECUTABLE=${LIBDIR}/flexbison/win_flex.exe -DBISON_EXECUTABLE=${LIBDIR}/flexbison/win_bison.exe)
  set(OSL_SIMD_FLAGS -DOIIO_NOSIMD=1 -DOIIO_SIMD=sse2)
  SET(OSL_PLATFORM_FLAGS -DLINKSTATIC=ON)
else()
  set(OSL_CMAKE_CXX_STANDARD_LIBRARIES)
  set(OSL_FLEX_BISON)
  set(OSL_OPENIMAGEIO_LIBRARY "${LIBDIR}/openimageio/lib/${LIBPREFIX}OpenImageIO${LIBEXT};${LIBDIR}/openimageio/lib/${LIBPREFIX}OpenImageIO_Util${LIBEXT};${LIBDIR}/png/lib/${LIBPREFIX}png16${LIBEXT};${LIBDIR}/jpg/lib/${LIBPREFIX}jpeg${LIBEXT};${LIBDIR}/tiff/lib/${LIBPREFIX}tiff${LIBEXT};${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${LIBEXT}")
  SET(OSL_PLATFORM_FLAGS)
endif()

set(OSL_ILMBASE_CUSTOM_LIBRARIES "${LIBDIR}/openexr/lib/Imath${OPENEXR_VERSION_POSTFIX}.lib^^${LIBDIR}/openexr/lib/Half{OPENEXR_VERSION_POSTFIX}.lib^^${LIBDIR}/openexr/lib/IlmThread${OPENEXR_VERSION_POSTFIX}.lib^^${LIBDIR}/openexr/lib/Iex${OPENEXR_VERSION_POSTFIX}.lib")

set(OSL_EXTRA_ARGS
  -DBoost_COMPILER:STRING=${BOOST_COMPILER_STRING}
  -DBoost_USE_MULTITHREADED=ON
  -DBoost_USE_STATIC_LIBS=ON
  -DBoost_USE_STATIC_RUNTIME=OFF
  -DBOOST_ROOT=${LIBDIR}/boost
  -DBOOST_LIBRARYDIR=${LIBDIR}/boost/lib/
  -DBoost_NO_SYSTEM_PATHS=ON
  -DBoost_NO_BOOST_CMAKE=ON
  -DOpenEXR_ROOT=${LIBDIR}/openexr/
  -DIlmBase_ROOT=${LIBDIR}/openexr/
  -DILMBASE_INCLUDE_DIR=${LIBDIR}/openexr/include/
  -DOPENEXR_HALF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Half${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_IMATH_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Imath${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_ILMTHREAD_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmThread${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_IEX_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}Iex${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOPENEXR_INCLUDE_DIR=${LIBDIR}/openexr/include/
  -DOPENEXR_ILMIMF_LIBRARY=${LIBDIR}/openexr/lib/${LIBPREFIX}IlmImf${OPENEXR_VERSION_POSTFIX}${LIBEXT}
  -DOpenImageIO_ROOT=${LIBDIR}/openimageio/
  -DOSL_BUILD_TESTS=OFF
  -DOSL_BUILD_MATERIALX=OFF
  -DZLIB_LIBRARY=${LIBDIR}/zlib/lib/${ZLIB_LIBRARY}
  -DZLIB_INCLUDE_DIR=${LIBDIR}/zlib/include/
  ${OSL_FLEX_BISON}
  -DCMAKE_CXX_STANDARD_LIBRARIES=${OSL_CMAKE_CXX_STANDARD_LIBRARIES}
  -DBUILD_SHARED_LIBS=OFF
  ${OSL_PLATFORM_FLAGS}
  -DOSL_BUILD_PLUGINS=OFF
  -DSTOP_ON_WARNING=OFF
  -DUSE_LLVM_BITCODE=OFF
  -DLLVM_ROOT=${LIBDIR}/llvm/
  -DLLVM_DIRECTORY=${LIBDIR}/llvm/
  -DUSE_PARTIO=OFF
  -DUSE_QT=OFF
  -DUSE_Qt5=OFF
  -DINSTALL_DOCS=OFF
  ${OSL_SIMD_FLAGS}
  -Dpugixml_ROOT=${LIBDIR}/pugixml
  -DUSE_PYTHON=OFF
)

# Apple arm64 uses LLVM 11, LLVM 10+ requires C++14
if (APPLE AND "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
  list(APPEND OSL_EXTRA_ARGS -DCMAKE_CXX_STANDARD=14)
endif()

ExternalProject_Add(external_osl
  URL file://${PACKAGE_DIR}/${OSL_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  LIST_SEPARATOR ^^
  URL_HASH ${OSL_HASH_TYPE}=${OSL_HASH}
  PREFIX ${BUILD_DIR}/osl
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/osl/src/external_osl < ${PATCH_DIR}/osl.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/osl -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ${DEFAULT_CMAKE_FLAGS} ${OSL_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/osl
)

add_dependencies(
  external_osl
  external_boost
  ll
  external_openexr
  external_zlib
  external_flexbison
  external_openimageio
  external_pugixml
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_osl after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/osl/ ${HARVEST_TARGET}/osl
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_osl after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslcomp.lib ${HARVEST_TARGET}/osl/lib/oslcomp_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslexec.lib ${HARVEST_TARGET}/osl/lib/oslexec_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslquery.lib ${HARVEST_TARGET}/osl/lib/oslquery_d.lib
      DEPENDEES install
    )
  endif()
endif()
