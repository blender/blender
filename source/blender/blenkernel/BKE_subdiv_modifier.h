/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hardcoded for until GPU shaders are automatically generated, then we will have a more
 * programmatic way of detecting this. */
#define MAX_GPU_SUBDIV_SSBOS 12

struct Mesh;
struct Object;
struct Scene;
struct Subdiv;
struct SubdivSettings;
struct SubsurfModifierData;

void BKE_subsurf_modifier_subdiv_settings_init(struct SubdivSettings *settings,
                                               const struct SubsurfModifierData *smd,
                                               bool use_render_params);

bool BKE_subsurf_modifier_use_custom_loop_normals(const struct SubsurfModifierData *smd,
                                                  const struct Mesh *mesh);

/**
 * Return true if GPU subdivision evaluation is disabled by force due to incompatible mesh or
 * modifier settings. This will only return true if GPU subdivision is enabled in the preferences
 * and supported by the GPU. It is mainly useful for showing UI messages.
 */
bool BKE_subsurf_modifier_force_disable_gpu_evaluation_for_mesh(
    const struct SubsurfModifierData *smd, const struct Mesh *mesh);
/**
 * \param skip_check_is_last: When true, we assume that the modifier passed is the last enabled
 * modifier in the stack.
 */
bool BKE_subsurf_modifier_can_do_gpu_subdiv_ex(const struct Scene *scene,
                                               const struct Object *ob,
                                               const struct Mesh *mesh,
                                               const struct SubsurfModifierData *smd,
                                               int required_mode,
                                               bool skip_check_is_last);

bool BKE_subsurf_modifier_can_do_gpu_subdiv(const struct Scene *scene,
                                            const struct Object *ob,
                                            const struct Mesh *mesh,
                                            int required_mode);

extern void (*BKE_subsurf_modifier_free_gpu_cache_cb)(struct Subdiv *subdiv);

/**
 * Main goal of this function is to give usable subdivision surface descriptor
 * which matches settings and topology.
 */
struct Subdiv *BKE_subsurf_modifier_subdiv_descriptor_ensure(
    const struct SubsurfModifierData *smd,
    const struct SubdivSettings *subdiv_settings,
    const struct Mesh *mesh,
    bool for_draw_code);

struct SubsurfRuntimeData *BKE_subsurf_modifier_ensure_runtime(struct SubsurfModifierData *smd);

/**
 * Return the #ModifierMode required for the evaluation of the subsurf modifier,
 * which should be used to check if the modifier is enabled.
 */
int BKE_subsurf_modifier_eval_required_mode(bool is_final_render, bool is_edit_mode);

#ifdef __cplusplus
}
#endif
