# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  include
  ../intern
  ../../editors/include
  ../../compositor
  ../../makesrna
  ../../compositor/algorithms
  ../../compositor/cached_resources
  ../../compositor/derived_resources
  ../../compositor/utilities
  ../../modifiers
  ../../io/common
  ../../io/csv
  ../../io/ply
  ../../io/stl
  ../../io/wavefront_obj
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  nodes/node_geo_accumulate_field.cc
  nodes/node_geo_attribute_capture.cc
  nodes/node_geo_attribute_domain_size.cc
  nodes/node_geo_attribute_statistic.cc
  nodes/node_geo_bake.cc
  nodes/node_geo_blur_attribute.cc
  nodes/node_geo_boolean.cc
  nodes/node_geo_bounding_box.cc
  nodes/node_geo_camera_info.cc
  nodes/node_geo_closure.cc
  nodes/node_geo_collection_info.cc
  nodes/node_geo_combine_bundle.cc
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
  nodes/node_geo_curves_to_grease_pencil.cc
  nodes/node_geo_deform_curves_on_surface.cc
  nodes/node_geo_delete_geometry.cc
  nodes/node_geo_distribute_points_in_grid.cc
  nodes/node_geo_distribute_points_in_volume.cc
  nodes/node_geo_distribute_points_on_faces.cc
  nodes/node_geo_dual_mesh.cc
  nodes/node_geo_duplicate_elements.cc
  nodes/node_geo_edge_paths_to_curves.cc
  nodes/node_geo_edge_paths_to_selection.cc
  nodes/node_geo_edge_split.cc
  nodes/node_geo_edges_to_face_groups.cc
  nodes/node_geo_enable_output.cc
  nodes/node_geo_evaluate_at_index.cc
  nodes/node_geo_evaluate_closure.cc
  nodes/node_geo_evaluate_on_domain.cc
  nodes/node_geo_extrude_mesh.cc
  nodes/node_geo_field_average.cc
  nodes/node_geo_field_min_and_max.cc
  nodes/node_geo_field_to_grid.cc
  nodes/node_geo_field_variance.cc
  nodes/node_geo_flip_faces.cc
  nodes/node_geo_foreach_geometry_element.cc
  nodes/node_geo_geometry_to_instance.cc
  nodes/node_geo_get_named_grid.cc
  nodes/node_geo_gizmo_dial.cc
  nodes/node_geo_gizmo_linear.cc
  nodes/node_geo_gizmo_transform.cc
  nodes/node_geo_grease_pencil_to_curves.cc
  nodes/node_geo_grid_advect.cc
  nodes/node_geo_grid_curl.cc
  nodes/node_geo_grid_divergence.cc
  nodes/node_geo_grid_gradient.cc
  nodes/node_geo_grid_info.cc
  nodes/node_geo_grid_laplacian.cc
  nodes/node_geo_grid_prune.cc
  nodes/node_geo_grid_to_mesh.cc
  nodes/node_geo_grid_voxelize.cc
  nodes/node_geo_image.cc
  nodes/node_geo_image_info.cc
  nodes/node_geo_image_texture.cc
  nodes/node_geo_import_csv.cc
  nodes/node_geo_import_obj.cc
  nodes/node_geo_import_ply.cc
  nodes/node_geo_import_stl.cc
  nodes/node_geo_import_text.cc
  nodes/node_geo_import_vdb.cc
  nodes/node_geo_index_of_nearest.cc
  nodes/node_geo_index_switch.cc
  nodes/node_geo_input_active_camera.cc
  nodes/node_geo_input_collection.cc
  nodes/node_geo_input_curve_handles.cc
  nodes/node_geo_input_curve_tilt.cc
  nodes/node_geo_input_edge_smooth.cc
  nodes/node_geo_input_face_smooth.cc
  nodes/node_geo_input_id.cc
  nodes/node_geo_input_index.cc
  nodes/node_geo_input_instance_bounds.cc
  nodes/node_geo_input_instance_rotation.cc
  nodes/node_geo_input_instance_scale.cc
  nodes/node_geo_input_instance_transform.cc
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
  nodes/node_geo_input_named_layer_selection.cc
  nodes/node_geo_input_normal.cc
  nodes/node_geo_input_object.cc
  nodes/node_geo_input_position.cc
  nodes/node_geo_input_radius.cc
  nodes/node_geo_input_scene_time.cc
  nodes/node_geo_input_shortest_edge_paths.cc
  nodes/node_geo_input_spline_cyclic.cc
  nodes/node_geo_input_spline_length.cc
  nodes/node_geo_input_spline_resolution.cc
  nodes/node_geo_input_tangent.cc
  nodes/node_geo_input_voxel_index.cc
  nodes/node_geo_instance_on_points.cc
  nodes/node_geo_instances_to_points.cc
  nodes/node_geo_interpolate_curves.cc
  nodes/node_geo_is_viewport.cc
  nodes/node_geo_join_bundle.cc
  nodes/node_geo_join_geometry.cc
  nodes/node_geo_list.cc
  nodes/node_geo_list_get_item.cc
  nodes/node_geo_list_length.cc
  nodes/node_geo_material_replace.cc
  nodes/node_geo_material_selection.cc
  nodes/node_geo_menu_switch.cc
  nodes/node_geo_merge_by_distance.cc
  nodes/node_geo_merge_layers.cc
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
  nodes/node_geo_mesh_to_density_grid.cc
  nodes/node_geo_mesh_to_points.cc
  nodes/node_geo_mesh_to_sdf_grid.cc
  nodes/node_geo_mesh_to_volume.cc
  nodes/node_geo_mesh_topology_corners_of_edge.cc
  nodes/node_geo_mesh_topology_corners_of_face.cc
  nodes/node_geo_mesh_topology_corners_of_vertex.cc
  nodes/node_geo_mesh_topology_edges_of_corner.cc
  nodes/node_geo_mesh_topology_edges_of_vertex.cc
  nodes/node_geo_mesh_topology_face_of_corner.cc
  nodes/node_geo_mesh_topology_offset_corner_in_face.cc
  nodes/node_geo_mesh_topology_vertex_of_corner.cc
  nodes/node_geo_mouse_position.cc
  nodes/node_geo_object_info.cc
  nodes/node_geo_offset_point_in_curve.cc
  nodes/node_geo_points.cc
  nodes/node_geo_points_to_curves.cc
  nodes/node_geo_points_to_sdf_grid.cc
  nodes/node_geo_points_to_vertices.cc
  nodes/node_geo_points_to_volume.cc
  nodes/node_geo_proximity.cc
  nodes/node_geo_raycast.cc
  nodes/node_geo_realize_instances.cc
  nodes/node_geo_remove_attribute.cc
  nodes/node_geo_repeat.cc
  nodes/node_geo_rotate_instances.cc
  nodes/node_geo_sample_grid.cc
  nodes/node_geo_sample_grid_index.cc
  nodes/node_geo_sample_index.cc
  nodes/node_geo_sample_nearest.cc
  nodes/node_geo_sample_nearest_surface.cc
  nodes/node_geo_sample_uv_surface.cc
  nodes/node_geo_scale_elements.cc
  nodes/node_geo_scale_instances.cc
  nodes/node_geo_sdf_grid_boolean.cc
  nodes/node_geo_sdf_grid_fillet.cc
  nodes/node_geo_sdf_grid_laplacian.cc
  nodes/node_geo_sdf_grid_mean.cc
  nodes/node_geo_sdf_grid_mean_curvature.cc
  nodes/node_geo_sdf_grid_median.cc
  nodes/node_geo_sdf_grid_offset.cc
  nodes/node_geo_self_object.cc
  nodes/node_geo_separate_bundle.cc
  nodes/node_geo_separate_components.cc
  nodes/node_geo_separate_geometry.cc
  nodes/node_geo_set_curve_handles.cc
  nodes/node_geo_set_curve_normal.cc
  nodes/node_geo_set_curve_radius.cc
  nodes/node_geo_set_curve_tilt.cc
  nodes/node_geo_set_geometry_name.cc
  nodes/node_geo_set_grease_pencil_color.cc
  nodes/node_geo_set_grease_pencil_depth_mode.cc
  nodes/node_geo_set_grease_pencil_softness.cc
  nodes/node_geo_set_grid_background.cc
  nodes/node_geo_set_grid_transform.cc
  nodes/node_geo_set_id.cc
  nodes/node_geo_set_instance_transform.cc
  nodes/node_geo_set_material.cc
  nodes/node_geo_set_material_index.cc
  nodes/node_geo_set_mesh_normal.cc
  nodes/node_geo_set_point_radius.cc
  nodes/node_geo_set_position.cc
  nodes/node_geo_set_shade_smooth.cc
  nodes/node_geo_set_spline_cyclic.cc
  nodes/node_geo_set_spline_resolution.cc
  nodes/node_geo_simulation.cc
  nodes/node_geo_sort_elements.cc
  nodes/node_geo_split_to_instances.cc
  nodes/node_geo_store_named_attribute.cc
  nodes/node_geo_store_named_grid.cc
  nodes/node_geo_string_join.cc
  nodes/node_geo_string_to_curves.cc
  nodes/node_geo_subdivision_surface.cc
  nodes/node_geo_switch.cc
  nodes/node_geo_tool_3d_cursor.cc
  nodes/node_geo_tool_active_element.cc
  nodes/node_geo_tool_face_set.cc
  nodes/node_geo_tool_selection.cc
  nodes/node_geo_tool_set_face_set.cc
  nodes/node_geo_tool_set_selection.cc
  nodes/node_geo_transform_geometry.cc
  nodes/node_geo_translate_instances.cc
  nodes/node_geo_triangulate.cc
  nodes/node_geo_uv_pack_islands.cc
  nodes/node_geo_uv_tangent.cc
  nodes/node_geo_uv_unwrap.cc
  nodes/node_geo_viewer.cc
  nodes/node_geo_viewport_transform.cc
  nodes/node_geo_volume_cube.cc
  nodes/node_geo_volume_to_mesh.cc
  nodes/node_geo_warning.cc

  include/NOD_geo_bake.hh
  include/NOD_geo_bundle.hh
  include/NOD_geo_capture_attribute.hh
  include/NOD_geo_closure.hh
  include/NOD_geo_field_to_grid.hh
  include/NOD_geo_foreach_geometry_element.hh
  include/NOD_geo_index_switch.hh
  include/NOD_geo_menu_switch.hh
  include/NOD_geo_repeat.hh
  include/NOD_geo_simulation.hh
  include/NOD_geo_viewer.hh

  node_geometry_tree.cc
  node_geometry_util.cc

  node_geometry_util.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::guardedalloc
  bf_nodes
  bf_compositor
  bf_io_common
  bf_io_csv
  PRIVATE bf::intern::atomic
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::extern::xxhash
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_IO_STL)
  list(APPEND LIB
    PRIVATE bf_io_common
    PRIVATE bf_io_stl
  )
  add_definitions(-DWITH_IO_STL)
