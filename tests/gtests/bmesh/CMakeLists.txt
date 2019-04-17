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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ..
  ../../../source/blender/blenlib
  ../../../source/blender/makesdna
  ../../../source/blender/bmesh
  ../../../intern/guardedalloc
)

set(LIB
  bf_blenloader  # Should not be needed but gives linking error without it.
  bf_intern_opencolorio # Should not be needed but gives windows linker errors if the ocio libs are linked before this
  bf_gpu # Should not be needed but gives windows linker errors if the ocio libs are linked before this
  bf_bmesh
)

include_directories(${INC})

setup_libdirs()

if(WITH_BUILDINFO)
  set(_buildinfo_src "$<TARGET_OBJECTS:buildinfoobj>")
else()
  set(_buildinfo_src "")
endif()
BLENDER_SRC_GTEST(bmesh_core "bmesh_core_test.cc;${_buildinfo_src}" "${LIB}")
unset(_buildinfo_src)

setup_liblinks(bmesh_core_test)
