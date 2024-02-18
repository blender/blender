/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"

#include "BLI_hash.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "IMB_colormanagement.hh"

#include "sculpt_intern.hh"

#include "IMB_imbuf.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

using namespace blender;
using namespace blender::bke::paint;

namespace blender::ed::sculpt_paint::color {
static void do_color_smooth_task(Object *ob, const Brush *brush, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    float smooth_color[4];
    float color[4];

    smooth::neighbor_color_average(ss, smooth_color, vd.vertex);

    SCULPT_vertex_color_get(ss, vd.vertex, color);
    blend_color_interpolate_float(color, color, smooth_color, fade);
    SCULPT_vertex_color_set(ss, vd.vertex, color);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_paint_brush_task(Object *ob,
                                const Brush *brush,
                                const float (*mat)[4],
                                float *wet_mix_sampled_color,
                                PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = fabsf(ss->cache->bstrength);

  const SculptAttribute *buffer_scl = ss->attrs.smear_previous;

  const bool do_accum = brush->flag & BRUSH_ACCUMULATE;

  PBVHVertexIter vd;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, ob, node, undo::Type::Color);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  float brush_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  copy_v3_v3(brush_color,
             ss->cache->invert ? BKE_brush_secondary_color_get(ss->scene, brush) :
                                 BKE_brush_color_get(ss->scene, brush));

  IMB_colormanagement_srgb_to_scene_linear_v3(brush_color, brush_color);

  /* get un-pressure-mapped alpha */
  float alpha = BKE_brush_alpha_get(ss->scene, brush);

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  if (brush->flag & BRUSH_USE_GRADIENT) {
    switch (brush->gradient_stroke_mode) {
      case BRUSH_GRADIENT_PRESSURE:
        BKE_colorband_evaluate(brush->gradient, ss->cache->pressure, brush_color);
        break;
      case BRUSH_GRADIENT_SPACING_REPEAT: {
        float coord = fmod(ss->cache->stroke_distance / brush->gradient_spacing, 1.0);
        BKE_colorband_evaluate(brush->gradient, coord, brush_color);
        break;
      }
      case BRUSH_GRADIENT_SPACING_CLAMP: {
        BKE_colorband_evaluate(
            brush->gradient, ss->cache->stroke_distance / brush->gradient_spacing, brush_color);
        break;
      }
    }
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_check_origdata(ss, vd.vertex);

    /* Check if we have a new stroke, in which we need to zero
     * our temp layer.  Do this here before the brush check
     * to ensure any geomtry dyntopo might subdivide has
     * valid state.
     */
    float *color_buffer = vertex_attr_ptr<float>(vd.vertex,
                                                 buffer_scl);  // mv->origcolor;
    if (blender::bke::sculpt::stroke_id_test(ss, vd.vertex, STROKEID_USER_PREV_COLOR)) {
      zero_v4(color_buffer);
    }

    bool affect_vertex = false;
    float distance_to_stroke_location = 0.0f;
    if (brush->tip_roundness < 1.0f) {
      affect_vertex = SCULPT_brush_test_cube(&test,
                                             vd.co,
                                             mat,
                                             brush->tip_roundness,
                                             brush->tip_scale_x,
                                             brush->falloff_shape != PAINT_FALLOFF_SHAPE_TUBE);
      distance_to_stroke_location = ss->cache->radius * test.dist;
    }
    else {
      affect_vertex = sculpt_brush_test_sq_fn(&test, vd.co);
      distance_to_stroke_location = sqrtf(test.dist);
    }

    if (!affect_vertex) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          distance_to_stroke_location,
                                                          vd.no,
                                                          vd.fno,
                                                          vd.mask,
                                                          vd.vertex,
                                                          thread_id,
                                                          &automask_data);

    /* Density. */
    float noise = 1.0f;
    const float density = ss->cache->paint_brush.density;
    if (density < 1.0f) {
      const float hash_noise = float(BLI_hash_int_01(ss->cache->density_seed * 1000 * vd.index));
      if (hash_noise > density) {
        noise = density * hash_noise;
        fade = fade * noise;
      }
    }

    /* Brush paint color, brush test falloff and flow. */
    float paint_color[4];
    float wet_mix_color[4];
    float buffer_color[4];

    mul_v4_v4fl(paint_color, brush_color, fade * ss->cache->paint_brush.flow);
    mul_v4_v4fl(wet_mix_color, wet_mix_sampled_color, fade * ss->cache->paint_brush.flow);

    /* Interpolate with the wet_mix color for wet paint mixing. */
    blend_color_interpolate_float(
        paint_color, paint_color, wet_mix_color, ss->cache->paint_brush.wet_mix);
    blend_color_mix_float(color_buffer, color_buffer, paint_color);

    /* Final mix over the original color using brush alpha. We apply auto-making again
     * at this point to avoid washing out non-binary masking modes like cavity masking. */
    float automasking = auto_mask::factor_get(
        ss->cache->automasking.get(), ss, vd.vertex, &automask_data);
    mul_v4_v4fl(buffer_color, color_buffer, alpha * automasking);

    float4 vcolor;
    SCULPT_vertex_color_get(ss, vd.vertex, vcolor);

    if (do_accum) {
      mul_v4_fl(buffer_color, fade);

      IMB_blend_color_float(vcolor, vcolor, buffer_color, IMB_BlendMode(brush->blend));
      vcolor[3] = 1.0f;
    }
    else {
      IMB_blend_color_float(vcolor,
                            vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_color),
                            buffer_color,
                            IMB_BlendMode(brush->blend));
    }

