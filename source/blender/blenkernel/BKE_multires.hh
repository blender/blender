/* SPDX-FileCopyrightText: 2007 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_subsurf.hh"
#include "BLI_utildefines.h"

struct Depsgraph;
struct DerivedMesh;
struct MDisps;
struct Mesh;
struct ModifierData;
struct MultiresModifierData;
struct Object;
struct Scene;
struct SubdivCCG;

struct MLoopTri;

/**
 * Delete mesh mdisps and grid paint masks.
 */
void multires_customdata_delete(struct Mesh *me);

void multires_set_tot_level(struct Object *ob, struct MultiresModifierData *mmd, int lvl);

void multires_mark_as_modified(struct Depsgraph *depsgraph,
                               struct Object *object,
                               enum MultiresModifiedFlags flags);

void multires_flush_sculpt_updates(struct Object *object);
void multires_force_sculpt_rebuild(struct Object *object);
void multires_force_external_reload(struct Object *object);

/* internal, only called in subsurf_ccg.cc */
void multires_modifier_update_mdisps(struct DerivedMesh *dm, struct Scene *scene);
void multires_modifier_update_hidden(struct DerivedMesh *dm);

/**
 * Reset the multi-res levels to match the number of mdisps.
 */
void multiresModifier_set_levels_from_disps(struct MultiresModifierData *mmd, struct Object *ob);

typedef enum MultiresFlags {
  MULTIRES_USE_LOCAL_MMD = 1,
  MULTIRES_USE_RENDER_PARAMS = 2,
  MULTIRES_ALLOC_PAINT_MASK = 4,
  MULTIRES_IGNORE_SIMPLIFY = 8,
} MultiresFlags;
ENUM_OPERATORS(MultiresFlags, MULTIRES_IGNORE_SIMPLIFY);

struct DerivedMesh *multires_make_derived_from_derived(struct DerivedMesh *dm,
                                                       struct MultiresModifierData *mmd,
                                                       struct Scene *scene,
                                                       struct Object *ob,
                                                       MultiresFlags flags);

struct MultiresModifierData *find_multires_modifier_before(struct Scene *scene,
                                                           struct ModifierData *lastmd);
/**
 * used for applying scale on mdisps layer and syncing subdivide levels when joining objects.
 * \param use_first: return first multi-res modifier if all multi-res'es are disabled.
 */
struct MultiresModifierData *get_multires_modifier(struct Scene *scene,
                                                   struct Object *ob,
                                                   bool use_first);
int multires_get_level(const struct Scene *scene,
                       const struct Object *ob,
                       const struct MultiresModifierData *mmd,
                       bool render,
                       bool ignore_simplify);

/**
 * Creates mesh with multi-res modifier applied on current object's deform mesh.
 */
struct Mesh *BKE_multires_create_mesh(struct Depsgraph *depsgraph,
                                      struct Object *object,
                                      struct MultiresModifierData *mmd);

/**
 * Get coordinates of a deformed base mesh which is an input to the given multi-res modifier.
 * \note The modifiers will be re-evaluated.
 */
float (*BKE_multires_create_deformed_base_mesh_vert_coords(struct Depsgraph *depsgraph,
                                                           struct Object *object,
                                                           struct MultiresModifierData *mmd,
                                                           int *r_num_deformed_verts))[3];

/**
 * \param direction: 1 for delete higher, 0 for lower (not implemented yet).
 */
void multiresModifier_del_levels(struct MultiresModifierData *mmd,
                                 struct Scene *scene,
                                 struct Object *object,
                                 int direction);
void multiresModifier_base_apply(struct Depsgraph *depsgraph,
                                 struct Object *object,
                                 struct MultiresModifierData *mmd);
int multiresModifier_rebuild_subdiv(struct Depsgraph *depsgraph,
                                    struct Object *object,
                                    struct MultiresModifierData *mmd,
                                    int rebuild_limit,
                                    bool switch_view_to_lower_level);
/**
 * If `ob_src` and `ob_dst` both have multi-res modifiers,
 * synchronize them such that `ob_dst` has the same total number of levels as `ob_src`.
 */
void multiresModifier_sync_levels_ex(struct Object *ob_dst,
                                     struct MultiresModifierData *mmd_src,
                                     struct MultiresModifierData *mmd_dst);

void multires_stitch_grids(struct Object *);

