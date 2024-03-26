# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenkernel
  ../blentranslation
  ../bmesh
  ../functions
  ../makesrna
  ../../../intern/eigen
)

set(INC_SYS
)

set(SRC
  intern/add_curves_on_mesh.cc
  intern/curve_constraints.cc
  intern/extend_curves.cc
  intern/fillet_curves.cc
  intern/join_geometries.cc
  intern/mesh_boolean.cc
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

  GEO_add_curves_on_mesh.hh
  GEO_curve_constraints.hh
  GEO_extend_curves.hh
  GEO_fillet_curves.hh
  GEO_join_geometries.hh
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
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::extern::fmtlib
)

if(WITH_OPENVDB)
  list(APPEND INC
    ../../../intern/openvdb
  )
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    bf_intern_openvdb
    ${OPENVDB_LIBRARIES}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

if(WITH_TBB)
  add_definitions(-DWITH_TBB)

  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
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

blender_add_lib(bf_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
