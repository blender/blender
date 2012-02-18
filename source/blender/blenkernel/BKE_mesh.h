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
 * Contributor(s): (mar-2001 nzc)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_MESH_H__
#define __BKE_MESH_H__

/** \file BKE_mesh.h
 *  \ingroup bke
 */

/***/

struct BoundBox;
struct DispList;
struct ListBase;
struct EditMesh;
struct Mesh;
struct MPoly;
struct MLoop;
struct MFace;
struct MEdge;
struct MVert;
struct MDeformVert;
struct MCol;
struct Object;
struct MTFace;
struct VecNor;
struct CustomData;
struct DerivedMesh;
struct Scene;
struct MLoopUV;
struct UvVertMap;
struct UvMapVert;
struct UvElementMap;
struct UvElement;
#ifdef __cplusplus
extern "C" {
#endif

struct EditMesh *BKE_mesh_get_editmesh(struct Mesh *me);
void BKE_mesh_end_editmesh(struct Mesh *me, struct EditMesh *em);

/* for forwards compat only quad->tri polys to mface, skip ngons.
 */
int mesh_mpoly_to_mface(struct CustomData *fdata, struct CustomData *ldata,
	struct CustomData *pdata, int totface, int totloop, int totpoly);

void unlink_mesh(struct Mesh *me);
void free_mesh(struct Mesh *me);
struct Mesh *add_mesh(const char *name);
struct Mesh *copy_mesh(struct Mesh *me);
void mesh_update_customdata_pointers(struct Mesh *me);
void make_local_mesh(struct Mesh *me);
void boundbox_mesh(struct Mesh *me, float *loc, float *size);
void tex_space_mesh(struct Mesh *me);
float *get_mesh_orco_verts(struct Object *ob);
void transform_mesh_orco_verts(struct Mesh *me, float (*orco)[3], int totvert, int invert);
int test_index_face(struct MFace *mface, struct CustomData *mfdata, int mfindex, int nr);
struct Mesh *get_mesh(struct Object *ob);
void set_mesh(struct Object *ob, struct Mesh *me);
void mball_to_mesh(struct ListBase *lb, struct Mesh *me);
int nurbs_to_mdata(struct Object *ob, struct MVert **allvert, int *_totvert,
	struct MEdge **alledge, int *_totedge, struct MFace **allface, int *_totface);
int nurbs_to_mdata_customdb(struct Object *ob, struct ListBase *dispbase,
	struct MVert **allvert, int *_totvert, struct MEdge **alledge, int *_totedge,
	struct MFace **allface, int *_totface);
void nurbs_to_mesh(struct Object *ob);
void mesh_to_curve(struct Scene *scene, struct Object *ob);
void free_dverts(struct MDeformVert *dvert, int totvert);
void copy_dverts(struct MDeformVert *dst, struct MDeformVert *src, int totvert); /* __NLA */
void mesh_delete_material_index(struct Mesh *me, short index);
void mesh_set_smooth_flag(struct Object *meshOb, int enableSmooth);

struct BoundBox *mesh_get_bb(struct Object *ob);
void mesh_get_texspace(struct Mesh *me, float *loc_r, float *rot_r, float *size_r);

/* if old, it converts mface->edcode to edge drawflags */
void make_edges(struct Mesh *me, int old);

void mesh_strip_loose_faces(struct Mesh *me);
void mesh_strip_loose_edges(struct Mesh *me);

	/* Calculate vertex and face normals, face normals are returned in *faceNors_r if non-NULL
	 * and vertex normals are stored in actual mverts.
	 */
void mesh_calc_normals(struct MVert *mverts, int numVerts, struct MFace *mfaces, int numFaces, float (*faceNors_r)[3]);

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

typedef struct UvElementMap {
	/* address UvElements by their vertex */
	struct UvElement **vert;
	/* UvElement Store */
	struct UvElement *buf;
	/* Total number of UVs in the layer. Useful to know */
	int totalUVs;
	/* Number of Islands in the mesh */
	int totalIslands;
	/* Stores the starting index in buf where each island begins */
	int *islandIndices;
} UvElementMap;

typedef struct UvElement {
	/* Next UvElement corresponding to same vertex */
	struct UvElement *next;
	/* Face the element belongs to */
	struct EditFace *face;
	/* Index in the editFace of the uv */
	unsigned char tfindex;
	/* Whether this element is the first of coincident elements */
	unsigned char separate;
	/* general use flag */
	unsigned char flag;
	/* If generating element map with island sorting, this stores the island index */
	unsigned short island;
} UvElement;

/* invalid island index is max short. If any one has the patience
 * to make that many islands, he can bite me :p */
#define INVALID_ISLAND 0xFFFF


UvVertMap *make_uv_vert_map(struct MFace *mface, struct MTFace *tface, unsigned int totface, unsigned int totvert, int selected, float *limit);
UvMapVert *get_uv_map_vert(UvVertMap *vmap, unsigned int v);
void free_uv_vert_map(UvVertMap *vmap);

/* Connectivity data */
typedef struct IndexNode {
	struct IndexNode *next, *prev;
	int index;
} IndexNode;
void create_vert_face_map(struct ListBase **map, IndexNode **mem, const struct MFace *mface,
                          const int totvert, const int totface);
void create_vert_edge_map(struct ListBase **map, IndexNode **mem, const struct MEdge *medge,
                          const int totvert, const int totedge);

/* functions for making menu's from customdata layers */
int mesh_layers_menu_charlen(struct CustomData *data, int type); /* use this to work out how many chars to allocate */
void mesh_layers_menu_concat(struct CustomData *data, int type, char *str);
int mesh_layers_menu(struct CustomData *data, int type);

/* vertex level transformations & checks (no derived mesh) */

int minmax_mesh(struct Mesh *me, float min[3], float max[3]);
int mesh_center_median(struct Mesh *me, float cent[3]);
int mesh_center_bounds(struct Mesh *me, float cent[3]);
void mesh_translate(struct Mesh *me, float offset[3], int do_keys);

/* mesh_validate.c */
int BKE_mesh_validate_arrays(
		struct Mesh *me,
        struct MVert *mverts, unsigned int totvert,
        struct MEdge *medges, unsigned int totedge,
        struct MFace *mfaces, unsigned int totface,
        struct MDeformVert *dverts, /* assume totvert length */
        const short do_verbose, const short do_fixes);
int BKE_mesh_validate(struct Mesh *me, int do_verbose);
int BKE_mesh_validate_dm(struct DerivedMesh *dm);

void BKE_mesh_calc_edges(struct Mesh *mesh, int update);

void BKE_mesh_ensure_navmesh(struct Mesh *me);

/*convert a triangle of loop facedata to mface facedata*/
void mesh_loops_to_mface_corners(struct CustomData *fdata, struct CustomData *ldata,
                                 struct CustomData *pdata, int lindex[4], int findex,
                                 const int polyindex, const int mf_len,
                                 const int numTex, const int numCol, const int hasWCol);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_H__ */
