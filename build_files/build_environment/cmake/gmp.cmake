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

set(GMP_EXTRA_ARGS -enable-cxx)

if(WIN32)
  # Shared for windows because static libs will drag in a libgcc dependency.
  set(GMP_OPTIONS --disable-static --enable-shared --host=x86_64-w64-mingw32 --build=x86_64-w64-mingw32)
else()
  set(GMP_OPTIONS --enable-static --disable-shared )
endif()

ExternalProject_Add(external_gmp
  URL ${GMP_URI}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH MD5=${GMP_HASH}
  PREFIX ${BUILD_DIR}/gmp
  CONFIGURE_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && ${CONFIGURE_COMMAND} --prefix=${LIBDIR}/gmp ${GMP_OPTIONS} ${GMP_EXTRA_ARGS}
  BUILD_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && make -j${MAKE_THREADS}
  INSTALL_COMMAND ${CONFIGURE_ENV_NO_PERL} && cd ${BUILD_DIR}/gmp/src/external_gmp/ && make install
  INSTALL_DIR ${LIBDIR}/gmp
)

if(MSVC)
  set_target_properties(external_gmp PROPERTIES FOLDER Mingw)
endif()

if(BUILD_MODE STREQUAL Release AND WIN32)
  ExternalProject_Add_Step(external_gmp after_install
      COMMAND  ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-3.dll.def ${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.def
      COMMAND  lib /def:${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.def /machine:x64 /out:${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.lib
      COMMAND  ${CMAKE_COMMAND} -E copy ${LIBDIR}/gmp/bin/libgmp-10.dll ${HARVEST_TARGET}/gmp/lib/libgmp-10.dll
      COMMAND  ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.lib ${HARVEST_TARGET}/gmp/lib/libgmp-10.lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/gmp/include ${HARVEST_TARGET}/gmp/include
    DEPENDEES install
  )
endif()

if(BUILD_MODE STREQUAL Debug AND WIN32)
ExternalProject_Add_Step(external_gmp after_install
      COMMAND  ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-3.dll.def ${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.def
      COMMAND  lib /def:${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.def /machine:x64 /out:${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.lib
    DEPENDEES install
  )
endif()

if(WIN32)
  # gmpxx is somewhat special, it builds on top of the C style gmp library but exposes C++ bindings
  # given the C++ ABI between MSVC and mingw is not compatible, we need to build the bindings
  # with MSVC, while GMP can only be build with mingw.
  ExternalProject_Add(external_gmpxx
    URL ${GMP_URI}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH MD5=${GMP_HASH}
    PREFIX ${BUILD_DIR}/gmpxx
    PATCH_COMMAND COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/cmakelists_gmpxx.txt ${BUILD_DIR}/gmpxx/src/external_gmpxx/CMakeLists.txt &&
                          ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/config_gmpxx.h ${BUILD_DIR}/gmpxx/src/external_gmpxx/config.h
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/gmpxx ${DEFAULT_CMAKE_FLAGS} -DGMP_LIBRARY=${BUILD_DIR}/gmp/src/external_gmp/.libs/libgmp-10.lib -DGMP_INCLUDE_DIR=${BUILD_DIR}/gmp/src/external_gmp -DCMAKE_DEBUG_POSTFIX=_d
    INSTALL_DIR ${LIBDIR}/gmpxx
  )
  set_target_properties(external_gmpxx PROPERTIES FOLDER Mingw)

  add_dependencies(
    external_gmpxx
    external_gmp
  )

  ExternalProject_Add_Step(external_gmpxx after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/gmpxx/ ${HARVEST_TARGET}/gmp
    DEPENDEES install
  )

endif()
