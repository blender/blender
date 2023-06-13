/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_prototypes.h"

#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_texture.h"

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "BLI_sys_types.h"
#include "ED_mesh.h" /* for face mask functions */

#include "DRW_select_buffer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "paint_intern.hh"

bool paint_convert_bb_to_rect(rcti *rect,
                              const float bb_min[3],
                              const float bb_max[3],
                              const ARegion *region,
                              RegionView3D *rv3d,
                              Object *ob)
{
  float projection_mat[4][4];
  int i, j, k;

  BLI_rcti_init_minmax(rect);

  /* return zero if the bounding box has non-positive volume */
  if (bb_min[0] > bb_max[0] || bb_min[1] > bb_max[1] || bb_min[2] > bb_max[2]) {
    return false;
  }

  ED_view3d_ob_project_mat_get(rv3d, ob, projection_mat);

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 2; j++) {
      for (k = 0; k < 2; k++) {
        float vec[3], proj[2];
        int proj_i[2];
        vec[0] = i ? bb_min[0] : bb_max[0];
        vec[1] = j ? bb_min[1] : bb_max[1];
        vec[2] = k ? bb_min[2] : bb_max[2];
        /* convert corner to screen space */
        ED_view3d_project_float_v2_m4(region, vec, proj, projection_mat);
        /* expand 2D rectangle */

        /* we could project directly to int? */
        proj_i[0] = proj[0];
        proj_i[1] = proj[1];

        BLI_rcti_do_minmax_v(rect, proj_i);
      }
    }
  }

  /* return false if the rectangle has non-positive area */
  return rect->xmin < rect->xmax && rect->ymin < rect->ymax;
}

void paint_calc_redraw_planes(float planes[4][4],
                              const ARegion *region,
                              Object *ob,
                              const rcti *screen_rect)
{
  BoundBox bb;
  rcti rect;

  /* use some extra space just in case */
  rect = *screen_rect;
  rect.xmin -= 2;
  rect.xmax += 2;
  rect.ymin -= 2;
  rect.ymax += 2;

  ED_view3d_clipping_calc(&bb, planes, region, ob, &rect);
}

float paint_calc_object_space_radius(ViewContext *vc, const float center[3], float pixel_radius)
{
  Object *ob = vc->obact;
  float delta[3], scale, loc[3];
  const float xy_delta[2] = {pixel_radius, 0.0f};

  mul_v3_m4v3(loc, ob->object_to_world, center);

  const float zfac = ED_view3d_calc_zfac(vc->rv3d, loc);
  ED_view3d_win_to_delta(vc->region, xy_delta, zfac, delta);

  scale = fabsf(mat4_to_scale(ob->object_to_world));
  scale = (scale == 0.0f) ? 1.0f : scale;

  return len_v3(delta) / scale;
}

bool paint_get_tex_pixel(const MTex *mtex,
                         float u,
                         float v,
                         ImagePool *pool,
                         int thread,
                         /* Return arguments. */
                         float *r_intensity,
                         float r_rgba[4])
{
  const float co[3] = {u, v, 0.0f};
  float intensity;
  const bool has_rgb = RE_texture_evaluate(
      mtex, co, thread, pool, false, false, &intensity, r_rgba);
  *r_intensity = intensity;

  if (!has_rgb) {
    r_rgba[0] = intensity;
    r_rgba[1] = intensity;
    r_rgba[2] = intensity;
    r_rgba[3] = 1.0f;
  }

  return has_rgb;
}

void paint_stroke_operator_properties(wmOperatorType *ot)
{
  static const EnumPropertyItem stroke_mode_items[] = {
      {BRUSH_STROKE_NORMAL, "NORMAL", 0, "Regular", "Apply brush normally"},
      {BRUSH_STROKE_INVERT,
       "INVERT",
       0,
       "Invert",
       "Invert action of brush for duration of stroke"},
      {BRUSH_STROKE_SMOOTH,
       "SMOOTH",
       0,
       "Smooth",
       "Switch brush to smooth mode for duration of stroke"},
      {0},
  };

  PropertyRNA *prop;

  prop = RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));

  RNA_def_enum(ot->srna,
               "mode",
               stroke_mode_items,
               BRUSH_STROKE_NORMAL,
               "Stroke Mode",
               "Action taken when a paint stroke is made");
}

/* 3D Paint */

