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

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Geometry;

void register_node_tree_type_geo(void);

void register_node_type_geo_group(void);

void register_node_type_geo_attribute_fill(void);
void register_node_type_geo_boolean(void);
void register_node_type_geo_edge_split(void);
void register_node_type_geo_transform(void);
void register_node_type_geo_subdivision_surface(void);
void register_node_type_geo_triangulate(void);
void register_node_type_geo_point_distribute(void);
void register_node_type_geo_point_instance(void);
void register_node_type_geo_object_info(void);
void register_node_type_geo_attribute_randomize(void);
void register_node_type_geo_attribute_math(void);
void register_node_type_geo_join_geometry(void);
void register_node_type_geo_attribute_mix(void);

#ifdef __cplusplus
}
#endif
