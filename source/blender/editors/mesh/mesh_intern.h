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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/mesh_intern.h
 *  \ingroup edmesh
 */


/* Internal for editmesh_xxxx.c functions */

#ifndef __MESH_INTERN_H__
#define __MESH_INTERN_H__

struct bContext;
struct wmOperatorType;
struct ViewContext;
struct BMEditMesh;
struct BMesh;
struct BMEdge;
struct BMFace;
struct BMOperator;
struct wmOperator;
struct wmKeyMap;
struct wmKeyConfig;
struct EnumPropertyItem;

/* ******************** bmeshutils.c */

/*
ok: the EDBM module is for editmode bmesh stuff.  in contrast, the 
    BMEdit module is for code shared with blenkernel that concerns
    the BMEditMesh structure.
*/

/*calls a bmesh op, reporting errors to the user, etc*/
int EDBM_CallOpf(struct BMEditMesh *em, struct wmOperator *op, const char *fmt, ...);

/*same as above, but doesn't report errors.*/
int EDBM_CallOpfSilent(struct BMEditMesh *em, const char *fmt, ...);

/*these next two functions are the split version of EDBM_CallOpf, so you can
  do stuff with a bmesh operator, after initializing it but before executing
  it.
  
  execute the operator with BM_Exec_Op*/
int EDBM_InitOpf(struct BMEditMesh *em, struct BMOperator *bmop,
                 struct wmOperator *op, const char *fmt, ...);
/*cleans up after a bmesh operator*/
int EDBM_FinishOp(struct BMEditMesh *em, struct BMOperator *bmop,
                  struct wmOperator *op, const int report);

void EDBM_flag_disable_all(struct BMEditMesh *em, const char hflag);
void EDBM_store_selection(struct BMEditMesh *em, void *data);
void EDBM_validate_selections(struct BMEditMesh *em);
void EDBM_remove_selection(struct BMEditMesh *em, void *data);
void EDBM_stats_update(struct BMEditMesh *em);

/* TODO, move to math_geometry.c */
float labda_PdistVL2Dfl(const float v1[3], const float v2[3], const float v3[3]);

/* ******************** editface.c */

void em_setup_viewcontext(struct bContext *C, struct ViewContext *vc);

void MESH_OT_separate(struct wmOperatorType *ot);

/* ******************* editmesh_add.c */
void MESH_OT_primitive_plane_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cube_add(struct wmOperatorType *ot);
void MESH_OT_primitive_circle_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cylinder_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cone_add(struct wmOperatorType *ot);
void MESH_OT_primitive_grid_add(struct wmOperatorType *ot);
void MESH_OT_primitive_monkey_add(struct wmOperatorType *ot);
void MESH_OT_primitive_uv_sphere_add(struct wmOperatorType *ot);
void MESH_OT_primitive_ico_sphere_add(struct wmOperatorType *ot);

void MESH_OT_edge_face_add(struct wmOperatorType *ot);
void MESH_OT_dupli_extrude_cursor(struct wmOperatorType *ot);
void MESH_OT_duplicate(struct wmOperatorType *ot);

extern int EM_view3d_poll(struct bContext *C);

struct wmKeyMap* knifetool_modal_keymap(struct wmKeyConfig *keyconf);

/* ******************* knifetool.c */

void MESH_OT_knife_cut(struct wmOperatorType *ot);

/* ******************* bmesh_select.c */
void MESH_OT_loop_select(struct wmOperatorType *ot);
void MESH_OT_select_all(struct wmOperatorType *ot);
void MESH_OT_select_interior_faces(struct wmOperatorType *ot);
void MESH_OT_select_more(struct wmOperatorType *ot);
void MESH_OT_select_less(struct wmOperatorType *ot);
void MESH_OT_select_non_manifold(struct wmOperatorType *ot);
void MESH_OT_select_linked(struct wmOperatorType *ot);
void MESH_OT_select_linked_pick(struct wmOperatorType *ot);
void MESH_OT_hide(struct wmOperatorType *ot);
void MESH_OT_reveal(struct wmOperatorType *ot);
void MESH_OT_select_by_number_vertices(struct wmOperatorType *ot);
void MESH_OT_select_loose_verts(struct wmOperatorType *ot);
void MESH_OT_select_mirror(struct wmOperatorType *ot);
void MESH_OT_normals_make_consistent(struct wmOperatorType *ot);
void MESH_OT_faces_select_linked_flat(struct wmOperatorType *ot);
void MESH_OT_edges_select_sharp(struct wmOperatorType *ot);
void MESH_OT_select_shortest_path(struct wmOperatorType *ot);
void MESH_OT_select_similar(struct wmOperatorType *ot);
void MESH_OT_select_random(struct wmOperatorType *ot);
void MESH_OT_loop_multi_select(struct wmOperatorType *ot);
void MESH_OT_mark_seam(struct wmOperatorType *ot);
void MESH_OT_mark_sharp(struct wmOperatorType *ot);
void MESH_OT_vertices_smooth(struct wmOperatorType *ot);
void MESH_OT_noise(struct wmOperatorType *ot);
void MESH_OT_flip_normals(struct wmOperatorType *ot);
void MESH_OT_solidify(struct wmOperatorType *ot);
void MESH_OT_select_nth(struct wmOperatorType *ot);
void MESH_OT_select_next_loop(struct wmOperatorType *ot);

