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

struct View3D;
struct ARegion;
struct EditMesh;
struct bContext;

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


/* especially for derivedmesh drawing callbacks */
typedef struct ViewContext {
	Scene *scene;
	Object *obact;
	Object *obedit;
	struct ARegion *ar;
	struct View3D *v3d;
	struct EditMesh *em;
	short mval[2];
} ViewContext;

/* meshtools.c */

intptr_t	mesh_octree_table(Object *ob, EditMesh *em, float *co, char mode);

/* editmesh.c */
void		make_editMesh(Scene *scene, Object *ob);
void		load_editMesh(Scene *scene, Object *ob);
void		remake_editMesh(Scene *scene, Object *ob);
void		free_editMesh(EditMesh *em);


void		EM_init_index_arrays(struct EditMesh *em, int forVert, int forEdge, int forFace);
void		EM_free_index_arrays(void);
EditVert	*EM_get_vert_for_index(int index);
EditEdge	*EM_get_edge_for_index(int index);
EditFace	*EM_get_face_for_index(int index);
int			EM_texFaceCheck(struct EditMesh *em);
int			EM_vertColorCheck(struct EditMesh *em);
void		undo_push_mesh(char *name);


/* editmesh_lib.c */

EditFace	*EM_get_actFace(struct EditMesh *em, int sloppy);

void		EM_select_edge(EditEdge *eed, int sel);
void		EM_select_face_fgon(struct EditMesh *em, EditFace *efa, int val);
void		EM_selectmode_flush(struct EditMesh *em);
void		EM_deselect_flush(struct EditMesh *em);
			
			/* exported to transform */
int			EM_get_actSelection(EditMesh *em, EditSelection *ese);
void		EM_editselection_normal(float *normal, EditSelection *ese);
void		EM_editselection_plane(float *plane, EditSelection *ese);


/* editmesh_mods.c */
extern unsigned int em_vertoffs, em_solidoffs, em_wireoffs;

void		mouse_mesh(struct bContext *C, short mval[2], short extend);
int			EM_check_backbuf(unsigned int index);
int			EM_mask_init_backbuf_border(struct View3D *v3d, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EM_free_backbuf(void);
int			EM_init_backbuf_border(struct View3D *v3d, short xmin, short ymin, short xmax, short ymax);
int			EM_init_backbuf_circle(struct View3D *v3d, short xs, short ys, short rads);


#endif /* ED_MESH_H */