static void imapaint_project(const float matrix[4][4], const float co[3], float pco[4])
{
  copy_v3_v3(pco, co);
  pco[3] = 1.0f;

  mul_m4_v4(matrix, pco);
}

static void imapaint_tri_weights(float matrix[4][4],
                                 const int view[4],
                                 const float v1[3],
                                 const float v2[3],
                                 const float v3[3],
                                 const float co[2],
                                 float w[3])
{
  float pv1[4], pv2[4], pv3[4], h[3], divw;
  float wmat[3][3], invwmat[3][3];

  /* compute barycentric coordinates */

  /* project the verts */
  imapaint_project(matrix, v1, pv1);
  imapaint_project(matrix, v2, pv2);
  imapaint_project(matrix, v3, pv3);

  /* do inverse view mapping, see gluProject man page */
  h[0] = (co[0] - view[0]) * 2.0f / view[2] - 1.0f;
  h[1] = (co[1] - view[1]) * 2.0f / view[3] - 1.0f;
  h[2] = 1.0f;

  /* Solve for `(w1,w2,w3)/perspdiv` in:
   * `h * perspdiv = Project * Model * (w1 * v1 + w2 * v2 + w3 * v3)`. */

  wmat[0][0] = pv1[0];
  wmat[1][0] = pv2[0];
  wmat[2][0] = pv3[0];
  wmat[0][1] = pv1[1];
  wmat[1][1] = pv2[1];
  wmat[2][1] = pv3[1];
  wmat[0][2] = pv1[3];
  wmat[1][2] = pv2[3];
  wmat[2][2] = pv3[3];

  invert_m3_m3(invwmat, wmat);
  mul_m3_v3(invwmat, h);

  copy_v3_v3(w, h);

  /* w is still divided by `perspdiv`, make it sum to one */
  divw = w[0] + w[1] + w[2];
  if (divw != 0.0f) {
    mul_v3_fl(w, 1.0f / divw);
  }
}

/* compute uv coordinates of mouse in face */
static void imapaint_pick_uv(const Mesh *me_eval,
                             Scene *scene,
                             Object *ob_eval,
                             uint faceindex,
                             const int xy[2],
                             float uv[2])
{
  int i, findex;
  float p[2], w[3], absw, minabsw;
  float matrix[4][4], proj[4][4];
  int view[4];
  const ePaintCanvasSource mode = ePaintCanvasSource(scene->toolsettings->imapaint.mode);

  const MLoopTri *lt = BKE_mesh_runtime_looptri_ensure(me_eval);
  const int tottri = BKE_mesh_runtime_looptri_len(me_eval);
  const int *looptri_polys = BKE_mesh_runtime_looptri_polys_ensure(me_eval);

  const float(*positions)[3] = BKE_mesh_vert_positions(me_eval);
  const int *corner_verts = BKE_mesh_corner_verts(me_eval);
  const int *index_mp_to_orig = static_cast<const int *>(
      CustomData_get_layer(&me_eval->pdata, CD_ORIGINDEX));

  /* get the needed opengl matrices */
  GPU_viewport_size_get_i(view);
  GPU_matrix_model_view_get(matrix);
  GPU_matrix_projection_get(proj);
  view[0] = view[1] = 0;
  mul_m4_m4m4(matrix, matrix, ob_eval->object_to_world);
  mul_m4_m4m4(matrix, proj, matrix);

  minabsw = 1e10;
  uv[0] = uv[1] = 0.0;

  const int *material_indices = (const int *)CustomData_get_layer_named(
      &me_eval->pdata, CD_PROP_INT32, "material_index");

  /* test all faces in the derivedmesh with the original index of the picked face */
  /* face means poly here, not triangle, indeed */
  for (i = 0; i < tottri; i++, lt++) {
    const int poly_i = looptri_polys[i];
    findex = index_mp_to_orig ? index_mp_to_orig[poly_i] : poly_i;

    if (findex == faceindex) {
      const float(*mloopuv)[2];
      const float *tri_uv[3];
      float tri_co[3][3];

      for (int j = 3; j--;) {
        copy_v3_v3(tri_co[j], positions[corner_verts[lt->tri[j]]]);
      }

      if (mode == PAINT_CANVAS_SOURCE_MATERIAL) {
        const Material *ma;
        const TexPaintSlot *slot;

        ma = BKE_object_material_get(
            ob_eval, material_indices == nullptr ? 1 : material_indices[poly_i] + 1);
        slot = &ma->texpaintslot[ma->paint_active_slot];

        if (!(slot && slot->uvname &&
              (mloopuv = static_cast<const float(*)[2]>(
                   CustomData_get_layer_named(&me_eval->ldata, CD_PROP_FLOAT2, slot->uvname)))))
        {
          mloopuv = static_cast<const float(*)[2]>(
              CustomData_get_layer(&me_eval->ldata, CD_PROP_FLOAT2));
        }
      }
      else {
        mloopuv = static_cast<const float(*)[2]>(
            CustomData_get_layer(&me_eval->ldata, CD_PROP_FLOAT2));
      }

      tri_uv[0] = mloopuv[lt->tri[0]];
      tri_uv[1] = mloopuv[lt->tri[1]];
      tri_uv[2] = mloopuv[lt->tri[2]];

      p[0] = xy[0];
      p[1] = xy[1];

      imapaint_tri_weights(matrix, view, UNPACK3(tri_co), p, w);
      absw = fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]);
      if (absw < minabsw) {
        uv[0] = tri_uv[0][0] * w[0] + tri_uv[1][0] * w[1] + tri_uv[2][0] * w[2];
        uv[1] = tri_uv[0][1] * w[0] + tri_uv[1][1] * w[1] + tri_uv[2][1] * w[2];
        minabsw = absw;
      }
    }
  }
}

