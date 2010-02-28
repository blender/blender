/**
 * $Id: 
 *
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

/* Internal for editmesh_xxxx.c functions */

#ifndef MESH_INTERN_H
#define MESH_INTERN_H

struct bContext;
struct wmOperatorType;
struct wmOperator;


#define UVCOPY(t, s) memcpy(t, s, 2 * sizeof(float));

/* ******************** editface.c */

int edgetag_context_check(Scene *scene, EditEdge *eed);
void edgetag_context_set(Scene *scene, EditEdge *eed, int val);
int edgetag_shortest_path(Scene *scene, EditMesh *em, EditEdge *source, EditEdge *target);

/* ******************* editmesh.c */

extern void free_editvert(EditMesh *em, EditVert *eve);
extern void free_editedge(EditMesh *em, EditEdge *eed);
extern void free_editface(EditMesh *em, EditFace *efa);
void free_editMesh(EditMesh *em);

extern void free_vertlist(EditMesh *em, ListBase *edve);
extern void free_edgelist(EditMesh *em, ListBase *lb);
extern void free_facelist(EditMesh *em, ListBase *lb);

extern void remedge(EditMesh *em, EditEdge *eed);

extern struct EditVert *addvertlist(EditMesh *em, float *vec, struct EditVert *example);
extern struct EditEdge *addedgelist(EditMesh *em, struct EditVert *v1, struct EditVert *v2, struct EditEdge *example);
extern struct EditFace *addfacelist(EditMesh *em, struct EditVert *v1, struct EditVert *v2, struct EditVert *v3, struct EditVert *v4, struct EditFace *example, struct EditFace *exampleEdges);
extern struct EditEdge *findedgelist(EditMesh *em, struct EditVert *v1, struct EditVert *v2);

void em_setup_viewcontext(struct bContext *C, ViewContext *vc);

void MESH_OT_separate(struct wmOperatorType *ot);

/* ******************* editmesh_add.c */
void MESH_OT_primitive_plane_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cube_add(struct wmOperatorType *ot);
void MESH_OT_primitive_circle_add(struct wmOperatorType *ot);
void MESH_OT_primitive_tube_add(struct wmOperatorType *ot);
void MESH_OT_primitive_cone_add(struct wmOperatorType *ot);
void MESH_OT_primitive_grid_add(struct wmOperatorType *ot);
void MESH_OT_primitive_monkey_add(struct wmOperatorType *ot);
void MESH_OT_primitive_uv_sphere_add(struct wmOperatorType *ot);
void MESH_OT_primitive_ico_sphere_add(struct wmOperatorType *ot);

void MESH_OT_edge_face_add(struct wmOperatorType *ot);
void MESH_OT_dupli_extrude_cursor(struct wmOperatorType *ot);
void MESH_OT_duplicate(struct wmOperatorType *ot);

void MESH_OT_fgon_make(struct wmOperatorType *ot);
void MESH_OT_fgon_clear(struct wmOperatorType *ot);

/* ******************* editmesh_lib.c */
void EM_stats_update(EditMesh *em);

extern void EM_fgon_flags(EditMesh *em);
extern void EM_hide_reset(EditMesh *em);

extern int faceselectedOR(EditFace *efa, int flag);
extern int faceselectedAND(EditFace *efa, int flag);

void EM_remove_selection(EditMesh *em, void *data, int type);
void EM_clear_flag_all(EditMesh *em, int flag);
void EM_set_flag_all(EditMesh *em, int flag);
void EM_set_flag_all_selectmode(EditMesh *em, int flag);

void EM_data_interp_from_verts(EditMesh *em, EditVert *v1, EditVert *v2, EditVert *eve, float fac);
void EM_data_interp_from_faces(EditMesh *em, EditFace *efa1, EditFace *efa2, EditFace *efan, int i1, int i2, int i3, int i4);

int EM_nvertices_selected(EditMesh *em);
int EM_nedges_selected(EditMesh *em);
int EM_nfaces_selected(EditMesh *em);

float EM_face_perimeter(EditFace *efa);

void EM_store_selection(EditMesh *em, void *data, int type);

extern EditFace *exist_face(EditMesh *em, EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4);
extern void flipface(EditMesh *em, EditFace *efa); // flips for normal direction
extern int compareface(EditFace *vl1, EditFace *vl2);

/* flag for selection bits, *nor will be filled with normal for extrusion constraint */
/* return value defines if such normal was set */
extern short extrudeflag_face_indiv(EditMesh *em, short flag, float *nor);
extern short extrudeflag_verts_indiv(EditMesh *em, short flag, float *nor);
extern short extrudeflag_edges_indiv(EditMesh *em, short flag, float *nor);
extern short extrudeflag_vert(Object *obedit, EditMesh *em, short flag, float *nor, int all);
extern short extrudeflag(Object *obedit, EditMesh *em, short flag, float *nor, int all);

extern void adduplicateflag(EditMesh *em, int flag);
extern void delfaceflag(EditMesh *em, int flag);

