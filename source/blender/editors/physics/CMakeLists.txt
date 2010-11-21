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

SET(INC
	../include
	../../blenkernel
	../../blenlib
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/elbeem/extern
	../../../../intern/guardedalloc
)

SET(SRC
	particle_boids.c
	particle_edit.c
	particle_object.c
	physics_fluid.c
	physics_ops.c
	physics_pointcache.c
)

IF(NOT WITH_MOD_FLUID)
	ADD_DEFINITIONS(-DDISABLE_ELBEEM)
ENDIF(NOT WITH_MOD_FLUID)

IF(WITH_OPENMP)
		ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

BLENDERLIB(bf_editor_physics "${SRC}" "${INC}")
