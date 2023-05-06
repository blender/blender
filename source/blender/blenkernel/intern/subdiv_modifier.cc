/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

#include "BKE_subdiv_modifier.h"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_subdiv.h"

#include "GPU_capabilities.h"
#include "GPU_context.h"

#include "opensubdiv_capi.h"

SubdivSettings BKE_subsurf_modifier_settings_init(const SubsurfModifierData *smd,
                                                  const bool use_render_params)
{
  const int requested_levels = (use_render_params) ? smd->renderLevels : smd->levels;

  SubdivSettings settings{};
  settings.is_simple = (smd->subdivType == SUBSURF_TYPE_SIMPLE);
  settings.is_adaptive = !(smd->flags & eSubsurfModifierFlag_UseRecursiveSubdivision);
  settings.level = settings.is_simple ? 1 :
                                        (settings.is_adaptive ? smd->quality : requested_levels);
  settings.use_creases = (smd->flags & eSubsurfModifierFlag_UseCrease);
  settings.vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      smd->boundary_smooth);
  settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      smd->uv_smooth);

  return settings;
}

bool BKE_subsurf_modifier_runtime_init(SubsurfModifierData *smd, const bool use_render_params)
{
  SubdivSettings settings = BKE_subsurf_modifier_settings_init(smd, use_render_params);

  SubsurfRuntimeData *runtime_data = (SubsurfRuntimeData *)smd->modifier.runtime;
  if (settings.level == 0) {
    /* Modifier is effectively disabled, but still update settings if runtime data
     * was already allocated. */
    if (runtime_data) {
      runtime_data->settings = settings;

      runtime_data->used_cpu = runtime_data->used_gpu = 0;
    }

    return false;
  }

  /* Allocate runtime data if it did not exist yet. */
  if (runtime_data == nullptr) {
    runtime_data = MEM_cnew<SubsurfRuntimeData>(__func__);
    smd->modifier.runtime = runtime_data;
  }
  runtime_data->settings = settings;
  return true;
}

static ModifierData *modifier_get_last_enabled_for_mode(const Scene *scene,
                                                        const Object *ob,
                                                        int required_mode)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  while (md) {
    if (BKE_modifier_is_enabled(scene, md, required_mode)) {
      break;
    }

    md = md->prev;
  }

  return md;
}

bool BKE_subsurf_modifier_use_custom_loop_normals(const SubsurfModifierData *smd, const Mesh *mesh)
{
  return (smd->flags & eSubsurfModifierFlag_UseCustomNormals) && (mesh->flag & ME_AUTOSMOOTH) &&
         CustomData_has_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL);
}

static bool subsurf_modifier_use_autosmooth_or_split_normals(const SubsurfModifierData *smd,
                                                             const Mesh *mesh)
{
  return (mesh->flag & ME_AUTOSMOOTH) || BKE_subsurf_modifier_use_custom_loop_normals(smd, mesh);
}

static bool is_subdivision_evaluation_possible_on_gpu()
{
  /* Only OpenGL is supported for OpenSubdiv evaluation for now. */
  if (GPU_backend_get_type() != GPU_BACKEND_OPENGL) {
    return false;
  }

  if (!(GPU_compute_shader_support() && GPU_shader_storage_buffer_objects_support())) {
    return false;
  }

  if (GPU_max_compute_shader_storage_blocks() < MAX_GPU_SUBDIV_SSBOS) {
    return false;
  }

  return true;
}

bool BKE_subsurf_modifier_force_disable_gpu_evaluation_for_mesh(const SubsurfModifierData *smd,
                                                                const Mesh *mesh)
{
  if ((U.gpu_flag & USER_GPU_FLAG_SUBDIVISION_EVALUATION) == 0) {
    /* GPU subdivision is explicitly disabled, so we don't force it. */
    return false;
  }

  if (!is_subdivision_evaluation_possible_on_gpu()) {
    /* The GPU type is not compatible with the subdivision. */
    return false;
  }

  return subsurf_modifier_use_autosmooth_or_split_normals(smd, mesh);
}

bool BKE_subsurf_modifier_can_do_gpu_subdiv(const Scene *scene,
                                            const Object *ob,
                                            const Mesh *mesh,
                                            const SubsurfModifierData *smd,
                                            int required_mode)
{
  if ((U.gpu_flag & USER_GPU_FLAG_SUBDIVISION_EVALUATION) == 0) {
    return false;
  }

  /* Deactivate GPU subdivision if autosmooth or custom split normals are used as those are
   * complicated to support on GPU, and should really be separate workflows. */
  if (subsurf_modifier_use_autosmooth_or_split_normals(smd, mesh)) {
    return false;
  }

  ModifierData *md = modifier_get_last_enabled_for_mode(scene, ob, required_mode);
  if (md != (const ModifierData *)smd) {
    return false;
  }

  return is_subdivision_evaluation_possible_on_gpu();
}

bool BKE_subsurf_modifier_has_gpu_subdiv(const Mesh *mesh)
{
  SubsurfRuntimeData *runtime_data = mesh->runtime->subsurf_runtime_data;
  return runtime_data && runtime_data->has_gpu_subdiv;
}

void (*BKE_subsurf_modifier_free_gpu_cache_cb)(Subdiv *subdiv) = nullptr;

Subdiv *BKE_subsurf_modifier_subdiv_descriptor_ensure(SubsurfRuntimeData *runtime_data,
                                                      const Mesh *mesh,
                                                      const bool for_draw_code)
{
  if (for_draw_code) {
    runtime_data->used_gpu = 2; /* countdown in frames */

    return runtime_data->subdiv_gpu = BKE_subdiv_update_from_mesh(
               runtime_data->subdiv_gpu, &runtime_data->settings, mesh);
  }
  runtime_data->used_cpu = 2;
  return runtime_data->subdiv_cpu = BKE_subdiv_update_from_mesh(
             runtime_data->subdiv_cpu, &runtime_data->settings, mesh);
}

int BKE_subsurf_modifier_eval_required_mode(bool is_final_render, bool is_edit_mode)
{
  if (is_final_render) {
    return eModifierMode_Render;
  }

  return eModifierMode_Realtime | (is_edit_mode ? int(eModifierMode_Editmode) : 0);
}
