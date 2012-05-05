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
struct BMEditMesh;
struct BMesh;
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

struct BMesh *BKE_mesh_to_bmesh(struct Mesh *me, struct Object *ob);

/*
 * this function recreates a tessellation.
 * returns number of tessellation faces.
 *
 * use_poly_origindex sets whether or not the tessellation faces' origindex
 * layer should point to original poly indices or real poly indices.
 *
 * use_face_origindex sets the tessellation faces' origindex layer
 * to point to the tessellation faces themselves, not the polys.
 *
 * if both of the above are 0, it'll use the indices of the mpolys of the MPoly
 * data in pdata, and ignore the origindex layer altogether.
 */
int mesh_recalcTessellation(struct CustomData *fdata, struct CustomData *ldata, struct CustomData *pdata,
                           struct MVert *mvert,
                           int totface, int totloop, int totpoly,
                           const int do_face_normals);

/* for forwards compat only quad->tri polys to mface, skip ngons.
 */
int mesh_mpoly_to_mface(struct CustomData *fdata, struct CustomData *ldata,
	struct CustomData *pdata, int totface, int totloop, int totpoly);

/*calculates a face normal.*/
void mesh_calc_poly_normal(struct MPoly *mpoly, struct MLoop *loopstart, 
                           struct MVert *mvarray, float no[3]);

void mesh_calc_poly_normal_coords(struct MPoly *mpoly, struct MLoop *loopstart,
                                  const float (*vertex_coords)[3], float no[3]);

void mesh_calc_poly_center(struct MPoly *mpoly, struct MLoop *loopstart,
                           struct MVert *mvarray, float cent[3]);

float mesh_calc_poly_area(struct MPoly *mpoly, struct MLoop *loopstart,
                          struct MVert *mvarray, float polynormal[3]);

/* Find the index of the loop in 'poly' which references vertex,
 * returns -1 if not found */
int poly_find_loop_from_vert(const struct MPoly *poly,
							 const struct MLoop *loopstart,
							 unsigned vert);

/* Fill 'adj_r' with the loop indices in 'poly' adjacent to the
 * vertex. Returns the index of the loop matching vertex, or -1 if the
 * vertex is not in 'poly' */
int poly_get_adj_loops_from_vert(unsigned adj_r[3], const struct MPoly *poly,
								 const struct MLoop *mloop, unsigned vert);

/* update the hide flag for edges and polys from the corresponding
 * flag in verts */
void mesh_flush_hidden_from_verts(const struct MVert *mvert,
								  const struct MLoop *mloop,
								  struct MEdge *medge, int totedge,
								  struct MPoly *mpoly, int totpoly);

void unlink_mesh(struct Mesh *me);
void BKE_mesh_free(struct Mesh *me, int unlink);
struct Mesh *add_mesh(const char *name);
struct Mesh *BKE_mesh_copy(struct Mesh *me);
void mesh_update_customdata_pointers(struct Mesh *me, const short do_ensure_tess_cd);

void BKE_mesh_make_local(struct Mesh *me);
void boundbox_mesh(struct Mesh *me, float *loc, float *size);
void tex_space_mesh(struct Mesh *me);
float *get_mesh_orco_verts(struct Object *ob);
void transform_mesh_orco_verts(struct Mesh *me, float (*orco)[3], int totvert, int invert);
int test_index_face(struct MFace *mface, struct CustomData *mfdata, int mfindex, int nr);
struct Mesh *get_mesh(struct Object *ob);
void set_mesh(struct Object *ob, struct Mesh *me);
void mball_to_mesh(struct ListBase *lb, struct Mesh *me);
int nurbs_to_mdata(struct Object *ob, struct MVert **allvert, int *totvert,
	struct MEdge **alledge, int *totedge, struct MLoop **allloop, struct MPoly **allpoly,
	int *totloop, int *totpoly);
int nurbs_to_mdata_customdb(struct Object *ob, struct ListBase *dispbase, struct MVert **allvert, int *_totvert,
	struct MEdge **alledge, int *_totedge, struct MLoop **allloop, struct MPoly **allpoly,
	int *_totloop, int *_totpoly);
void nurbs_to_mesh(struct Object *ob);
void mesh_to_curve(struct Scene *scene, struct Object *ob);
void free_dverts(struct MDeformVert *dvert, int totvert);
void copy_dverts(struct MDeformVert *dst, struct MDeformVert *src, int totvert); /* __NLA */
void mesh_delete_material_index(struct Mesh *me, short index);
void mesh_set_smooth_flag(struct Object *meshOb, int enableSmooth);
void BKE_mesh_convert_mfaces_to_mpolys(struct Mesh *mesh);
void mesh_calc_normals_tessface(struct MVert *mverts, int numVerts, struct MFace *mfaces, int numFaces, float (*faceNors_r)[3]);

/* used for unit testing; compares two meshes, checking only
 * differences we care about.  should be usable with leaf's
 * testing framework I get RNA work done, will use hackish
 * testing code for now.*/
const char *mesh_cmp(struct Mesh *me1, struct Mesh *me2, float thresh);

