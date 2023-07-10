# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../depsgraph
  ../../../intern/clog
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
  intern/bmesh_interp.cc
  intern/bmesh_interp.h
  intern/bmesh_iterators.cc
  intern/bmesh_iterators.h
  intern/bmesh_iterators_inline.h
  intern/bmesh_log.c
  intern/bmesh_log.h
  intern/bmesh_marking.c
  intern/bmesh_marking.h
  intern/bmesh_mesh.cc
  intern/bmesh_mesh.h
  intern/bmesh_mesh_convert.cc
  intern/bmesh_mesh_convert.h
  intern/bmesh_mesh_debug.c
  intern/bmesh_mesh_debug.h
  intern/bmesh_mesh_duplicate.c
  intern/bmesh_mesh_duplicate.h
  intern/bmesh_mesh_normals.cc
  intern/bmesh_mesh_normals.h
  intern/bmesh_mesh_partial_update.c
  intern/bmesh_mesh_partial_update.h
  intern/bmesh_mesh_tessellate.c
  intern/bmesh_mesh_tessellate.h
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
  intern/bmesh_query_uv.cc
  intern/bmesh_query_uv.h
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
  tools/bmesh_boolean.cc
  tools/bmesh_boolean.h
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
  tools/bmesh_path_region_uv.c
  tools/bmesh_path_region_uv.h
  tools/bmesh_path_uv.c
  tools/bmesh_path_uv.h
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
  PRIVATE bf::dna
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
