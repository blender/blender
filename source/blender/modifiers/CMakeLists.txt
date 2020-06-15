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

set(INC
  .
  intern
  ../blenfont
  ../blenkernel
  ../blentranslation
  ../blenlib
  ../blenloader
  ../bmesh
  ../depsgraph
  ../editors/include
  ../makesdna
  ../makesrna
  ../render/extern/include
  ../windowmanager
  ../../../intern/eigen
  ../../../intern/guardedalloc

  # dna_type_offsets.h in BLO_read_write.h
  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern
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
  intern/MOD_fluid.c
  intern/MOD_hook.c
  intern/MOD_laplaciandeform.c
  intern/MOD_laplaciansmooth.c
  intern/MOD_lattice.c
  intern/MOD_mask.cc
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
  intern/MOD_simulation.cc
  intern/MOD_skin.c
  intern/MOD_smooth.c
  intern/MOD_softbody.c
  intern/MOD_solidify.c
  intern/MOD_solidify_extrude.c
  intern/MOD_solidify_nonmanifold.c
  intern/MOD_subsurf.c
  intern/MOD_surface.c
  intern/MOD_surfacedeform.c
  intern/MOD_triangulate.c
  intern/MOD_ui_common.c
  intern/MOD_util.c
  intern/MOD_uvproject.c
  intern/MOD_uvwarp.c
  intern/MOD_warp.c
  intern/MOD_wave.c
  intern/MOD_weighted_normal.c
  intern/MOD_weightvg_util.c
  intern/MOD_weightvgedit.c
  intern/MOD_weightvgmix.c
  intern/MOD_weightvgproximity.c
  intern/MOD_weld.c
  intern/MOD_wireframe.c

  MOD_modifiertypes.h
  intern/MOD_meshcache_util.h
  intern/MOD_solidify_util.h
  intern/MOD_ui_common.h
  intern/MOD_util.h
  intern/MOD_weightvg_util.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
  list(APPEND INC
    ../io/alembic
  )
  list(APPEND LIB
    bf_alembic
  )
endif()

if(WITH_MOD_REMESH)
  list(APPEND INC
    ../../../intern/dualcon
  )
  list(APPEND LIB
    bf_intern_dualcon
  )
  add_definitions(-DWITH_MOD_REMESH)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
  add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_BULLET)
  list(APPEND LIB
    extern_bullet
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

# To disable adaptive subdivision test in subsurf UI without cycles
if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
endif()

# So we can have special tricks in modifier system.
add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_modifiers "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
