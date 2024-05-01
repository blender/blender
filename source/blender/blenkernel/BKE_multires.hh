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
namespace blender::bke::subdiv {
struct Settings;
struct ToMeshSettings;
}  // namespace blender::bke::subdiv

/**
 * Delete mesh mdisps and grid paint masks.
 */
void multires_customdata_delete(Mesh *mesh);

void multires_set_tot_level(Object *ob, MultiresModifierData *mmd, int lvl);

void multires_mark_as_modified(Depsgraph *depsgraph, Object *object, MultiresModifiedFlags flags);

void multires_flush_sculpt_updates(Object *object);
void multires_force_sculpt_rebuild(Object *object);
void multires_force_external_reload(Object *object);

/* internal, only called in subsurf_ccg.cc */
void multires_modifier_update_mdisps(DerivedMesh *dm, Scene *scene);
void multires_modifier_update_hidden(DerivedMesh *dm);

/**
 * Reset the multi-res levels to match the number of mdisps.
 */
void multiresModifier_set_levels_from_disps(MultiresModifierData *mmd, Object *ob);

enum MultiresFlags {
  MULTIRES_USE_LOCAL_MMD = 1,
  MULTIRES_USE_RENDER_PARAMS = 2,
  MULTIRES_ALLOC_PAINT_MASK = 4,
  MULTIRES_IGNORE_SIMPLIFY = 8,
};
ENUM_OPERATORS(MultiresFlags, MULTIRES_IGNORE_SIMPLIFY);

DerivedMesh *multires_make_derived_from_derived(
    DerivedMesh *dm, MultiresModifierData *mmd, Scene *scene, Object *ob, MultiresFlags flags);

MultiresModifierData *find_multires_modifier_before(Scene *scene, ModifierData *lastmd);
/**
 * used for applying scale on mdisps layer and syncing subdivide levels when joining objects.
 * \param use_first: return first multi-res modifier if all multi-res'es are disabled.
 */
MultiresModifierData *get_multires_modifier(Scene *scene, Object *ob, bool use_first);
int multires_get_level(const Scene *scene,
                       const Object *ob,
                       const MultiresModifierData *mmd,
                       bool render,
                       bool ignore_simplify);

/**
 * Creates mesh with multi-res modifier applied on current object's deform mesh.
 */
Mesh *BKE_multires_create_mesh(Depsgraph *depsgraph, Object *object, MultiresModifierData *mmd);

/**
 * Get coordinates of a deformed base mesh which is an input to the given multi-res modifier.
 * \note The modifiers will be re-evaluated.
 */
blender::Array<blender::float3> BKE_multires_create_deformed_base_mesh_vert_coords(
    Depsgraph *depsgraph, Object *object, MultiresModifierData *mmd);

/**
 * \param direction: 1 for delete higher, 0 for lower (not implemented yet).
 */
void multiresModifier_del_levels(MultiresModifierData *mmd,
                                 Scene *scene,
                                 Object *object,
                                 int direction);
void multiresModifier_base_apply(Depsgraph *depsgraph, Object *object, MultiresModifierData *mmd);
int multiresModifier_rebuild_subdiv(Depsgraph *depsgraph,
                                    Object *object,
                                    MultiresModifierData *mmd,
                                    int rebuild_limit,
                                    bool switch_view_to_lower_level);
/**
 * If `ob_src` and `ob_dst` both have multi-res modifiers,
 * synchronize them such that `ob_dst` has the same total number of levels as `ob_src`.
 */
void multiresModifier_sync_levels_ex(Object *ob_dst,
                                     const MultiresModifierData *mmd_src,
                                     MultiresModifierData *mmd_dst);

void multires_stitch_grids(Object *);

void multiresModifier_scale_disp(Depsgraph *depsgraph, Scene *scene, Object *ob);
void multiresModifier_prepare_join(Depsgraph *depsgraph, Scene *scene, Object *ob, Object *to_ob);

int multires_mdisp_corners(const MDisps *s);

/**
 * Update multi-res data after topology changing.
 */
void multires_topology_changed(Mesh *mesh);

/**
 * Makes sure data from an external file is fully read.
 *
 * Since the multi-res data files only contain displacement vectors without knowledge about
 * subdivision level some extra work is needed. Namely make is to all displacement grids have
 * proper level and number of displacement vectors set.
 */
void multires_ensure_external_read(Mesh *mesh, int top_level);
void multiresModifier_ensure_external_read(Mesh *mesh, const MultiresModifierData *mmd);

/**** interpolation stuff ****/
/* Adapted from `sculptmode.c` */

void old_mdisps_bilinear(float out[3], float (*disps)[3], int st, float u, float v);
/**
 * Find per-corner coordinate with given per-face UV coord.
 */
int mdisp_rot_face_to_crn(int face_size, int face_side, float u, float v, float *x, float *y);

/* Reshaping, define in multires_reshape.cc */

bool multiresModifier_reshapeFromVertcos(Depsgraph *depsgraph,
                                         Object *object,
                                         MultiresModifierData *mmd,
                                         const float (*vert_coords)[3],
                                         int num_vert_coords);
/**
 * Returns truth on success, false otherwise.
 *
 * This function might fail in cases like source and destination not having
 * matched amount of vertices.
 */
bool multiresModifier_reshapeFromObject(Depsgraph *depsgraph,
                                        MultiresModifierData *mmd,
                                        Object *dst,
                                        Object *src);
bool multiresModifier_reshapeFromDeformModifier(Depsgraph *depsgraph,
                                                Object *ob,
                                                MultiresModifierData *mmd,
                                                ModifierData *deform_md);
bool multiresModifier_reshapeFromCCG(int tot_level, Mesh *coarse_mesh, SubdivCCG *subdiv_ccg);

/* Subdivide multi-res displacement once. */

enum eMultiresSubdivideModeType {
  MULTIRES_SUBDIVIDE_CATMULL_CLARK,
  MULTIRES_SUBDIVIDE_SIMPLE,
  MULTIRES_SUBDIVIDE_LINEAR,
};

void multiresModifier_subdivide(Object *object,
                                MultiresModifierData *mmd,
                                eMultiresSubdivideModeType mode);
void multires_subdivide_create_tangent_displacement_linear_grids(Object *object,
                                                                 MultiresModifierData *mmd);

/**
 * Subdivide displacement to the given level.
 * If level is lower than the current top level nothing happens.
 */
void multiresModifier_subdivide_to_level(Object *object,
                                         MultiresModifierData *mmd,
                                         int top_level,
                                         eMultiresSubdivideModeType mode);

/* Subdivision integration, defined in multires_subdiv.cc */

void BKE_multires_subdiv_settings_init(blender::bke::subdiv::Settings *settings,
                                       const MultiresModifierData *mmd);

/* TODO(sergey): Replace this set of boolean flags with bitmask. */
void BKE_multires_subdiv_mesh_settings_init(blender::bke::subdiv::ToMeshSettings *mesh_settings,
                                            const Scene *scene,
                                            const Object *object,
                                            const MultiresModifierData *mmd,
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
void multires_do_versions_simple_to_catmull_clark(Object *object, MultiresModifierData *mmd);

#include "intern/multires_inline.hh"
