# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../../../intern/eigen
  ../../../extern/rangetree
)

set(INC_SYS

)

set(SRC
  # Naming convention for BMesh operators is: bmo_*action*_*details*.cc
  operators/bmo_beautify.cc
  operators/bmo_bevel.cc
  operators/bmo_bisect_plane.cc
  operators/bmo_bridge.cc
  operators/bmo_connect.cc
  operators/bmo_connect_concave.cc
  operators/bmo_connect_nonplanar.cc
  operators/bmo_connect_pair.cc
  operators/bmo_create.cc
  operators/bmo_dissolve.cc
  operators/bmo_dupe.cc
  operators/bmo_edgenet.cc
  operators/bmo_extrude.cc
  operators/bmo_fill_attribute.cc
  operators/bmo_fill_edgeloop.cc
  operators/bmo_fill_grid.cc
  operators/bmo_fill_holes.cc
  operators/bmo_hull.cc
  operators/bmo_inset.cc
  operators/bmo_join_triangles.cc
  operators/bmo_mesh_convert.cc
  operators/bmo_mirror.cc
  operators/bmo_normals.cc
  operators/bmo_offset_edgeloops.cc
  operators/bmo_planar_faces.cc
  operators/bmo_poke.cc
  operators/bmo_primitive.cc
  operators/bmo_removedoubles.cc
  operators/bmo_rotate_edges.cc
  operators/bmo_smooth_laplacian.cc
  operators/bmo_split_edges.cc
  operators/bmo_subdivide.cc
  operators/bmo_subdivide_edgering.cc
  operators/bmo_symmetrize.cc
  operators/bmo_triangulate.cc
  operators/bmo_unsubdivide.cc
  operators/bmo_utils.cc
  operators/bmo_wireframe.cc

  intern/bmesh_callback_generic.cc
  intern/bmesh_callback_generic.hh
  intern/bmesh_construct.cc
  intern/bmesh_construct.hh
  intern/bmesh_core.cc
  intern/bmesh_core.hh
  intern/bmesh_delete.cc
  intern/bmesh_delete.hh
  intern/bmesh_edgeloop.cc
  intern/bmesh_edgeloop.hh
  intern/bmesh_inline.hh
  intern/bmesh_interp.cc
  intern/bmesh_interp.hh
  intern/bmesh_iterators.cc
  intern/bmesh_iterators.hh
  intern/bmesh_iterators_inline.hh
  intern/bmesh_log.cc
  intern/bmesh_log.hh
  intern/bmesh_marking.cc
  intern/bmesh_marking.hh
  intern/bmesh_mesh.cc
  intern/bmesh_mesh.hh
  intern/bmesh_mesh_convert.cc
  intern/bmesh_mesh_convert.hh
  intern/bmesh_mesh_debug.cc
  intern/bmesh_mesh_debug.hh
  intern/bmesh_mesh_duplicate.cc
  intern/bmesh_mesh_duplicate.hh
  intern/bmesh_mesh_normals.cc
  intern/bmesh_mesh_normals.hh
  intern/bmesh_mesh_partial_update.cc
  intern/bmesh_mesh_partial_update.hh
  intern/bmesh_mesh_tessellate.cc
  intern/bmesh_mesh_tessellate.hh
  intern/bmesh_mesh_validate.cc
  intern/bmesh_mesh_validate.hh
  intern/bmesh_mods.cc
  intern/bmesh_mods.hh
  intern/bmesh_opdefines.cc
  intern/bmesh_operator_api_inline.hh
  intern/bmesh_operators.cc
  intern/bmesh_operators.hh
  intern/bmesh_operators_private.hh
  intern/bmesh_polygon.cc
  intern/bmesh_polygon.hh
  intern/bmesh_polygon_edgenet.cc
  intern/bmesh_polygon_edgenet.hh
  intern/bmesh_private.hh
  intern/bmesh_query.cc
  intern/bmesh_query.hh
  intern/bmesh_query_inline.hh
  intern/bmesh_query_uv.cc
  intern/bmesh_query_uv.hh
  intern/bmesh_structure.cc
  intern/bmesh_structure.hh
  intern/bmesh_structure_inline.hh
  intern/bmesh_uvselect.cc
  intern/bmesh_uvselect.hh
  intern/bmesh_walkers.cc
  intern/bmesh_walkers.hh
  intern/bmesh_walkers_impl.cc
  intern/bmesh_walkers_private.hh

  intern/bmesh_error.hh
  intern/bmesh_operator_api.hh

  tools/bmesh_beautify.cc
  tools/bmesh_beautify.hh
  tools/bmesh_bevel.cc
  tools/bmesh_bevel.hh
  tools/bmesh_bisect_plane.cc
  tools/bmesh_bisect_plane.hh
  tools/bmesh_boolean.cc
  tools/bmesh_boolean.hh
  tools/bmesh_decimate.hh
  tools/bmesh_decimate_collapse.cc
  tools/bmesh_decimate_dissolve.cc
  tools/bmesh_decimate_unsubdivide.cc
  tools/bmesh_edgenet.cc
  tools/bmesh_edgenet.hh
  tools/bmesh_edgesplit.cc
  tools/bmesh_edgesplit.hh
  tools/bmesh_intersect.cc
  tools/bmesh_intersect.hh
  tools/bmesh_intersect_edges.cc
  tools/bmesh_intersect_edges.hh
  tools/bmesh_path.cc
  tools/bmesh_path.hh
  tools/bmesh_path_region.cc
  tools/bmesh_path_region.hh
  tools/bmesh_path_region_uv.cc
  tools/bmesh_path_region_uv.hh
  tools/bmesh_path_uv.cc
  tools/bmesh_path_uv.hh
  tools/bmesh_region_match.cc
  tools/bmesh_region_match.hh
  tools/bmesh_separate.cc
  tools/bmesh_separate.hh
  tools/bmesh_triangulate.cc
  tools/bmesh_triangulate.hh
  tools/bmesh_wireframe.cc
  tools/bmesh_wireframe.hh

  bmesh_class.hh

  # public includes
  bmesh.hh
  bmesh_tools.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  extern_rangetree
  PRIVATE bf::intern::atomic
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    ../../../intern/rigidbody
  )
  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

blender_add_lib(bf_bmesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::bmesh ALIAS bf_bmesh)

if(MSVC AND NOT MSVC_CLANG)
  target_compile_options(bf_bmesh PRIVATE /WX /wd4101)
endif()

if(WITH_GTESTS)
  set(TEST_SRC
    tests/bmesh_core_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    bf_bmesh
  )
  blender_add_test_suite_lib(bmesh "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
