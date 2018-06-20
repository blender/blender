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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain
#                 Ben Batt
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	../blenkernel
	../blenlib
	../blenfont
	../depsgraph
	../makesdna
	../makesrna
	../bmesh
	../render/extern/include
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/eigen
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/MOD_armature.c
	intern/MOD_array.c
	intern/MOD_bevel.c
	intern/MOD_boolean.c
	intern/MOD_build.c
	intern/MOD_cast.c
	intern/MOD_cloth.c
	intern/MOD_collision.c
	intern/MOD_correctivesmooth.c
	intern/MOD_curve.c
	intern/MOD_datatransfer.c
	intern/MOD_decimate.c
	intern/MOD_displace.c
	intern/MOD_dynamicpaint.c
	intern/MOD_edgesplit.c
	intern/MOD_explode.c
	intern/MOD_fluidsim.c
	intern/MOD_fluidsim_util.c
	intern/MOD_hook.c
	intern/MOD_laplaciandeform.c
	intern/MOD_laplaciansmooth.c
	intern/MOD_lattice.c
	intern/MOD_mask.c
	intern/MOD_meshcache.c
	intern/MOD_meshcache_mdd.c
	intern/MOD_meshcache_pc2.c
	intern/MOD_meshcache_util.c
	intern/MOD_meshdeform.c
	intern/MOD_meshsequencecache.c
	intern/MOD_mirror.c
	intern/MOD_multires.c
	intern/MOD_none.c
	intern/MOD_normal_edit.c
	intern/MOD_ocean.c
	intern/MOD_particleinstance.c
	intern/MOD_particlesystem.c
	intern/MOD_remesh.c
	intern/MOD_screw.c
	intern/MOD_shapekey.c
	intern/MOD_shrinkwrap.c
	intern/MOD_simpledeform.c
	intern/MOD_skin.c
	intern/MOD_smoke.c
	intern/MOD_smooth.c
	intern/MOD_softbody.c
	intern/MOD_solidify.c
	intern/MOD_subsurf.c
	intern/MOD_surface.c
	intern/MOD_surfacedeform.c
	intern/MOD_triangulate.c
	intern/MOD_util.c
	intern/MOD_uvwarp.c
	intern/MOD_uvproject.c
	intern/MOD_warp.c
	intern/MOD_wave.c
	intern/MOD_weighted_normal.c
	intern/MOD_weightvg_util.c
	intern/MOD_weightvgedit.c
	intern/MOD_weightvgmix.c
	intern/MOD_weightvgproximity.c
	intern/MOD_wireframe.c

	MOD_modifiertypes.h
	intern/MOD_fluidsim_util.h
	intern/MOD_meshcache_util.h
	intern/MOD_util.h
	intern/MOD_weightvg_util.h
)

if(WITH_ALEMBIC)
	add_definitions(-DWITH_ALEMBIC)
	list(APPEND INC
		../alembic
	)
endif()

if(WITH_MOD_REMESH)
	add_definitions(-DWITH_MOD_REMESH)
	list(APPEND INC
		../../../intern/dualcon
	)
endif()

if(WITH_MOD_FLUID)
	add_definitions(-DWITH_MOD_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
	add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_BULLET)
	add_definitions(-DWITH_BULLET)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
endif()

# So we can have special tricks in modifier system.
add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_modifiers "${SRC}" "${INC}" "${INC_SYS}")
