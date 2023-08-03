/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"

struct BMesh;
struct BMeshCreateParams;
struct BMeshFromMeshParams;
struct BMeshToMeshParams;
struct BoundBox;
struct CustomData;
struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
struct LinkNode;
struct ListBase;
struct MDeformVert;
struct MDisps;
struct MFace;
struct MLoopTri;
struct Main;
struct MemArena;
struct Mesh;
struct Object;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: Move to `BKE_mesh_types.hh` when possible. */
typedef enum eMeshBatchDirtyMode {
  BKE_MESH_BATCH_DIRTY_ALL = 0,
  BKE_MESH_BATCH_DIRTY_SELECT,
  BKE_MESH_BATCH_DIRTY_SELECT_PAINT,
  BKE_MESH_BATCH_DIRTY_SHADING,
  BKE_MESH_BATCH_DIRTY_UVEDIT_ALL,
  BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT,
} eMeshBatchDirtyMode;

/*  mesh_runtime.cc  */

/**
 * Call after changing vertex positions to tag lazily calculated caches for recomputation.
 */
void BKE_mesh_tag_positions_changed(struct Mesh *mesh);

/**
 * Call after moving every mesh vertex by the same translation.
 */
void BKE_mesh_tag_positions_changed_uniformly(struct Mesh *mesh);

void BKE_mesh_tag_topology_changed(struct Mesh *mesh);

/**
 * Call when new edges and vertices have been created but positions and faces haven't changed.
 */
void BKE_mesh_tag_edges_split(struct Mesh *mesh);

/**
 * Call when face vertex order has changed but positions and faces haven't changed
 */
void BKE_mesh_tag_face_winding_changed(struct Mesh *mesh);

/* `mesh.cc` */

struct BMesh *BKE_mesh_to_bmesh_ex(const struct Mesh *me,
                                   const struct BMeshCreateParams *create_params,
                                   const struct BMeshFromMeshParams *convert_params);
struct BMesh *BKE_mesh_to_bmesh(struct Mesh *me,
                                struct Object *ob,
                                bool add_key_index,
                                const struct BMeshCreateParams *params);

struct Mesh *BKE_mesh_from_bmesh_nomain(struct BMesh *bm,
                                        const struct BMeshToMeshParams *params,
                                        const struct Mesh *me_settings);
struct Mesh *BKE_mesh_from_bmesh_for_eval_nomain(struct BMesh *bm,
                                                 const struct CustomData_MeshMasks *cd_mask_extra,
                                                 const struct Mesh *me_settings);

/**
 * Add original index (#CD_ORIGINDEX) layers if they don't already exist. This is meant to be used
 * when creating an evaluated mesh from an original edit mode mesh, to allow mapping from the
 * evaluated vertices to the originals.
 *
 * The mesh is expected to of a `ME_WRAPPER_TYPE_MDATA` wrapper type. This is asserted.
 */
void BKE_mesh_ensure_default_orig_index_customdata(struct Mesh *mesh);

/**
 * Same as #BKE_mesh_ensure_default_orig_index_customdata but does not perform any checks: they
 * must be done by the caller.
 */
void BKE_mesh_ensure_default_orig_index_customdata_no_check(struct Mesh *mesh);

#ifdef __cplusplus

/**
 * Sets each output array element to the edge index if it is a real edge, or -1.
 */
void BKE_mesh_looptri_get_real_edges(const blender::int2 *edges,
                                     const int *corner_verts,
                                     const int *corner_edges,
                                     const struct MLoopTri *tri,
                                     int r_edges[3]);

#endif

/**
 * Free (or release) any data used by this mesh (does not free the mesh itself).
 * Only use for undo, in most cases `BKE_id_free(nullptr, me)` should be used.
 */
void BKE_mesh_free_data_for_undo(struct Mesh *me);

/**
 * Remove all geometry and derived data like caches from the mesh.
 */
void BKE_mesh_clear_geometry(struct Mesh *me);

/**
 * Same as #BKE_mesh_clear_geometry, but also clears attribute meta-data like active attribute
 * names and vertex group names. Used when the geometry is *entirely* replaced.
 */
