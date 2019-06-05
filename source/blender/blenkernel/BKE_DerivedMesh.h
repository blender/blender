/*
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
 */

#ifndef __BKE_DERIVEDMESH_H__
#define __BKE_DERIVEDMESH_H__

/** \file
 * \ingroup bke
 *
 * Basic design of the DerivedMesh system:
 *
 * DerivedMesh is a common set of interfaces for mesh systems.
 *
 * There are three main mesh data structures in Blender:
 * #Mesh, #CDDerivedMesh and #BMesh.
 *
 * These, and a few others, all implement DerivedMesh interfaces,
 * which contains unified drawing interfaces, a few utility interfaces,
 * and a bunch of read-only interfaces intended mostly for conversion from
 * one format to another.
 *
 * All Mesh structures in blender make use of CustomData, which is used to store
 * per-element attributes and interpolate them (e.g. uvs, vcols, vgroups, etc).
 *
 * Mesh is the "serialized" structure, used for storing object-mode mesh data
 * and also for saving stuff to disk.  It's interfaces are also what DerivedMesh
 * uses to communicate with.
 *
 * CDDM is a little mesh library, that uses Mesh data structures in the backend.
 * It's mostly used for modifiers, and has the advantages of not taking much
 * resources.
 *
 * BMesh is a full-on brep, used for editmode, some modifiers, etc.  It's much
 * more capable (if memory-intensive) then CDDM.
 *
 * DerivedMesh is somewhat hackish.  Many places assumes that a DerivedMesh is
 * a CDDM (most of the time by simply copying it and converting it to one).
 * CDDM is the original structure for modifiers, but has since been superseded
 * by BMesh, at least for the foreseeable future.
 */

/*
 * Note: This structure is read-only, for all practical purposes.
 *       At some point in the future, we may want to consider
 *       creating a replacement structure that implements a proper
 *       abstract mesh kernel interface.  Or, we can leave this
 *       as it is and stick with using BMesh and CDDM.
 */

#include "DNA_defs.h"
#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_compiler_attrs.h"

#include "BKE_customdata.h"
#include "BKE_bvhutils.h"

struct BMEditMesh;
struct CCGElem;
struct CCGKey;
struct CustomData_MeshMasks;
struct Depsgraph;
struct MEdge;
struct MFace;
struct MLoopNorSpaceArray;
struct MVert;
struct Mesh;
struct ModifierData;
struct Object;
struct PBVH;
struct Scene;

/*
 * Note: all mface interfaces now officially operate on tessellated data.
 *       Also, the mface origindex layer indexes mpolys, not mfaces.
 */

/* keep in sync with MFace/MPoly types */
typedef struct DMFlagMat {
  short mat_nr;
  char flag;
} DMFlagMat;

typedef enum DerivedMeshType {
  DM_TYPE_CDDM,
  DM_TYPE_CCGDM,
} DerivedMeshType;

typedef enum DMForeachFlag {
  DM_FOREACH_NOP = 0,
  DM_FOREACH_USE_NORMAL =
      (1 << 0), /* foreachMappedVert, foreachMappedLoop, foreachMappedFaceCenter */
} DMForeachFlag;

typedef enum DMDirtyFlag {
  /* dm has valid tessellated faces, but tessellated CDDATA need to be updated. */
  DM_DIRTY_TESS_CDLAYERS = 1 << 0,

  /* check this with modifier dependsOnNormals callback to see if normals need recalculation */
  DM_DIRTY_NORMALS = 1 << 1,
} DMDirtyFlag;

typedef struct DerivedMesh DerivedMesh;
struct DerivedMesh {
  /** Private DerivedMesh data, only for internal DerivedMesh use */
  CustomData vertData, edgeData, faceData, loopData, polyData;
  int numVertData, numEdgeData, numTessFaceData, numLoopData, numPolyData;
  int needsFree;    /* checked on ->release, is set to 0 for cached results */
  int deformedOnly; /* set by modifier stack if only deformed from original */
  BVHCache *bvhCache;
  DerivedMeshType type;
  DMDirtyFlag dirty;
  int totmat;            /* total materials. Will be valid only before object drawing. */
  struct Material **mat; /* material array. Will be valid only before object drawing */

  /**
   * \warning Typical access is done via #getLoopTriArray, #getNumLoopTri.
   */
  struct {
    /* WARNING! swapping between array (ready-to-be-used data) and array_wip
     * (where data is actually computed) shall always be protected by same
     * lock as one used for looptris computing. */
    struct MLoopTri *array, *array_wip;
    int num;
    int num_alloc;
  } looptris;

  /* use for converting to BMesh which doesn't store bevel weight and edge crease by default */
  char cd_flag;

  short tangent_mask; /* which tangent layers are calculated */