    vcolor = math::clamp(vcolor, 0.0f, 1.0f);
    SCULPT_vertex_color_set(ss, vd.vertex, vcolor);
  }
  BKE_pbvh_vertex_iter_end;
}

struct SampleWetPaintData {
  int tot_samples;
  float color[4];
};

static void do_sample_wet_paint_task(SculptSession *ss,
                                     const Brush *brush,
                                     PBVHNode *node,
                                     SampleWetPaintData *swptd)
{
  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  test.radius *= brush->wet_paint_radius_factor;
  test.radius_squared = test.radius * test.radius;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float col[4];
    SCULPT_vertex_color_get(ss, vd.vertex, col);

    add_v4_v4(swptd->color, col);
    swptd->tot_samples++;
  }
  BKE_pbvh_vertex_iter_end;
}

void do_paint_brush(PaintModeSettings *paint_mode_settings,
                    Scene *scene,
                    Sculpt *sd,
                    Object *ob,
                    Span<PBVHNode *> nodes,
                    Span<PBVHNode *> texnodes)
{
  if (SCULPT_use_image_paint_brush(paint_mode_settings, ob)) {
    SCULPT_do_paint_brush_image(paint_mode_settings, sd, ob, texnodes);
    return;
  }

  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_has_colors(ss)) {
    return;
  }

  BKE_sculpt_ensure_origcolor(ob);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      ss->cache->density_seed = float(BLI_hash_int_01(ss->cache->location[0] * 1000));
    }
    return;
  }

  BKE_curvemapping_init(brush->curve);

  float mat[4][4];

  /* If the brush is round the tip does not need to be aligned to the surface, so this saves a
   * whole iteration over the affected nodes. */
  if (brush->tip_roundness < 1.0f) {
    SCULPT_cube_tip_init(sd, ob, brush, mat);

    if (is_zero_m4(mat)) {
      return;
    }
  }

  float brush_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  if (ss->cache->invert) {
    copy_v4_v4(brush_color, BKE_brush_secondary_color_get(scene, brush));
  }
  else {
    copy_v4_v4(brush_color, BKE_brush_color_get(scene, brush));
  }

  /* Smooth colors mode. */
  if (ss->cache->alt_smooth) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_color_smooth_task(ob, brush, nodes[i]);
      }
    });
    return;
  }

  /* Regular Paint mode. */

  /* Wet paint color sampling. */
  float4 wet_color(0);
  if (ss->cache->paint_brush.wet_mix > 0.0f) {
    const SampleWetPaintData swptd = threading::parallel_reduce(
        nodes.index_range(),
        1,
        SampleWetPaintData{},
        [&](const IndexRange range, SampleWetPaintData swptd) {
          for (const int i : range) {
            do_sample_wet_paint_task(ss, brush, nodes[i], &swptd);
          }
          return swptd;
        },
        [](const SampleWetPaintData &a, const SampleWetPaintData &b) {
          SampleWetPaintData joined{};
          joined.tot_samples = a.tot_samples + b.tot_samples;
          add_v4_v4v4(joined.color, a.color, b.color);
          return joined;
        });

    if (swptd.tot_samples > 0 && is_finite_v4(swptd.color)) {
      copy_v4_v4(wet_color, swptd.color);
      mul_v4_fl(wet_color, 1.0f / swptd.tot_samples);
      wet_color = math::clamp(wet_color, 0.0f, 1.0f);

      if (ss->cache->first_time) {
        copy_v4_v4(ss->cache->wet_mix_prev_color, wet_color);
      }
      blend_color_interpolate_float(wet_color,
                                    wet_color,
                                    ss->cache->wet_mix_prev_color,
                                    ss->cache->paint_brush.wet_persistence);
      copy_v4_v4(ss->cache->wet_mix_prev_color, wet_color);
      ss->cache->wet_mix_prev_color = math::clamp(ss->cache->wet_mix_prev_color, 0.0f, 1.0f);
    }
  }

  SculptAttributeParams params = {};
  SculptAttributeParams params_id = {};
  params.stroke_only = true;
  params_id.nointerp = params.stroke_only = true;

  BKE_sculpt_ensure_origcolor(ob);

  if (!ss->attrs.smear_previous) {
    ss->attrs.smear_previous = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_COLOR, SCULPT_ATTRIBUTE_NAME(smear_previous), &params);
  }

  SCULPT_stroke_id_ensure(ob);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_paint_brush_task(ob, brush, mat, wet_color, nodes[i]);
    }
  });
}