void BKE_mesh_clear_geometry_and_metadata(struct Mesh *me);

struct Mesh *BKE_mesh_add(struct Main *bmain, const char *name);

void BKE_mesh_free_editmesh(struct Mesh *mesh);

/**
 * A version of #BKE_mesh_copy_parameters that is intended for evaluated output
 * (the modifier stack for example).
 *
 * \warning User counts are not handled for ID's.
 */
void BKE_mesh_copy_parameters_for_eval(struct Mesh *me_dst, const struct Mesh *me_src);
/**
 * Copy user editable settings that we want to preserve
 * when a new mesh is based on an existing mesh.
 */
void BKE_mesh_copy_parameters(struct Mesh *me_dst, const struct Mesh *me_src);
void BKE_mesh_ensure_skin_customdata(struct Mesh *me);

/** Add face offsets to describe faces to a new mesh. */
void BKE_mesh_face_offsets_ensure_alloc(struct Mesh *mesh);

struct Mesh *BKE_mesh_new_nomain(int verts_num, int edges_num, int faces_num, int loops_num);
struct Mesh *BKE_mesh_new_nomain_from_template(
    const struct Mesh *me_src, int verts_num, int edges_num, int faces_num, int loops_num);
struct Mesh *BKE_mesh_new_nomain_from_template_ex(const struct Mesh *me_src,
                                                  int verts_num,
                                                  int edges_num,
                                                  int tessface_num,
                                                  int faces_num,
                                                  int loops_num,
                                                  struct CustomData_MeshMasks mask);

void BKE_mesh_eval_delete(struct Mesh *mesh_eval);

/**
 * Performs copy for use during evaluation,
 * optional referencing original arrays to reduce memory.
 */
struct Mesh *BKE_mesh_copy_for_eval(const struct Mesh *source);

/**
 * These functions construct a new Mesh,
 * contrary to #BKE_mesh_to_curve_nurblist which modifies ob itself.
 */
struct Mesh *BKE_mesh_new_nomain_from_curve(const struct Object *ob);
struct Mesh *BKE_mesh_new_nomain_from_curve_displist(const struct Object *ob,
                                                     const struct ListBase *dispbase);

bool BKE_mesh_attribute_required(const char *name);

float (*BKE_mesh_orco_verts_get(struct Object *ob))[3];
void BKE_mesh_orco_verts_transform(struct Mesh *me, float (*orco)[3], int totvert, int invert);

/**
 * Add a #CD_ORCO layer to the Mesh if there is none already.
 */
void BKE_mesh_orco_ensure(struct Object *ob, struct Mesh *mesh);

struct Mesh *BKE_mesh_from_object(struct Object *ob);
void BKE_mesh_assign_object(struct Main *bmain, struct Object *ob, struct Mesh *me);
void BKE_mesh_to_curve_nurblist(const struct Mesh *me,
                                struct ListBase *nurblist,
                                int edge_users_test);
void BKE_mesh_to_curve(struct Main *bmain,
                       struct Depsgraph *depsgraph,
                       struct Scene *scene,
                       struct Object *ob);
void BKE_mesh_to_pointcloud(struct Main *bmain,
                            struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *ob);
void BKE_pointcloud_to_mesh(struct Main *bmain,
                            struct Depsgraph *depsgraph,
                            struct Scene *scene,
                            struct Object *ob);
void BKE_mesh_material_index_remove(struct Mesh *me, short index);
bool BKE_mesh_material_index_used(struct Mesh *me, short index);
void BKE_mesh_material_index_clear(struct Mesh *me);
void BKE_mesh_material_remap(struct Mesh *me, const unsigned int *remap, unsigned int remap_len);
void BKE_mesh_smooth_flag_set(struct Mesh *me, bool use_smooth);
void BKE_mesh_auto_smooth_flag_set(struct Mesh *me, bool use_auto_smooth, float auto_smooth_angle);

/**
 * Used for unit testing; compares two meshes, checking only
 * differences we care about.  should be usable with leaf's
 * testing framework I get RNA work done, will use hackish
 * testing code for now.
 */
