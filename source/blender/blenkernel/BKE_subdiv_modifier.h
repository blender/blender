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
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

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

/* If skip_check_is_last is true, we assume that the modifier passed is the last enabled modifier
 * in the stack. */
bool BKE_subsurf_modifier_can_do_gpu_subdiv_ex(const struct Scene *scene,
                                               const struct Object *ob,
                                               const struct SubsurfModifierData *smd,
                                               int required_mode,
                                               bool skip_check_is_last);

bool BKE_subsurf_modifier_can_do_gpu_subdiv(const struct Scene *scene,
                                            const struct Object *ob,
                                            int required_mode);

extern void (*BKE_subsurf_modifier_free_gpu_cache_cb)(struct Subdiv *subdiv);

struct Subdiv *BKE_subsurf_modifier_subdiv_descriptor_ensure(
    const struct SubsurfModifierData *smd,
    const struct SubdivSettings *subdiv_settings,
    const struct Mesh *mesh,
    bool for_draw_code);

struct SubsurfRuntimeData *BKE_subsurf_modifier_ensure_runtime(struct SubsurfModifierData *smd);

/* Return the #ModifierMode required for the evaluation of the subsurf modifier, which should be
 * used to check if the modifier is enabled. */
int BKE_subsurf_modifier_eval_required_mode(bool is_final_render, bool is_edit_mode);

#ifdef __cplusplus
}
#endif
