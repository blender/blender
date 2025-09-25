/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "ED_view3d.hh"

#include "DNA_material_types.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_object.hh"

#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_memblock.h"

#include "IMB_colormanagement.hh"

#include "gpencil_engine_private.hh"

#include "DEG_depsgraph.hh"

#include "UI_resources.hh"

namespace blender::draw::gpencil {

/* -------------------------------------------------------------------- */
/** \name Object
 * \{ */

tObject *gpencil_object_cache_add(Instance *inst,
                                  Object *ob,
                                  const bool is_stroke_order_3d,
                                  const Bounds<float3> bounds)
{
  tObject *tgp_ob = static_cast<tObject *>(BLI_memblock_alloc(inst->gp_object_pool));

  tgp_ob->layers.first = tgp_ob->layers.last = nullptr;
  tgp_ob->vfx.first = tgp_ob->vfx.last = nullptr;
  tgp_ob->camera_z = dot_v3v3(inst->camera_z_axis, ob->object_to_world().location());
  tgp_ob->is_drawmode3d = is_stroke_order_3d;

  /* Check if any material with holdout flag enabled. */
  tgp_ob->do_mat_holdout = false;
  const int tot_materials = BKE_object_material_used_with_fallback_eval(*ob);
  for (int i = 0; i < tot_materials; i++) {
    MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, i + 1);
    if (((gp_style != nullptr) && (gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT)) ||
        (gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT))
    {
      tgp_ob->do_mat_holdout = true;
      break;
    }
  }

  /* Find the normal most likely to represent the gpObject. */
  /* TODO: This does not work quite well if you use
   * strokes not aligned with the object axes. Maybe we could try to
   * compute the minimum axis of all strokes. But this would be more
   * computationally heavy and should go into the GPData evaluation. */
  float3 size = (bounds.max - bounds.min) * 0.5f;
  float3 center = math::midpoint(bounds.min, bounds.max);
  /* Convert bbox to matrix */
  float mat[4][4];
  unit_m4(mat);
  copy_v3_v3(mat[3], center);
  /* Avoid division by 0.0 later. */
  add_v3_fl(size, 1e-8f);
  rescale_m4(mat, size);
  /* BBox space to World. */
  mul_m4_m4m4(mat, ob->object_to_world().ptr(), mat);
  if (View::default_get().is_persp()) {
    /* BBox center to camera vector. */
    sub_v3_v3v3(tgp_ob->plane_normal, inst->camera_pos, mat[3]);
  }
  else {
    copy_v3_v3(tgp_ob->plane_normal, inst->camera_z_axis);
  }
  /* World to BBox space. */
  invert_m4(mat);
  /* Normalize the vector in BBox space. */
  mul_mat3_m4_v3(mat, tgp_ob->plane_normal);
  normalize_v3(tgp_ob->plane_normal);

  transpose_m4(mat);
  /* mat is now a "normal" matrix which will transform
   * BBox space normal to world space. */
  mul_mat3_m4_v3(mat, tgp_ob->plane_normal);
  normalize_v3(tgp_ob->plane_normal);

  /* Define a matrix that will be used to render a triangle to merge the depth of the rendered
   * gpencil object with the rest of the scene. */
  unit_m4(tgp_ob->plane_mat);
  copy_v3_v3(tgp_ob->plane_mat[2], tgp_ob->plane_normal);
  orthogonalize_m4(tgp_ob->plane_mat, 2);
  mul_mat3_m4_v3(ob->object_to_world().ptr(), size);
  float radius = len_v3(size);
  mul_m4_v3(ob->object_to_world().ptr(), center);
  rescale_m4(tgp_ob->plane_mat, float3{radius, radius, radius});
  copy_v3_v3(tgp_ob->plane_mat[3], center);

  /* Add to corresponding list if is in front. */
  if (ob->dtx & OB_DRAW_IN_FRONT) {
    BLI_LINKS_APPEND(&inst->tobjects_infront, tgp_ob);
  }
  else {
    BLI_LINKS_APPEND(&inst->tobjects, tgp_ob);
  }

