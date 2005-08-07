/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* External for editmesh_xxxx.c functions */

#ifndef BIF_EDITMESH_H
#define BIF_EDITMESH_H

struct EditMesh;
struct EditFace;
struct EditEdge;
struct EditVert;
struct Mesh;
struct bDeformGroup;
struct View3D;

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

/* ******************* editmesh_add.c */
extern void add_primitiveMesh(int type);
extern void adduplicate_mesh(void);
extern void addvert_mesh(void);
extern void addedgeface_mesh(void);

/* ******************* editmesh_lib.c */

extern void EM_set_flag_all(int flag);
extern void EM_clear_flag_all(int flag);

extern void EM_select_face(struct EditFace *efa, int sel);
extern void EM_select_edge(struct EditEdge *eed, int sel);

extern void EM_deselect_flush(void);	// vertices to edges/faces (exception!)
extern void EM_select_flush(void);	// vertices to edges/faces (exception!)
extern void EM_selectmode_set(void); // when mode changes
extern void EM_selectmode_flush(void); // when selection changes

extern int EM_nfaces_selected(void);
extern int EM_nvertices_selected(void);

extern int faceselectedAND(struct EditFace *efa, int flag);
extern void recalc_editnormals(void);
extern void flip_editnormals(void);

/* ******************* editmesh_mods.c */

extern void EM_select_face_fgon(struct EditFace *efa, int sel);

extern int EM_init_backbuf_border(short xmin, short ymin, short xmax, short ymax);
extern int EM_mask_init_backbuf_border(short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
extern int EM_init_backbuf_circle(short xs, short ys, short rads);
extern int EM_check_backbuf_border(unsigned int index);
extern void EM_free_backbuf_border(void);

extern void EM_selectmode_menu(void);


extern void vertexnoise(void);
extern void vertexsmooth(void);
extern void righthandfaces(int select);
extern void mouse_mesh(void);

extern void deselectall_mesh(void);
extern void selectconnected_mesh(int qual);
extern void selectswap_mesh(void);

extern void hide_mesh(int swap);
extern void reveal_mesh(void);

extern void vertices_to_sphere(void);

	/** Aligns the selected TFace's of @a me to the @a v3d,
	 * using the given axis (0-2). Can give a user error.
	 */
extern void faceselect_align_view_to_selected(struct View3D *v3d, struct Mesh *me, int axis);
	/** Aligns the selected faces or vertices of @a me to the @a v3d,
	 * using the given axis (0-2). Can give a user error.
	 */
extern void editmesh_align_view_to_selected(struct View3D *v3d, int axis);

	/* Selection */
extern void select_non_manifold(void);
extern void select_more(void);
extern void select_less(void);
extern void selectrandom_mesh(void);
extern void editmesh_select_by_material(int index);
extern void editmesh_deselect_by_material(int index);

extern void Edge_Menu(void);
extern void editmesh_mark_seam(int clear);


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
extern void convert_to_triface(int all);
extern int removedoublesflag(short flag, float limit);
extern void xsortvert_flag(int flag);
extern void hashvert_flag(int flag);
extern void subdivideflag(int flag, float rad, int beauty);
extern void esubdivideflag(int flag, float rad, int beauty, int numcuts, int selecttype);
extern void extrude_mesh(void);
extern void split_mesh(void);
extern void extrude_repeat_mesh(int steps, float offs);
extern void spin_mesh(int steps,int degr,float *dvec, int mode);
extern void screw_mesh(int steps,int turns);
extern void delete_mesh(void);
extern void beauty_fill(void);
extern void join_triangles(void);
extern void edge_flip(void);
extern void fill_mesh(void);
extern void bevel_menu();
void edge_rotate_selected(int dir);
int EdgeSlide(short immediate, float imperc);
void EdgeLoopDelete(void);
 
#endif

