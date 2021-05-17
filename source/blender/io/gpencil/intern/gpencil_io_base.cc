

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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4x4.hh"
#include "BLI_path_util.h"
#include "BLI_span.hh"

#include "DNA_gpencil_types.h"
#include "DNA_layer_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_main.h"
#include "BKE_material.h"

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
  prepare_camera_params(iparams);
}

void GpencilIO::prepare_camera_params(const GpencilIOParams *iparams)
{
  params_ = *iparams;
  const bool is_pdf = params_.mode == GP_EXPORT_TO_PDF;
  const bool any_camera = (params_.v3d->camera != nullptr);
  const bool force_camera_view = is_pdf && any_camera;

  /* Calculate camera matrix. */
  Object *cam_ob = params_.v3d->camera;
  if (cam_ob != nullptr) {
    /* Set up parameters. */
    CameraParams params;
    BKE_camera_params_init(&params);
    BKE_camera_params_from_object(&params, cam_ob);

    /* Compute matrix, viewplane, .. */
    RenderData *rd = &scene_->r;
    BKE_camera_params_compute_viewplane(&params, rd->xsch, rd->ysch, rd->xasp, rd->yasp);
    BKE_camera_params_compute_matrix(&params);

    float viewmat[4][4];
    invert_m4_m4(viewmat, cam_ob->obmat);

    mul_m4_m4m4(persmat_, params.winmat, viewmat);
    is_ortho_ = params.is_ortho;
  }
  else {
    unit_m4(persmat_);
    is_ortho_ = false;
  }

  winx_ = params_.region->winx;
  winy_ = params_.region->winy;

  /* Camera rectangle. */
  if ((rv3d_->persp == RV3D_CAMOB) || (force_camera_view)) {
    render_x_ = (scene_->r.xsch * scene_->r.size) / 100;
    render_y_ = (scene_->r.ysch * scene_->r.size) / 100;

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
    is_ortho_ = false;
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

/** Create a list of selected objects sorted from back to front */
void GpencilIO::create_object_list()
{
  ViewLayer *view_layer = CTX_data_view_layer(params_.C);

  float3 camera_z_axis;
  copy_v3_v3(camera_z_axis, rv3d_->viewinv[2]);
  ob_list_.clear();

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object = base->object;

    if (object->type != OB_GPENCIL) {
      continue;
    }
    if ((params_.select_mode == GP_EXPORT_ACTIVE) && (params_.ob != object)) {
      continue;
    }

    if ((params_.select_mode == GP_EXPORT_SELECTED) && ((base->flag & BASE_SELECTED) == 0)) {
      continue;
    }

    /* Save z-depth from view to sort from back to front. */
    if (is_camera_) {
      float camera_z = dot_v3v3(camera_z_axis, object->obmat[3]);
      ObjectZ obz = {camera_z, object};
      ob_list_.append(obz);
    }
    else {
      float zdepth = 0;
      if (rv3d_) {
        if (rv3d_->is_persp) {
          zdepth = ED_view3d_calc_zfac(rv3d_, object->obmat[3], nullptr);
        }
        else {
          zdepth = -dot_v3v3(rv3d_->viewinv[2], object->obmat[3]);
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

/**
 * Set file input_text full path.
 * \param filename: Path of the file provided by save dialog.
 */
void GpencilIO::filename_set(const char *filename)
{
  BLI_strncpy(filename_, filename, FILE_MAX);
  BLI_path_abs(filename_, BKE_main_blendfile_path(bmain_));
}

/** Convert to screenspace. */
bool GpencilIO::gpencil_3D_point_to_screen_space(const float3 co, float2 &r_co)
{
  float3 parent_co = diff_mat_ * co;
  float2 screen_co;
  eV3DProjTest test = (eV3DProjTest)(V3D_PROJ_RET_OK);
  if (ED_view3d_project_float_global(params_.region, parent_co, screen_co, test) ==
      V3D_PROJ_RET_OK) {
    if (!ELEM(V2D_IS_CLIPPED, screen_co[0], screen_co[1])) {
      copy_v2_v2(r_co, screen_co);
      /* Invert X axis. */
      if (invert_axis_[0]) {
        r_co[0] = winx_ - r_co[0];
      }
      /* Invert Y axis. */
      if (invert_axis_[1]) {
        r_co[1] = winy_ - r_co[1];
      }
      /* Apply offset and scale. */
      sub_v2_v2(r_co, &offset_.x);
      mul_v2_fl(r_co, camera_ratio_);

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

/** Convert to render space. */
float2 GpencilIO::gpencil_3D_point_to_render_space(const float3 co, const bool is_ortho)
{
  float3 parent_co = diff_mat_ * co;
  mul_m4_v3(persmat_, parent_co);

  if (!is_ortho) {
    parent_co.x = parent_co.x / max_ff(FLT_MIN, parent_co.z);
    parent_co.y = parent_co.y / max_ff(FLT_MIN, parent_co.z);
  }

  float2 r_co;
  r_co.x = (parent_co.x + 1.0f) / 2.0f * (float)render_x_;
  r_co.y = (parent_co.y + 1.0f) / 2.0f * (float)render_y_;

  /* Invert X axis. */
  if (invert_axis_[0]) {
    r_co.x = (float)render_x_ - r_co.x;
  }
  /* Invert Y axis. */
  if (invert_axis_[1]) {
    r_co.y = (float)render_y_ - r_co.y;
  }

  return r_co;
}

/** Convert to 2D. */
float2 GpencilIO::gpencil_3D_point_to_2D(const float3 co)
{
  const bool is_camera = (bool)(rv3d_->persp == RV3D_CAMOB);
  if (is_camera) {
    return gpencil_3D_point_to_render_space(co, is_orthographic());
  }
  float2 result;
  gpencil_3D_point_to_screen_space(co, result);
  return result;
}

/** Get radius of point. */
float GpencilIO::stroke_point_radius_get(bGPDlayer *gpl, bGPDstroke *gps)
{
  bGPDspoint *pt = &gps->points[0];
  const float2 screen_co = gpencil_3D_point_to_2D(&pt->x);

  /* Radius. */
  bGPDstroke *gps_perimeter = BKE_gpencil_stroke_perimeter_from_view(
      rv3d_, gpd_, gpl, gps, 3, diff_mat_.values);

  pt = &gps_perimeter->points[0];
  const float2 screen_ex = gpencil_3D_point_to_2D(&pt->x);

  const float2 v1 = screen_co - screen_ex;
  float radius = v1.length();
  BKE_gpencil_free_stroke(gps_perimeter);

  return MAX2(radius, 1.0f);
}

void GpencilIO::prepare_layer_export_matrix(Object *ob, bGPDlayer *gpl)
{
  BKE_gpencil_layer_transform_matrix_get(depsgraph_, ob, gpl, diff_mat_.values);
  diff_mat_ = diff_mat_ * float4x4(gpl->layer_invmat);
}

void GpencilIO::prepare_stroke_export_colors(Object *ob, bGPDstroke *gps)
{
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);

  /* Stroke color. */
  copy_v4_v4(stroke_color_, gp_style->stroke_rgba);
  avg_opacity_ = 0.0f;
  /* Get average vertex color and apply. */
  float avg_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  for (const bGPDspoint &pt : Span(gps->points, gps->totpoints)) {
    add_v4_v4(avg_color, pt.vert_color);
    avg_opacity_ += pt.strength;
  }

  mul_v4_v4fl(avg_color, avg_color, 1.0f / (float)gps->totpoints);
  interp_v3_v3v3(stroke_color_, stroke_color_, avg_color, avg_color[3]);
  avg_opacity_ /= (float)gps->totpoints;

  /* Fill color. */
  copy_v4_v4(fill_color_, gp_style->fill_rgba);
  /* Apply vertex color for fill. */
  interp_v3_v3v3(fill_color_, fill_color_, gps->vert_color_fill, gps->vert_color_fill[3]);
}

float GpencilIO::stroke_average_opacity_get()
{
  return avg_opacity_;
}

bool GpencilIO::is_camera_mode()
{
  return is_camera_;
}

bool GpencilIO::is_orthographic()
{
  return is_ortho_;
}

/* Calculate selected strokes boundbox. */
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
      BKE_gpencil_layer_transform_matrix_get(depsgraph_, ob_eval, gpl, diff_mat_.values);

      bGPDframe *gpf = gpl->actframe;
      if (gpf == nullptr) {
        continue;
      }

      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (gps->totpoints == 0) {
          continue;
        }
        for (const bGPDspoint &pt : MutableSpan(gps->points, gps->totpoints)) {
          const float2 screen_co = gpencil_3D_point_to_2D(&pt.x);
          minmax_v2v2_v2(min, max, screen_co);
        }
      }
    }
  }
  /* Add small gap. */
  add_v2_fl(min, gap * -1.0f);
  add_v2_fl(max, gap);

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
