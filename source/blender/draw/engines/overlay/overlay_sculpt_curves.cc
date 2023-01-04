/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "draw_cache_impl.h"
#include "overlay_private.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"

void OVERLAY_sculpt_curves_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->sculpt_curves_selection_ps, state | pd->clipping_state);

  GPUShader *sh = OVERLAY_shader_sculpt_curves_selection();
  pd->sculpt_curves_selection_grp = DRW_shgroup_create(sh, psl->sculpt_curves_selection_ps);
  DRWShadingGroup *grp = pd->sculpt_curves_selection_grp;

  /* Reuse the same mask opacity from sculpt mode, since it wasn't worth it to add a different
   * property yet. */
  DRW_shgroup_uniform_float_copy(grp, "selection_opacity", pd->overlay.sculpt_mode_mask_opacity);
}

static bool everything_selected(const Curves &curves_id)
{
  using namespace blender;
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
  const VArray<bool> selection = curves.attributes().lookup_or_default<bool>(
      ".selection", ATTR_DOMAIN_POINT, true);
  return selection.is_single() && selection.get_internal_single();
}

void OVERLAY_sculpt_curves_cache_populate(OVERLAY_Data *vedata, Object *object)
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

void OVERLAY_sculpt_curves_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(pd->painting.in_front ? fbl->overlay_in_front_fb :
                                                 fbl->overlay_default_fb);
  }

  DRW_draw_pass(psl->sculpt_curves_selection_ps);
}
