/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

/* External for editmesh_xxxx.c functions */

#ifndef BIF_EDITMESH_H
#define BIF_EDITMESH_H

#include "BKE_mesh.h"

struct EditMesh;
struct EditFace;
struct EditEdge;
struct EditVert;
struct Mesh;
struct bDeformGroup;
struct View3D;
struct EditSelection;
struct CustomData;

// edge and face flag both
#define EM_FGON		2
// face flag
#define EM_FGON_DRAW	1

extern unsigned int em_vertoffs, em_solidoffs, em_wireoffs;

/* ******************* editmesh.c */
extern void make_editMesh(void);
extern void load_editMesh(void);
extern void free_editMesh(struct EditMesh *);
extern void remake_editMesh(void);

	/* Editmesh Undo code */
extern void undo_push_mesh(char *name);

extern void separatemenu(void);
extern void separate_mesh(void);
extern void separate_mesh_loose(void);
extern void separate_material(void);

/* ******************* editmesh_add.c */
extern void make_prim(int type, float imat[3][3], int tot, int seg,
		int subdiv, float dia, float d, int ext, int fill,
        float cent[3] );
extern void add_primitiveMesh(int type);
extern void adduplicate_mesh(void);
extern void add_click_mesh(void);
extern void addedgeface_mesh(void);
void addfaces_from_edgenet();

/* ******************* editmesh_lib.c */

extern void EM_set_flag_all(int flag);
extern void EM_clear_flag_all(int flag);

extern void EM_select_face(struct EditFace *efa, int sel);
extern void EM_select_edge(struct EditEdge *eed, int sel);
extern float EM_face_area(struct EditFace *efa);
extern float EM_face_perimeter(struct EditFace *efa);
extern void EM_editselection_center(float *center, struct EditSelection *ese);
extern void EM_editselection_normal(float *normal, struct EditSelection *ese);
extern void EM_editselection_plane(float *plane, struct EditSelection *ese);

extern void EM_deselect_flush(void);	// vertices to edges/faces (exception!)
extern void EM_select_flush(void);	// vertices to edges/faces (exception!)
extern void EM_selectmode_set(void); // when mode changes
extern void EM_selectmode_flush(void); // when selection changes
extern void EM_convertsel(short oldmode, short selectmode);
extern void EM_remove_selection(void *data, int type);
extern void EM_store_selection(void *data, int type);
extern void EM_validate_selections(void);

extern int EM_nfaces_selected(void);
extern int EM_nvertices_selected(void);

extern int faceselectedAND(struct EditFace *efa, int flag);
extern void recalc_editnormals(void);
extern void flip_editnormals(void);

extern void EM_data_interp_from_verts(struct EditVert *v1,
	struct EditVert *v2, struct EditVert *eve, float fac);
extern struct EditFace *EM_face_from_faces(struct EditFace *efa1,
	struct EditFace *efa2, int i1, int i2, int i3, int i4);
extern void EM_data_interp_from_faces(struct EditFace *efa1,
	struct EditFace *efa2, struct EditFace *efan, int i1, int i2, int i3, int i4);

void EM_add_data_layer(struct CustomData *data, int type);
void EM_free_data_layer(struct CustomData *data, int type);

/* ******************* editmesh_mods.c */

extern void EM_init_index_arrays(int forVert, int forEdge, int forFace);
extern void EM_free_index_arrays(void);

extern struct EditVert *EM_get_vert_for_index(int index);
extern struct EditEdge *EM_get_edge_for_index(int index);
extern struct EditFace *EM_get_face_for_index(int index);

extern void EM_select_face_fgon(struct EditFace *efa, int sel);