const char *BKE_mesh_cmp(struct Mesh *me1, struct Mesh *me2, float thresh);

struct BoundBox *BKE_mesh_boundbox_get(struct Object *ob);

void BKE_mesh_texspace_calc(struct Mesh *me);
void BKE_mesh_texspace_ensure(struct Mesh *me);
void BKE_mesh_texspace_get(struct Mesh *me,
                           float r_texspace_location[3],
                           float r_texspace_size[3]);
void BKE_mesh_texspace_get_reference(struct Mesh *me,
                                     char **r_texspace_flag,
                                     float **r_texspace_location,
                                     float **r_texspace_size);

/**
 * Create new mesh from the given object at its current state.
 * The caller owns the result mesh.
 *
 * If \a preserve_all_data_layers is true then the modifier stack is re-evaluated to ensure it
 * preserves all possible custom data layers.
 *
 * \note Dependency graph argument is required when preserve_all_data_layers is true, and is
 * ignored otherwise.
 */
struct Mesh *BKE_mesh_new_from_object(struct Depsgraph *depsgraph,
                                      struct Object *object,
                                      bool preserve_all_data_layers,
                                      bool preserve_origindex);

/**
 * This is a version of BKE_mesh_new_from_object() which stores mesh in the given main database.
 * However, that function enforces object type to be a geometry one, and ensures a mesh is always
 * generated, be it empty.
 */
struct Mesh *BKE_mesh_new_from_object_to_bmain(struct Main *bmain,
                                               struct Depsgraph *depsgraph,
                                               struct Object *object,
                                               bool preserve_all_data_layers);

/**
 * Move data from a mesh outside of the main data-base into a mesh in the data-base.
 * Takes ownership of the source mesh.
 */
void BKE_mesh_nomain_to_mesh(struct Mesh *mesh_src, struct Mesh *mesh_dst, struct Object *ob);
void BKE_mesh_nomain_to_meshkey(struct Mesh *mesh_src, struct Mesh *mesh_dst, struct KeyBlock *kb);

/* vertex level transformations & checks (no derived mesh) */

/* basic vertex data functions */
void BKE_mesh_transform(struct Mesh *me, const float mat[4][4], bool do_keys);
void BKE_mesh_translate(struct Mesh *me, const float offset[3], bool do_keys);

void BKE_mesh_tessface_clear(struct Mesh *mesh);

void BKE_mesh_mselect_clear(struct Mesh *me);
void BKE_mesh_mselect_validate(struct Mesh *me);
/**
 * \return the index within `me->mselect`, or -1
 */
int BKE_mesh_mselect_find(struct Mesh *me, int index, int type);
/**
 * \return The index of the active element.
 */
int BKE_mesh_mselect_active_get(struct Mesh *me, int type);
void BKE_mesh_mselect_active_set(struct Mesh *me, int index, int type);

void BKE_mesh_count_selected_items(const struct Mesh *mesh, int r_count[3]);

float (*BKE_mesh_vert_coords_alloc(const struct Mesh *mesh, int *r_vert_len))[3];

void BKE_mesh_vert_coords_apply_with_mat4(struct Mesh *mesh,
                                          const float (*vert_coords)[3],
                                          const float mat[4][4]);
void BKE_mesh_vert_coords_apply(struct Mesh *mesh, const float (*vert_coords)[3]);

/* *** mesh_tessellate.cc *** */

/**
 * See #bke::mesh::looptris_calc
 */
void BKE_mesh_recalc_looptri(const int *corner_verts,
                             const int *face_offsets,
                             const float (*vert_positions)[3],
                             int totvert,
                             int totloop,
                             int faces_num,
                             struct MLoopTri *mlooptri);

/* *** mesh_normals.cc *** */

/**
 * See #Mesh::vert_normals().
 * \warning May return null if the mesh is empty.
 */
const float (*BKE_mesh_vert_normals_ensure(const struct Mesh *mesh))[3];

