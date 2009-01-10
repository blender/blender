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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_MESH_H
#define ED_MESH_H

struct ID;
struct View3D;
struct ARegion;
struct EditMesh;
struct EditVert;
struct EditEdge;
struct EditFace;
struct bContext;
struct wmWindowManager;
struct EditSelection;
struct ViewContext;
struct bDeformGroup;
struct MDeformWeight;
struct MDeformVert;

// edge and face flag both
#define EM_FGON		2
// face flag
#define EM_FGON_DRAW	1

/* editbutflag */
#define B_CLOCKWISE		1
#define B_KEEPORIG		2
#define B_BEAUTY		4
#define B_SMOOTH		8
#define B_BEAUTY_SHORT  	16
#define B_AUTOFGON		32
#define B_KNIFE			0x80
#define B_PERCENTSUBD		0x40
#define B_MESH_X_MIRROR		0x100
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000


/* meshtools.c */

intptr_t	mesh_octree_table(Object *ob, struct EditMesh *em, float *co, char mode);
struct EditVert   *editmesh_get_x_mirror_vert(Object *ob, struct EditMesh *em, float *co);
int			mesh_get_x_mirror_vert(Object *ob, int index);

/* mesh_ops.c */
void		ED_operatortypes_mesh(void);
void		ED_keymap_mesh(struct wmWindowManager *wm);


/* editmesh.c */
void		ED_spacetypes_init(void);
void		ED_keymap_mesh(struct wmWindowManager *wm);

void		make_editMesh(Scene *scene, Object *ob);
void		load_editMesh(Scene *scene, Object *ob);
void		remake_editMesh(Scene *scene, Object *ob);
void		free_editMesh(struct EditMesh *em);

void		recalc_editnormals(struct EditMesh *em);

void		EM_init_index_arrays(struct EditMesh *em, int forVert, int forEdge, int forFace);
void		EM_free_index_arrays(void);
struct EditVert	*EM_get_vert_for_index(int index);
struct EditEdge	*EM_get_edge_for_index(int index);
struct EditFace	*EM_get_face_for_index(int index);
int			EM_texFaceCheck(struct EditMesh *em);
int			EM_vertColorCheck(struct EditMesh *em);

void		undo_push_mesh(struct bContext *C, char *name);


/* editmesh_lib.c */

struct EditFace	*EM_get_actFace(struct EditMesh *em, int sloppy);

void		EM_select_edge(struct EditEdge *eed, int sel);
void		EM_select_face_fgon(struct EditMesh *em, struct EditFace *efa, int val);
void		EM_selectmode_flush(struct EditMesh *em);
void		EM_deselect_flush(struct EditMesh *em);
void		EM_selectmode_set(struct EditMesh *em);
void		EM_convertsel(struct EditMesh *em, short oldmode, short selectmode);
			
			/* exported to transform */
int			EM_get_actSelection(struct EditMesh *em, struct EditSelection *ese);
void		EM_editselection_normal(float *normal, struct EditSelection *ese);
void		EM_editselection_plane(float *plane, struct EditSelection *ese);


/* editmesh_mods.c */
extern unsigned int em_vertoffs, em_solidoffs, em_wireoffs;

void		mouse_mesh(struct bContext *C, short mval[2], short extend);
int			EM_check_backbuf(unsigned int index);
int			EM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EM_free_backbuf(void);
int			EM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

/* editdeform.c XXX rename functions? */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

void		add_defgroup (Object *ob);
void		create_dverts(struct ID *id);
float		get_vert_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum);
void		remove_vert_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum);
void		remove_verts_defgroup (Object *obedit, int allverts);
void		vertexgroup_select_by_name(Object *ob, char *name);
void		add_vert_to_defgroup (Object *ob, struct bDeformGroup *dg, int vertnum, 
                           float weight, int assignmode);

struct bDeformGroup		*add_defgroup_name (Object *ob, char *name);
struct MDeformWeight	*verify_defweight (struct MDeformVert *dv, int defgroup);
struct MDeformWeight	*get_defweight (struct MDeformVert *dv, int defgroup);


#endif /* ED_MESH_H */

