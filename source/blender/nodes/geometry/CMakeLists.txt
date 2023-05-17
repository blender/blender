# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../intern
  ../../editors/include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../functions
  ../../geometry
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)


set(SRC
  nodes/node_geo_accumulate_field.cc
  nodes/node_geo_attribute_capture.cc
  nodes/node_geo_attribute_domain_size.cc
  nodes/node_geo_attribute_statistic.cc
  nodes/node_geo_blur_attribute.cc
  nodes/node_geo_boolean.cc
  nodes/node_geo_bounding_box.cc
  nodes/node_geo_collection_info.cc
  nodes/node_geo_common.cc
  nodes/node_geo_convex_hull.cc
  nodes/node_geo_curve_endpoint_selection.cc
  nodes/node_geo_curve_fill.cc
  nodes/node_geo_curve_fillet.cc
  nodes/node_geo_curve_handle_type_selection.cc
  nodes/node_geo_curve_length.cc
  nodes/node_geo_curve_primitive_arc.cc
  nodes/node_geo_curve_primitive_bezier_segment.cc
  nodes/node_geo_curve_primitive_circle.cc
  nodes/node_geo_curve_primitive_line.cc
  nodes/node_geo_curve_primitive_quadratic_bezier.cc
  nodes/node_geo_curve_primitive_quadrilateral.cc
  nodes/node_geo_curve_primitive_spiral.cc
  nodes/node_geo_curve_primitive_star.cc
  nodes/node_geo_curve_resample.cc
  nodes/node_geo_curve_reverse.cc
  nodes/node_geo_curve_sample.cc
  nodes/node_geo_curve_set_handle_type.cc
  nodes/node_geo_curve_spline_parameter.cc
  nodes/node_geo_curve_spline_type.cc
  nodes/node_geo_curve_subdivide.cc
  nodes/node_geo_curve_to_mesh.cc
  nodes/node_geo_curve_to_points.cc
  nodes/node_geo_curve_topology_curve_of_point.cc
  nodes/node_geo_curve_topology_points_of_curve.cc
  nodes/node_geo_curve_trim.cc
  nodes/node_geo_deform_curves_on_surface.cc
  nodes/node_geo_delete_geometry.cc
  nodes/node_geo_distribute_points_in_volume.cc
  nodes/node_geo_distribute_points_on_faces.cc
  nodes/node_geo_dual_mesh.cc
  nodes/node_geo_duplicate_elements.cc
  nodes/node_geo_edge_paths_to_curves.cc
  nodes/node_geo_edge_paths_to_selection.cc
  nodes/node_geo_edge_split.cc
  nodes/node_geo_edges_to_face_groups.cc
  nodes/node_geo_evaluate_at_index.cc
  nodes/node_geo_evaluate_on_domain.cc
  nodes/node_geo_extrude_mesh.cc
  nodes/node_geo_flip_faces.cc
  nodes/node_geo_geometry_to_instance.cc
  nodes/node_geo_image.cc
  nodes/node_geo_image_info.cc
  nodes/node_geo_image_texture.cc
  nodes/node_geo_index_of_nearest.cc
  nodes/node_geo_input_curve_handles.cc
  nodes/node_geo_input_curve_tilt.cc
  nodes/node_geo_input_id.cc
  nodes/node_geo_input_index.cc
  nodes/node_geo_input_instance_rotation.cc
  nodes/node_geo_input_instance_scale.cc
  nodes/node_geo_input_material.cc
  nodes/node_geo_input_material_index.cc
  nodes/node_geo_input_mesh_edge_angle.cc
  nodes/node_geo_input_mesh_edge_neighbors.cc
  nodes/node_geo_input_mesh_edge_vertices.cc
  nodes/node_geo_input_mesh_face_area.cc
  nodes/node_geo_input_mesh_face_is_planar.cc
  nodes/node_geo_input_mesh_face_neighbors.cc
  nodes/node_geo_input_mesh_island.cc
  nodes/node_geo_input_mesh_vertex_neighbors.cc
  nodes/node_geo_input_named_attribute.cc
  nodes/node_geo_input_normal.cc
  nodes/node_geo_input_position.cc
  nodes/node_geo_input_radius.cc
  nodes/node_geo_input_scene_time.cc
  nodes/node_geo_input_shade_smooth.cc
  nodes/node_geo_input_shortest_edge_paths.cc
  nodes/node_geo_input_signed_distance.cc
  nodes/node_geo_input_spline_cyclic.cc
  nodes/node_geo_input_spline_length.cc
  nodes/node_geo_input_spline_resolution.cc
  nodes/node_geo_input_tangent.cc
  nodes/node_geo_instance_on_points.cc
  nodes/node_geo_instances_to_points.cc
  nodes/node_geo_interpolate_curves.cc
  nodes/node_geo_is_viewport.cc
  nodes/node_geo_join_geometry.cc
  nodes/node_geo_material_replace.cc
  nodes/node_geo_material_selection.cc
  nodes/node_geo_mean_filter_sdf_volume.cc
  nodes/node_geo_merge_by_distance.cc
  nodes/node_geo_mesh_face_group_boundaries.cc
  nodes/node_geo_mesh_primitive_circle.cc
  nodes/node_geo_mesh_primitive_cone.cc
  nodes/node_geo_mesh_primitive_cube.cc
  nodes/node_geo_mesh_primitive_cylinder.cc
  nodes/node_geo_mesh_primitive_grid.cc
  nodes/node_geo_mesh_primitive_ico_sphere.cc
  nodes/node_geo_mesh_primitive_line.cc
  nodes/node_geo_mesh_primitive_uv_sphere.cc
  nodes/node_geo_mesh_subdivide.cc
  nodes/node_geo_mesh_to_curve.cc
  nodes/node_geo_mesh_to_points.cc
  nodes/node_geo_mesh_to_sdf_volume.cc
  nodes/node_geo_mesh_to_volume.cc
  nodes/node_geo_mesh_topology_corners_of_face.cc
  nodes/node_geo_mesh_topology_corners_of_vertex.cc
  nodes/node_geo_mesh_topology_edges_of_corner.cc
  nodes/node_geo_mesh_topology_edges_of_vertex.cc
  nodes/node_geo_mesh_topology_face_of_corner.cc
  nodes/node_geo_mesh_topology_offset_corner_in_face.cc
  nodes/node_geo_mesh_topology_vertex_of_corner.cc
  nodes/node_geo_object_info.cc
  nodes/node_geo_offset_point_in_curve.cc
  nodes/node_geo_offset_sdf_volume.cc
  nodes/node_geo_points.cc
  nodes/node_geo_points_to_sdf_volume.cc
  nodes/node_geo_points_to_vertices.cc
  nodes/node_geo_points_to_volume.cc
  nodes/node_geo_proximity.cc
  nodes/node_geo_raycast.cc
  nodes/node_geo_realize_instances.cc
  nodes/node_geo_remove_attribute.cc
  nodes/node_geo_rotate_instances.cc
  nodes/node_geo_sample_index.cc
  nodes/node_geo_sample_nearest.cc
  nodes/node_geo_sample_nearest_surface.cc
  nodes/node_geo_sample_uv_surface.cc
  nodes/node_geo_sample_volume.cc
  nodes/node_geo_scale_elements.cc
  nodes/node_geo_scale_instances.cc
  nodes/node_geo_sdf_volume_sphere.cc
  nodes/node_geo_self_object.cc
  nodes/node_geo_separate_components.cc
  nodes/node_geo_separate_geometry.cc
  nodes/node_geo_set_curve_handles.cc
  nodes/node_geo_set_curve_normal.cc
  nodes/node_geo_set_curve_radius.cc
  nodes/node_geo_set_curve_tilt.cc
  nodes/node_geo_set_id.cc
  nodes/node_geo_set_material.cc
  nodes/node_geo_set_material_index.cc
  nodes/node_geo_set_point_radius.cc
  nodes/node_geo_set_position.cc
  nodes/node_geo_set_shade_smooth.cc
  nodes/node_geo_set_spline_cyclic.cc
  nodes/node_geo_set_spline_resolution.cc
  nodes/node_geo_simulation_input.cc
  nodes/node_geo_simulation_output.cc
  nodes/node_geo_store_named_attribute.cc
  nodes/node_geo_string_join.cc
  nodes/node_geo_string_to_curves.cc
  nodes/node_geo_subdivision_surface.cc
  nodes/node_geo_switch.cc
  nodes/node_geo_transform_geometry.cc
  nodes/node_geo_translate_instances.cc
  nodes/node_geo_triangulate.cc
  nodes/node_geo_uv_pack_islands.cc
  nodes/node_geo_uv_unwrap.cc
  nodes/node_geo_viewer.cc
  nodes/node_geo_volume_cube.cc
  nodes/node_geo_volume_to_mesh.cc

  node_geometry_register.cc
  node_geometry_tree.cc
  node_geometry_util.cc

  node_geometry_register.hh
  node_geometry_util.hh
)

set(LIB
  bf_bmesh
  bf_functions
  bf_geometry
  bf_nodes
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    ../../../../intern/rigidbody
  )
  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_TBB)
  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the stl versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
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

if(WITH_OPENVDB)
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
endif()

blender_add_lib(bf_nodes_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_UNITY_BUILD)
  set_target_properties(bf_nodes_geometry PROPERTIES UNITY_BUILD ON)
  set_target_properties(bf_nodes_geometry PROPERTIES UNITY_BUILD_BATCH_SIZE 10)
endif()

# RNA_prototypes.h
add_dependencies(bf_nodes_geometry bf_rna)