/**
 * Retrieve write access to the cached vertex normals, ensuring that they are allocated but *not*
 * that they are calculated. The provided vertex normals should be the same as if they were
 * calculated automatically.
 *
 * \note In order to clear the dirty flag, this function should be followed by a call to
 * #BKE_mesh_vert_normals_clear_dirty. This is separate so that normals are still tagged dirty
 * while they are being assigned.
 *
 * \warning The memory returned by this function is not initialized if it was not previously
 * allocated.
 */
float (*BKE_mesh_vert_normals_for_write(struct Mesh *mesh))[3];

/**
 * Mark the mesh's vertex normals non-dirty, for when they are calculated or assigned manually.
 */
void BKE_mesh_vert_normals_clear_dirty(struct Mesh *mesh);

/**
 * Return true if the mesh vertex normals either are not stored or are dirty.
 * This can be used to help decide whether to transfer them when copying a mesh.
 */
bool BKE_mesh_vert_normals_are_dirty(const struct Mesh *mesh);

/**
 * Return true if the mesh face normals either are not stored or are dirty.
 * This can be used to help decide whether to transfer them when copying a mesh.
 */
bool BKE_mesh_face_normals_are_dirty(const struct Mesh *mesh);

/**
 * Called after calculating all modifiers.
 */
void BKE_mesh_ensure_normals_for_display(struct Mesh *mesh);

/**
 * References a contiguous loop-fan with normal offset vars.
 */
typedef struct MLoopNorSpace {
  /** Automatically computed loop normal. */
  float vec_lnor[3];
  /** Reference vector, orthogonal to vec_lnor. */
  float vec_ref[3];
  /** Third vector, orthogonal to vec_lnor and vec_ref. */
  float vec_ortho[3];
  /** Reference angle, around vec_ortho, in ]0, pi] range (0.0 marks that space as invalid). */
  float ref_alpha;
  /** Reference angle, around vec_lnor, in ]0, 2pi] range (0.0 marks that space as invalid). */
  float ref_beta;
  /** All loops using this lnor space (i.e. smooth fan of loops),
   * as (depending on owning MLoopNorSpaceArrary.data_type):
   *     - Indices (uint_in_ptr), or
   *     - BMLoop pointers. */
  struct LinkNode *loops;
  char flags;
} MLoopNorSpace;
/**
 * MLoopNorSpace.flags
 */
enum {
  MLNOR_SPACE_IS_SINGLE = 1 << 0,
};

/**
 * Collection of #MLoopNorSpace basic storage & pre-allocation.
 */
typedef struct MLoopNorSpaceArray {
  MLoopNorSpace **lspacearr; /* Face corner aligned array */
  struct LinkNode
      *loops_pool; /* Allocated once, avoids to call BLI_linklist_prepend_arena() for each loop! */
  char data_type;  /* Whether we store loop indices, or pointers to BMLoop. */
  int spaces_num;  /* Number of clnors spaces defined in this array. */
  struct MemArena *mem;
} MLoopNorSpaceArray;
/**
 * MLoopNorSpaceArray.data_type
 */
enum {
  MLNOR_SPACEARR_LOOP_INDEX = 0,
  MLNOR_SPACEARR_BMLOOP_PTR = 1,
};

/* Low-level custom normals functions. */
void BKE_lnor_spacearr_init(MLoopNorSpaceArray *lnors_spacearr, int numLoops, char data_type);
void BKE_lnor_spacearr_clear(MLoopNorSpaceArray *lnors_spacearr);
void BKE_lnor_spacearr_free(MLoopNorSpaceArray *lnors_spacearr);

/**
 * Utility for multi-threaded calculation that ensures
 * `lnors_spacearr_tls` doesn't share memory with `lnors_spacearr`
 * that would cause it not to be thread safe.
 *
 * \note This works as long as threads never operate on the same loops at once.
 */
void BKE_lnor_spacearr_tls_init(MLoopNorSpaceArray *lnors_spacearr,
                                MLoopNorSpaceArray *lnors_spacearr_tls);
/**
 * Utility for multi-threaded calculation
 * that merges `lnors_spacearr_tls` into `lnors_spacearr`.
 */