  /** Calculate vert and face normals */
  void (*calcNormals)(DerivedMesh *dm);

  /** Calculate loop (split) normals */
  void (*calcLoopNormals)(DerivedMesh *dm, const bool use_split_normals, const float split_angle);

  /** Calculate loop (split) normals, and returns split loop normal spacearr. */
  void (*calcLoopNormalsSpaceArray)(DerivedMesh *dm,
                                    const bool use_split_normals,
                                    const float split_angle,
                                    struct MLoopNorSpaceArray *r_lnors_spacearr);

  void (*calcLoopTangents)(DerivedMesh *dm,
                           bool calc_active_tangent,
                           const char (*tangent_names)[MAX_NAME],
                           int tangent_names_count);

  /** Recalculates mesh tessellation */
  void (*recalcTessellation)(DerivedMesh *dm);

  /** Loop tessellation cache (WARNING! Only call inside threading-protected code!) */
  void (*recalcLoopTri)(DerivedMesh *dm);
  /** accessor functions */
  const struct MLoopTri *(*getLoopTriArray)(DerivedMesh *dm);
  int (*getNumLoopTri)(DerivedMesh *dm);

  /* Misc. Queries */

  /* Also called in Editmode */
  int (*getNumVerts)(DerivedMesh *dm);
  int (*getNumEdges)(DerivedMesh *dm);
  int (*getNumTessFaces)(DerivedMesh *dm);
  int (*getNumLoops)(DerivedMesh *dm);
  int (*getNumPolys)(DerivedMesh *dm);

  /** Copy a single vert/edge/tessellated face from the derived mesh into
   * ``*r_{vert/edge/face}``. note that the current implementation
   * of this function can be quite slow, iterating over all
   * elements (editmesh)
   */
  void (*getVert)(DerivedMesh *dm, int index, struct MVert *r_vert);
  void (*getEdge)(DerivedMesh *dm, int index, struct MEdge *r_edge);
  void (*getTessFace)(DerivedMesh *dm, int index, struct MFace *r_face);

  /** Return a pointer to the entire array of verts/edges/face from the
   * derived mesh. if such an array does not exist yet, it will be created,
   * and freed on the next ->release(). consider using getVert/Edge/Face if
   * you are only interested in a few verts/edges/faces.
   */
  struct MVert *(*getVertArray)(DerivedMesh *dm);
  struct MEdge *(*getEdgeArray)(DerivedMesh *dm);
  struct MFace *(*getTessFaceArray)(DerivedMesh *dm);
  struct MLoop *(*getLoopArray)(DerivedMesh *dm);
  struct MPoly *(*getPolyArray)(DerivedMesh *dm);

  /** Copy all verts/edges/faces from the derived mesh into
   * *{vert/edge/face}_r (must point to a buffer large enough)
   */
  void (*copyVertArray)(DerivedMesh *dm, struct MVert *r_vert);
  void (*copyEdgeArray)(DerivedMesh *dm, struct MEdge *r_edge);
  void (*copyTessFaceArray)(DerivedMesh *dm, struct MFace *r_face);
  void (*copyLoopArray)(DerivedMesh *dm, struct MLoop *r_loop);
  void (*copyPolyArray)(DerivedMesh *dm, struct MPoly *r_poly);

  /** Return a copy of all verts/edges/faces from the derived mesh
   * it is the caller's responsibility to free the returned pointer
   */
  struct MVert *(*dupVertArray)(DerivedMesh *dm);
  struct MEdge *(*dupEdgeArray)(DerivedMesh *dm);
  struct MFace *(*dupTessFaceArray)(DerivedMesh *dm);
  struct MLoop *(*dupLoopArray)(DerivedMesh *dm);
  struct MPoly *(*dupPolyArray)(DerivedMesh *dm);

  /** Return a pointer to a single element of vert/edge/face custom data
   * from the derived mesh (this gives a pointer to the actual data, not
   * a copy)
   */
  void *(*getVertData)(DerivedMesh *dm, int index, int type);
  void *(*getEdgeData)(DerivedMesh *dm, int index, int type);
  void *(*getTessFaceData)(DerivedMesh *dm, int index, int type);
  void *(*getPolyData)(DerivedMesh *dm, int index, int type);

  /** Return a pointer to the entire array of vert/edge/face custom data
   * from the derived mesh (this gives a pointer to the actual data, not
   * a copy)
   */
  void *(*getVertDataArray)(DerivedMesh *dm, int type);
  void *(*getEdgeDataArray)(DerivedMesh *dm, int type);
  void *(*getTessFaceDataArray)(DerivedMesh *dm, int type);
  void *(*getLoopDataArray)(DerivedMesh *dm, int type);
  void *(*getPolyDataArray)(DerivedMesh *dm, int type);