endif()

if(WITH_IO_PLY)
  list(APPEND LIB
    PRIVATE bf_io_common
    PRIVATE bf_io_ply
  )
  add_definitions(-DWITH_IO_PLY)
endif()

if(WITH_IO_WAVEFRONT_OBJ)
  list(APPEND LIB
    PRIVATE bf_io_common
    PRIVATE bf_io_wavefront_obj
  )
  add_definitions(-DWITH_IO_WAVEFRONT_OBJ)
endif()

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
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_IMAGE_OPENEXR)
  add_definitions(-DWITH_IMAGE_OPENEXR)
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

add_node_discovery(
  bf_nodes_geometry_generated
  "${SRC}"
  ${CMAKE_CURRENT_BINARY_DIR}/register_geometry_nodes.cc
  register_geometry_nodes
)

list(APPEND LIB
  bf_nodes_geometry_generated
)

# If compiling with MSVC clang we need to add the `D_LIBCPP_VERSION` define
# so we don't run into TBB errors when compiling with lib.
if(WITH_TBB AND MSVC_CLANG)
  string(APPEND CMAKE_CXX_FLAGS " /D_LIBCPP_VERSION")
endif()


blender_add_lib(bf_nodes_geometry "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
target_link_libraries(bf_nodes_geometry_generated bf_nodes_geometry)

blender_set_target_unity_build(bf_nodes_geometry 10)

# RNA_prototypes.hh
add_dependencies(bf_nodes_geometry bf_rna)
