# $Id: CMakeLists.txt 12931 2007-12-17 18:20:48Z theeth $
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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenkernel
	../../blenlib
	../../blenloader
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/elbeem/extern
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	particle_boids.c
	particle_edit.c
	particle_object.c
	physics_fluid.c
	physics_ops.c
	physics_pointcache.c

	physics_intern.h
)

if(NOT WITH_MOD_FLUID)
	add_definitions(-DDISABLE_ELBEEM)
endif()

if(WITH_OPENMP)
	add_definitions(-DPARALLEL=1)
endif()

blender_add_lib(bf_editor_physics "${SRC}" "${INC}" "${INC_SYS}")
