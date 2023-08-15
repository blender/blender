/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "draw_cache_impl.hh"
#include "overlay_private.hh"

#include "BKE_attribute.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"

#include "DEG_depsgraph_query.h"

void OVERLAY_sculpt_curves_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const View3DOverlay &overlay = vedata->stl->pd->overlay;

  /* Selection overlay. */
  {
    const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->sculpt_curves_selection_ps, state | pd->clipping_state);

    GPUShader *sh = OVERLAY_shader_sculpt_curves_selection();
    pd->sculpt_curves_selection_grp = DRW_shgroup_create(sh, psl->sculpt_curves_selection_ps);
    DRWShadingGroup *grp = pd->sculpt_curves_selection_grp;

    /* Reuse the same mask opacity from sculpt mode, since it wasn't worth it to add a different
     * property yet. */
    DRW_shgroup_uniform_float_copy(grp, "selection_opacity", pd->overlay.sculpt_mode_mask_opacity);
  }
  /* Cage overlay. */
  {
    const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
                           DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->sculpt_curves_cage_ps, state | pd->clipping_state);

    GPUShader *sh = OVERLAY_shader_sculpt_curves_cage();
    pd->sculpt_curves_cage_lines_grp = DRW_shgroup_create(sh, psl->sculpt_curves_cage_ps);
    DRW_shgroup_uniform_float_copy(
        pd->sculpt_curves_cage_lines_grp, "opacity", overlay.sculpt_curves_cage_opacity);
  }
}

static bool everything_selected(const Curves &curves_id)
{
  using namespace blender;
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
      ".selection", ATTR_DOMAIN_POINT, true);
  return selection.is_single() && selection.get_internal_single();
}

static void populate_selection_overlay(OVERLAY_Data *vedata, Object *object)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  Curves *curves = static_cast<Curves *>(object->data);

  /* As an optimization, return early if everything is selected. */
  if (everything_selected(*curves)) {
    return;
  }

  /* Retrieve the location of the texture. */
  bool is_point_domain;
  GPUVertBuf **texture = DRW_curves_texture_for_evaluated_attribute(
      curves, ".selection", &is_point_domain);
  if (texture == nullptr) {
    return;
  }

  /* Evaluate curves and their attributes if necessary. */
  DRWShadingGroup *grp = DRW_shgroup_curves_create_sub(
      object, pd->sculpt_curves_selection_grp, nullptr);
  if (*texture == nullptr) {
    return;
  }

  DRW_shgroup_uniform_bool_copy(grp, "is_point_domain", is_point_domain);
  DRW_shgroup_buffer_texture(grp, "selection_tx", *texture);
}

static void populate_edit_overlay(OVERLAY_Data *vedata, Object *object)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  Curves *curves = static_cast<Curves *>(object->data);

  GPUBatch *geom_lines = DRW_curves_batch_cache_get_edit_lines(curves);
  DRW_shgroup_call_no_cull(pd->sculpt_curves_cage_lines_grp, geom_lines, object);
}

void OVERLAY_sculpt_curves_cache_populate(OVERLAY_Data *vedata, Object *object)
{
  populate_selection_overlay(vedata, object);
  const View3DOverlay &overlay = vedata->stl->pd->overlay;
  if ((overlay.flag & V3D_OVERLAY_SCULPT_CURVES_CAGE) && overlay.sculpt_curves_cage_opacity > 0.0f)
  {
    populate_edit_overlay(vedata, object);
  }
}

void OVERLAY_sculpt_curves_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->sculpt_curves_selection_ps);
}

void OVERLAY_sculpt_curves_draw_wires(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->sculpt_curves_cage_ps);
}