static void do_smear_brush_task(Object *ob, const Brush *brush, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  const float bstrength = ss->cache->bstrength;

  const float blend = brush->smear_deform_blend;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  float brush_delta[3];

  if (brush->flag & BRUSH_ANCHORED) {
    copy_v3_v3(brush_delta, ss->cache->grab_delta_symmetry);
  }
  else {
    sub_v3_v3v3(brush_delta, ss->cache->location, ss->cache->last_location);
  }

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    auto_mask::node_update(automask_data, vd);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask,
                                                                vd.vertex,
                                                                thread_id,
                                                                &automask_data);

    float current_disp[3];
    float current_disp_norm[3];
    float interp_color[4];
    float *prev_color = vertex_attr_ptr<float>(vd.vertex, ss->attrs.smear_previous);

    copy_v4_v4(interp_color, prev_color);

    float no[3];
    SCULPT_vertex_normal_get(ss, vd.vertex, no);

    switch (brush->smear_deform_type) {
      case BRUSH_SMEAR_DEFORM_DRAG:
        copy_v3_v3(current_disp, brush_delta);
        break;
      case BRUSH_SMEAR_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss->cache->location, vd.co);
        break;
      case BRUSH_SMEAR_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss->cache->location);
        break;
    }

    /* Project into vertex plane. */
    madd_v3_v3fl(current_disp, no, -dot_v3v3(current_disp, no));

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, bstrength);

    float accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float totw = 0.0f;

    /*
     * NOTE: we have to do a nested iteration here to avoid
     * blocky artifacts on quad topologies.  The runtime cost
     * is not as bad as it seems due to neighbor iteration
     * in the sculpt code being cache bound; once the data is in
     * the cache iterating over it a few more times is not terribly
     * costly.
     */

    SculptVertexNeighborIter ni2;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni2) {
      const float *nco = SCULPT_vertex_co_get(ss, ni2.vertex);

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ni2.vertex, ni) {
        if (ni.index == vd.index) {
          continue;
        }

        float vertex_disp[3];
        float vertex_disp_norm[3];

        sub_v3_v3v3(vertex_disp, SCULPT_vertex_co_get(ss, ni.vertex), vd.co);

        /* Weight by how close we are to our target distance from vd.co. */
        float w = (1.0f + fabsf(len_v3(vertex_disp) / bstrength - 1.0f));

        /* TODO: use cotangents (or at least face areas) here. */
        float len = len_v3v3(SCULPT_vertex_co_get(ss, ni.vertex), nco);
        if (len > 0.0f) {
          len = bstrength / len;
        }
        else { /* Coincident point. */
          len = 1.0f;
        }

        /* Multiply weight with edge lengths (in the future this will be
         * cotangent weights or face areas). */
        w *= len;

        /* Build directional weight. */

        /* Project into vertex plane. */
        madd_v3_v3fl(vertex_disp, no, -dot_v3v3(no, vertex_disp));
        normalize_v3_v3(vertex_disp_norm, vertex_disp);

        if (dot_v3v3(current_disp_norm, vertex_disp_norm) >= 0.0f) {
          continue;
        }

        const float *neighbor_color = vertex_attr_ptr<const float>(ni.vertex,
                                                                   ss->attrs.smear_previous);
        float color_interp = -dot_v3v3(current_disp_norm, vertex_disp_norm);

        /* Square directional weight to get a somewhat sharper result. */
        w *= color_interp * color_interp;

        madd_v4_v4fl(accum, neighbor_color, w);
        totw += w;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni2);

    if (totw != 0.0f) {
      mul_v4_fl(accum, 1.0f / totw);
    }

    blend_color_mix_float(interp_color, interp_color, accum);

    float col[4];
    SCULPT_vertex_color_get(ss, vd.vertex, col);

    blend_color_interpolate_float(col, prev_color, interp_color, fade * blend);
    SCULPT_vertex_color_set(ss, vd.vertex, col);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_smear_store_prev_colors_task(SculptSession *ss, PBVHNode *node)
{
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_color_get(
        ss, vd.vertex, vertex_attr_ptr<float>(vd.vertex, ss->attrs.smear_previous));
  }
  BKE_pbvh_vertex_iter_end;
}

void do_smear_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_has_colors(ss) || ss->cache->bstrength == 0.0f) {
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);

  if (!ss->attrs.smear_previous) {
    SculptAttributeParams params = {};
    params.stroke_only = true;

    ss->attrs.smear_previous = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_COLOR, SCULPT_ATTRIBUTE_NAME(smear_previous), &params);
  }

  BKE_curvemapping_init(brush->curve);

  /* Smooth colors mode. */
  if (ss->cache->alt_smooth) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_color_smooth_task(ob, brush, nodes[i]);
      }
    });
  }
  else {
    /* Smear mode. */
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_smear_store_prev_colors_task(ss, nodes[i]);
      }
    });
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        do_smear_brush_task(ob, brush, nodes[i]);
      }
    });
  }
}

}  // namespace blender::ed::sculpt_paint::color
