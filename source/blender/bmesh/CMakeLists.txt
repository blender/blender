# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenkernel
  ../blentranslation
  ../depsgraph
  ../../../intern/clog
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
  intern/bmesh_callback_generic.h
  intern/bmesh_collapse.cc
  intern/bmesh_collapse.hh
  intern/bmesh_construct.cc
  intern/bmesh_construct.h
  intern/bmesh_core.cc
  intern/bmesh_core.h
  intern/bmesh_delete.cc
  intern/bmesh_delete.h
  intern/bmesh_edgeloop.cc
  intern/bmesh_edgeloop.h
  intern/bmesh_idmap.cc
  intern/bmesh_idmap.h
  intern/bmesh_inline.h
  intern/bmesh_interp.cc
  intern/bmesh_interp.h
  intern/bmesh_iterators.cc
  intern/bmesh_iterators.h
  intern/bmesh_iterators_inline.h
  intern/bmesh_log.cc
  intern/bmesh_log_intern.h
  intern/bmesh_log.h
  intern/bmesh_marking.cc
  intern/bmesh_marking.h
  intern/bmesh_mesh.cc
  intern/bmesh_mesh.h
  intern/bmesh_mesh_convert.cc
  intern/bmesh_mesh_convert.h
  intern/bmesh_mesh_debug.cc
  intern/bmesh_mesh_debug.h
  intern/bmesh_mesh_duplicate.cc
  intern/bmesh_mesh_duplicate.h
  intern/bmesh_mesh_normals.cc
  intern/bmesh_mesh_normals.h
  intern/bmesh_mesh_partial_update.cc
  intern/bmesh_mesh_partial_update.h
  intern/bmesh_mesh_tessellate.cc
  intern/bmesh_mesh_tessellate.h
  intern/bmesh_mesh_validate.cc
  intern/bmesh_mesh_validate.h
  intern/bmesh_mods.cc
  intern/bmesh_mods.h
  intern/bmesh_opdefines.cc
  intern/bmesh_operator_api_inline.h
  intern/bmesh_operators.cc
  intern/bmesh_operators.h
  intern/bmesh_operators_private.h
  intern/bmesh_polygon.cc
  intern/bmesh_polygon.h
  intern/bmesh_polygon_edgenet.cc
  intern/bmesh_polygon_edgenet.h
  intern/bmesh_private.h
  intern/bmesh_query.cc
  intern/bmesh_query.h
  intern/bmesh_query_inline.h
  intern/bmesh_query_uv.cc
  intern/bmesh_query_uv.h
  intern/bmesh_structure.cc
  intern/bmesh_structure.h
  intern/bmesh_structure_inline.h
  intern/bmesh_walkers.cc
  intern/bmesh_walkers.h
  intern/bmesh_walkers_impl.cc
  intern/bmesh_walkers_private.h

  intern/bmesh_error.h
  intern/bmesh_operator_api.h

  tools/bmesh_beautify.cc
  tools/bmesh_beautify.h
  tools/bmesh_bevel.cc
  tools/bmesh_bevel.h
  tools/bmesh_bisect_plane.cc
  tools/bmesh_bisect_plane.h
  tools/bmesh_boolean.cc
  tools/bmesh_boolean.h
  tools/bmesh_decimate.h
  tools/bmesh_decimate_collapse.cc
  tools/bmesh_decimate_dissolve.cc
  tools/bmesh_decimate_unsubdivide.cc
  tools/bmesh_edgenet.cc
  tools/bmesh_edgenet.h
  tools/bmesh_edgesplit.cc
  tools/bmesh_edgesplit.h
  tools/bmesh_intersect.cc
  tools/bmesh_intersect.h
  tools/bmesh_intersect_edges.cc
  tools/bmesh_intersect_edges.h
  tools/bmesh_path.cc
  tools/bmesh_path.h
  tools/bmesh_path_region.cc
  tools/bmesh_path_region.h
  tools/bmesh_path_region_uv.cc
  tools/bmesh_path_region_uv.h
  tools/bmesh_path_uv.cc
  tools/bmesh_path_uv.h
  tools/bmesh_region_match.cc
  tools/bmesh_region_match.h
  tools/bmesh_separate.cc
  tools/bmesh_separate.h
  tools/bmesh_triangulate.cc
  tools/bmesh_triangulate.h
  tools/bmesh_wireframe.cc
  tools/bmesh_wireframe.h

  bmesh_class.h
  bmesh_idmap.h
  bmesh_log.h

  # public includes
  bmesh.h
  bmesh_tools.h
  bmesh_varray.hh
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
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
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

blender_add_lib(bf_bmesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

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
  include(GTestTesting)
  blender_add_test_lib(bf_bmesh_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