extern int EM_init_backbuf_border(short xmin, short ymin, short xmax, short ymax);
extern int EM_mask_init_backbuf_border(short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
extern int EM_init_backbuf_circle(short xs, short ys, short rads);
extern int EM_check_backbuf(unsigned int index);
extern void EM_free_backbuf(void);

extern void EM_selectmode_menu(void);
extern void EM_mesh_copy_face(short type);

extern void vertexnoise(void);
extern void vertexsmooth(void);
extern void righthandfaces(int select);
extern void mouse_mesh(void);

extern void deselectall_mesh(void);
extern void selectconnected_mesh_all(void);
extern void selectconnected_mesh(void);
extern void selectconnected_delimit_mesh(void);
extern void selectconnected_delimit_mesh_all(void);
extern void selectswap_mesh(void);

extern void hide_mesh(int swap);
extern void reveal_mesh(void);

extern void vertices_to_sphere(void);

	/** Aligns the selected MTFace's of @a me to the @a v3d,
	 * using the given axis (0-2). Can give a user error.
	 */
extern void faceselect_align_view_to_selected(struct View3D *v3d, struct Mesh *me, int axis);
	/** Aligns the selected faces or vertices of @a me to the @a v3d,
	 * using the given axis (0-2). Can give a user error.
	 */
extern void editmesh_align_view_to_selected(struct View3D *v3d, int axis);

	/* Selection */
extern void select_non_manifold(void);
extern void select_sharp_edges(void);
extern void select_linked_flat_faces(void);
extern void select_faces_by_numverts(int numverts);
extern void select_more(void);
extern void select_less(void);
extern void selectrandom_mesh(void);
extern void editmesh_select_by_material(int index);
extern void editmesh_deselect_by_material(int index);

extern void Vertex_Menu(void);
extern void Edge_Menu(void);
extern void Face_Menu(void);
extern void select_mesh_group_menu(void);
extern void editmesh_mark_seam(int clear);
extern void loop_multiselect(int looptype);

extern void EM_select_more(void);
extern void EM_select_less(void);

/* ******************* editmesh_loop.c */

#define KNIFE_PROMPT 0
#define KNIFE_EXACT 1
#define KNIFE_MIDPOINT 2
#define KNIFE_MULTICUT 3

extern void CutEdgeloop(int numcuts);
extern void KnifeSubdivide(char mode);
extern void LoopMenu(void);

#define LOOP_SELECT	1
#define LOOP_CUT	2

extern short sharesFace(struct EditEdge* e1, struct EditEdge* e2);

/* ******************* editmesh_tools.c */

#define SUBDIV_SELECT_ORIG      0
#define SUBDIV_SELECT_INNER     1
#define SUBDIV_SELECT_INNER_SEL 2
#define SUBDIV_SELECT_LOOPCUT 3

extern void convert_to_triface(int direction);
extern int removedoublesflag(short flag, short automerge, float limit);
extern void xsortvert_flag(int flag);
extern void hashvert_flag(int flag);

extern void esubdivideflag(int flag, float rad, int beauty, int numcuts, int selecttype);

extern void extrude_mesh(void);
extern void split_mesh(void);
extern void extrude_repeat_mesh(int steps, float offs);
extern void spin_mesh(int steps,float degr,float *dvec, int mode);
extern void screw_mesh(int steps,int turns);
extern void delete_mesh(void);
extern void beauty_fill(void);
extern void join_triangles(void);
extern void edge_flip(void);
extern void fill_mesh(void);
extern void bevel_menu();
void mesh_set_face_flags(short mode);
extern void mesh_set_smooth_faces(short event);
extern void mesh_rotate_uvs(void);
extern void mesh_mirror_uvs(void);
extern void mesh_rotate_colors(void);
extern void mesh_mirror_colors(void);
void mesh_copy_menu(void);
void edge_rotate_selected(int dir);
int EdgeSlide(short immediate, float imperc);
int EdgeLoopDelete(void);
void mesh_rip(void);
 
struct EditVert *editedge_getOtherVert(struct EditEdge *eed, struct EditVert *ev);
struct EditVert *editedge_getSharedVert(struct EditEdge *eed, struct EditEdge *eed2);
int editedge_containsVert(struct EditEdge *eed, struct EditVert *eve);
int editface_containsVert(struct EditFace *efa, struct EditVert *eve);
int editface_containsEdge(struct EditFace *efa, struct EditEdge *eed);

void shape_copy_select_from(void);
void shape_propagate(void);

int collapseEdges(void);
int merge_firstlast(int first, int uvmerge);
int merge_target( int target, int uvmerge);

void pathselect(void);
void loop_to_region(void);
void region_to_loop(void);

UvVertMap *make_uv_vert_map_EM(int selected, int do_face_idx_array, float *limit);
UvMapVert *get_uv_map_vert_EM(UvVertMap *vmap, unsigned int v);
void free_uv_vert_map_EM(UvVertMap *vmap);

int EM_texFaceCheck(void); /* can we edit UV's for this mesh?*/
int EM_vertColorCheck(void); /* can we edit colors for this mesh?*/

void EM_set_actFace(struct EditFace *efa);
struct EditFace * EM_get_actFace(int sloppy);
int EM_get_actSelection(struct EditSelection *ese);

#endif
