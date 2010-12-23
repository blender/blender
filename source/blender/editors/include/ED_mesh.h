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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
struct BMEditMesh;
struct BMEditSelection;
struct BMesh;
struct BMVert;
struct BMEdge;
struct BMFace;
struct UvVertMap;
struct UvMapVert;
struct Material;
struct Object;
struct rcti;
struct wmOperator;

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
//#define B_MESH_X_MIRROR		0x100 // deprecated, use mesh
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000
#define B_FRACTAL		0x2000
#define B_SPHERE		0x4000

float *bm_get_cd_float(struct CustomData *cdata, void *data, int type);

/* bmeshutils.c */

/*x-mirror editing api.  usage:
  
  EDBM_CacheMirrorVerts(em);
  ...
  ...
  BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
     mirrorv = EDBM_GetMirrorVert(em, v);
  }
  ...
  ...
  EDBM_EndMirrorCache(em);
 
  note: why do we only allow x axis mirror editing?
  */
void EDBM_CacheMirrorVerts(struct BMEditMesh *em);

/*retrieves mirrored cache vert, or NULL if there isn't one.
  note: calling this without ensuring the mirror cache state
  is bad.*/
struct BMVert *EDBM_GetMirrorVert(struct BMEditMesh *em, struct BMVert *v);
void EDBM_EndMirrorCache(struct BMEditMesh *em);

void EDBM_RecalcNormals(struct BMEditMesh *em);

void EDBM_MakeEditBMesh(struct ToolSettings *ts, struct Scene *scene, struct Object *ob);
void EDBM_FreeEditBMesh(struct BMEditMesh *tm);
void EDBM_LoadEditBMesh(struct Scene *scene, struct Object *ob);

void EDBM_init_index_arrays(struct BMEditMesh *em, int forvert, int foredge, int forface);
void EDBM_free_index_arrays(struct BMEditMesh *em);
struct BMVert *EDBM_get_vert_for_index(struct BMEditMesh *em, int index);
struct BMEdge *EDBM_get_edge_for_index(struct BMEditMesh *em, int index);
struct BMFace *EDBM_get_face_for_index(struct BMEditMesh *em, int index);
struct BMFace *EDBM_get_actFace(struct BMEditMesh *em, int sloppy);

int EDBM_CallAndSelectOpf(struct BMEditMesh *em, struct wmOperator *op, 
						  char *selectslot, char *fmt, ...);

/*flushes based on the current select mode.  if in vertex select mode,
  verts select/deselect edges and faces, if in edge select mode,
  edges select/deselect faces and vertices, and in face select mode faces select/deselect
  edges and vertices.*/
void EDBM_selectmode_flush(struct BMEditMesh *em);

int EDBM_get_actSelection(struct BMEditMesh *em, struct BMEditSelection *ese);

/*exactly the same as EDBM_selectmode_flush, but you pass in the selectmode
  instead of using the current one*/
void EDBM_select_flush(struct BMEditMesh *em, int selectmode);
void EDBM_deselect_flush(struct BMEditMesh *em);

void EDBM_selectmode_set(struct BMEditMesh *em);
void EDBM_convertsel(struct BMEditMesh *em, short oldmode, short selectmode);

void EDBM_editselection_center(struct BMEditMesh *em, float *center, struct BMEditSelection *ese);
void EDBM_editselection_plane(struct BMEditMesh *em, float *plane, struct BMEditSelection *ese);
void EDBM_editselection_normal(float *normal, struct BMEditSelection *ese);
int EDBM_vertColorCheck(struct BMEditMesh *em);

void EDBM_pin_mesh(struct BMEditMesh *em, int swap);
void EDBM_unpin_mesh(struct BMEditMesh *em, int swap);

void EDBM_hide_mesh(struct BMEditMesh *em, int swap);
void EDBM_reveal_mesh(struct BMEditMesh *em);

int			EDBM_check_backbuf(unsigned int index);
int			EDBM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EDBM_free_backbuf(void);
int			EDBM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EDBM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

