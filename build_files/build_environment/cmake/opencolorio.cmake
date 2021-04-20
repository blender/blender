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

set(OPENCOLORIO_EXTRA_ARGS
  -DOCIO_BUILD_APPS=OFF
  -DOCIO_BUILD_PYTHON=OFF
  -DOCIO_BUILD_NUKE=OFF
  -DOCIO_BUILD_JAVA=OFF
  -DBUILD_SHARED_LIBS=OFF
  -DOCIO_BUILD_DOCS=OFF
  -DOCIO_BUILD_TESTS=OFF
  -DOCIO_BUILD_GPU_TESTS=OFF
  -DOCIO_USE_SSE=ON

  # Manually build ext packages except for pystring, which does not have
  # a CMake or autotools build system that we can easily use.
  -DOCIO_INSTALL_EXT_PACKAGES=MISSING
  -DHalf_ROOT=${LIBDIR}/openexr
  -DHalf_STATIC_LIBRARY=ON
  -Dexpat_ROOT=${LIBDIR}/expat
  -Dyaml-cpp_ROOT=${LIBDIR}/yamlcpp
)

if(BLENDER_PLATFORM_ARM)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    -DOCIO_USE_SSE=OFF
  )
endif()

if(WIN32)
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
    -DOCIO_INLINES_HIDDEN=OFF
  )
else()
  set(OPENCOLORIO_EXTRA_ARGS
    ${OPENCOLORIO_EXTRA_ARGS}
  )
endif()

ExternalProject_Add(external_opencolorio
  URL file://${PACKAGE_DIR}/${OPENCOLORIO_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${OPENCOLORIO_HASH_TYPE}=${OPENCOLORIO_HASH}
  PREFIX ${BUILD_DIR}/opencolorio
  PATCH_COMMAND ${PATCH_CMD} -p 1 -N -d ${BUILD_DIR}/opencolorio/src/external_opencolorio < ${PATCH_DIR}/opencolorio.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/opencolorio ${DEFAULT_CMAKE_FLAGS} ${OPENCOLORIO_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/opencolorio
)

add_dependencies(
  external_opencolorio
  external_yamlcpp
  external_expat
  external_openexr
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/include ${HARVEST_TARGET}/opencolorio/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencolorio/lib ${HARVEST_TARGET}/opencolorio/lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/yamlcpp/lib/libyaml-cppmd.lib ${HARVEST_TARGET}/opencolorio/lib/libyaml-cpp.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/expat/lib/libexpatMD.lib ${HARVEST_TARGET}/opencolorio/lib/libexpatMD.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/opencolorio/src/external_opencolorio-build/ext/dist/lib/pystring.lib ${HARVEST_TARGET}/opencolorio/lib/pystring.lib
      DEPENDEES install
    )
  endif()
  if(BUILD_MODE STREQUAL Debug)
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/opencolorio/lib/Opencolorio.lib ${HARVEST_TARGET}/opencolorio/lib/OpencolorIO_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/yamlcpp/lib/libyaml-cppmdd.lib ${HARVEST_TARGET}/opencolorio/lib/libyaml-cpp_d.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/expat/lib/libexpatdMD.lib ${HARVEST_TARGET}/opencolorio/lib/libexpatdMD.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/opencolorio/src/external_opencolorio-build/ext/dist/lib/pystring.lib ${HARVEST_TARGET}/opencolorio/lib/pystring_d.lib
      DEPENDEES install
    )
  endif()
else()
    ExternalProject_Add_Step(external_opencolorio after_install
      COMMAND cp ${LIBDIR}/yamlcpp/lib/libyaml-cpp.a ${LIBDIR}/opencolorio/lib/
      COMMAND cp ${LIBDIR}/expat/lib/libexpat.a ${LIBDIR}/opencolorio/lib/
      COMMAND cp ${BUILD_DIR}/opencolorio/src/external_opencolorio-build/ext/dist/lib/libpystring.a ${LIBDIR}/opencolorio/lib/
      DEPENDEES install
    )
endif()
