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
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../makesdna
  ../../../intern/atomic
  ../../../intern/eigen
  ../../../intern/guardedalloc
  ../../../extern/rangetree
)

set(INC_SYS

)

set(SRC
  # Naming convention for BMesh operators is: bmo_*action*_*details*.c
  operators/bmo_beautify.c
  operators/bmo_bevel.c
  operators/bmo_bisect_plane.c
  operators/bmo_bridge.c
  operators/bmo_connect.c
  operators/bmo_connect_concave.c
  operators/bmo_connect_nonplanar.c
  operators/bmo_connect_pair.c
  operators/bmo_create.c
  operators/bmo_dissolve.c
  operators/bmo_dupe.c
  operators/bmo_edgenet.c
  operators/bmo_extrude.c
  operators/bmo_fill_attribute.c
  operators/bmo_fill_edgeloop.c
  operators/bmo_fill_grid.c
  operators/bmo_fill_holes.c
  operators/bmo_hull.c
  operators/bmo_inset.c
  operators/bmo_join_triangles.c
  operators/bmo_mesh_convert.c
  operators/bmo_mirror.c
  operators/bmo_normals.c
  operators/bmo_offset_edgeloops.c
  operators/bmo_planar_faces.c
  operators/bmo_poke.c
  operators/bmo_primitive.c
  operators/bmo_removedoubles.c
  operators/bmo_rotate_edges.c
  operators/bmo_smooth_laplacian.c
  operators/bmo_split_edges.c
  operators/bmo_subdivide.c
  operators/bmo_subdivide_edgering.c
  operators/bmo_symmetrize.c
  operators/bmo_triangulate.c
  operators/bmo_unsubdivide.c
  operators/bmo_utils.c
  operators/bmo_wireframe.c

  intern/bmesh_callback_generic.c
  intern/bmesh_callback_generic.h
  intern/bmesh_construct.c
  intern/bmesh_construct.h
  intern/bmesh_core.c
  intern/bmesh_core.h
  intern/bmesh_delete.c
  intern/bmesh_delete.h
  intern/bmesh_edgeloop.c
  intern/bmesh_edgeloop.h
  intern/bmesh_inline.h
  intern/bmesh_interp.c
  intern/bmesh_interp.h
  intern/bmesh_iterators.c
  intern/bmesh_iterators.h
  intern/bmesh_iterators_inline.h
  intern/bmesh_log.c
  intern/bmesh_log.h
  intern/bmesh_marking.c
  intern/bmesh_marking.h
  intern/bmesh_mesh.c
  intern/bmesh_mesh.h
  intern/bmesh_mesh_convert.c
  intern/bmesh_mesh_convert.h
  intern/bmesh_mesh_duplicate.c
  intern/bmesh_mesh_duplicate.h
  intern/bmesh_mesh_validate.c
  intern/bmesh_mesh_validate.h
  intern/bmesh_mods.c
  intern/bmesh_mods.h
  intern/bmesh_opdefines.c
  intern/bmesh_operator_api_inline.h
  intern/bmesh_operators.c
  intern/bmesh_operators.h
  intern/bmesh_operators_private.h
  intern/bmesh_polygon.c
  intern/bmesh_polygon.h
  intern/bmesh_polygon_edgenet.c
  intern/bmesh_polygon_edgenet.h
  intern/bmesh_private.h
  intern/bmesh_query.c
  intern/bmesh_query.h
  intern/bmesh_query_inline.h
  intern/bmesh_structure.c
  intern/bmesh_structure.h
  intern/bmesh_structure_inline.h
  intern/bmesh_walkers.c
  intern/bmesh_walkers.h
  intern/bmesh_walkers_impl.c
  intern/bmesh_walkers_private.h

  intern/bmesh_error.h
  intern/bmesh_operator_api.h

  tools/bmesh_beautify.c
  tools/bmesh_beautify.h
  tools/bmesh_bevel.c
  tools/bmesh_bevel.h
  tools/bmesh_bisect_plane.c
  tools/bmesh_bisect_plane.h
  tools/bmesh_decimate.h
  tools/bmesh_decimate_collapse.c
  tools/bmesh_decimate_dissolve.c
  tools/bmesh_decimate_unsubdivide.c
  tools/bmesh_edgenet.c
  tools/bmesh_edgenet.h
  tools/bmesh_edgesplit.c
  tools/bmesh_edgesplit.h
  tools/bmesh_intersect.c
  tools/bmesh_intersect.h
  tools/bmesh_intersect_edges.c
  tools/bmesh_intersect_edges.h
  tools/bmesh_path.c
  tools/bmesh_path.h
  tools/bmesh_path_region.c
  tools/bmesh_path_region.h
  tools/bmesh_region_match.c
  tools/bmesh_region_match.h
  tools/bmesh_separate.c
  tools/bmesh_separate.h
  tools/bmesh_triangulate.c
  tools/bmesh_triangulate.h
  tools/bmesh_wireframe.c
  tools/bmesh_wireframe.h

  bmesh_class.h

  # public includes
  bmesh.h
  bmesh_tools.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  extern_rangetree
)

if(MSVC AND NOT MSVC_CLANG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /WX /wd4101")
endif()

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
  )
  list(APPEND LIB
    extern_bullet

    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

blender_add_lib(bf_bmesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