/* returns 0 if not found, otherwise 1 */
static int imapaint_pick_face(ViewContext *vc, const int mval[2], uint *r_index, uint totpoly)
{
  if (totpoly == 0) {
    return 0;
  }

  /* sample only on the exact position */
  ED_view3d_select_id_validate(vc);
  *r_index = DRW_select_buffer_sample_point(vc->depsgraph, vc->region, vc->v3d, mval);

  if ((*r_index) == 0 || (*r_index) > uint(totpoly)) {
    return 0;
  }

  (*r_index)--;

  return 1;
}

void paint_sample_color(
    bContext *C, ARegion *region, int x, int y, bool texpaint_proj, bool use_palette)
{
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = BKE_paint_palette(paint);
  PaletteColor *color = nullptr;
  Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  CLAMP(x, 0, region->winx);
  CLAMP(y, 0, region->winy);

  if (use_palette) {
    if (!palette) {
      palette = BKE_palette_add(CTX_data_main(C), "Palette");
      BKE_paint_palette_set(paint, palette);
    }

    color = BKE_palette_color_add(palette);
    palette->active_color = BLI_listbase_count(&palette->colors) - 1;
  }

  SpaceImage *sima = CTX_wm_space_image(C);
  const View3D *v3d = CTX_wm_view3d(C);

  if (v3d && texpaint_proj) {
    /* first try getting a color directly from the mesh faces if possible */
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob = BKE_view_layer_active_object_get(view_layer);
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    ImagePaintSettings *imapaint = &scene->toolsettings->imapaint;
    bool use_material = (imapaint->mode == IMAGEPAINT_MODE_MATERIAL);

    if (ob) {
      CustomData_MeshMasks cddata_masks = CD_MASK_BAREMESH;
      cddata_masks.pmask |= CD_MASK_ORIGINDEX;
      Mesh *me = (Mesh *)ob->data;
      const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
      const int *material_indices = (const int *)CustomData_get_layer_named(
          &me_eval->pdata, CD_PROP_INT32, "material_index");

      ViewContext vc;
      const int mval[2] = {x, y};
      uint faceindex;
      uint totpoly = me->totpoly;

      if (CustomData_has_layer(&me_eval->ldata, CD_PROP_FLOAT2)) {
        ED_view3d_viewcontext_init(C, &vc, depsgraph);

        view3d_operator_needs_opengl(C);

        if (imapaint_pick_face(&vc, mval, &faceindex, totpoly)) {
          Image *image = nullptr;
          int interp = SHD_INTERP_LINEAR;

          if (use_material) {
            /* Image and texture interpolation from material. */
            Material *ma = BKE_object_material_get(
                ob_eval, material_indices ? material_indices[faceindex] + 1 : 1);

            /* Force refresh since paint slots are not updated when changing interpolation. */
            BKE_texpaint_slot_refresh_cache(scene, ma, ob);

            if (ma && ma->texpaintslot) {
              image = ma->texpaintslot[ma->paint_active_slot].ima;
              interp = ma->texpaintslot[ma->paint_active_slot].interp;
            }
          }
          else {
            /* Image and texture interpolation from tool settings. */
            image = imapaint->canvas;
            interp = imapaint->interp;
          }

          if (image) {
            float uv[2];
            float u, v;
            /* XXX get appropriate ImageUser instead */
            ImageUser iuser;
            BKE_imageuser_default(&iuser);
            iuser.framenr = image->lastframe;

            imapaint_pick_uv(me_eval, scene, ob_eval, faceindex, mval, uv);

            if (image->source == IMA_SRC_TILED) {
              float new_uv[2];
              iuser.tile = BKE_image_get_tile_from_pos(image, uv, new_uv, nullptr);
              u = new_uv[0];
              v = new_uv[1];
            }
            else {
              u = fmodf(uv[0], 1.0f);
              v = fmodf(uv[1], 1.0f);

              if (u < 0.0f) {
                u += 1.0f;
              }
              if (v < 0.0f) {
                v += 1.0f;
              }
            }

            ImBuf *ibuf = BKE_image_acquire_ibuf(image, &iuser, nullptr);
            if (ibuf && (ibuf->byte_buffer.data || ibuf->float_buffer.data)) {
              u = u * ibuf->x;
              v = v * ibuf->y;

              if (ibuf->float_buffer.data) {
                float rgba_f[4];
                if (interp == SHD_INTERP_CLOSEST) {
                  nearest_interpolation_color_wrap(ibuf, nullptr, rgba_f, u, v);
                }
                else {
                  bilinear_interpolation_color_wrap(ibuf, nullptr, rgba_f, u, v);
                }
                straight_to_premul_v4(rgba_f);
                if (use_palette) {
                  linearrgb_to_srgb_v3_v3(color->rgb, rgba_f);
                }
                else {
                  linearrgb_to_srgb_v3_v3(rgba_f, rgba_f);
                  BKE_brush_color_set(scene, br, rgba_f);
                }
              }
              else {
                uchar rgba[4];
                if (interp == SHD_INTERP_CLOSEST) {
                  nearest_interpolation_color_wrap(ibuf, rgba, nullptr, u, v);
                }
                else {
                  bilinear_interpolation_color_wrap(ibuf, rgba, nullptr, u, v);
                }
                if (use_palette) {
                  rgb_uchar_to_float(color->rgb, rgba);
                }
                else {
                  float rgba_f[3];
                  rgb_uchar_to_float(rgba_f, rgba);
                  BKE_brush_color_set(scene, br, rgba_f);
                }
              }
              BKE_image_release_ibuf(image, ibuf, nullptr);
              return;
            }

            BKE_image_release_ibuf(image, ibuf, nullptr);
          }
        }
      }
    }
  }
  else if (sima != nullptr) {
    /* Sample from the active image buffer. The sampled color is in
     * Linear Scene Reference Space. */
    float rgba_f[3];
    bool is_data;
    if (ED_space_image_color_sample(sima, region, blender::int2(x, y), rgba_f, &is_data)) {
      if (!is_data) {
        linearrgb_to_srgb_v3_v3(rgba_f, rgba_f);
      }

      if (use_palette) {
        copy_v3_v3(color->rgb, rgba_f);
      }
      else {
        BKE_brush_color_set(scene, br, rgba_f);
      }
      return;
    }
  }

  /* No sample found; sample directly from the GPU front buffer. */
  {
    float rgba_f[4];
    GPU_frontbuffer_read_color(
        x + region->winrct.xmin, y + region->winrct.ymin, 1, 1, 4, GPU_DATA_FLOAT, &rgba_f);

    if (use_palette) {
      copy_v3_v3(color->rgb, rgba_f);
    }
    else {
      BKE_brush_color_set(scene, br, rgba_f);
    }
  }
}

