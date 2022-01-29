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

#include "BKE_subdiv_modifier.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_modifier.h"
#include "BKE_subdiv.h"

#include "GPU_capabilities.h"
#include "GPU_context.h"

#include "opensubdiv_capi.h"

void BKE_subsurf_modifier_subdiv_settings_init(SubdivSettings *settings,
                                               const SubsurfModifierData *smd,
                                               const bool use_render_params)
{
  const int requested_levels = (use_render_params) ? smd->renderLevels : smd->levels;

  settings->is_simple = (smd->subdivType == SUBSURF_TYPE_SIMPLE);
  settings->is_adaptive = !(smd->flags & eSubsurfModifierFlag_UseRecursiveSubdivision);
  settings->level = settings->is_simple ?
                        1 :
                        (settings->is_adaptive ? smd->quality : requested_levels);
  settings->use_creases = (smd->flags & eSubsurfModifierFlag_UseCrease);
  settings->vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      smd->boundary_smooth);
  settings->fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      smd->uv_smooth);
}

static ModifierData *modifier_get_last_enabled_for_mode(const Scene *scene,
                                                        const Object *ob,
                                                        int required_mode)
{
  ModifierData *md = ob->modifiers.last;

  while (md) {
    if (BKE_modifier_is_enabled(scene, md, required_mode)) {
      break;
    }

    md = md->prev;
  }

  return md;
}

bool BKE_subsurf_modifier_can_do_gpu_subdiv_ex(const Scene *scene,
                                               const Object *ob,
                                               const SubsurfModifierData *smd,
                                               int required_mode,
                                               bool skip_check_is_last)
{
  if ((U.gpu_flag & USER_GPU_FLAG_SUBDIVISION_EVALUATION) == 0) {
    return false;
  }

  if (!skip_check_is_last) {
    ModifierData *md = modifier_get_last_enabled_for_mode(scene, ob, required_mode);
    if (md != (const ModifierData *)smd) {
      return false;
    }
  }

  /* Only OpenGL is supported for OpenSubdiv evaluation for now. */
  if (GPU_backend_get_type() != GPU_BACKEND_OPENGL) {
    return false;
  }

  if (!(GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support())) {
    return false;
  }

  const int available_evaluators = openSubdiv_getAvailableEvaluators();
  if ((available_evaluators & OPENSUBDIV_EVALUATOR_GLSL_COMPUTE) == 0) {
    return false;
  }

  return true;
}

bool BKE_subsurf_modifier_can_do_gpu_subdiv(const Scene *scene,
                                            const Object *ob,
                                            int required_mode)
{
  ModifierData *md = modifier_get_last_enabled_for_mode(scene, ob, required_mode);

  if (!md) {
    return false;
  }

  if (md->type != eModifierType_Subsurf) {
    return false;
  }

  return BKE_subsurf_modifier_can_do_gpu_subdiv_ex(
      scene, ob, (SubsurfModifierData *)md, required_mode, true);
}

void (*BKE_subsurf_modifier_free_gpu_cache_cb)(Subdiv *subdiv) = NULL;

Subdiv *BKE_subsurf_modifier_subdiv_descriptor_ensure(const SubsurfModifierData *smd,
                                                      const SubdivSettings *subdiv_settings,
                                                      const Mesh *mesh,
                                                      const bool for_draw_code)
{
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  if (runtime_data->subdiv && runtime_data->set_by_draw_code != for_draw_code) {
    BKE_subdiv_free(runtime_data->subdiv);
    runtime_data->subdiv = NULL;
  }
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(runtime_data->subdiv, subdiv_settings, mesh);
  runtime_data->subdiv = subdiv;
  runtime_data->set_by_draw_code = for_draw_code;
  return subdiv;
}

SubsurfRuntimeData *BKE_subsurf_modifier_ensure_runtime(SubsurfModifierData *smd)
{
  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  if (runtime_data == NULL) {
    runtime_data = MEM_callocN(sizeof(*runtime_data), "subsurf runtime");
    smd->modifier.runtime = runtime_data;
  }
  return runtime_data;
}

int BKE_subsurf_modifier_eval_required_mode(bool is_final_render, bool is_edit_mode)
{
  if (is_final_render) {
    return eModifierMode_Render;
  }

  return eModifierMode_Realtime | (is_edit_mode ? eModifierMode_Editmode : 0);
}
