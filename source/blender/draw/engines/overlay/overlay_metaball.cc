/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_meta_types.h"

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "ED_mball.h"

#include "overlay_private.hh"

void OVERLAY_metaball_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  OVERLAY_InstanceFormats *formats = OVERLAY_shader_instance_formats_get();

#define BUF_INSTANCE DRW_shgroup_call_buffer_instance

  for (int i = 0; i < 2; i++) {
    DRWState infront_state = (DRW_state_is_select() && (i == 1)) ? DRW_STATE_IN_FRONT_SELECT :
                                                                   DRWState(0);
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
    DRW_PASS_CREATE(psl->metaball_ps[i], state | pd->clipping_state | infront_state);

    /* Reuse armature shader as it's perfect to outline ellipsoids. */
    GPUVertFormat *format = formats->instance_bone;
    GPUShader *sh = OVERLAY_shader_armature_sphere(true);
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->metaball_ps[i]);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    pd->mball.handle[i] = BUF_INSTANCE(grp, format, DRW_cache_bone_point_wire_outline_get());
  }
}

static void metaball_instance_data_set(
    BoneInstanceData *data, Object *ob, const float *pos, const float radius, const float color[4])
{
  /* Bone point radius is 0.05. Compensate for that. */
  mul_v3_v3fl(data->mat[0], ob->object_to_world[0], radius / 0.05f);
  mul_v3_v3fl(data->mat[1], ob->object_to_world[1], radius / 0.05f);
  mul_v3_v3fl(data->mat[2], ob->object_to_world[2], radius / 0.05f);
  mul_v3_m4v3(data->mat[3], ob->object_to_world, pos);
  /* WATCH: Reminder, alpha is wire-size. */
  OVERLAY_bone_instance_data_set_color(data, color);
}

void OVERLAY_edit_metaball_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  const bool do_in_front = (ob->dtx & OB_DRAW_IN_FRONT) != 0;
  const bool is_select = DRW_state_is_select();
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  MetaBall *mb = static_cast<MetaBall *>(ob->data);

  const float *color;
  const float *col_radius = G_draw.block.color_mball_radius;
  const float *col_radius_select = G_draw.block.color_mball_radius_select;
  const float *col_stiffness = G_draw.block.color_mball_stiffness;
  const float *col_stiffness_select = G_draw.block.color_mball_stiffness_select;

  int select_id = 0;
  if (is_select) {
    select_id = ob->runtime.select_id;
  }

  LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
    const bool is_selected = (ml->flag & SELECT) != 0;
    const bool is_scale_radius = (ml->flag & MB_SCALE_RAD) != 0;
    float stiffness_radius = ml->rad * atanf(ml->s) / float(M_PI_2);
    BoneInstanceData instdata;

    if (is_select) {
      DRW_select_load_id(select_id | MBALLSEL_RADIUS);
    }
    color = (is_selected && is_scale_radius) ? col_radius_select : col_radius;
    metaball_instance_data_set(&instdata, ob, &ml->x, ml->rad, color);
    DRW_buffer_add_entry_struct(pd->mball.handle[do_in_front], &instdata);

    if (is_select) {
      DRW_select_load_id(select_id | MBALLSEL_STIFF);
    }
    color = (is_selected && !is_scale_radius) ? col_stiffness_select : col_stiffness;
    metaball_instance_data_set(&instdata, ob, &ml->x, stiffness_radius, color);
    DRW_buffer_add_entry_struct(pd->mball.handle[do_in_front], &instdata);

    select_id += 0x10000;
  }

  /* Needed so object centers and geometry are not detected as meta-elements. */
  if (is_select) {
    DRW_select_load_id(-1);
  }
}

void OVERLAY_metaball_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  const bool do_in_front = (ob->dtx & OB_DRAW_IN_FRONT) != 0;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  MetaBall *mb = static_cast<MetaBall *>(ob->data);
  const DRWContextState *draw_ctx = DRW_context_state_get();

  float *color;
  DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);

  LISTBASE_FOREACH (MetaElem *, ml, &mb->elems) {
    /* Draw radius only. */
    BoneInstanceData instdata;
    metaball_instance_data_set(&instdata, ob, &ml->x, ml->rad, color);
    DRW_buffer_add_entry_struct(pd->mball.handle[do_in_front], &instdata);
  }
}

void OVERLAY_metaball_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->metaball_ps[0]);
}

void OVERLAY_metaball_in_front_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->metaball_ps[1]);
}
