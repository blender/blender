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
# The Original Code is Copyright (C) 2019, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

# This suppresses the warning "This file includes at least one deprecated or antiquated
# header which may be removed without further notice at a future date", which is caused
# by the USD library including <ext/hash_set> on Linux. This has been reported at:
# https://github.com/PixarAnimationStudios/USD/issues/1057.
if(UNIX AND NOT APPLE)
  add_definitions(-D_GLIBCXX_PERMIT_BACKWARD_HASH)
endif()
if(WIN32)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()
add_definitions(-DPXR_STATIC)

set(INC
  .
  ../common
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${TBB_INCLUDE_DIR}
)

set(SRC
  intern/usd_capi.cc
  intern/usd_hierarchy_iterator.cc
  intern/usd_writer_abstract.cc
  intern/usd_writer_camera.cc
  intern/usd_writer_hair.cc
  intern/usd_writer_light.cc
  intern/usd_writer_mesh.cc
  intern/usd_writer_metaball.cc
  intern/usd_writer_transform.cc

  usd.h
  intern/usd_exporter_context.h
  intern/usd_hierarchy_iterator.h
  intern/usd_writer_abstract.h
  intern/usd_writer_camera.h
  intern/usd_writer_hair.h
  intern/usd_writer_light.h
  intern/usd_writer_mesh.h
  intern/usd_writer_metaball.h
  intern/usd_writer_transform.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_io_common
)

list(APPEND LIB
  ${BOOST_LIBRARIES}
)

list(APPEND LIB
)

blender_add_lib(bf_usd "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WIN32)
  set_property(TARGET bf_usd APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " /WHOLEARCHIVE:${USD_DEBUG_LIB}")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " /WHOLEARCHIVE:${USD_RELEASE_LIB}")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY LINK_FLAGS_RELWITHDEBINFO " /WHOLEARCHIVE:${USD_RELEASE_LIB}")
  set_property(TARGET bf_usd APPEND_STRING PROPERTY LINK_FLAGS_MINSIZEREL " /WHOLEARCHIVE:${USD_RELEASE_LIB}")
endif()

# Source: https://github.com/PixarAnimationStudios/USD/blob/master/BUILDING.md#linking-whole-archives
if(WIN32)
  target_link_libraries(bf_usd INTERFACE ${USD_LIBRARIES})
elseif(CMAKE_COMPILER_IS_GNUCXX)
  target_link_libraries(bf_usd INTERFACE "-Wl,--whole-archive ${USD_LIBRARIES} -Wl,--no-whole-archive ${TBB_LIBRARIES}")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  target_link_libraries(bf_usd INTERFACE -Wl,-force_load ${USD_LIBRARIES})
else()
  message(FATAL_ERROR "Unknown how to link USD with your compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

target_link_libraries(bf_usd INTERFACE ${TBB_LIBRARIES})