  return tgp_ob;
}

#define SORT_IMPL_LINKTYPE tObject

#define SORT_IMPL_FUNC gpencil_tobject_sort_fn_r
#include "../../blenlib/intern/list_sort_impl.h"
#undef SORT_IMPL_FUNC

#undef SORT_IMPL_LINKTYPE

static int gpencil_tobject_dist_sort(const void *a, const void *b)
{
  const tObject *ob_a = (const tObject *)a;
  const tObject *ob_b = (const tObject *)b;
  /* Reminder, camera_z is negative in front of the camera. */
  if (ob_a->camera_z > ob_b->camera_z) {
    return 1;
  }
  if (ob_a->camera_z < ob_b->camera_z) {
    return -1;
  }

  return 0;
}

void gpencil_object_cache_sort(Instance *inst)
{
  if (inst->is_sorted) {
    return;
  }
  /* Sort object by distance to the camera. */
  if (inst->tobjects.first) {
    inst->tobjects.first = gpencil_tobject_sort_fn_r(inst->tobjects.first,
                                                     gpencil_tobject_dist_sort);
    /* Relink last pointer. */
    while (inst->tobjects.last->next) {
      inst->tobjects.last = inst->tobjects.last->next;
    }
  }
  if (inst->tobjects_infront.first) {
    inst->tobjects_infront.first = gpencil_tobject_sort_fn_r(inst->tobjects_infront.first,
                                                             gpencil_tobject_dist_sort);
    /* Relink last pointer. */
    while (inst->tobjects_infront.last->next) {
      inst->tobjects_infront.last = inst->tobjects_infront.last->next;
    }
  }

  /* Join both lists, adding in front. */
  if (inst->tobjects_infront.first != nullptr) {
    if (inst->tobjects.last != nullptr) {
      inst->tobjects.last->next = inst->tobjects_infront.first;
      inst->tobjects.last = inst->tobjects_infront.last;
      inst->tobjects_infront.first = inst->tobjects.last = nullptr;
    }
    else {
      /* Only in front objects. */
      inst->tobjects.first = inst->tobjects_infront.first;
      inst->tobjects.last = inst->tobjects_infront.last;
      inst->tobjects_infront.first = inst->tobjects.last = nullptr;
    }
  }
  inst->is_sorted = true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Layer
 * \{ */

static float grease_pencil_layer_final_opacity_get(const Instance *inst,
                                                   const Object *ob,
                                                   const GreasePencil &grease_pencil,
                                                   const bke::greasepencil::Layer &layer)
{
  const bool is_obact = ((inst->obact) && (inst->obact == ob));
  const bool is_fade = (inst->fade_layer_opacity > -1.0f) && (is_obact) &&
                       !grease_pencil.is_layer_active(&layer);

  /* Defines layer opacity. For active object depends of layer opacity factor, and
   * for no active object, depends if the fade grease pencil objects option is enabled. */
  if (!inst->is_render) {
    if (is_obact && is_fade) {
      return layer.opacity * inst->fade_layer_opacity;
    }
    if (!is_obact && (inst->fade_gp_object_opacity > -1.0f)) {
      return layer.opacity * inst->fade_gp_object_opacity;
    }
  }
  return layer.opacity;
}

static float4 grease_pencil_layer_final_tint_and_alpha_get(const Instance *inst,
                                                           const GreasePencil &grease_pencil,
                                                           const int onion_id,
                                                           float *r_alpha)
{
  const bool use_onion = (onion_id != 0);
  if (use_onion && inst->do_onion) {
    const bool use_onion_custom_col = (grease_pencil.onion_skinning_settings.flag &
                                       GP_ONION_SKINNING_USE_CUSTOM_COLORS) != 0;
    const bool use_onion_fade = (grease_pencil.onion_skinning_settings.flag &
                                 GP_ONION_SKINNING_USE_FADE) != 0;
    const bool use_next_col = onion_id > 0;

    const float onion_factor = grease_pencil.onion_skinning_settings.opacity;

    float3 color_next, color_prev;
    if (use_onion_custom_col) {
      color_next = float3(grease_pencil.onion_skinning_settings.color_after);
      color_prev = float3(grease_pencil.onion_skinning_settings.color_before);
    }
    else {
      UI_GetThemeColor3fv(TH_FRAME_AFTER, color_next);
      UI_GetThemeColor3fv(TH_FRAME_BEFORE, color_prev);
    }

    const float4 onion_col_custom = use_next_col ? float4(color_next, 1.0f) :
                                                   float4(color_prev, 1.0f);

    *r_alpha = use_onion_fade ? (1.0f / abs(onion_id)) : 0.5f;
    *r_alpha *= onion_factor;
    *r_alpha = (onion_factor > 0.0f) ? clamp_f(*r_alpha, 0.1f, 1.0f) :
                                       clamp_f(*r_alpha, 0.01f, 1.0f);
    *r_alpha *= inst->xray_alpha;

    return onion_col_custom;
  }

  /* Layer tint is not a property in GPv3 anymore. It's only used for onion skinning. The previous
   * property is replaced by a tint modifier during conversion. */
  float4 layer_tint(0.0f);
  if (GPENCIL_SIMPLIFY_TINT(inst->scene)) {
    layer_tint[3] = 0.0f;
  }
  *r_alpha = 1.0f;
  *r_alpha *= inst->xray_alpha;

  return layer_tint;
}

/* Random color by layer. */
static void grease_pencil_layer_random_color_get(const Object *ob,
                                                 const bke::greasepencil::Layer &layer,
                                                 float r_color[3])
{
  const float hsv_saturation = 0.7f;
  const float hsv_value = 0.6f;

  uint ob_hash = BLI_ghashutil_strhash_p_murmur(ob->id.name);
  uint gpl_hash = BLI_ghashutil_strhash_p_murmur(layer.name().c_str());
  float hue = BLI_hash_int_01(ob_hash * gpl_hash);
  const float hsv[3] = {hue, hsv_saturation, hsv_value};
  hsv_to_rgb_v(hsv, r_color);
  IMB_colormanagement_rec709_to_scene_linear(r_color, r_color);
}

tLayer *grease_pencil_layer_cache_get(tObject *tgp_ob, int layer_id, const bool skip_onion)
{
  BLI_assert(layer_id >= 0);
  for (tLayer *layer = tgp_ob->layers.first; layer != nullptr; layer = layer->next) {
    if (skip_onion && layer->is_onion) {
      continue;
    }
    if (layer->layer_id == layer_id) {
      return layer;
    }
  }
  return nullptr;
}

tLayer *grease_pencil_layer_cache_add(Instance *inst,
                                      const Object *ob,
                                      const bke::greasepencil::Layer &layer,
                                      const int onion_id,
                                      const bool is_used_as_mask,
                                      tObject *tgp_ob)

{
  using namespace bke::greasepencil;
  const GreasePencil &grease_pencil = DRW_object_get_data_for_drawing<GreasePencil>(*ob);

  const bool is_in_front = (ob->dtx & OB_DRAW_IN_FRONT);

  const bool override_vertcol = (inst->v3d_color_type != -1);
  /* In draw mode and vertex paint mode it's possible to draw vertex colors so we want to make sure
   * to render them. Otherwise this can lead to unexpected behavior. */
  const bool is_vert_col_mode = (inst->v3d_color_type == V3D_SHADING_VERTEX_COLOR) ||
                                (ob->mode & OB_MODE_VERTEX_PAINT) != 0 ||
                                (ob->mode & OB_MODE_PAINT_GREASE_PENCIL) != 0 || inst->is_render;
  const bool is_viewlayer_render = inst->is_render && !layer.view_layer_name().is_empty() &&
                                   STREQ(inst->view_layer->name, layer.view_layer_name().c_str());
  const bool disable_masks_render = is_viewlayer_render &&
                                    (layer.base.flag &
                                     GP_LAYER_TREE_NODE_DISABLE_MASKS_IN_VIEWLAYER) != 0;
  bool is_masked = !disable_masks_render && layer.use_masks() &&
                   !BLI_listbase_is_empty(&layer.masks);

  const float vert_col_opacity = (override_vertcol) ?
                                     (is_vert_col_mode ? inst->vertex_paint_opacity : 0.0f) :
                                     (inst->is_render ? 1.0f : inst->vertex_paint_opacity);
  /* If the layer is used as a mask (but is otherwise not visible in the render), render it with a
   * opacity of 0 so that it can still mask other layers. */
  const float layer_opacity = !is_used_as_mask ? grease_pencil_layer_final_opacity_get(
                                                     inst, ob, grease_pencil, layer) :
                                                 0.0f;

  float layer_alpha = inst->xray_alpha;
  const float4 layer_tint = grease_pencil_layer_final_tint_and_alpha_get(
      inst, grease_pencil, onion_id, &layer_alpha);

  /* Create the new layer descriptor. */
  int64_t id = inst->gp_layer_pool->append_and_get_index({});
  tLayer *tgp_layer = &(*inst->gp_layer_pool)[id];
  BLI_LINKS_APPEND(&tgp_ob->layers, tgp_layer);
  tgp_layer->layer_id = *grease_pencil.get_layer_index(layer);
  tgp_layer->is_onion = onion_id != 0;
  tgp_layer->mask_bits = nullptr;
  tgp_layer->mask_invert_bits = nullptr;
  tgp_layer->blend_ps = nullptr;

  /* Masking: Go through mask list and extract valid masks in a bitmap. */
  if (is_masked) {
    bool valid_mask = false;
    /* WARNING: only #GP_MAX_MASKBITS amount of bits.
     * TODO(fclem): Find a better system without any limitation. */
    tgp_layer->mask_bits = static_cast<BLI_bitmap *>(BLI_memblock_alloc(inst->gp_maskbit_pool));
    tgp_layer->mask_invert_bits = static_cast<BLI_bitmap *>(
        BLI_memblock_alloc(inst->gp_maskbit_pool));
    BLI_bitmap_set_all(tgp_layer->mask_bits, false, GP_MAX_MASKBITS);

    LISTBASE_FOREACH (GreasePencilLayerMask *, mask, &layer.masks) {
      if (mask->flag & GP_LAYER_MASK_HIDE) {
        continue;
      }
      const TreeNode *node = grease_pencil.find_node_by_name(mask->layer_name);
      if (node == nullptr) {
        continue;
      }
      const Layer &mask_layer = node->as_layer();
      if ((&mask_layer == &layer) || !mask_layer.is_visible()) {
        continue;
      }
      const int index = *grease_pencil.get_layer_index(mask_layer);
      if (index < GP_MAX_MASKBITS) {
        const bool invert = (mask->flag & GP_LAYER_MASK_INVERT) != 0;
        BLI_BITMAP_SET(tgp_layer->mask_bits, index, true);
        BLI_BITMAP_SET(tgp_layer->mask_invert_bits, index, invert);
        valid_mask = true;
      }
    }

    if (valid_mask) {
      inst->use_mask_fb = true;
    }
    else {
      tgp_layer->mask_bits = nullptr;
    }
    is_masked = valid_mask;
  }

  /* Blending: Force blending for masked layer. */
  if (is_masked || (layer.blend_mode != GP_LAYER_BLEND_NONE) || (layer_opacity < 1.0f)) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL;
    switch (layer.blend_mode) {
      case GP_LAYER_BLEND_NONE:
        state |= DRW_STATE_BLEND_ALPHA_PREMUL;
        break;
      case GP_LAYER_BLEND_ADD:
        state |= DRW_STATE_BLEND_ADD_FULL;
        break;
      case GP_LAYER_BLEND_SUBTRACT:
        state |= DRW_STATE_BLEND_SUB;
        break;
      case GP_LAYER_BLEND_MULTIPLY:
      case GP_LAYER_BLEND_DIVIDE:
      case GP_LAYER_BLEND_HARDLIGHT:
        state |= DRW_STATE_BLEND_MUL;
        break;
    }

    if (ELEM(layer.blend_mode, GP_LAYER_BLEND_SUBTRACT, GP_LAYER_BLEND_HARDLIGHT)) {
      /* For these effect to propagate, we need a signed floating point buffer. */
      inst->use_signed_fb = true;
    }

    if (tgp_layer->blend_ps == nullptr) {
      tgp_layer->blend_ps = std::make_unique<PassSimple>("GPencil Blend Layer");
    }
    PassSimple &pass = *tgp_layer->blend_ps;
    pass.init();
    pass.state_set(state);
    pass.shader_set(ShaderCache::get().layer_blend.get());
    pass.push_constant("blend_mode", int(layer.blend_mode));
    pass.push_constant("blend_opacity", layer_opacity);
    pass.bind_texture("color_buf", &inst->color_layer_tx);
    pass.bind_texture("reveal_buf", &inst->reveal_layer_tx);
    pass.bind_texture("mask_buf", (is_masked) ? &inst->mask_tx : &inst->dummy_tx);
    pass.state_stencil(0xFF, 0xFF, 0xFF);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    if (layer.blend_mode == GP_LAYER_BLEND_HARDLIGHT) {
      /* We cannot do custom blending on Multi-Target frame-buffers.
       * Workaround by doing 2 passes. */
      pass.state_set((state & ~DRW_STATE_BLEND_MUL) | DRW_STATE_BLEND_ADD_FULL);
      pass.push_constant("blend_mode", 999);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }

    inst->use_layer_fb = true;
  }

  /* Geometry pass */
  {
    if (tgp_layer->geom_ps == nullptr) {
      tgp_layer->geom_ps = std::make_unique<PassSimple>("GPencil Layer");
    }

    PassSimple &pass = *tgp_layer->geom_ps;

    gpu::Texture **depth_tex = (is_in_front) ? &inst->dummy_depth : &inst->scene_depth_tx;
    gpu::Texture **mask_tex = (is_masked) ? &inst->mask_tx : &inst->dummy_tx;

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_BLEND_ALPHA_PREMUL;
    /* For 2D mode, we render all strokes with uniform depth (increasing with stroke id). */
    state |= tgp_ob->is_drawmode3d ? DRW_STATE_DEPTH_LESS_EQUAL : DRW_STATE_DEPTH_GREATER;
    /* Always write stencil. Only used as optimization for blending. */
    state |= DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS;

    pass.state_set(state);
    pass.shader_set(ShaderCache::get().geometry.get());
    pass.bind_texture("gp_scene_depth_tx", depth_tex);
    pass.bind_texture("gp_mask_tx", mask_tex);
    pass.push_constant("gp_normal", tgp_ob->plane_normal);
    pass.push_constant("gp_stroke_order3d", tgp_ob->is_drawmode3d);
    pass.push_constant("gp_vertex_color_opacity", vert_col_opacity);

    pass.bind_texture("gp_fill_tx", inst->dummy_tx);
    pass.bind_texture("gp_stroke_tx", inst->dummy_tx);

    /* If random color type, need color by layer. */
    float4 gpl_color;
    copy_v4_v4(gpl_color, layer_tint);
    if (inst->v3d_color_type == V3D_SHADING_RANDOM_COLOR) {
      grease_pencil_layer_random_color_get(ob, layer, gpl_color);
      gpl_color[3] = 1.0f;
    }
    pass.push_constant("gp_layer_tint", gpl_color);

    pass.push_constant("gp_layer_opacity", layer_alpha);
    pass.state_stencil(0xFF, 0xFF, 0xFF);
  }

  return tgp_layer;
}
/** \} */

}  // namespace blender::draw::gpencil
