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

void splitop_exec(BMesh *bm, BMOperator *op);
void spinop_exec(BMesh *bm, BMOperator *op);
void dupeop_exec(BMesh *bm, BMOperator *op);
void delop_exec(BMesh *bm, BMOperator *op);
void esubdivide_exec(BMesh *bmesh, BMOperator *op);
void edit2bmesh_exec(BMesh *bmesh, BMOperator *op);
void bmesh2edit_exec(BMesh *bmesh, BMOperator *op);
void triangulate_exec(BMesh *bmesh, BMOperator *op);
void dissolvefaces_exec(BMesh *bmesh, BMOperator *op);
void dissolveverts_exec(BMesh *bmesh, BMOperator *op);
void dissolvelimit_exec(BMesh *bmesh, BMOperator *op);
void bmesh_make_fgons_exec(BMesh *bmesh, BMOperator *op);
void extrude_edge_context_exec(BMesh *bm, BMOperator *op);
void connectverts_exec(BMesh *bm, BMOperator *op);
void makeprim_exec(BMesh *bm, BMOperator *op);
void extrude_vert_indiv_exec(BMesh *bm, BMOperator *op);
void mesh_to_bmesh_exec(BMesh *bm, BMOperator *op);
void bmesh_to_mesh_exec(BMesh *bm, BMOperator *op);
void bmesh_translate_exec(BMesh *bm, BMOperator *op);
void bmesh_transform_exec(BMesh *bm, BMOperator *op);
void bmesh_contextual_create_exec(BMesh *bm, BMOperator *op);
void bmesh_edgenet_fill_exec(BMesh *bm, BMOperator *op);
void bmesh_rotate_exec(BMesh *bm, BMOperator *op);
void bmesh_makevert_exec(BMesh *bm, BMOperator *op);
void dissolveedges_exec(BMesh *bm, BMOperator *op);
void dissolve_edgeloop_exec(BMesh *bm, BMOperator *op);
void bmesh_weldverts_exec(BMesh *bm, BMOperator *op);
void bmesh_removedoubles_exec(BMesh *bm, BMOperator *op);
void bmesh_finddoubles_exec(BMesh *bm, BMOperator *op);
void bmesh_mirror_exec(BMesh *bm, BMOperator *op);
void esplit_exec(BMesh *bm, BMOperator *op);
void bmesh_reversefaces_exec(BMesh *bm, BMOperator *op);
void bmesh_edgerotate_exec(BMesh *bm, BMOperator *op);
void bmesh_regionextend_exec(BMesh *bm, BMOperator *op);
void bmesh_righthandfaces_exec(BMesh *bm, BMOperator *op);
void bmesh_vertexsmooth_exec(BMesh *bm, BMOperator *op);
void bmesh_extrude_onlyedge_exec(BMesh *bm, BMOperator *op);
void bmesh_extrude_face_indiv_exec(BMesh *bm, BMOperator *op);
void bmesh_collapsecon_exec(BMesh *bm, BMOperator *op);
void bmesh_pointmerge_exec(BMesh *bm, BMOperator *op);
void bmesh_collapse_exec(BMesh *bm, BMOperator *op);
void bmesh_similarfaces_exec(BMesh *bm, BMOperator *op);
void bmesh_similaredges_exec(BMesh *bm, BMOperator *op);
void bmesh_similarverts_exec(BMesh *bm, BMOperator *op);
void bmesh_pointmerge_facedata_exec(BMesh *bm, BMOperator *op);
void bmesh_vert_average_facedata_exec(BMesh *bm, BMOperator *op);
void bmesh_rotateuvs_exec(BMesh *bm, BMOperator *op);
void object_load_bmesh_exec(BMesh *bm, BMOperator *op);
void bmesh_reverseuvs_exec(BMesh *bm, BMOperator *op);
void bmesh_edgenet_prepare(BMesh *bm, BMOperator *op);
void bmesh_rotatecolors_exec(BMesh *bm, BMOperator *op);
void bmesh_reversecolors_exec(BMesh *bm, BMOperator *op);
void bmesh_vertexshortestpath_exec(BMesh *bm, BMOperator *op);
void bmesh_scale_exec(BMesh *bm, BMOperator *op);
void bmesh_edgesplitop_exec(BMesh *bm, BMOperator *op);
void bmesh_automerge_exec(BMesh *bm, BMOperator *op);
void bmesh_create_cone_exec(BMesh *bm, BMOperator *op);
void bmesh_create_monkey_exec(BMesh *bm, BMOperator *op);
void bmesh_create_icosphere_exec(BMesh *bm, BMOperator *op);
void bmesh_create_uvsphere_exec(BMesh *bm, BMOperator *op);
void bmesh_create_grid_exec(BMesh *bm, BMOperator *op);
void bmesh_create_cube_exec(BMesh *bm, BMOperator *op);
void bmesh_jointriangles_exec(BMesh *bm, BMOperator *op);
void bmesh_bevel_exec(BMesh *bm, BMOperator *op);
void bmesh_beautify_fill_exec(BMesh *bm, BMOperator *op);
void bmesh_triangle_fill_exec(BMesh *bm, BMOperator *op);
void bmesh_create_circle_exec(BMesh *bm, BMOperator *op);
void bmesh_bridge_loops_exec(BMesh *bm, BMOperator *op);
void bmesh_solidify_face_region_exec(BMesh *bm, BMOperator *op);

#endif /* __BMESH_OPERATORS_PRIVATE_H__ */
