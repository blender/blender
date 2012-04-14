/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_OPERATORS_PRIVATE_H__
#define __BMESH_OPERATORS_PRIVATE_H__

/** \file blender/bmesh/intern/bmesh_operators_private.h
 *  \ingroup bmesh
 */

struct BMesh;
struct BMOperator;

void BMO_push(BMesh *bm, BMOperator *op);
void BMO_pop(BMesh *bm);

void bmo_split_exec(BMesh *bm, BMOperator *op);
void bmo_spin_exec(BMesh *bm, BMOperator *op);
void bmo_dupe_exec(BMesh *bm, BMOperator *op);
void bmo_del_exec(BMesh *bm, BMOperator *op);
void bmo_esubd_exec(BMesh *bmesh, BMOperator *op);
void bmo_triangulate_exec(BMesh *bmesh, BMOperator *op);
void bmo_dissolve_faces_exec(BMesh *bmesh, BMOperator *op);
void bmo_dissolve_verts_exec(BMesh *bmesh, BMOperator *op);
void bmo_dissolve_limit_exec(BMesh *bmesh, BMOperator *op);
void bmo_extrude_face_region_exec(BMesh *bm, BMOperator *op);
void bmo_vert_slide_exec(BMesh *bm, BMOperator *op);
void bmo_connectverts_exec(BMesh *bm, BMOperator *op);
void bmo_extrude_vert_indiv_exec(BMesh *bm, BMOperator *op);
void bmo_mesh_to_bmesh_exec(BMesh *bm, BMOperator *op);
void bmo_bmesh_to_mesh_exec(BMesh *bm, BMOperator *op);
void bmo_translate_exec(BMesh *bm, BMOperator *op);
void bmo_transform_exec(BMesh *bm, BMOperator *op);
void bmo_contextual_create_exec(BMesh *bm, BMOperator *op);
void bmo_edgenet_fill_exec(BMesh *bm, BMOperator *op);
void bmo_rotate_exec(BMesh *bm, BMOperator *op);
void bmo_makevert_exec(BMesh *bm, BMOperator *op);
void bmo_dissolve_edges_exec(BMesh *bm, BMOperator *op);
void bmo_dissolve_edgeloop_exec(BMesh *bm, BMOperator *op);
void bmo_weldverts_exec(BMesh *bm, BMOperator *op);
void bmo_removedoubles_exec(BMesh *bm, BMOperator *op);
void bmo_finddoubles_exec(BMesh *bm, BMOperator *op);
void bmo_mirror_exec(BMesh *bm, BMOperator *op);
void bmo_edgebisect_exec(BMesh *bm, BMOperator *op);
void bmo_reversefaces_exec(BMesh *bm, BMOperator *op);
void bmo_edgerotate_exec(BMesh *bm, BMOperator *op);
void bmo_regionextend_exec(BMesh *bm, BMOperator *op);
void bmo_righthandfaces_exec(BMesh *bm, BMOperator *op);
void bmo_vertexsmooth_exec(BMesh *bm, BMOperator *op);
void bmo_extrude_edge_only_exec(BMesh *bm, BMOperator *op);
void bmo_extrude_face_indiv_exec(BMesh *bm, BMOperator *op);
void bmo_collapse_uvs_exec(BMesh *bm, BMOperator *op);
void bmo_pointmerge_exec(BMesh *bm, BMOperator *op);
void bmo_collapse_exec(BMesh *bm, BMOperator *op);
void bmo_similarfaces_exec(BMesh *bm, BMOperator *op);
void bmo_similaredges_exec(BMesh *bm, BMOperator *op);
void bmo_similarverts_exec(BMesh *bm, BMOperator *op);
void bmo_pointmerge_facedata_exec(BMesh *bm, BMOperator *op);
void bmo_vert_average_facedata_exec(BMesh *bm, BMOperator *op);
void bmo_face_rotateuvs_exec(BMesh *bm, BMOperator *op);
void bmo_object_load_bmesh_exec(BMesh *bm, BMOperator *op);
void bmo_face_reverseuvs_exec(BMesh *bm, BMOperator *op);
void bmo_edgenet_prepare(BMesh *bm, BMOperator *op);
void bmo_rotatecolors_exec(BMesh *bm, BMOperator *op);
void bmo_face_reversecolors_exec(BMesh *bm, BMOperator *op);
void bmo_vertexshortestpath_exec(BMesh *bm, BMOperator *op);
void bmo_scale_exec(BMesh *bm, BMOperator *op);
void bmo_edgesplit_exec(BMesh *bm, BMOperator *op);
void bmo_automerge_exec(BMesh *bm, BMOperator *op);
void bmo_create_cone_exec(BMesh *bm, BMOperator *op);
void bmo_create_monkey_exec(BMesh *bm, BMOperator *op);
void bmo_create_icosphere_exec(BMesh *bm, BMOperator *op);
void bmo_create_uvsphere_exec(BMesh *bm, BMOperator *op);
void bmo_create_grid_exec(BMesh *bm, BMOperator *op);
void bmo_create_cube_exec(BMesh *bm, BMOperator *op);
void bmo_join_triangles_exec(BMesh *bm, BMOperator *op);
void bmo_bevel_exec(BMesh *bm, BMOperator *op);
void bmo_beautify_fill_exec(BMesh *bm, BMOperator *op);
void bmo_triangle_fill_exec(BMesh *bm, BMOperator *op);
void bmo_create_circle_exec(BMesh *bm, BMOperator *op);
void bmo_bridge_loops_exec(BMesh *bm, BMOperator *op);
void bmo_solidify_face_region_exec(BMesh *bm, BMOperator *op);
void bmo_inset_exec(BMesh *bm, BMOperator *op);

#endif /* __BMESH_OPERATORS_PRIVATE_H__ */
