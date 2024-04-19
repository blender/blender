/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"

void BKE_multires_subdiv_settings_init(blender::bke::subdiv::Settings *settings,
                                       const MultiresModifierData *mmd)
{
  settings->is_simple = false;
  settings->is_adaptive = true;
  settings->level = settings->is_simple ? 1 : mmd->quality;
  settings->use_creases = (mmd->flags & eMultiresModifierFlag_UseCrease);
  settings->vtx_boundary_interpolation =
      blender::bke::subdiv::vtx_boundary_interpolation_from_subsurf(mmd->boundary_smooth);
  settings->fvar_linear_interpolation = blender::bke::subdiv::fvar_interpolation_from_uv_smooth(
      mmd->uv_smooth);
}

void BKE_multires_subdiv_mesh_settings_init(blender::bke::subdiv::ToMeshSettings *mesh_settings,
                                            const Scene *scene,
                                            const Object *object,
                                            const MultiresModifierData *mmd,
                                            const bool use_render_params,
                                            const bool ignore_simplify,
                                            const bool ignore_control_edges)
{
  const int level = multires_get_level(scene, object, mmd, use_render_params, ignore_simplify);
  mesh_settings->resolution = (1 << level) + 1;
  mesh_settings->use_optimal_display = (mmd->flags & eMultiresModifierFlag_ControlEdges) &&
                                       !ignore_control_edges;
}
