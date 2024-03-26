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

struct BMesh;
struct BMeshCreateParams;
struct BMeshFromMeshParams;
struct BMeshToMeshParams;
struct CustomData;
struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
struct LinkNode;
struct ListBase;
struct MDeformVert;
struct MDisps;
struct MFace;
struct Main;
struct MemArena;
struct Mesh;
struct Object;
struct Scene;

/* TODO: Move to `BKE_mesh_types.hh` when possible. */
enum eMeshBatchDirtyMode {
  BKE_MESH_BATCH_DIRTY_ALL = 0,
  BKE_MESH_BATCH_DIRTY_SELECT,
  BKE_MESH_BATCH_DIRTY_SELECT_PAINT,
  BKE_MESH_BATCH_DIRTY_SHADING,
  BKE_MESH_BATCH_DIRTY_UVEDIT_ALL,
  BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT,
};

/* `mesh.cc` */

BMesh *BKE_mesh_to_bmesh_ex(const Mesh *mesh,
                            const BMeshCreateParams *create_params,
                            const BMeshFromMeshParams *convert_params);
BMesh *BKE_mesh_to_bmesh(Mesh *mesh,
                         Object *ob,
                         bool add_key_index,
                         const BMeshCreateParams *params);

Mesh *BKE_mesh_from_bmesh_nomain(BMesh *bm,
                                 const BMeshToMeshParams *params,
                                 const Mesh *me_settings);
Mesh *BKE_mesh_from_bmesh_for_eval_nomain(BMesh *bm,
                                          const CustomData_MeshMasks *cd_mask_extra,
                                          const Mesh *me_settings);

/**
 * Add original index (#CD_ORIGINDEX) layers if they don't already exist. This is meant to be used
 * when creating an evaluated mesh from an original edit mode mesh, to allow mapping from the
 * evaluated vertices to the originals.
 *
 * The mesh is expected to of a `ME_WRAPPER_TYPE_MDATA` wrapper type. This is asserted.
 */
void BKE_mesh_ensure_default_orig_index_customdata(Mesh *mesh);

/**
 * Same as #BKE_mesh_ensure_default_orig_index_customdata but does not perform any checks: they
 * must be done by the caller.
 */
void BKE_mesh_ensure_default_orig_index_customdata_no_check(Mesh *mesh);

/**
 * Free (or release) any data used by this mesh (does not free the mesh itself).
 * Only use for undo, in most cases `BKE_id_free(nullptr, me)` should be used.
 */
void BKE_mesh_free_data_for_undo(Mesh *mesh);

/**
 * Remove all geometry and derived data like caches from the mesh.
 */
void BKE_mesh_clear_geometry(Mesh *mesh);

/**
 * Same as #BKE_mesh_clear_geometry, but also clears attribute meta-data like active attribute
 * names and vertex group names. Used when the geometry is *entirely* replaced.
 */
void BKE_mesh_clear_geometry_and_metadata(Mesh *mesh);

Mesh *BKE_mesh_add(Main *bmain, const char *name);

void BKE_mesh_free_editmesh(Mesh *mesh);

/**
 * A version of #BKE_mesh_copy_parameters that is intended for evaluated output
 * (the modifier stack for example).
 *
 * \warning User counts are not handled for ID's.
 */
void BKE_mesh_copy_parameters_for_eval(Mesh *me_dst, const Mesh *me_src);
/**
 * Copy user editable settings that we want to preserve
 * when a new mesh is based on an existing mesh.
 */
void BKE_mesh_copy_parameters(Mesh *me_dst, const Mesh *me_src);
void BKE_mesh_ensure_skin_customdata(Mesh *mesh);

/** Add face offsets to describe faces to a new mesh. */
void BKE_mesh_face_offsets_ensure_alloc(Mesh *mesh);

Mesh *BKE_mesh_new_nomain(int verts_num, int edges_num, int faces_num, int corners_num);
Mesh *BKE_mesh_new_nomain_from_template(
    const Mesh *me_src, int verts_num, int edges_num, int faces_num, int corners_num);
Mesh *BKE_mesh_new_nomain_from_template_ex(const Mesh *me_src,
                                           int verts_num,
                                           int edges_num,
                                           int tessface_num,
                                           int faces_num,
                                           int corners_num,
                                           CustomData_MeshMasks mask);

void BKE_mesh_eval_delete(Mesh *mesh_eval);

/**
 * Performs copy for use during evaluation,
 * optional referencing original arrays to reduce memory.
 */
Mesh *BKE_mesh_copy_for_eval(const Mesh *source);

/**
 * These functions construct a new Mesh,
 * contrary to #BKE_mesh_to_curve_nurblist which modifies ob itself.
 */
Mesh *BKE_mesh_new_nomain_from_curve(const Object *ob);
Mesh *BKE_mesh_new_nomain_from_curve_displist(const Object *ob, const ListBase *dispbase);