void BKE_lnor_spacearr_tls_join(MLoopNorSpaceArray *lnors_spacearr,
                                MLoopNorSpaceArray *lnors_spacearr_tls);

MLoopNorSpace *BKE_lnor_space_create(MLoopNorSpaceArray *lnors_spacearr);

#ifdef __cplusplus

/**
 * Should only be called once.
 * Beware, this modifies ref_vec and other_vec in place!
 * In case no valid space can be generated, ref_alpha and ref_beta are set to zero
 * (which means 'use auto lnors').
 */
void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           float vec_ref[3],
                           float vec_other[3],
                           blender::Span<blender::float3> edge_vectors);

#endif

/**
 * Add a new given loop to given lnor_space.
 * Depending on \a lnor_space->data_type, we expect \a bm_loop to be a pointer to BMLoop struct
 * (in case of BMLOOP_PTR), or nullptr (in case of LOOP_INDEX), loop index is then stored in
 * pointer. If \a is_single is set, the BMLoop or loop index is directly stored in \a
 * lnor_space->loops pointer (since there is only one loop in this fan), else it is added to the
 * linked list of loops in the fan.
 */
void BKE_lnor_space_add_loop(MLoopNorSpaceArray *lnors_spacearr,
                             MLoopNorSpace *lnor_space,
                             int ml_index,
                             void *bm_loop,
                             bool is_single);
void BKE_lnor_space_custom_data_to_normal(const MLoopNorSpace *lnor_space,
                                          const short clnor_data[2],
                                          float r_custom_lnor[3]);
void BKE_lnor_space_custom_normal_to_data(const MLoopNorSpace *lnor_space,
                                          const float custom_lnor[3],
                                          short r_clnor_data[2]);

/**
 * Computes average per-vertex normals from given custom loop normals.
 *
 * \param clnors: The computed custom loop normals.
 * \param r_vert_clnors: The (already allocated) array where to store averaged per-vertex normals.
 */
void BKE_mesh_normals_loop_to_vertex(int numVerts,
                                     const int *corner_verts,
                                     int numLoops,
                                     const float (*clnors)[3],
                                     float (*r_vert_clnors)[3]);

/**
 * High-level custom normals functions.
 */
bool BKE_mesh_has_custom_loop_normals(struct Mesh *me);

void BKE_mesh_calc_normals_split(struct Mesh *mesh);
/**
 * Compute 'split' (aka loop, or per face corner's) normals.
 *
 * \param r_lnors_spacearr: Allows to get computed loop normal space array.
 * That data, among other things, contains 'smooth fan' info, useful e.g.
 * to split geometry along sharp edges.
 */
void BKE_mesh_calc_normals_split_ex(struct Mesh *mesh,
                                    struct MLoopNorSpaceArray *r_lnors_spacearr,
                                    float (*r_corner_normals)[3]);

/**
 * Higher level functions hiding most of the code needed around call to
 * #normals_loop_custom_set().
 *
 * \param r_custom_loop_normals: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals(struct Mesh *mesh, float (*r_custom_loop_normals)[3]);
/**
 * Higher level functions hiding most of the code needed around call to
 * #normals_loop_custom_set_from_verts().
 *
 * \param r_custom_vert_normals: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals_from_verts(struct Mesh *mesh, float (*r_custom_vert_normals)[3]);

/* *** mesh_evaluate.cc *** */

float BKE_mesh_calc_area(const struct Mesh *me);

bool BKE_mesh_center_median(const struct Mesh *me, float r_cent[3]);
/**
 * Calculate the center from faces,
 * use when we want to ignore vertex locations that don't have connected faces.
 */
bool BKE_mesh_center_median_from_faces(const struct Mesh *me, float r_cent[3]);
bool BKE_mesh_center_of_surface(const struct Mesh *me, float r_cent[3]);
/**
 * \note Mesh must be manifold with consistent face-winding,
 * see #mesh_calc_face_volume_centroid for details.
 */
bool BKE_mesh_center_of_volume(const struct Mesh *me, float r_cent[3]);