static int brush_curve_preset_exec(bContext *C, wmOperator *op)
{
  Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  if (br) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_brush_curve_preset(br, eCurveMappingPreset(RNA_enum_get(op->ptr, "shape")));
    BKE_paint_invalidate_cursor_overlay(scene, view_layer, br->curve);
  }

  return OPERATOR_FINISHED;
}

static bool brush_curve_preset_poll(bContext *C)
{
  Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  return br && br->curve;
}

static const EnumPropertyItem prop_shape_items[] = {
    {CURVE_PRESET_SHARP, "SHARP", 0, "Sharp", ""},
    {CURVE_PRESET_SMOOTH, "SMOOTH", 0, "Smooth", ""},
    {CURVE_PRESET_MAX, "MAX", 0, "Max", ""},
    {CURVE_PRESET_LINE, "LINE", 0, "Line", ""},
    {CURVE_PRESET_ROUND, "ROUND", 0, "Round", ""},
    {CURVE_PRESET_ROOT, "ROOT", 0, "Root", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

void BRUSH_OT_curve_preset(wmOperatorType *ot)
{
  ot->name = "Preset";
  ot->description = "Set brush shape";
  ot->idname = "BRUSH_OT_curve_preset";

  ot->exec = brush_curve_preset_exec;
  ot->poll = brush_curve_preset_poll;

  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
}

static bool brush_sculpt_curves_falloff_preset_poll(bContext *C)
{
  Brush *br = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  return br && br->curves_sculpt_settings && br->curves_sculpt_settings->curve_parameter_falloff;
}

static int brush_sculpt_curves_falloff_preset_exec(bContext *C, wmOperator *op)
{
  Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  CurveMapping *mapping = brush->curves_sculpt_settings->curve_parameter_falloff;
  mapping->preset = RNA_enum_get(op->ptr, "shape");
  CurveMap *map = mapping->cm;
  BKE_curvemap_reset(map, &mapping->clipr, mapping->preset, CURVEMAP_SLOPE_POSITIVE);
  return OPERATOR_FINISHED;
}

void BRUSH_OT_sculpt_curves_falloff_preset(wmOperatorType *ot)
{
  ot->name = "Curve Falloff Preset";
  ot->description = "Set Curve Falloff Preset";
  ot->idname = "BRUSH_OT_sculpt_curves_falloff_preset";

  ot->exec = brush_sculpt_curves_falloff_preset_exec;
  ot->poll = brush_sculpt_curves_falloff_preset_poll;

  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */
}

/* face-select ops */
static int paint_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  paintface_select_linked(C, CTX_data_active_object(C), nullptr, true);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->description = "Select linked faces";
  ot->idname = "PAINT_OT_face_select_linked";

  ot->exec = paint_select_linked_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int paint_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  view3d_operator_needs_opengl(C);
  paintface_select_linked(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked_pick(wmOperatorType *ot)
{
  ot->name = "Select Linked Pick";
  ot->description = "Select linked faces under the cursor";
  ot->idname = "PAINT_OT_face_select_linked_pick";

  ot->invoke = paint_select_linked_pick_invoke;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "Deselect rather than select items");
}

static int face_select_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (paintface_deselect_all_visible(C, ob, RNA_enum_get(op->ptr, "action"), true)) {
    ED_region_tag_redraw(CTX_wm_region(C));
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_face_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->description = "Change selection for all faces";
  ot->idname = "PAINT_OT_face_select_all";

  ot->exec = face_select_all_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static int paint_select_more_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintface_select_more(mesh, face_step);
  paintface_flush_flags(C, ob, true, false);

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->description = "Select Faces connected to existing selection";
  ot->idname = "PAINT_OT_face_select_more";

  ot->exec = paint_select_more_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also select faces that only touch on a corner");
}

static int paint_select_less_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintface_select_less(mesh, face_step);
  paintface_flush_flags(C, ob, true, false);

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->description = "Deselect Faces connected to existing selection";
  ot->idname = "PAINT_OT_face_select_less";

  ot->exec = paint_select_less_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also deselect faces that only touch on a corner");
}

static int paintface_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  if (!extend) {
    paintface_deselect_all_visible(C, CTX_data_active_object(C), SEL_DESELECT, false);
  }
  view3d_operator_needs_opengl(C);
  paintface_select_loop(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_loop(wmOperatorType *ot)
{
  ot->name = "Select Loop";
  ot->description = "Select face loop under the cursor";
  ot->idname = "PAINT_OT_face_select_loop";

  ot->invoke = paintface_select_loop_invoke;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "If false, faces will be deselected");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

static int vert_select_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  paintvert_deselect_all_visible(ob, RNA_enum_get(op->ptr, "action"), true);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->description = "Change selection for all vertices";
  ot->idname = "PAINT_OT_vert_select_all";

  ot->exec = vert_select_all_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static int vert_select_ungrouped_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *me = static_cast<Mesh *>(ob->data);

  if (BLI_listbase_is_empty(&me->vertex_group_names) || (BKE_mesh_deform_verts(me) == nullptr)) {
    BKE_report(op->reports, RPT_ERROR, "No weights/vertex groups on object");
    return OPERATOR_CANCELLED;
  }

  paintvert_select_ungrouped(ob, RNA_boolean_get(op->ptr, "extend"), true);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_ungrouped(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Ungrouped";
  ot->idname = "PAINT_OT_vert_select_ungrouped";
  ot->description = "Select vertices without a group";

  /* api callbacks */
  ot->exec = vert_select_ungrouped_exec;
  ot->poll = vert_paint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

static int paintvert_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  paintvert_select_linked(C, CTX_data_active_object(C));
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked Vertices";
  ot->description = "Select linked vertices";
  ot->idname = "PAINT_OT_vert_select_linked";

  ot->exec = paintvert_select_linked_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int paintvert_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  view3d_operator_needs_opengl(C);

  paintvert_select_linked_pick(C, CTX_data_active_object(C), event->mval, select);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_linked_pick(wmOperatorType *ot)
{
  ot->name = "Select Linked Vertices Pick";
  ot->description = "Select linked vertices under the cursor";
  ot->idname = "PAINT_OT_vert_select_linked_pick";

  ot->invoke = paintvert_select_linked_pick_invoke;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "select",
                  true,
                  "Select",
                  "Whether to select or deselect linked vertices under the cursor");
}

static int paintvert_select_more_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintvert_select_more(mesh, face_step);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->description = "Select Vertices connected to existing selection";
  ot->idname = "PAINT_OT_vert_select_more";

  ot->exec = paintvert_select_more_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also select faces that only touch on a corner");
}

static int paintvert_select_less_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return OPERATOR_CANCELLED;
  }

  const bool face_step = RNA_boolean_get(op->ptr, "face_step");
  paintvert_select_less(mesh, face_step);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->description = "Deselect Vertices connected to existing selection";
  ot->idname = "PAINT_OT_vert_select_less";

  ot->exec = paintvert_select_less_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "face_step", true, "Face Step", "Also deselect faces that only touch on a corner");
}