bool BKE_mesh_attribute_required(const char *name);

float (*BKE_mesh_orco_verts_get(Object *ob))[3];
void BKE_mesh_orco_verts_transform(Mesh *mesh, float (*orco)[3], int totvert, bool invert);

/**
 * Add a #CD_ORCO layer to the Mesh if there is none already.
 */
void BKE_mesh_orco_ensure(Object *ob, Mesh *mesh);

Mesh *BKE_mesh_from_object(Object *ob);
void BKE_mesh_assign_object(Main *bmain, Object *ob, Mesh *mesh);
void BKE_mesh_to_curve_nurblist(const Mesh *mesh, ListBase *nurblist, int edge_users_test);
void BKE_mesh_to_curve(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void BKE_mesh_to_pointcloud(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void BKE_pointcloud_to_mesh(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);
void BKE_mesh_material_index_remove(Mesh *mesh, short index);
bool BKE_mesh_material_index_used(Mesh *mesh, short index);
void BKE_mesh_material_index_clear(Mesh *mesh);
void BKE_mesh_material_remap(Mesh *mesh, const unsigned int *remap, unsigned int remap_len);

void BKE_mesh_texspace_calc(Mesh *mesh);
void BKE_mesh_texspace_ensure(Mesh *mesh);
void BKE_mesh_texspace_get(Mesh *mesh, float r_texspace_location[3], float r_texspace_size[3]);
void BKE_mesh_texspace_get_reference(Mesh *mesh,
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
Mesh *BKE_mesh_new_from_object(Depsgraph *depsgraph,
                               Object *object,
                               bool preserve_all_data_layers,
                               bool preserve_origindex);

/**
 * This is a version of BKE_mesh_new_from_object() which stores mesh in the given main database.
 * However, that function enforces object type to be a geometry one, and ensures a mesh is always
 * generated, be it empty.
 */
Mesh *BKE_mesh_new_from_object_to_bmain(Main *bmain,
                                        Depsgraph *depsgraph,
                                        Object *object,
                                        bool preserve_all_data_layers);

/**
 * Move data from a mesh outside of the main data-base into a mesh in the data-base.
 * Takes ownership of the source mesh.
 */
void BKE_mesh_nomain_to_mesh(Mesh *mesh_src, Mesh *mesh_dst, Object *ob);
void BKE_mesh_nomain_to_meshkey(Mesh *mesh_src, Mesh *mesh_dst, KeyBlock *kb);

/* vertex level transformations & checks (no derived mesh) */

/* basic vertex data functions */
void BKE_mesh_transform(Mesh *mesh, const float mat[4][4], bool do_keys);
void BKE_mesh_translate(Mesh *mesh, const float offset[3], bool do_keys);

void BKE_mesh_tessface_clear(Mesh *mesh);

void BKE_mesh_mselect_clear(Mesh *mesh);
void BKE_mesh_mselect_validate(Mesh *mesh);
/**
 * \return the index within `me->mselect`, or -1
 */
int BKE_mesh_mselect_find(Mesh *mesh, int index, int type);
/**
 * \return The index of the active element.
 */
int BKE_mesh_mselect_active_get(Mesh *mesh, int type);
void BKE_mesh_mselect_active_set(Mesh *mesh, int index, int type);

void BKE_mesh_count_selected_items(const Mesh *mesh, int r_count[3]);

/* *** mesh_normals.cc *** */

/** Return true if the mesh vertex normals either are not stored or are dirty. */
bool BKE_mesh_vert_normals_are_dirty(const Mesh *mesh);

/** Return true if the mesh face normals either are not stored or are dirty. */
bool BKE_mesh_face_normals_are_dirty(const Mesh *mesh);

/**
 * References a contiguous loop-fan with normal offset vars.
 */
struct MLoopNorSpace {
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
};
/**
 * MLoopNorSpace.flags
 */
enum {
  MLNOR_SPACE_IS_SINGLE = 1 << 0,
};

/**
 * Collection of #MLoopNorSpace basic storage & pre-allocation.
 */
struct MLoopNorSpaceArray {
  MLoopNorSpace **lspacearr; /* Face corner aligned array */
  struct LinkNode
      *loops_pool; /* Allocated once, avoids to call BLI_linklist_prepend_arena() for each loop! */
  char data_type;  /* Whether we store loop indices, or pointers to BMLoop. */
  int spaces_num;  /* Number of clnors spaces defined in this array. */
  struct MemArena *mem;
};
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

/**
 * Should only be called once.
 * Beware, this modifies ref_vec and other_vec in place!
 * In case no valid space can be generated, ref_alpha and ref_beta are set to zero
 * (which means 'use auto lnors').
 */
void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           const float vec_ref[3],
                           const float vec_other[3],
                           blender::Span<blender::float3> edge_vectors);

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
                             int corner,
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
bool BKE_mesh_has_custom_loop_normals(Mesh *mesh);

/**
 * Higher level functions hiding most of the code needed around call to
 * #normals_corner_custom_set().
 *
 * \param r_custom_loop_normals: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals(Mesh *mesh, float (*r_custom_loop_normals)[3]);
/**
 * Higher level functions hiding most of the code needed around call to
 * #normals_corner_custom_set_from_verts().
 *
 * \param r_custom_vert_normals: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals_from_verts(Mesh *mesh, float (*r_custom_vert_normals)[3]);

/* *** mesh_evaluate.cc *** */

float BKE_mesh_calc_area(const Mesh *mesh);

bool BKE_mesh_center_median(const Mesh *mesh, float r_cent[3]);
/**
 * Calculate the center from faces,
 * use when we want to ignore vertex locations that don't have connected faces.
 */
bool BKE_mesh_center_median_from_faces(const Mesh *mesh, float r_cent[3]);
bool BKE_mesh_center_of_surface(const Mesh *mesh, float r_cent[3]);
/**
 * \note Mesh must be manifold with consistent face-winding,
 * see #mesh_calc_face_volume_centroid for details.
 */
bool BKE_mesh_center_of_volume(const Mesh *mesh, float r_cent[3]);

/**
 * Calculate the volume and center.
 *
 * \param r_volume: Volume (unsigned).
 * \param r_center: Center of mass.
 */
void BKE_mesh_calc_volume(const float (*vert_positions)[3],
                          int mverts_num,
                          const blender::int3 *corner_tris,
                          int corner_tris_num,
                          const int *corner_verts,
                          float *r_volume,
                          float r_center[3]);

/**
 * Flip a single corner's #MDisps structure,
 * low level function to be called from face-flipping code which re-arranged the mdisps themselves.
 */
void BKE_mesh_mdisp_flip(MDisps *md, bool use_loop_mdisp_flip);

/**
 * Account for custom-data such as UVs becoming detached because of imprecision
 * in custom-data interpolation.
 * Without running this operation subdivision surface can cause UVs to be disconnected,
 * see: #81065.
 */
void BKE_mesh_merge_customdata_for_apply_modifier(Mesh *mesh);

/* Flush flags. */

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
bool BKE_mesh_validate(Mesh *mesh, bool do_verbose, bool cddata_check_mask);
/**
 * Checks if a Mesh is valid without any modification. This is always verbose.
 * \returns True if the mesh is valid.
 */
bool BKE_mesh_is_valid(Mesh *mesh);
/**
 * Check all material indices of faces are valid, invalid ones are set to 0.
 * \returns True if the material indices are valid.
 */
bool BKE_mesh_validate_material_indices(Mesh *mesh);

/**
 * Validate the mesh, \a do_fixes requires \a mesh to be non-null.
 *
 * \return false if no changes needed to be made.
 */
bool BKE_mesh_validate_arrays(Mesh *mesh,
                              float (*vert_positions)[3],
                              unsigned int verts_num,
                              blender::int2 *edges,
                              unsigned int edges_num,
                              MFace *legacy_faces,
                              unsigned int legacy_faces_num,
                              int *corner_verts,
                              int *corner_edges,
                              unsigned int corners_num,
                              const int *face_offsets,
                              unsigned int faces_num,
                              MDeformVert *dverts, /* assume totvert length */
                              bool do_verbose,
                              bool do_fixes,
                              bool *r_change);

/**
 * \returns is_valid.
 */
bool BKE_mesh_validate_all_customdata(CustomData *vert_data,
                                      uint verts_num,
                                      CustomData *edge_data,
                                      uint edges_num,
                                      CustomData *corner_data,
                                      uint corners_num,
                                      CustomData *face_data,
                                      uint faces_num,
                                      bool check_meshmask,
                                      bool do_verbose,
                                      bool do_fixes,
                                      bool *r_change);

void BKE_mesh_strip_loose_faces(Mesh *mesh);

/* In DerivedMesh.cc */
void BKE_mesh_wrapper_deferred_finalize_mdata(Mesh *mesh_eval);

/* **** Depsgraph evaluation **** */

void BKE_mesh_eval_geometry(Depsgraph *depsgraph, Mesh *mesh);

/* Draw Cache */
void BKE_mesh_batch_cache_dirty_tag(Mesh *mesh, eMeshBatchDirtyMode mode);
void BKE_mesh_batch_cache_free(void *batch_cache);

extern void (*BKE_mesh_batch_cache_dirty_tag_cb)(Mesh *mesh, eMeshBatchDirtyMode mode);
extern void (*BKE_mesh_batch_cache_free_cb)(void *batch_cache);

/* `mesh_debug.cc` */

#ifndef NDEBUG
char *BKE_mesh_debug_info(const Mesh *mesh) ATTR_NONNULL(1) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void BKE_mesh_debug_print(const Mesh *mesh) ATTR_NONNULL(1);
#endif
