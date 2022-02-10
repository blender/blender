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

struct Mesh;
struct Object;
struct Scene;
struct Subdiv;
struct SubdivSettings;
struct SubsurfModifierData;

void BKE_subsurf_modifier_subdiv_settings_init(struct SubdivSettings *settings,
                                               const struct SubsurfModifierData *smd,
                                               bool use_render_params);

/**
 * \param skip_check_is_last: When true, we assume that the modifier passed is the last enabled
 * modifier in the stack.
 */
bool BKE_subsurf_modifier_can_do_gpu_subdiv_ex(const struct Scene *scene,
                                               const struct Object *ob,
                                               const struct SubsurfModifierData *smd,
                                               int required_mode,
                                               bool skip_check_is_last);

bool BKE_subsurf_modifier_can_do_gpu_subdiv(const struct Scene *scene,
                                            const struct Object *ob,
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
