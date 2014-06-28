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
# Contributor(s): Howard Trickey
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	..
	../../../source/blender/blenlib
	../../../source/blender/makesdna
	../../../source/blender/bmesh
	../../../intern/guardedalloc
)

include_directories(${INC})

setup_libdirs()
get_property(BLENDER_SORTED_LIBS GLOBAL PROPERTY BLENDER_SORTED_LIBS_PROP)

# Current BLENDER_SORTED_LIBS works with starting list of symbols in creator, but not
# for this test. Doubling the list does let all the symbols be resolved, but link time is a bit painful.
set(BLENDER_SORTED_LIBS ${BLENDER_SORTED_LIBS} ${BLENDER_SORTED_LIBS})

if(WITH_BUILDINFO)
	set(_buildinfo_src "$<TARGET_OBJECTS:buildinfoobj>")
else()
	set(_buildinfo_src "")
endif()
BLENDER_SRC_GTEST(bmesh_core "bmesh_core_test.cc;${_buildinfo_src}" "${BLENDER_SORTED_LIBS}")
unset(_buildinfo_src)

setup_liblinks(bmesh_core_test)
