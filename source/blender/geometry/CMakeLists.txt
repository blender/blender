# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../makesrna
  ../../../intern/eigen
)

set(INC_SYS
)

set(SRC
  intern/add_curves_on_mesh.cc
  intern/curve_constraints.cc
  intern/curves_remove_and_split.cc
  intern/extend_curves.cc
  intern/extract_elements.cc
  intern/fillet_curves.cc
  intern/fit_curves.cc
  intern/foreach_geometry.cc
  intern/interpolate_curves.cc
  intern/join_geometries.cc
  intern/merge_curves.cc
  intern/merge_layers.cc
  intern/mesh_boolean.cc
  intern/mesh_boolean_manifold.cc
  intern/mesh_copy_selection.cc
  intern/mesh_merge_by_distance.cc
  intern/mesh_primitive_cuboid.cc
  intern/mesh_primitive_cylinder_cone.cc
  intern/mesh_primitive_grid.cc
  intern/mesh_primitive_line.cc
  intern/mesh_primitive_uv_sphere.cc
  intern/mesh_selection.cc
  intern/mesh_split_edges.cc
  intern/mesh_to_curve_convert.cc
  intern/mesh_to_volume.cc
  intern/mesh_triangulate.cc
  intern/mix_geometries.cc
  intern/point_merge_by_distance.cc
  intern/points_to_volume.cc
  intern/randomize.cc
  intern/realize_instances.cc
  intern/reorder.cc
  intern/resample_curves.cc
  intern/reverse_uv_sampler.cc
  intern/separate_geometry.cc
  intern/set_curve_type.cc
  intern/simplify_curves.cc
  intern/smooth_curves.cc
  intern/subdivide_curves.cc
  intern/transform.cc
  intern/trim_curves.cc
  intern/uv_pack.cc
  intern/uv_parametrizer.cc
  intern/volume_grid_resample.cc

  GEO_add_curves_on_mesh.hh
  GEO_curve_constraints.hh
  GEO_curves_remove_and_split.hh
  GEO_extend_curves.hh
  GEO_extract_elements.hh
  GEO_fillet_curves.hh
  GEO_fit_curves.hh
  GEO_foreach_geometry.hh
  GEO_interpolate_curves.hh
  GEO_join_geometries.hh
  GEO_merge_curves.hh
  GEO_merge_layers.hh
  GEO_mesh_boolean.hh
  GEO_mesh_copy_selection.hh
  GEO_mesh_merge_by_distance.hh
  GEO_mesh_primitive_cuboid.hh
  GEO_mesh_primitive_cylinder_cone.hh
  GEO_mesh_primitive_grid.hh
  GEO_mesh_primitive_line.hh
  GEO_mesh_primitive_uv_sphere.hh
  GEO_mesh_selection.hh
  GEO_mesh_split_edges.hh
  GEO_mesh_to_curve.hh
  GEO_mesh_to_volume.hh
  GEO_mesh_triangulate.hh
  GEO_mix_geometries.hh
  GEO_point_merge_by_distance.hh
  GEO_points_to_volume.hh
  GEO_randomize.hh
  GEO_realize_instances.hh
  GEO_reorder.hh
  GEO_resample_curves.hh
  GEO_reverse_uv_sampler.hh
  GEO_separate_geometry.hh
  GEO_set_curve_type.hh
  GEO_simplify_curves.hh
  GEO_smooth_curves.hh
  GEO_subdivide_curves.hh
  GEO_transform.hh
  GEO_trim_curves.hh
  GEO_uv_pack.hh
  GEO_uv_parametrizer.hh
  GEO_volume_grid_resample.hh
  intern/mesh_boolean_manifold.hh
  intern/mesh_boolean_intern.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::curve_fit_nd
  PRIVATE bf::dependencies::optional::manifold
)

if(WITH_UV_SLIM)
  list(APPEND INC
    ../../../intern/slim
  )
  add_definitions(-DWITH_UV_SLIM)
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

if(WITH_MANIFOLD)
  add_definitions(-DWITH_MANIFOLD)
endif()

blender_add_lib(bf_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::geometry ALIAS bf_geometry)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    tests/GEO_interpolate_curves_test.cc
    tests/GEO_merge_curves_test.cc
    tests/GEO_realize_instances_test.cc
  )
  set(TEST_LIB
    PRIVATE bf::intern::clog
  )
  blender_add_test_suite_lib(bf_geometry_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