/**
 * Calculate the volume and center.
 *
 * \param r_volume: Volume (unsigned).
 * \param r_center: Center of mass.
 */
void BKE_mesh_calc_volume(const float (*vert_positions)[3],
                          int mverts_num,
                          const struct MLoopTri *mlooptri,
                          int looptri_num,
                          const int *corner_verts,
                          float *r_volume,
                          float r_center[3]);

/**
 * Flip a single corner's #MDisps structure,
 * low level function to be called from face-flipping code which re-arranged the mdisps themselves.
 */
void BKE_mesh_mdisp_flip(struct MDisps *md, bool use_loop_mdisp_flip);

/**
 * Account for custom-data such as UVs becoming detached because of imprecision
 * in custom-data interpolation.
 * Without running this operation subdivision surface can cause UVs to be disconnected,
 * see: #81065.
 */
void BKE_mesh_merge_customdata_for_apply_modifier(struct Mesh *me);

/* Flush flags. */

/**
 * Update the hide flag for edges and faces from the corresponding flag in verts.
 */
void BKE_mesh_flush_hidden_from_verts(struct Mesh *me);
void BKE_mesh_flush_hidden_from_faces(struct Mesh *me);

void BKE_mesh_flush_select_from_faces(struct Mesh *me);
void BKE_mesh_flush_select_from_verts(struct Mesh *me);

/* spatial evaluation */
/**
 * This function takes the difference between 2 vertex-coord-arrays
 * (\a vert_cos_src, \a vert_cos_dst),
 * and applies the difference to \a vert_cos_new relative to \a vert_cos_org.
 *
 * \param vert_cos_src: reference deform source.
 * \param vert_cos_dst: reference deform destination.
 *
 * \param vert_cos_org: reference for the output location.
 * \param vert_cos_new: resulting coords.
 */
void BKE_mesh_calc_relative_deform(const int *face_offsets,
                                   int faces_num,
                                   const int *corner_verts,
                                   int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3]);

/* *** mesh_validate.cc *** */

/**
 * Validates and corrects a Mesh.
 *
 * \returns true if a change is made.
 */
bool BKE_mesh_validate(struct Mesh *me, bool do_verbose, bool cddata_check_mask);
/**
 * Checks if a Mesh is valid without any modification. This is always verbose.
 * \returns True if the mesh is valid.
 */
bool BKE_mesh_is_valid(struct Mesh *me);
/**
 * Check all material indices of faces are valid, invalid ones are set to 0.
 * \returns True if the material indices are valid.
 */
bool BKE_mesh_validate_material_indices(struct Mesh *me);

#ifdef __cplusplus

/**
 * Validate the mesh, \a do_fixes requires \a mesh to be non-null.
 *
 * \return false if no changes needed to be made.
 *
 * Vertex Normals
 * ==============
 *
 * While zeroed normals are checked, these checks aren't comprehensive.
 * Technically, to detect errors here a normal recalculation and comparison is necessary.
 * However this function is mainly to prevent severe errors in geometry
 * (invalid data that will crash Blender, or cause some features to behave incorrectly),
 * not to detect subtle differences in the resulting normals which could be caused
 * by importers that load normals (for example).
 */
bool BKE_mesh_validate_arrays(struct Mesh *me,
                              float (*vert_positions)[3],
                              unsigned int totvert,
                              blender::int2 *edges,
                              unsigned int totedge,
                              struct MFace *mfaces,
                              unsigned int totface,
                              int *corner_verts,
                              int *corner_edges,
                              unsigned int totloop,
                              int *face_offsets,
                              unsigned int faces_num,
                              struct MDeformVert *dverts, /* assume totvert length */
                              bool do_verbose,
                              bool do_fixes,
                              bool *r_change);

#endif

/**
 * \returns is_valid.
 */
bool BKE_mesh_validate_all_customdata(struct CustomData *vert_data,
                                      uint totvert,
                                      struct CustomData *edge_data,
                                      uint totedge,
                                      struct CustomData *loop_data,
                                      uint totloop,
                                      struct CustomData *pdata,
                                      uint faces_num,
                                      bool check_meshmask,
                                      bool do_verbose,
                                      bool do_fixes,
                                      bool *r_change);

