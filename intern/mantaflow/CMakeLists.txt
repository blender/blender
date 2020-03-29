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
#
# The Original Code is: all of this file.
#
# Contributor(s): Sebastian Barschkis (sebbas)
#
# ***** END GPL LICENSE BLOCK *****

add_definitions(-DWITH_FLUID=1)

if(WITH_OPENVDB)
  add_definitions(-DOPENVDB=1)
else()
  add_definitions(-DOPENVDB=0)
endif()

if(WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
endif()


set(INC
  extern
  intern/strings
  ../../source/blender/makesdna
  ../../source/blender/blenlib
)

# Python is always required
add_definitions(-DWITH_PYTHON)

set(INC_SYS
  ../../extern/mantaflow/helper/util
  ../../extern/mantaflow/helper/pwrapper
  ../../extern/mantaflow/preprocessed
  ../guardedalloc
  ${PYTHON_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

set(SRC
  intern/manta_python_API.cpp
  intern/manta_fluid_API.cpp
  intern/MANTA_main.cpp

  extern/manta_python_API.h
  extern/manta_fluid_API.h
  intern/MANTA_main.h
  intern/strings/fluid_script.h
  intern/strings/smoke_script.h
  intern/strings/liquid_script.h
)

set(LIB
  extern_mantaflow

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
  ${ZLIB_LIBRARIES}
)

blender_add_lib(bf_intern_mantaflow "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
