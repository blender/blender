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
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../
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
)


set(SRC
  nodes/legacy/node_geo_legacy_align_rotation_to_vector.cc
  nodes/legacy/node_geo_legacy_attribute_clamp.cc
  nodes/legacy/node_geo_legacy_attribute_color_ramp.cc
  nodes/legacy/node_geo_legacy_attribute_combine_xyz.cc
  nodes/legacy/node_geo_legacy_attribute_compare.cc
  nodes/legacy/node_geo_legacy_attribute_convert.cc
  nodes/legacy/node_geo_legacy_attribute_curve_map.cc
  nodes/legacy/node_geo_legacy_attribute_fill.cc
  nodes/legacy/node_geo_legacy_attribute_map_range.cc
  nodes/legacy/node_geo_legacy_attribute_math.cc
  nodes/legacy/node_geo_legacy_attribute_mix.cc
  nodes/legacy/node_geo_legacy_attribute_proximity.cc
  nodes/legacy/node_geo_legacy_attribute_randomize.cc
  nodes/legacy/node_geo_legacy_attribute_sample_texture.cc
  nodes/legacy/node_geo_legacy_attribute_separate_xyz.cc
  nodes/legacy/node_geo_legacy_attribute_transfer.cc
  nodes/legacy/node_geo_legacy_attribute_vector_math.cc
  nodes/legacy/node_geo_legacy_attribute_vector_rotate.cc
  nodes/legacy/node_geo_legacy_curve_endpoints.cc
  nodes/legacy/node_geo_legacy_curve_reverse.cc
  nodes/legacy/node_geo_legacy_curve_select_by_handle_type.cc
  nodes/legacy/node_geo_legacy_curve_set_handles.cc
  nodes/legacy/node_geo_legacy_curve_spline_type.cc
  nodes/legacy/node_geo_legacy_curve_subdivide.cc
  nodes/legacy/node_geo_legacy_curve_to_points.cc
  nodes/legacy/node_geo_legacy_delete_geometry.cc
  nodes/legacy/node_geo_legacy_edge_split.cc
  nodes/legacy/node_geo_legacy_material_assign.cc
  nodes/legacy/node_geo_legacy_mesh_to_curve.cc
  nodes/legacy/node_geo_legacy_point_distribute.cc
  nodes/legacy/node_geo_legacy_point_instance.cc
  nodes/legacy/node_geo_legacy_point_rotate.cc
  nodes/legacy/node_geo_legacy_point_scale.cc
  nodes/legacy/node_geo_legacy_point_separate.cc
  nodes/legacy/node_geo_legacy_point_translate.cc
  nodes/legacy/node_geo_legacy_points_to_volume.cc
  nodes/legacy/node_geo_legacy_raycast.cc
  nodes/legacy/node_geo_legacy_select_by_material.cc
  nodes/legacy/node_geo_legacy_subdivision_surface.cc
  nodes/legacy/node_geo_legacy_volume_to_mesh.cc

  nodes/node_geo_accumulate_field.cc
  nodes/node_geo_attribute_capture.cc
  nodes/node_geo_attribute_domain_size.cc
  nodes/node_geo_attribute_remove.cc
  nodes/node_geo_attribute_statistic.cc
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
  nodes/node_geo_curve_set_handles.cc
  nodes/node_geo_curve_spline_parameter.cc
  nodes/node_geo_curve_spline_type.cc
  nodes/node_geo_curve_subdivide.cc
  nodes/node_geo_curve_to_mesh.cc
  nodes/node_geo_curve_to_points.cc
  nodes/node_geo_curve_trim.cc
  nodes/node_geo_delete_geometry.cc
  nodes/node_geo_distribute_points_on_faces.cc
  nodes/node_geo_dual_mesh.cc
  nodes/node_geo_edge_split.cc
  nodes/node_geo_geometry_to_instance.cc
  nodes/node_geo_image_texture.cc
  nodes/node_geo_input_curve_handles.cc
  nodes/node_geo_input_curve_tilt.cc
  nodes/node_geo_input_id.cc
  nodes/node_geo_input_index.cc
  nodes/node_geo_input_material_index.cc
  nodes/node_geo_input_material.cc
  nodes/node_geo_input_mesh_edge_neighbors.cc
  nodes/node_geo_input_mesh_edge_vertices.cc
  nodes/node_geo_input_mesh_face_area.cc
  nodes/node_geo_input_mesh_face_neighbors.cc
  nodes/node_geo_input_mesh_island.cc
  nodes/node_geo_input_mesh_vertex_neighbors.cc
  nodes/node_geo_input_normal.cc
  nodes/node_geo_input_position.cc
  nodes/node_geo_input_radius.cc
  nodes/node_geo_input_scene_time.cc
  nodes/node_geo_input_shade_smooth.cc
  nodes/node_geo_input_spline_cyclic.cc
  nodes/node_geo_input_spline_length.cc
  nodes/node_geo_input_spline_resolution.cc
  nodes/node_geo_input_tangent.cc
  nodes/node_geo_instance_on_points.cc
  nodes/node_geo_instances_to_points.cc
  nodes/node_geo_is_viewport.cc
  nodes/node_geo_join_geometry.cc
  nodes/node_geo_material_replace.cc
  nodes/node_geo_material_selection.cc
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
  nodes/node_geo_object_info.cc
  nodes/node_geo_points_to_vertices.cc
  nodes/node_geo_points_to_volume.cc
  nodes/node_geo_proximity.cc
  nodes/node_geo_raycast.cc
  nodes/node_geo_realize_instances.cc
  nodes/node_geo_rotate_instances.cc
  nodes/node_geo_scale_instances.cc
  nodes/node_geo_separate_components.cc
  nodes/node_geo_separate_geometry.cc
  nodes/node_geo_set_curve_handles.cc
  nodes/node_geo_set_curve_radius.cc
  nodes/node_geo_set_curve_tilt.cc
  nodes/node_geo_set_id.cc
  nodes/node_geo_set_material_index.cc
  nodes/node_geo_set_material.cc
  nodes/node_geo_set_point_radius.cc
  nodes/node_geo_set_position.cc
  nodes/node_geo_set_shade_smooth.cc
  nodes/node_geo_set_spline_cyclic.cc
  nodes/node_geo_set_spline_resolution.cc
  nodes/node_geo_string_join.cc
  nodes/node_geo_string_to_curves.cc
  nodes/node_geo_subdivision_surface.cc
  nodes/node_geo_switch.cc
  nodes/node_geo_transfer_attribute.cc
  nodes/node_geo_transform.cc
  nodes/node_geo_translate_instances.cc
  nodes/node_geo_triangulate.cc
  nodes/node_geo_viewer.cc
  nodes/node_geo_volume_to_mesh.cc

  node_geometry_exec.cc
  node_geometry_tree.cc
  node_geometry_util.cc

  node_geometry_util.hh
)

set(LIB
  bf_bmesh
  bf_functions
  bf_geometry
)

if(WITH_BULLET)
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
    "../../../../intern/rigidbody/"
  )
  if(NOT WITH_SYSTEM_BULLET)
  list(APPEND LIB
    extern_bullet
    )
  endif()

  list(APPEND LIB
    ${BULLET_LIBRARIES}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${PYTHON_LINKFLAGS}
    ${PYTHON_LIBRARIES}
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
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
