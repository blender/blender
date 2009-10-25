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
struct wmOperator;
struct wmWindowManager;
struct wmKeyConfig;
struct ReportList;
struct EditSelection;
struct ViewContext;
struct bDeformGroup;
struct MDeformWeight;
struct MDeformVert;
struct Scene;
struct Mesh;
struct MCol;
struct UvVertMap;
struct UvMapVert;
struct CustomData;
struct Material;
struct Object;

#define EM_FGON_DRAW	1 // face flag
#define EM_FGON			2 // edge and face flag both

/* editbutflag */
#define B_CLOCKWISE			1
#define B_KEEPORIG			2
#define B_BEAUTY			4
#define B_SMOOTH			8
#define B_BEAUTY_SHORT  	0x10
#define B_AUTOFGON			0x20
#define B_KNIFE				0x80
#define B_PERCENTSUBD		0x40
//#define B_MESH_X_MIRROR		0x100 // deprecated, use mesh
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000
#define B_FRACTAL			0x2000
#define B_SPHERE			0x4000

/* meshtools.c */

intptr_t	mesh_octree_table(struct Object *ob, struct EditMesh *em, float *co, char mode);
struct EditVert   *editmesh_get_x_mirror_vert(struct Object *ob, struct EditMesh *em, float *co);
int			mesh_get_x_mirror_vert(struct Object *ob, int index);
int			*mesh_get_x_mirror_faces(struct Object *ob, struct EditMesh *em);

int			join_mesh_exec(struct bContext *C, struct wmOperator *op);

/* mesh_ops.c */
void		ED_operatortypes_mesh(void);
void		ED_operatormacros_mesh(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);


/* editmesh.c */

void		ED_spacetypes_init(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);

void		make_editMesh(struct Scene *scene, struct Object *ob);
void		load_editMesh(struct Scene *scene, struct Object *ob);
void		remake_editMesh(struct Scene *scene, struct Object *ob);
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
void             EM_set_actFace(struct EditMesh *em, struct EditFace *efa);
float            EM_face_area(struct EditFace *efa);

void		EM_select_edge(struct EditEdge *eed, int sel);
void		EM_select_face(struct EditFace *efa, int sel);
void		EM_select_face_fgon(struct EditMesh *em, struct EditFace *efa, int val);
void		EM_select_swap(struct EditMesh *em);
void		EM_toggle_select_all(struct EditMesh *em);
void		EM_select_all(struct EditMesh *em);
void		EM_selectmode_flush(struct EditMesh *em);
void		EM_deselect_flush(struct EditMesh *em);
void		EM_selectmode_set(struct EditMesh *em);
void		EM_select_flush(struct EditMesh *em);
void		EM_convertsel(struct EditMesh *em, short oldmode, short selectmode);
void		EM_validate_selections(struct EditMesh *em);

			/* exported to transform */
int			EM_get_actSelection(struct EditMesh *em, struct EditSelection *ese);
void		EM_editselection_normal(float *normal, struct EditSelection *ese);
void		EM_editselection_plane(float *plane, struct EditSelection *ese);
void		EM_editselection_center(float *center, struct EditSelection *ese);			

struct UvVertMap *EM_make_uv_vert_map(struct EditMesh *em, int selected, int do_face_idx_array, float *limit);
struct UvMapVert *EM_get_uv_map_vert(struct UvVertMap *vmap, unsigned int v);
void              EM_free_uv_vert_map(struct UvVertMap *vmap);

void		EM_add_data_layer(struct EditMesh *em, struct CustomData *data, int type);
void		EM_free_data_layer(struct EditMesh *em, struct CustomData *data, int type);

/* editmesh_mods.c */
extern unsigned int em_vertoffs, em_solidoffs, em_wireoffs;

void		mouse_mesh(struct bContext *C, short mval[2], short extend);
int			EM_check_backbuf(unsigned int index);
int			EM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EM_free_backbuf(void);
int			EM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

void		EM_hide_mesh(struct EditMesh *em, int swap);
void		EM_reveal_mesh(struct EditMesh *em);

void		EM_select_by_material(struct EditMesh *em, int index);
void		EM_deselect_by_material(struct EditMesh *em, int index); 

void		EM_automerge(struct Scene *scene, struct Object *obedit, int update);

/* editface.c */
struct MTFace	*EM_get_active_mtface(struct EditMesh *em, struct EditFace **act_efa, struct MCol **mcol, int sloppy);

/* object_vgroup.c */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

struct bDeformGroup		*ED_vgroup_add(struct Object *ob);
struct bDeformGroup		*ED_vgroup_add_name(struct Object *ob, char *name);
void					ED_vgroup_select_by_name(struct Object *ob, char *name);
void					ED_vgroup_data_create(struct ID *id);

void		ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum,  float weight, int assignmode);
void		ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum);
float		ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum);

struct MDeformWeight	*ED_vgroup_weight_verify(struct MDeformVert *dv, int defgroup);
struct MDeformWeight	*ED_vgroup_weight_get(struct MDeformVert *dv, int defgroup);

/*needed by edge slide*/
struct EditVert *editedge_getOtherVert(struct EditEdge *eed, struct EditVert *eve);
struct EditVert *editedge_getSharedVert(struct EditEdge *eed, struct EditEdge *eed2);
int editedge_containsVert(struct EditEdge *eed, struct EditVert *eve);
int editface_containsVert(struct EditFace *efa, struct EditVert *eve);
int editface_containsEdge(struct EditFace *efa, struct EditEdge *eed);
short sharesFace(struct EditMesh *em, struct EditEdge *e1, struct EditEdge *e2);

/* mesh_data.c */

void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces);
void ED_mesh_transform(struct Mesh *me, float *mat);
void ED_mesh_calc_normals(struct Mesh *me);
void ED_mesh_material_add(struct Mesh *me, struct Material *ma);
void ED_mesh_update(struct Mesh *mesh, struct bContext *C, int calc_edges);

int ED_mesh_uv_texture_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me);
int ED_mesh_uv_texture_remove(struct bContext *C, struct Object *ob, struct Mesh *me);
int ED_mesh_color_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me);
int ED_mesh_color_remove(struct bContext *C, struct Object *ob, struct Mesh *me);

#endif /* ED_MESH_H */