void EDBM_select_swap(struct BMEditMesh *em); /* exported for UV */
void EDBM_set_actFace(struct BMEditMesh *em, struct BMFace *efa);

int EDBM_texFaceCheck(struct BMEditMesh *em);
struct MTexPoly *EDBM_get_active_mtexpoly(struct BMEditMesh *em, struct BMFace **act_efa, int sloppy);

void EDBM_free_uv_vert_map(struct UvVertMap *vmap);
struct UvMapVert *EDBM_get_uv_map_vert(struct UvVertMap *vmap, unsigned int v);
struct UvVertMap *EDBM_make_uv_vert_map(struct BMEditMesh *em, int selected, int do_face_idx_array, float *limit);

void EDBM_toggle_select_all(struct BMEditMesh *em);
void EDBM_set_flag_all(struct BMEditMesh *em, int flag);

/* meshtools.c */

/*these functinos are all deprecated, use stuff in editbmesh_bvh.h instead*/
intptr_t	mesh_octree_table(struct Object *ob, struct BMEditMesh *em, float *co, char mode);
struct BMVert   *editmesh_get_x_mirror_vert(struct Object *ob, struct BMEditMesh *em, float *co);
int			mesh_get_x_mirror_vert(struct Object *ob, int index);
int			*mesh_get_x_mirror_faces(struct Object *ob, struct BMEditMesh *em);

int			join_mesh_exec(struct bContext *C, struct wmOperator *op);
int			join_mesh_shapes_exec(struct bContext *C, struct wmOperator *op);

/* mesh_ops.c */
void		ED_operatortypes_mesh(void);
void		ED_operatormacros_mesh(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);


/* editmesh.c */

void		ED_spacetypes_init(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);
/* bmesh_mods.c */
extern unsigned int bm_vertoffs, bm_solidoffs, bm_wireoffs;


/* object_vgroup.c */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

struct bDeformGroup		*ED_vgroup_add(struct Object *ob);
struct bDeformGroup		*ED_vgroup_add_name(struct Object *ob, char *name);
void					ED_vgroup_select_by_name(struct Object *ob, char *name);
void					ED_vgroup_data_create(struct ID *id);
int						ED_vgroup_give_array(struct ID *id, struct MDeformVert **dvert_arr, int *dvert_tot);
int						ED_vgroup_copy_array(struct Object *ob, struct Object *ob_from);
void					ED_vgroup_mirror(struct Object *ob, int mirror_weights, int flip_vgroups);

void		ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum,  float weight, int assignmode);
void		ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum);
float		ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum);

/**
 * findnearestvert
 * 
 * dist (in/out): minimal distance to the nearest and at the end, actual distance
 * sel: selection bias
 * 		if SELECT, selected vertice are given a 5 pixel bias to make them farter than unselect verts
 * 		if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */

struct BMVert *EDBM_findnearestvert(struct ViewContext *vc, int *dist, short sel, short strict);
struct BMEdge *EDBM_findnearestedge(struct ViewContext *vc, int *dist);
struct BMFace *EDBM_findnearestface(struct ViewContext *vc, int *dist);

/* mesh_data.c */
// void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces);
void ED_mesh_faces_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count);
void ED_mesh_vertices_add(struct Mesh *mesh, struct ReportList *reports, int count);

void ED_mesh_transform(struct Mesh *me, float *mat);
void ED_mesh_calc_normals(struct Mesh *me);
void ED_mesh_material_link(struct Mesh *me, struct Material *ma);
void ED_mesh_update(struct Mesh *mesh, struct bContext *C, int calc_edges);

int ED_mesh_uv_texture_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me, const char *name, int active_set);
int ED_mesh_uv_texture_remove(struct bContext *C, struct Object *ob, struct Mesh *me);
int ED_mesh_color_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me, const char *name, int active_set);
int ED_mesh_color_remove(struct bContext *C, struct Object *ob, struct Mesh *me);

#include "../mesh/editbmesh_bvh.h"

#endif /* ED_MESH_H */
