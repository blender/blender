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
#
# Contributor(s): Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	..
	../../../source/blender/blenlib
	../../../source/blender/makesdna
	../../../intern/guardedalloc
)

include_directories(${INC})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}")


BLENDER_TEST(BLI_stack "bf_blenlib")
BLENDER_TEST(BLI_math_color "bf_blenlib")
BLENDER_TEST(BLI_math_geom "bf_blenlib")
BLENDER_TEST(BLI_string "bf_blenlib")
BLENDER_TEST(BLI_path_util "bf_blenlib;extern_wcwidth;${ZLIB_LIBRARIES}")
BLENDER_TEST(BLI_listbase "bf_blenlib")