static int face_select_hide_exec(bContext *C, wmOperator *op)
{
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  Object *ob = CTX_data_active_object(C);
  paintface_hide(C, ob, unselected);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_hide(wmOperatorType *ot)
{
  ot->name = "Face Select Hide";
  ot->description = "Hide selected faces";
  ot->idname = "PAINT_OT_face_select_hide";

  ot->exec = face_select_hide_exec;
  ot->poll = facemask_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
}

static int vert_select_hide_exec(bContext *C, wmOperator *op)
{
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");
  Object *ob = CTX_data_active_object(C);
  paintvert_hide(C, ob, unselected);
  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

void PAINT_OT_vert_select_hide(wmOperatorType *ot)
{
  ot->name = "Vertex Select Hide";
  ot->description = "Hide selected vertices";
  ot->idname = "PAINT_OT_vert_select_hide";

  ot->exec = vert_select_hide_exec;
  ot->poll = vert_paint_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected vertices");
}

static int face_vert_reveal_exec(bContext *C, wmOperator *op)
{
  const bool select = RNA_boolean_get(op->ptr, "select");
  Object *ob = CTX_data_active_object(C);

  if (BKE_paint_select_vert_test(ob)) {
    paintvert_reveal(C, ob, select);
  }
  else {
    paintface_reveal(C, ob, select);
  }

  ED_region_tag_redraw(CTX_wm_region(C));
  return OPERATOR_FINISHED;
}

static bool face_vert_reveal_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  /* Allow using this operator when no selection is enabled but hiding is applied. */
  return BKE_paint_select_elem_test(ob) || BKE_paint_always_hide_test(ob);
}

void PAINT_OT_face_vert_reveal(wmOperatorType *ot)
{
  ot->name = "Reveal Faces/Vertices";
  ot->description = "Reveal hidden faces and vertices";
  ot->idname = "PAINT_OT_face_vert_reveal";

  ot->exec = face_vert_reveal_exec;
  ot->poll = face_vert_reveal_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "select",
                  true,
                  "Select",
                  "Specifies whether the newly revealed geometry should be selected");
}
