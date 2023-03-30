/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_math_matrix.hh"
#include "BLI_path_util.h"
#include "BLI_span.hh"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "UI_view2d.h"

#include "ED_view3d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_io_base.hh"

using blender::Span;

namespace blender::io::gpencil {

/* Constructor. */
GpencilIO::GpencilIO(const GpencilIOParams *iparams)
{
  params_ = *iparams;

  /* Easy access data. */
  bmain_ = CTX_data_main(params_.C);
  depsgraph_ = CTX_data_depsgraph_pointer(params_.C);
  scene_ = CTX_data_scene(params_.C);
  rv3d_ = (RegionView3D *)params_.region->regiondata;
  gpd_ = (params_.ob != nullptr) ? (bGPdata *)params_.ob->data : nullptr;
  cfra_ = iparams->frame_cur;

  /* Calculate camera matrix. */
  prepare_camera_params(scene_, iparams);
}

void GpencilIO::prepare_camera_params(Scene *scene, const GpencilIOParams *iparams)
{
  params_ = *iparams;
  const bool is_pdf = params_.mode == GP_EXPORT_TO_PDF;
  const bool any_camera = (params_.v3d->camera != nullptr);
  const bool force_camera_view = is_pdf && any_camera;

  /* Ensure camera switch is applied. */
  BKE_scene_camera_switch_update(scene);

  /* Calculate camera matrix. */
  Object *cam_ob = scene->camera;
  if (cam_ob != nullptr) {
    /* Set up parameters. */
    CameraParams params;
    BKE_camera_params_init(&params);
    BKE_camera_params_from_object(&params, cam_ob);

    /* Compute matrix, view-plane, etc. */
    RenderData *rd = &scene_->r;
    BKE_camera_params_compute_viewplane(&params, rd->xsch, rd->ysch, rd->xasp, rd->yasp);
    BKE_camera_params_compute_matrix(&params);

    float4x4 viewmat = math::invert(float4x4(cam_ob->object_to_world));

    persmat_ = float4x4(params.winmat) * viewmat;
  }
  else {
    persmat_ = float4x4::identity();
  }

  winx_ = params_.region->winx;
  winy_ = params_.region->winy;

  /* Camera rectangle. */
  if ((rv3d_->persp == RV3D_CAMOB) || (force_camera_view)) {
    BKE_render_resolution(&scene->r, false, &render_x_, &render_y_);

    ED_view3d_calc_camera_border(CTX_data_scene(params_.C),
                                 depsgraph_,
                                 params_.region,
                                 params_.v3d,
                                 rv3d_,
                                 &camera_rect_,
                                 true);
    is_camera_ = true;
    camera_ratio_ = render_x_ / (camera_rect_.xmax - camera_rect_.xmin);
    offset_.x = camera_rect_.xmin;
    offset_.y = camera_rect_.ymin;
  }
  else {
    is_camera_ = false;
    /* Calc selected object boundbox. Need set initial value to some variables. */
    camera_ratio_ = 1.0f;
    offset_.x = 0.0f;
    offset_.y = 0.0f;

    create_object_list();

    selected_objects_boundbox_calc();
    rctf boundbox;
    selected_objects_boundbox_get(&boundbox);

    render_x_ = boundbox.xmax - boundbox.xmin;
    render_y_ = boundbox.ymax - boundbox.ymin;
    offset_.x = boundbox.xmin;
    offset_.y = boundbox.ymin;
  }
}

void GpencilIO::create_object_list()
{
  Scene *scene = CTX_data_scene(params_.C);
  ViewLayer *view_layer = CTX_data_view_layer(params_.C);

  float3 camera_z_axis = float3(rv3d_->viewinv[2]);
  ob_list_.clear();

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *object = base->object;

    if (object->type != OB_GPENCIL_LEGACY) {
      continue;
    }
    if ((params_.select_mode == GP_EXPORT_ACTIVE) && (params_.ob != object)) {
      continue;
    }

    if ((params_.select_mode == GP_EXPORT_SELECTED) && ((base->flag & BASE_SELECTED) == 0)) {
      continue;
    }

    float3 object_position = float3(object->object_to_world[3]);

    /* Save z-depth from view to sort from back to front. */
    if (is_camera_) {
      float camera_z = math::dot(camera_z_axis, object_position);
      ObjectZ obz = {camera_z, object};
      ob_list_.append(obz);
    }
    else {
      float zdepth = 0;
      if (rv3d_) {
        if (rv3d_->is_persp) {
          zdepth = ED_view3d_calc_zfac(rv3d_, object_position);
        }
        else {
          zdepth = -math::dot(camera_z_axis, object_position);
        }
        ObjectZ obz = {zdepth * -1.0f, object};
        ob_list_.append(obz);
      }
    }
  }
  /* Sort list of objects from point of view. */
  std::sort(ob_list_.begin(), ob_list_.end(), [](const ObjectZ &obz1, const ObjectZ &obz2) {
    return obz1.zdepth < obz2.zdepth;
  });
}

void GpencilIO::filepath_set(const char *filepath)
{
  BLI_strncpy(filepath_, filepath, FILE_MAX);
  BLI_path_abs(filepath_, BKE_main_blendfile_path(bmain_));
}

