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

set(INC
  .
  ../guardedalloc
)

set(INC_SYS
  ../../extern/quadriflow/src
  ${BOOST_INCLUDE_DIR}
  ${EIGEN3_INCLUDE_DIRS}
)

set(SRC
  quadriflow_capi.cpp
  quadriflow_capi.hpp
)

set(LIB
  extern_quadriflow
  ${BOOST_LIBRARIES}
)

if(WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
endif()

blender_add_lib(bf_intern_quadriflow "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