void BKE_mesh_strip_loose_faces(struct Mesh *me);

/**
 * Calculate edges from faces.
 */
void BKE_mesh_calc_edges(struct Mesh *mesh, bool keep_existing_edges, bool select_new_edges);
/**
 * Calculate/create edges from tessface data
 *
 * \param mesh: The mesh to add edges into
 */
void BKE_mesh_calc_edges_tessface(struct Mesh *mesh);

/* In DerivedMesh.cc */
void BKE_mesh_wrapper_deferred_finalize_mdata(struct Mesh *me_eval,
                                              const struct CustomData_MeshMasks *cd_mask_finalize);

/* **** Depsgraph evaluation **** */

void BKE_mesh_eval_geometry(struct Depsgraph *depsgraph, struct Mesh *mesh);

/* Draw Cache */
void BKE_mesh_batch_cache_dirty_tag(struct Mesh *me, eMeshBatchDirtyMode mode);
void BKE_mesh_batch_cache_free(void *batch_cache);

extern void (*BKE_mesh_batch_cache_dirty_tag_cb)(struct Mesh *me, eMeshBatchDirtyMode mode);
extern void (*BKE_mesh_batch_cache_free_cb)(void *batch_cache);

/* `mesh_debug.cc` */

#ifndef NDEBUG
char *BKE_mesh_debug_info(const struct Mesh *me)
    ATTR_NONNULL(1) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BKE_mesh_debug_print(const struct Mesh *me) ATTR_NONNULL(1);
#endif

/* -------------------------------------------------------------------- */
/** \name Inline Mesh Data Access
 * \{ */

/**
 * \return The material index for each face. May be null.
 * \note In C++ code, prefer using the attribute API (#AttributeAccessor).
 */
BLI_INLINE const int *BKE_mesh_material_indices(const Mesh *mesh)
{
  return (const int *)CustomData_get_layer_named(
      &mesh->face_data, CD_PROP_INT32, "material_index");
}

/**
 * \return The material index for each face. Create the layer if it doesn't exist.
 * \note In C++ code, prefer using the attribute API (#MutableAttributeAccessor).
 */
BLI_INLINE int *BKE_mesh_material_indices_for_write(Mesh *mesh)
{
  int *indices = (int *)CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_INT32, "material_index", mesh->faces_num);
  if (indices) {
    return indices;
  }
  return (int *)CustomData_add_layer_named(
      &mesh->face_data, CD_PROP_INT32, CD_SET_DEFAULT, mesh->faces_num, "material_index");
}

BLI_INLINE const float (*BKE_mesh_vert_positions(const Mesh *mesh))[3]
{
  return (const float(*)[3])CustomData_get_layer_named(
      &mesh->vert_data, CD_PROP_FLOAT3, "position");
}

BLI_INLINE const int *BKE_mesh_face_offsets(const Mesh *mesh)
{
  return mesh->face_offset_indices;
}

BLI_INLINE const int *BKE_mesh_corner_verts(const Mesh *mesh)
{
  return (const int *)CustomData_get_layer_named(&mesh->loop_data, CD_PROP_INT32, ".corner_vert");
}

BLI_INLINE const MDeformVert *BKE_mesh_deform_verts(const Mesh *mesh)
{
  return (const MDeformVert *)CustomData_get_layer(&mesh->vert_data, CD_MDEFORMVERT);
}
BLI_INLINE MDeformVert *BKE_mesh_deform_verts_for_write(Mesh *mesh)
{
  MDeformVert *dvert = (MDeformVert *)CustomData_get_layer_for_write(
      &mesh->vert_data, CD_MDEFORMVERT, mesh->totvert);
  if (dvert) {
    return dvert;
  }
  return (MDeformVert *)CustomData_add_layer(
      &mesh->vert_data, CD_MDEFORMVERT, CD_SET_DEFAULT, mesh->totvert);
}

#ifdef __cplusplus
}
#endif

/** \} */