bool GpencilIO::gpencil_3D_point_to_screen_space(const float3 co, float2 &r_co)
{
  float3 parent_co = math::transform_point(diff_mat_, co);
  float2 screen_co;
  eV3DProjTest test = (eV3DProjTest)(V3D_PROJ_RET_OK);
  if (ED_view3d_project_float_global(params_.region, parent_co, screen_co, test) ==
      V3D_PROJ_RET_OK) {
    if (!ELEM(V2D_IS_CLIPPED, screen_co[0], screen_co[1])) {
      r_co = screen_co;
      /* Invert X axis. */
      if (invert_axis_[0]) {
        r_co[0] = winx_ - r_co[0];
      }
      /* Invert Y axis. */
      if (invert_axis_[1]) {
        r_co[1] = winy_ - r_co[1];
      }
      /* Apply offset and scale. */
      r_co -= offset_;
      r_co *= camera_ratio_;

      return true;
    }
  }
  r_co[0] = V2D_IS_CLIPPED;
  r_co[1] = V2D_IS_CLIPPED;

  /* Invert X axis. */
  if (invert_axis_[0]) {
    r_co[0] = winx_ - r_co[0];
  }
  /* Invert Y axis. */
  if (invert_axis_[1]) {
    r_co[1] = winy_ - r_co[1];
  }

  return false;
}

float2 GpencilIO::gpencil_3D_point_to_render_space(const float3 co)
{
  float3 parent_co = math::transform_point(diff_mat_, co);

  float2 r_co = float2(math::project_point(persmat_, parent_co));
  r_co = ((r_co + 1.0f) / 2.0f) * float2(render_x_, render_y_);

  /* Invert X axis. */
  if (invert_axis_[0]) {
    r_co.x = float(render_x_) - r_co.x;
  }
  /* Invert Y axis. */
  if (invert_axis_[1]) {
    r_co.y = float(render_y_) - r_co.y;
  }

  return r_co;
}

float2 GpencilIO::gpencil_3D_point_to_2D(const float3 co)
{
  const bool is_camera = bool(rv3d_->persp == RV3D_CAMOB);
  if (is_camera) {
    return gpencil_3D_point_to_render_space(co);
  }
  float2 result;
  gpencil_3D_point_to_screen_space(co, result);
  return result;
}

float GpencilIO::stroke_point_radius_get(bGPDlayer *gpl, bGPDstroke *gps)
{
  bGPDspoint *pt = &gps->points[0];
  const float2 screen_co = gpencil_3D_point_to_2D(&pt->x);

  /* Radius. */
  bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
      rv3d_->viewmat, gpd_, gpl, gps, 3, diff_mat_.ptr(), 0.0f);

  pt = &gps_perimeter->points[0];
  const float2 screen_ex = gpencil_3D_point_to_2D(&pt->x);

  const float2 v1 = screen_co - screen_ex;
  float radius = math::length(v1);
  BKE_gpencil_free_stroke(gps_perimeter);

  return MAX2(radius, 1.0f);
}

void GpencilIO::prepare_layer_export_matrix(Object *ob, bGPDlayer *gpl)
{
  BKE_gpencil_layer_transform_matrix_get(depsgraph_, ob, gpl, diff_mat_.ptr());
  diff_mat_ = diff_mat_ * float4x4(gpl->layer_invmat);
}

void GpencilIO::prepare_stroke_export_colors(Object *ob, bGPDstroke *gps)
{
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  /* Stroke color. */
  avg_opacity_ = 0.0f;
  /* Get average vertex color and apply. */
  float4 avg_color = {0.0f, 0.0f, 0.0f, 0.0f};
  for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
    avg_color += pt.vert_color;
    avg_opacity_ += pt.strength;
  }

  avg_color /= float(gps->totpoints);
  avg_opacity_ /= float(gps->totpoints);
  stroke_color_ = math::interpolate(float4(gp_style->stroke_rgba), avg_color, avg_color[3]);

  /* Apply vertex color for fill. */
  fill_color_ = math::interpolate(
      float4(gp_style->fill_rgba), float4(gps->vert_color_fill), gps->vert_color_fill[3]);
}

float GpencilIO::stroke_average_opacity_get()
{
  return avg_opacity_;
}

bool GpencilIO::is_camera_mode()
{
  return is_camera_;
}

void GpencilIO::selected_objects_boundbox_calc()
{
  const float gap = 10.0f;

  float2 min, max;
  INIT_MINMAX2(min, max);

  for (ObjectZ &obz : ob_list_) {
    Object *ob = obz.ob;
    /* Use evaluated version to get strokes with modifiers. */
    Object *ob_eval = (Object *)DEG_get_evaluated_id(depsgraph_, &ob->id);
    bGPdata *gpd_eval = (bGPdata *)ob_eval->data;

    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_eval->layers) {
      if (gpl->flag & GP_LAYER_HIDE) {
        continue;
      }
      BKE_gpencil_layer_transform_matrix_get(depsgraph_, ob_eval, gpl, diff_mat_.ptr());

      bGPDframe *gpf = gpl->actframe;
      if (gpf == nullptr) {
        continue;
      }

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->totpoints == 0) {
          continue;
        }
        for (const bGPDspoint &pt : MutableSpan(gps->points, gps->totpoints)) {
          math::min_max(gpencil_3D_point_to_2D(&pt.x), min, max);
        }
      }
    }
  }
  /* Add small gap. */
  min -= gap;
  max += gap;

  select_boundbox_.xmin = min[0];
  select_boundbox_.ymin = min[1];
  select_boundbox_.xmax = max[0];
  select_boundbox_.ymax = max[1];
}

void GpencilIO::selected_objects_boundbox_get(rctf *boundbox)
{
  boundbox->xmin = select_boundbox_.xmin;
  boundbox->xmax = select_boundbox_.xmax;
  boundbox->ymin = select_boundbox_.ymin;
  boundbox->ymax = select_boundbox_.ymax;
}

void GpencilIO::frame_number_set(const int value)
{
  cfra_ = value;
}

}  // namespace blender::io::gpencil
