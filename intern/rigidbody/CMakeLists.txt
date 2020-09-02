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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

add_definitions(-DBT_USE_DOUBLE_PRECISION)

set(INC
  .
)

set(INC_SYS
  ${BULLET_INCLUDE_DIRS}
)

set(SRC
  rb_bullet_api.cpp
  rb_convex_hull_api.cpp

  RBI_hull_api.h
  RBI_api.h
)

set(LIB
  extern_bullet
  ${BULLET_LIBRARIES}
)

blender_add_lib(bf_intern_rigidbody "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