  /** Retrieves the base CustomData structures for
   * verts/edges/tessfaces/loops/facdes*/
  CustomData *(*getVertDataLayout)(DerivedMesh *dm);
  CustomData *(*getEdgeDataLayout)(DerivedMesh *dm);
  CustomData *(*getTessFaceDataLayout)(DerivedMesh *dm);
  CustomData *(*getLoopDataLayout)(DerivedMesh *dm);
  CustomData *(*getPolyDataLayout)(DerivedMesh *dm);

  /** Copies all customdata for an element source into dst at index dest */
  void (*copyFromVertCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);
  void (*copyFromEdgeCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);
  void (*copyFromFaceCData)(DerivedMesh *dm, int source, CustomData *dst, int dest);

  /** Optional grid access for subsurf */
  int (*getNumGrids)(DerivedMesh *dm);
  int (*getGridSize)(DerivedMesh *dm);
  struct CCGElem **(*getGridData)(DerivedMesh *dm);
  int *(*getGridOffset)(DerivedMesh *dm);
  void (*getGridKey)(DerivedMesh *dm, struct CCGKey *key);
  DMFlagMat *(*getGridFlagMats)(DerivedMesh *dm);
  unsigned int **(*getGridHidden)(DerivedMesh *dm);

  /** Iterate over each mapped vertex in the derived mesh, calling the
   * given function with the original vert and the mapped vert's new
   * coordinate and normal. For historical reasons the normal can be
   * passed as a float or short array, only one should be non-NULL.
   */
  void (*foreachMappedVert)(DerivedMesh *dm,
                            void (*func)(void *userData,
                                         int index,
                                         const float co[3],
                                         const float no_f[3],
                                         const short no_s[3]),
                            void *userData,
                            DMForeachFlag flag);

  /** Iterate over each mapped edge in the derived mesh, calling the
   * given function with the original edge and the mapped edge's new
   * coordinates.
   */
  void (*foreachMappedEdge)(
      DerivedMesh *dm,
      void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
      void *userData);

  /** Iterate over each mapped loop in the derived mesh, calling the given function
   * with the original loop index and the mapped loops's new coordinate and normal.
   */
  void (*foreachMappedLoop)(DerivedMesh *dm,
                            void (*func)(void *userData,
                                         int vertex_index,
                                         int face_index,
                                         const float co[3],
                                         const float no[3]),
                            void *userData,
                            DMForeachFlag flag);

  /** Iterate over each mapped face in the derived mesh, calling the
   * given function with the original face and the mapped face's (or
   * faces') center and normal.
   */
  void (*foreachMappedFaceCenter)(
      DerivedMesh *dm,
      void (*func)(void *userData, int index, const float cent[3], const float no[3]),
      void *userData,
      DMForeachFlag flag);

  /** Iterate over all vertex points, calling DO_MINMAX with given args.
   *
   * Also called in Editmode
   */
  void (*getMinMax)(DerivedMesh *dm, float r_min[3], float r_max[3]);

  /** Direct Access Operations
   * - Can be undefined
   * - Must be defined for modifiers that only deform however */

  /** Get vertex location, undefined if index is not valid */
  void (*getVertCo)(DerivedMesh *dm, int index, float r_co[3]);

  /** Fill the array (of length .getNumVerts()) with all vertex locations */
  void (*getVertCos)(DerivedMesh *dm, float (*r_cos)[3]);

  /** Get smooth vertex normal, undefined if index is not valid */
  void (*getVertNo)(DerivedMesh *dm, int index, float r_no[3]);
  void (*getPolyNo)(DerivedMesh *dm, int index, float r_no[3]);

  /** Get a map of vertices to faces
   */
  const struct MeshElemMap *(*getPolyMap)(struct Object *ob, DerivedMesh *dm);

  /** Get the BVH used for paint modes
   */
  struct PBVH *(*getPBVH)(struct Object *ob, DerivedMesh *dm);

  /** Release reference to the DerivedMesh. This function decides internally
   * if the DerivedMesh will be freed, or cached for later use. */
  void (*release)(DerivedMesh *dm);
};

void DM_init_funcs(DerivedMesh *dm);

void DM_init(DerivedMesh *dm,
             DerivedMeshType type,
             int numVerts,
             int numEdges,
             int numFaces,
             int numLoops,
             int numPolys);

void DM_from_template_ex(DerivedMesh *dm,
                         DerivedMesh *source,
                         DerivedMeshType type,
                         int numVerts,
                         int numEdges,
                         int numTessFaces,
                         int numLoops,
                         int numPolys,
                         const struct CustomData_MeshMasks *mask);
void DM_from_template(DerivedMesh *dm,
                      DerivedMesh *source,
                      DerivedMeshType type,
                      int numVerts,
                      int numEdges,
                      int numFaces,
                      int numLoops,
                      int numPolys);

