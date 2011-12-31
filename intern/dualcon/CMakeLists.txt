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

set(INC
	.
	intern
	../../extern/Eigen3
)

set(SRC
	intern/manifold_table.cpp
	intern/marching_cubes_table.cpp
	intern/octree.cpp
	intern/Projections.cpp
	
	intern/cubes.h
	intern/GeoCommon.h
	intern/manifold_table.h
	intern/marching_cubes_table.h
	intern/MemoryAllocator.h
	intern/ModelReader.h
	intern/octree.h
	intern/Projections.h
	intern/Queue.h

	intern/dualcon_c_api.cpp
	dualcon.h
)

blender_add_lib(bf_intern_dualcon "${SRC}" "${INC}" "")