void multiresModifier_scale_disp(struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct Object *ob);
void multiresModifier_prepare_join(struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct Object *to_ob);

int multires_mdisp_corners(const struct MDisps *s);

/**
 * Update multi-res data after topology changing.
 */
void multires_topology_changed(struct Mesh *me);

/**
 * Makes sure data from an external file is fully read.
 *
 * Since the multi-res data files only contain displacement vectors without knowledge about
 * subdivision level some extra work is needed. Namely make is to all displacement grids have
 * proper level and number of displacement vectors set.
 */
void multires_ensure_external_read(struct Mesh *mesh, int top_level);
void multiresModifier_ensure_external_read(struct Mesh *mesh,
                                           const struct MultiresModifierData *mmd);

/**** interpolation stuff ****/
/* Adapted from `sculptmode.c` */

void old_mdisps_bilinear(float out[3], float (*disps)[3], int st, float u, float v);
/**
 * Find per-corner coordinate with given per-face UV coord.
 */
int mdisp_rot_face_to_crn(int face_size, int face_side, float u, float v, float *x, float *y);

/* Reshaping, define in multires_reshape.cc */

bool multiresModifier_reshapeFromVertcos(struct Depsgraph *depsgraph,
                                         struct Object *object,
                                         struct MultiresModifierData *mmd,
                                         const float (*vert_coords)[3],
                                         int num_vert_coords);
/**
 * Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices.
 */
bool multiresModifier_reshapeFromObject(struct Depsgraph *depsgraph,
                                        struct MultiresModifierData *mmd,
                                        struct Object *dst,
                                        struct Object *src);
bool multiresModifier_reshapeFromDeformModifier(struct Depsgraph *depsgraph,
                                                struct Object *ob,
                                                struct MultiresModifierData *mmd,
                                                struct ModifierData *deform_md);
bool multiresModifier_reshapeFromCCG(int tot_level,
                                     struct Mesh *coarse_mesh,
                                     struct SubdivCCG *subdiv_ccg);

/* Subdivide multi-res displacement once. */

typedef enum eMultiresSubdivideModeType {
  MULTIRES_SUBDIVIDE_CATMULL_CLARK,
  MULTIRES_SUBDIVIDE_SIMPLE,
  MULTIRES_SUBDIVIDE_LINEAR,
} eMultiresSubdivideModeType;

void multiresModifier_subdivide(struct Object *object,
                                struct MultiresModifierData *mmd,
                                eMultiresSubdivideModeType mode);
void multires_subdivide_create_tangent_displacement_linear_grids(struct Object *object,
                                                                 struct MultiresModifierData *mmd);

/**
 * Subdivide displacement to the given level.
 * If level is lower than the current top level nothing happens.
 */
void multiresModifier_subdivide_to_level(struct Object *object,
                                         struct MultiresModifierData *mmd,
                                         int top_level,
                                         eMultiresSubdivideModeType mode);

/* Subdivision integration, defined in multires_subdiv.cc */

struct SubdivSettings;
struct SubdivToMeshSettings;

void BKE_multires_subdiv_settings_init(struct SubdivSettings *settings,
                                       const struct MultiresModifierData *mmd);

/* TODO(sergey): Replace this set of boolean flags with bitmask. */
void BKE_multires_subdiv_mesh_settings_init(struct SubdivToMeshSettings *mesh_settings,
                                            const struct Scene *scene,
                                            const struct Object *object,
                                            const struct MultiresModifierData *mmd,
                                            bool use_render_params,
                                            bool ignore_simplify,
                                            bool ignore_control_edges);

/* General helpers. */

/**
 * For a given partial derivatives of a PTEX face get tangent matrix for displacement.
 *
 * Corner needs to be known to properly "rotate" partial derivatives when the
 * matrix is being constructed for quad. For non-quad the corner is to be set to 0.
 */
BLI_INLINE void BKE_multires_construct_tangent_matrix(float tangent_matrix[3][3],
                                                      const float dPdu[3],
                                                      const float dPdv[3],
                                                      int corner);

/* Versioning. */

/**
 * Convert displacement which is stored for simply-subdivided mesh to a Catmull-Clark
 * subdivided mesh.
 */
void multires_do_versions_simple_to_catmull_clark(struct Object *object,
                                                  struct MultiresModifierData *mmd);

#include "intern/multires_inline.hh"