/** utility function to release a DerivedMesh's layers
 * returns 1 if DerivedMesh has to be released by the backend, 0 otherwise
 */
int DM_release(DerivedMesh *dm);

/** utility function to convert a DerivedMesh to a Mesh
 */
void DM_to_mesh(DerivedMesh *dm,
                struct Mesh *me,
                struct Object *ob,
                const struct CustomData_MeshMasks *mask,
                bool take_ownership);

void DM_set_only_copy(DerivedMesh *dm, const struct CustomData_MeshMasks *mask);

/* adds a vertex/edge/face custom data layer to a DerivedMesh, optionally
 * backed by an external data array
 * alloctype defines how the layer is allocated or copied, and how it is
 * freed, see BKE_customdata.h for the different options
 */
void DM_add_vert_layer(struct DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer);
void DM_add_edge_layer(struct DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer);
void DM_add_tessface_layer(struct DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer);
void DM_add_loop_layer(DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer);
void DM_add_poly_layer(struct DerivedMesh *dm, int type, eCDAllocType alloctype, void *layer);

/* custom data access functions
 * return pointer to data from first layer which matches type
 * if they return NULL for valid indices, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_edge_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_tessface_data(struct DerivedMesh *dm, int index, int type);
void *DM_get_poly_data(struct DerivedMesh *dm, int index, int type);

/* custom data layer access functions
 * return pointer to first data layer which matches type (a flat array)
 * if they return NULL, data doesn't exist
 * note these return pointers - any change modifies the internals of the mesh
 */
void *DM_get_vert_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_edge_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_tessface_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_poly_data_layer(struct DerivedMesh *dm, int type);
void *DM_get_loop_data_layer(struct DerivedMesh *dm, int type);

/* custom data copy functions
 * copy count elements from source_index in source to dest_index in dest
 * these copy all layers for which the CD_FLAG_NOCOPY flag is not set
 */
void DM_copy_vert_data(struct DerivedMesh *source,
                       struct DerivedMesh *dest,
                       int source_index,
                       int dest_index,
                       int count);

/*sets up mpolys for a DM based on face iterators in source*/
void DM_DupPolys(DerivedMesh *source, DerivedMesh *target);

void DM_ensure_normals(DerivedMesh *dm);

void DM_ensure_looptri_data(DerivedMesh *dm);

void DM_interp_vert_data(struct DerivedMesh *source,
                         struct DerivedMesh *dest,
                         int *src_indices,
                         float *weights,
                         int count,
                         int dest_index);

void mesh_get_mapped_verts_coords(struct Mesh *me_eval, float (*r_cos)[3], const int totcos);

DerivedMesh *mesh_create_derived_render(struct Depsgraph *depsgraph,
                                        struct Scene *scene,
                                        struct Object *ob,
                                        const struct CustomData_MeshMasks *dataMask);

/* same as above but wont use render settings */
DerivedMesh *mesh_create_derived(struct Mesh *me, float (*vertCos)[3]);

struct Mesh *editbmesh_get_eval_cage(struct Depsgraph *depsgraph,
                                     struct Scene *scene,
                                     struct Object *,
                                     struct BMEditMesh *em,
                                     const struct CustomData_MeshMasks *dataMask);
struct Mesh *editbmesh_get_eval_cage_from_orig(struct Depsgraph *depsgraph,
                                               struct Scene *scene,
                                               struct Object *object,
                                               const struct CustomData_MeshMasks *dataMask);
struct Mesh *editbmesh_get_eval_cage_and_final(struct Depsgraph *depsgraph,
                                               struct Scene *scene,
                                               struct Object *,
                                               struct BMEditMesh *em,
                                               const struct CustomData_MeshMasks *dataMask,
                                               struct Mesh **r_final);

float (*editbmesh_get_vertex_cos(struct BMEditMesh *em, int *r_numVerts))[3];
bool editbmesh_modifier_is_enabled(struct Scene *scene,
                                   struct ModifierData *md,
                                   bool has_prev_mesh);
void makeDerivedMesh(struct Depsgraph *depsgraph,
                     struct Scene *scene,
                     struct Object *ob,
                     struct BMEditMesh *em,
                     const struct CustomData_MeshMasks *dataMask);

void DM_calc_loop_tangents(DerivedMesh *dm,
                           bool calc_active_tangent,
                           const char (*tangent_names)[MAX_NAME],
                           int tangent_names_count);

/* debug only */
#ifndef NDEBUG
char *DM_debug_info(DerivedMesh *dm);
void DM_debug_print(DerivedMesh *dm);
void DM_debug_print_cdlayers(CustomData *cdata);

bool DM_is_valid(DerivedMesh *dm);
#endif

#endif /* __BKE_DERIVEDMESH_H__ */