struct BoundBox *mesh_get_bb(struct Object *ob);
void mesh_get_texspace(struct Mesh *me, float r_loc[3], float r_rot[3], float r_size[3]);

/* if old, it converts mface->edcode to edge drawflags */
void make_edges(struct Mesh *me, int old);

void mesh_strip_loose_faces(struct Mesh *me); /* Needed for compatibility (some old read code). */
void mesh_strip_loose_polysloops(struct Mesh *me);
void mesh_strip_loose_edges(struct Mesh *me);

	/* Calculate vertex and face normals, face normals are returned in *faceNors_r if non-NULL
	 * and vertex normals are stored in actual mverts.
	 */
void mesh_calc_normals_mapping(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys, int numLoops, int numPolys, float (*polyNors_r)[3],
        struct MFace *mfaces, int numFaces, int *origIndexFace, float (*faceNors_r)[3]);
	/* extended version of 'mesh_calc_normals' with option not to calc vertex normals */
void mesh_calc_normals_mapping_ex(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys, int numLoops, int numPolys, float (*polyNors_r)[3],
        struct MFace *mfaces, int numFaces, int *origIndexFace, float (*faceNors_r)[3],
        const short only_face_normals);

void mesh_calc_normals(
        struct MVert *mverts, int numVerts,
        struct MLoop *mloop, struct MPoly *mpolys,
        int numLoops, int numPolys, float (*polyNors_r)[3]);

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

/* UvElement stores per uv information so that we can quickly access information for a uv.
 * it is actually an improved UvMapVert, including an island and a direct pointer to the face
 * to avoid initializing face arrays */
typedef struct UvElement {
	/* Next UvElement corresponding to same vertex */
	struct UvElement *next;
	/* Face the element belongs to */
	struct BMFace *face;
	/* Index in the editFace of the uv */
	struct BMLoop *l;
	/* index in loop. */
	unsigned short tfindex;
	/* Whether this element is the first of coincident elements */
	unsigned char separate;
	/* general use flag */
	unsigned char flag;
	/* If generating element map with island sorting, this stores the island index */
	unsigned short island;
} UvElement;


/* UvElementMap is a container for UvElements of a mesh. It stores some UvElements belonging to the
 * same uv island in sequence and the number of uvs per island so it is possible to access all uvs
 * belonging to an island directly by iterating through the buffer.
 */
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

/* invalid island index is max short. If any one has the patience
 * to make that many islands, he can bite me :p */
#define INVALID_ISLAND 0xFFFF

UvVertMap *make_uv_vert_map(struct MPoly *mpoly, struct MLoop *mloop, struct MLoopUV *mloopuv, unsigned int totpoly, unsigned int totvert, int selected, float *limit);
UvMapVert *get_uv_map_vert(UvVertMap *vmap, unsigned int v);
void free_uv_vert_map(UvVertMap *vmap);

/* Connectivity data */
typedef struct MeshElemMap {
	int *indices;
	int count;
} MeshElemMap;
	
typedef struct IndexNode {
	struct IndexNode *next, *prev;
	int index;
} IndexNode;

void create_vert_poly_map(MeshElemMap **map, int **mem,
                          const struct MPoly *mface, const struct MLoop *mloop,
                          int totvert, int totface, int totloop);
	
void create_vert_edge_map(struct ListBase **map, IndexNode **mem, const struct MEdge *medge,
                          const int totvert, const int totedge);

/* vertex level transformations & checks (no derived mesh) */

int minmax_mesh(struct Mesh *me, float min[3], float max[3]);
int mesh_center_median(struct Mesh *me, float cent[3]);
int mesh_center_bounds(struct Mesh *me, float cent[3]);
void mesh_translate(struct Mesh *me, float offset[3], int do_keys);

/* mesh_validate.c */
/* XXX Loop v/e are unsigned, so using max uint_32 value as invalid marker... */
#define INVALID_LOOP_EDGE_MARKER 4294967295u
int BKE_mesh_validate_arrays(
        struct Mesh *me,
        struct MVert *mverts, unsigned int totvert,
        struct MEdge *medges, unsigned int totedge,
        struct MLoop *mloops, unsigned int totloop,
        struct MPoly *mpolys, unsigned int totpoly,
        struct MDeformVert *dverts, /* assume totvert length */
        const short do_verbose, const short do_fixes);
int BKE_mesh_validate(struct Mesh *me, int do_verbose);
int BKE_mesh_validate_dm(struct DerivedMesh *dm);

void BKE_mesh_calc_edges(struct Mesh *mesh, int update);

void BKE_mesh_ensure_navmesh(struct Mesh *me);

void BKE_mesh_tessface_calc(struct Mesh *mesh);
void BKE_mesh_tessface_ensure(struct Mesh *mesh);
void BKE_mesh_tessface_clear(struct Mesh *mesh);

/* Convert a triangle or quadrangle of loop/poly data to tessface data */
void mesh_loops_to_mface_corners(struct CustomData *fdata, struct CustomData *ldata,
                                 struct CustomData *pdata, int lindex[4], int findex,
                                 const int polyindex, const int mf_len,
                                 const int numTex, const int numCol, const int hasPCol, const int hasOrigSpace);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_H__ */
