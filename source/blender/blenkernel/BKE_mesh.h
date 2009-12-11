/**
 * blenlib/BKE_mesh.h (mar-2001 nzc)
 *	
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
#ifndef BKE_MESH_H
#define BKE_MESH_H

/***/

struct BoundBox;
struct DispList;
struct ListBase;
struct EditMesh;
struct MDeformVert;
struct Mesh;
struct MFace;
struct MEdge;
struct MVert;
struct MCol;
struct Object;
struct MTFace;
struct VecNor;
struct CustomData;

#ifdef __cplusplus
extern "C" {
#endif

struct EditMesh *BKE_mesh_get_editmesh(struct Mesh *me);
void BKE_mesh_end_editmesh(struct Mesh *me, struct EditMesh *em);

void unlink_mesh(struct Mesh *me);
void free_mesh(struct Mesh *me);
struct Mesh *add_mesh(char *name);
struct Mesh *copy_mesh(struct Mesh *me);
void mesh_update_customdata_pointers(struct Mesh *me);
void make_local_tface(struct Mesh *me);
void make_local_mesh(struct Mesh *me);
void boundbox_mesh(struct Mesh *me, float *loc, float *size);
void tex_space_mesh(struct Mesh *me);
float *get_mesh_orco_verts(struct Object *ob);
void transform_mesh_orco_verts(struct Mesh *me, float (*orco)[3], int totvert, int invert);
int test_index_face(struct MFace *mface, struct CustomData *mfdata, int mfindex, int nr);
struct Mesh *get_mesh(struct Object *ob);
void set_mesh(struct Object *ob, struct Mesh *me);
void mball_to_mesh(struct ListBase *lb, struct Mesh *me);
void nurbs_to_mesh(struct Object *ob);
void mesh_to_curve(struct Scene *scene, struct Object *ob);
void free_dverts(struct MDeformVert *dvert, int totvert);
void copy_dverts(struct MDeformVert *dst, struct MDeformVert *src, int totvert); /* __NLA */
void mesh_delete_material_index(struct Mesh *me, int index);
void mesh_set_smooth_flag(struct Object *meshOb, int enableSmooth);

struct BoundBox *mesh_get_bb(struct Object *ob);
void mesh_get_texspace(struct Mesh *me, float *loc_r, float *rot_r, float *size_r);

/* if old, it converts mface->edcode to edge drawflags */
void make_edges(struct Mesh *me, int old);
void mesh_strip_loose_faces(struct Mesh *me);

	/* Calculate vertex and face normals, face normals are returned in *faceNors_r if non-NULL
	 * and vertex normals are stored in actual mverts.
	 */
void mesh_calc_normals(struct MVert *mverts, int numVerts, struct MFace *mfaces, int numFaces, float **faceNors_r);

	/* Return a newly MEM_malloc'd array of all the mesh vertex locations
	 * (_numVerts_r_ may be NULL) */
float (*mesh_getVertexCos(struct Mesh *me, int *numVerts_r))[3];

/* map from uv vertex to face (for select linked, stitch, uv suburf) */

/* UvVertMap */

#define STD_UV_CONNECT_LIMIT	0.0001f

typedef struct UvVertMap {
	struct UvMapVert **vert;
	struct UvMapVert *buf;
} UvVertMap;

typedef struct UvMapVert {
	struct UvMapVert *next;
	unsigned int f;
	unsigned char tfindex, separate, flag;
} UvMapVert;

UvVertMap *make_uv_vert_map(struct MFace *mface, struct MTFace *tface, unsigned int totface, unsigned int totvert, int selected, float *limit);
UvMapVert *get_uv_map_vert(UvVertMap *vmap, unsigned int v);
void free_uv_vert_map(UvVertMap *vmap);

/* Connectivity data */
typedef struct IndexNode {
	struct IndexNode *next, *prev;
	int index;
} IndexNode;
void create_vert_face_map(ListBase **map, IndexNode **mem, const struct MFace *mface,
			  const int totvert, const int totface);
void create_vert_edge_map(ListBase **map, IndexNode **mem, const struct MEdge *medge,
			  const int totvert, const int totedge);

/* Partial Mesh Visibility */
struct PartialVisibility *mesh_pmv_copy(struct PartialVisibility *);
void mesh_pmv_free(struct PartialVisibility *);
void mesh_pmv_revert(struct Object *ob, struct Mesh *me);
void mesh_pmv_off(struct Object *ob, struct Mesh *me);

/* functions for making menu's from customdata layers */
int mesh_layers_menu_charlen(struct CustomData *data, int type); /* use this to work out how many chars to allocate */
void mesh_layers_menu_concat(struct CustomData *data, int type, char *str);
int mesh_layers_menu(struct CustomData *data, int type);



#ifdef __cplusplus
}
#endif

#endif

