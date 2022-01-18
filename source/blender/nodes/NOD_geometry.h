/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BKE_node.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Geometry;

void register_node_tree_type_geo(void);

void register_node_type_geo_group(void);
void register_node_type_geo_custom_group(bNodeType *ntype);

void register_node_type_geo_legacy_attribute_proximity(void);
void register_node_type_geo_legacy_attribute_randomize(void);
void register_node_type_geo_legacy_attribute_transfer(void);
void register_node_type_geo_legacy_curve_endpoints(void);
void register_node_type_geo_legacy_curve_reverse(void);
void register_node_type_geo_legacy_curve_set_handles(void);
void register_node_type_geo_legacy_curve_spline_type(void);
void register_node_type_geo_legacy_curve_subdivide(void);
void register_node_type_geo_legacy_curve_to_points(void);
void register_node_type_geo_legacy_delete_geometry(void);
void register_node_type_geo_legacy_edge_split(void);
void register_node_type_geo_legacy_material_assign(void);
void register_node_type_geo_legacy_mesh_to_curve(void);
void register_node_type_geo_legacy_points_to_volume(void);
void register_node_type_geo_legacy_raycast(void);
void register_node_type_geo_legacy_select_by_handle_type(void);
void register_node_type_geo_legacy_select_by_material(void);
void register_node_type_geo_legacy_subdivision_surface(void);
void register_node_type_geo_legacy_volume_to_mesh(void);