extern struct EnumPropertyItem *corner_type_items;

void MESH_OT_merge(struct wmOperatorType *ot);
void MESH_OT_subdivide(struct wmOperatorType *ot);
void MESH_OT_remove_doubles(struct wmOperatorType *ot);
void MESH_OT_vertices_randomize(struct wmOperatorType *ot);
void MESH_OT_vertices_sort(struct wmOperatorType *ot);
void MESH_OT_spin(struct wmOperatorType *ot);
void MESH_OT_screw(struct wmOperatorType *ot);

void MESH_OT_fill(struct wmOperatorType *ot);
void MESH_OT_beautify_fill(struct wmOperatorType *ot);
void MESH_OT_quads_convert_to_tris(struct wmOperatorType *ot);
void MESH_OT_tris_convert_to_quads(struct wmOperatorType *ot);
void MESH_OT_dissolve_limited(struct wmOperatorType *ot);
void MESH_OT_faces_shade_smooth(struct wmOperatorType *ot);
void MESH_OT_faces_shade_flat(struct wmOperatorType *ot);
void MESH_OT_split(struct wmOperatorType *ot);
void MESH_OT_extrude_repeat(struct wmOperatorType *ot);
void MESH_OT_edge_rotate(struct wmOperatorType *ot);
void MESH_OT_select_vertex_path(struct wmOperatorType *ot);
void MESH_OT_loop_to_region(struct wmOperatorType *ot);
void MESH_OT_region_to_loop(struct wmOperatorType *ot);
void MESH_OT_select_axis(struct wmOperatorType *ot);

void MESH_OT_uvs_rotate(struct wmOperatorType *ot);
//void MESH_OT_uvs_mirror(struct wmOperatorType *ot);
void MESH_OT_uvs_reverse(struct wmOperatorType *ot);
void MESH_OT_colors_rotate(struct wmOperatorType *ot);
//void MESH_OT_colors_mirror(struct wmOperatorType *ot);

void MESH_OT_colors_reverse(struct wmOperatorType *ot);

void MESH_OT_delete(struct wmOperatorType *ot);
void MESH_OT_rip(struct wmOperatorType *ot);

void MESH_OT_shape_propagate_to_all(struct wmOperatorType *ot);
void MESH_OT_blend_from_shape(struct wmOperatorType *ot);
void MESH_OT_sort_faces(struct wmOperatorType *ot);

/* ******************* mesh_data.c */

void MESH_OT_uv_texture_add(struct wmOperatorType *ot);
void MESH_OT_uv_texture_remove(struct wmOperatorType *ot);
void MESH_OT_vertex_color_add(struct wmOperatorType *ot);
void MESH_OT_vertex_color_remove(struct wmOperatorType *ot);
void MESH_OT_sticky_add(struct wmOperatorType *ot);
void MESH_OT_sticky_remove(struct wmOperatorType *ot);
void MESH_OT_drop_named_image(struct wmOperatorType *ot);

/* ************* bmesh_tools.c ***********/
void MESH_OT_vert_connect(struct wmOperatorType *ot);
void MESH_OT_edge_split(struct wmOperatorType *ot);
void MESH_OT_extrude_region(struct wmOperatorType *ot);
void MESH_OT_extrude_verts_indiv(struct wmOperatorType *ot);
void MESH_OT_extrude_edges_indiv(struct wmOperatorType *ot);
void MESH_OT_extrude_faces_indiv(struct wmOperatorType *ot);
void MESH_OT_bm_test(struct wmOperatorType *ot);

void MESH_OT_edgering_select(struct wmOperatorType *ot);
void MESH_OT_loopcut(struct wmOperatorType *ot);

void MESH_OT_knifetool(struct wmOperatorType *ot);
void MESH_OT_bevel(struct wmOperatorType *ot);

void MESH_OT_bridge_edge_loops(struct wmOperatorType *ot);

/* ******************* mesh_navmesh.c */
void MESH_OT_navmesh_make(struct wmOperatorType *ot);
void MESH_OT_navmesh_face_copy(struct wmOperatorType *ot);
void MESH_OT_navmesh_face_add(struct wmOperatorType *ot);
void MESH_OT_navmesh_reset(struct wmOperatorType *ot);
void MESH_OT_navmesh_clear(struct wmOperatorType *ot);

#endif // __MESH_INTERN_H__