extern void rotateflag(EditMesh *em, short flag, float *cent, float rotmat[][3]);
extern void translateflag(EditMesh *em, short flag, float *vec);

extern int convex(float *v1, float *v2, float *v3, float *v4);

extern struct EditFace *EM_face_from_faces(EditMesh *em, struct EditFace *efa1,
										   struct EditFace *efa2, int i1, int i2, int i3, int i4);

extern int EM_view3d_poll(struct bContext *C);

/* ******************* editmesh_loop.c */

#define LOOP_SELECT	1
#define LOOP_CUT	2

void MESH_OT_knife_cut(struct wmOperatorType *ot);

/* ******************* editmesh_mods.c */
void MESH_OT_loop_select(struct wmOperatorType *ot);
void MESH_OT_select_all(struct wmOperatorType *ot);
void MESH_OT_select_more(struct wmOperatorType *ot);
void MESH_OT_select_less(struct wmOperatorType *ot);
void MESH_OT_select_inverse(struct wmOperatorType *ot);
void MESH_OT_select_non_manifold(struct wmOperatorType *ot);
void MESH_OT_select_linked(struct wmOperatorType *ot);
void MESH_OT_select_linked_pick(struct wmOperatorType *ot);
void MESH_OT_hide(struct wmOperatorType *ot);
void MESH_OT_reveal(struct wmOperatorType *ot);
void MESH_OT_select_by_number_vertices(struct wmOperatorType *ot);
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
void MESH_OT_flip_normals(struct wmOperatorType *ot);
void MESH_OT_solidify(struct wmOperatorType *ot);
void MESH_OT_select_nth(struct wmOperatorType *ot);


extern EditEdge *findnearestedge(ViewContext *vc, int *dist);
extern void EM_automerge(Scene *scene, Object *obedit, int update);
void editmesh_select_by_material(EditMesh *em, int index);
void EM_recalc_normal_direction(EditMesh *em, int inside, int select);	/* makes faces righthand turning */
void EM_select_more(EditMesh *em);
void selectconnected_mesh_all(EditMesh *em);
void faceloop_select(EditMesh *em, EditEdge *startedge, int select);

/**
 * findnearestvert
 * 
 * dist (in/out): minimal distance to the nearest and at the end, actual distance
 * sel: selection bias
 * 		if SELECT, selected vertice are given a 5 pixel bias to make them farter than unselect verts
 * 		if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */
extern EditVert *findnearestvert(ViewContext *vc, int *dist, short sel, short strict);


/* ******************* editmesh_tools.c */

#define SUBDIV_SELECT_ORIG      0
#define SUBDIV_SELECT_INNER     1
#define SUBDIV_SELECT_INNER_SEL 2
#define SUBDIV_SELECT_LOOPCUT 3

void join_triangles(EditMesh *em);
int removedoublesflag(EditMesh *em, short flag, short automerge, float limit);		/* return amount */
void esubdivideflag(Object *obedit, EditMesh *em, int flag, float smooth, float fractal, int beautify, int numcuts, int seltype);
int EdgeSlide(EditMesh *em, struct wmOperator *op, short immediate, float imperc);

void MESH_OT_merge(struct wmOperatorType *ot);
void MESH_OT_subdivide(struct wmOperatorType *ot);
void MESH_OT_remove_doubles(struct wmOperatorType *ot);
void MESH_OT_extrude(struct wmOperatorType *ot);
void MESH_OT_spin(struct wmOperatorType *ot);
void MESH_OT_screw(struct wmOperatorType *ot);

void MESH_OT_fill(struct wmOperatorType *ot);
void MESH_OT_beautify_fill(struct wmOperatorType *ot);
void MESH_OT_quads_convert_to_tris(struct wmOperatorType *ot);
void MESH_OT_tris_convert_to_quads(struct wmOperatorType *ot);
void MESH_OT_edge_flip(struct wmOperatorType *ot);
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
void MESH_OT_uvs_mirror(struct wmOperatorType *ot);
void MESH_OT_colors_rotate(struct wmOperatorType *ot);
void MESH_OT_colors_mirror(struct wmOperatorType *ot);

void MESH_OT_delete(struct wmOperatorType *ot);
void MESH_OT_rip(struct wmOperatorType *ot);

void MESH_OT_shape_propagate_to_all(struct wmOperatorType *ot);
void MESH_OT_blend_from_shape(struct wmOperatorType *ot);

/* ******************* mesh_data.c */

void MESH_OT_uv_texture_add(struct wmOperatorType *ot);
void MESH_OT_uv_texture_remove(struct wmOperatorType *ot);
void MESH_OT_vertex_color_add(struct wmOperatorType *ot);
void MESH_OT_vertex_color_remove(struct wmOperatorType *ot);
void MESH_OT_sticky_add(struct wmOperatorType *ot);
void MESH_OT_sticky_remove(struct wmOperatorType *ot);
void MESH_OT_drop_named_image(struct wmOperatorType *ot);

void MESH_OT_edgering_select(struct wmOperatorType *ot);
void MESH_OT_loopcut(struct wmOperatorType *ot);

#endif // MESH_INTERN_H