void register_node_type_geo_accumulate_field(void);
void register_node_type_geo_align_rotation_to_vector(void);
void register_node_type_geo_attribute_capture(void);
void register_node_type_geo_attribute_clamp(void);
void register_node_type_geo_attribute_color_ramp(void);
void register_node_type_geo_attribute_combine_xyz(void);
void register_node_type_geo_attribute_compare(void);
void register_node_type_geo_attribute_convert(void);
void register_node_type_geo_attribute_curve_map(void);
void register_node_type_geo_attribute_domain_size(void);
void register_node_type_geo_attribute_fill(void);
void register_node_type_geo_attribute_map_range(void);
void register_node_type_geo_attribute_math(void);
void register_node_type_geo_attribute_mix(void);
void register_node_type_geo_attribute_remove(void);
void register_node_type_geo_attribute_separate_xyz(void);
void register_node_type_geo_attribute_statistic(void);
void register_node_type_geo_attribute_vector_math(void);
void register_node_type_geo_attribute_vector_rotate(void);
void register_node_type_geo_boolean(void);
void register_node_type_geo_bounding_box(void);
void register_node_type_geo_collection_info(void);
void register_node_type_geo_convex_hull(void);
void register_node_type_geo_curve_endpoint_selection(void);
void register_node_type_geo_curve_fill(void);
void register_node_type_geo_curve_fillet(void);
void register_node_type_geo_curve_handle_type_selection(void);
void register_node_type_geo_curve_length(void);
void register_node_type_geo_curve_primitive_bezier_segment(void);
void register_node_type_geo_curve_primitive_circle(void);
void register_node_type_geo_curve_primitive_line(void);
void register_node_type_geo_curve_primitive_quadratic_bezier(void);
void register_node_type_geo_curve_primitive_quadrilateral(void);
void register_node_type_geo_curve_primitive_spiral(void);
void register_node_type_geo_curve_primitive_star(void);
void register_node_type_geo_curve_resample(void);
void register_node_type_geo_curve_reverse(void);
void register_node_type_geo_curve_sample(void);
void register_node_type_geo_curve_set_handles(void);
void register_node_type_geo_curve_spline_parameter(void);
void register_node_type_geo_curve_spline_type(void);
void register_node_type_geo_curve_subdivide(void);
void register_node_type_geo_curve_to_mesh(void);
void register_node_type_geo_curve_to_points(void);
void register_node_type_geo_curve_trim(void);
void register_node_type_geo_delete_geometry(void);
void register_node_type_geo_distribute_points_on_faces(void);
void register_node_type_geo_dual_mesh(void);
void register_node_type_geo_edge_split(void);
void register_node_type_geo_field_at_index(void);
void register_node_type_geo_geometry_to_instance(void);
void register_node_type_geo_image_texture(void);
void register_node_type_geo_input_curve_handles(void);
void register_node_type_geo_input_curve_tilt(void);
void register_node_type_geo_input_id(void);
void register_node_type_geo_input_index(void);
void register_node_type_geo_input_material_index(void);
void register_node_type_geo_input_material(void);
void register_node_type_geo_input_mesh_edge_angle(void);
void register_node_type_geo_input_mesh_edge_neighbors(void);
void register_node_type_geo_input_mesh_edge_vertices(void);
void register_node_type_geo_input_mesh_face_area(void);
void register_node_type_geo_input_mesh_face_neighbors(void);
void register_node_type_geo_input_mesh_island(void);
void register_node_type_geo_input_mesh_vertex_neighbors(void);
void register_node_type_geo_input_normal(void);
void register_node_type_geo_input_position(void);
void register_node_type_geo_input_radius(void);
void register_node_type_geo_input_scene_time(void);
void register_node_type_geo_input_shade_smooth(void);
void register_node_type_geo_input_spline_cyclic(void);
void register_node_type_geo_input_spline_length(void);
void register_node_type_geo_input_spline_resolution(void);
void register_node_type_geo_input_tangent(void);
void register_node_type_geo_instance_on_points(void);
void register_node_type_geo_instances_to_points(void);
void register_node_type_geo_is_viewport(void);
void register_node_type_geo_join_geometry(void);
void register_node_type_geo_material_replace(void);
void register_node_type_geo_material_selection(void);
void register_node_type_geo_mesh_primitive_circle(void);
void register_node_type_geo_mesh_primitive_cone(void);
void register_node_type_geo_mesh_primitive_cube(void);
void register_node_type_geo_mesh_primitive_cylinder(void);
void register_node_type_geo_mesh_primitive_grid(void);
void register_node_type_geo_mesh_primitive_ico_sphere(void);
void register_node_type_geo_mesh_primitive_line(void);
void register_node_type_geo_mesh_primitive_uv_sphere(void);
void register_node_type_geo_mesh_subdivide(void);
void register_node_type_geo_mesh_to_curve(void);
void register_node_type_geo_mesh_to_points(void);
void register_node_type_geo_object_info(void);
void register_node_type_geo_point_distribute(void);
void register_node_type_geo_point_instance(void);
void register_node_type_geo_point_rotate(void);
void register_node_type_geo_point_scale(void);
void register_node_type_geo_point_separate(void);
void register_node_type_geo_point_translate(void);
void register_node_type_geo_points_to_vertices(void);
void register_node_type_geo_points_to_volume(void);
void register_node_type_geo_proximity(void);
void register_node_type_geo_raycast(void);
void register_node_type_geo_realize_instances(void);
void register_node_type_geo_rotate_instances(void);
void register_node_type_geo_sample_texture(void);
void register_node_type_geo_scale_instances(void);
void register_node_type_geo_select_by_handle_type(void);
void register_node_type_geo_separate_components(void);
void register_node_type_geo_separate_geometry(void);
void register_node_type_geo_set_curve_handles(void);
void register_node_type_geo_set_curve_radius(void);
void register_node_type_geo_set_curve_tilt(void);
void register_node_type_geo_set_id(void);
void register_node_type_geo_set_material_index(void);
void register_node_type_geo_set_material(void);
void register_node_type_geo_set_point_radius(void);
void register_node_type_geo_set_position(void);
void register_node_type_geo_set_shade_smooth(void);
void register_node_type_geo_set_spline_cyclic(void);
void register_node_type_geo_set_spline_resolution(void);
void register_node_type_geo_string_join(void);
void register_node_type_geo_string_to_curves(void);
void register_node_type_geo_subdivision_surface(void);
void register_node_type_geo_switch(void);
void register_node_type_geo_transfer_attribute(void);
void register_node_type_geo_transform(void);
void register_node_type_geo_translate_instances(void);
void register_node_type_geo_triangulate(void);
void register_node_type_geo_viewer(void);
void register_node_type_geo_volume_to_mesh(void);

#ifdef __cplusplus
}
#endif
