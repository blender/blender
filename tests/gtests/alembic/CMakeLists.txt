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
# Contributor(s): Sybren A. Stüvel
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	..
	../../../source/blender/blenlib
	../../../source/blender/alembic
	${ALEMBIC_INCLUDE_DIRS}
	${BOOST_INCLUDE_DIR}
	${HDF5_INCLUDE_DIRS}
	${OPENEXR_INCLUDE_DIRS}
)

include_directories(${INC})

setup_libdirs()
get_property(BLENDER_SORTED_LIBS GLOBAL PROPERTY BLENDER_SORTED_LIBS_PROP)

# For motivation on doubling BLENDER_SORTED_LIBS, see ../bmesh/CMakeLists.txt
BLENDER_SRC_GTEST(abc_matrix "abc_matrix_test.cc" "${BLENDER_SORTED_LIBS};${BLENDER_SORTED_LIBS}")

setup_liblinks(abc_matrix_test)
