/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "camera.hh"

#include "BKE_camera.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "hydra/object.hh"

namespace blender::render::hydra {

static pxr::GfCamera gf_camera(const CameraParams &params,
                               const pxr::GfVec2i &res,
                               const pxr::GfVec4f &border)
{
  pxr::GfCamera camera;

  camera.SetProjection(params.is_ortho ? pxr::GfCamera::Projection::Orthographic :
                                         pxr::GfCamera::Projection::Perspective);
  camera.SetClippingRange(pxr::GfRange1f(params.clip_start, params.clip_end));
  camera.SetFocalLength(params.lens);

  pxr::GfVec2f b_pos(border[0], border[1]), b_size(border[2], border[3]);
  float sensor_size = BKE_camera_sensor_size(params.sensor_fit, params.sensor_x, params.sensor_y);
  pxr::GfVec2f sensor_scale = (BKE_camera_sensor_fit(params.sensor_fit, res[0], res[1]) ==
                               CAMERA_SENSOR_FIT_HOR) ?
                                  pxr::GfVec2f(1.0f, float(res[1]) / res[0]) :
                                  pxr::GfVec2f(float(res[0]) / res[1], 1.0f);
  pxr::GfVec2f aperture = pxr::GfVec2f((params.is_ortho) ? params.ortho_scale : sensor_size);
  aperture = pxr::GfCompMult(aperture, sensor_scale);
  aperture = pxr::GfCompMult(aperture, b_size);
  aperture *= params.zoom;
  if (params.is_ortho) {
    /* Use tenths of a world unit according to USD docs
     * https://graphics.pixar.com/usd/docs/api/class_gf_camera.html */
    aperture *= 10.0f;
  }
  camera.SetHorizontalAperture(aperture[0]);
  camera.SetVerticalAperture(aperture[1]);

  pxr::GfVec2f lens_shift = pxr::GfVec2f(params.shiftx, params.shifty);
  lens_shift = pxr::GfCompDiv(lens_shift, sensor_scale);
  lens_shift += pxr::GfVec2f(params.offsetx, params.offsety);
  lens_shift += b_pos + b_size * 0.5f - pxr::GfVec2f(0.5f);
  lens_shift = pxr::GfCompDiv(lens_shift, b_size);
  camera.SetHorizontalApertureOffset(lens_shift[0] * aperture[0]);
  camera.SetVerticalApertureOffset(lens_shift[1] * aperture[1]);

  return camera;
}

pxr::GfCamera gf_camera(const Depsgraph *depsgraph,
                        const View3D *v3d,
                        const ARegion *region,
                        const pxr::GfVec4f &border)
{
  const RegionView3D *region_data = (const RegionView3D *)region->regiondata;

  CameraParams params;
  BKE_camera_params_init(&params);
  BKE_camera_params_from_view3d(&params, depsgraph, v3d, region_data);

  pxr::GfCamera camera = gf_camera(params, pxr::GfVec2i(region->winx, region->winy), border);
  camera.SetTransform(io::hydra::gf_matrix_from_transform(region_data->viewmat).GetInverse());

  return camera;
}

pxr::GfCamera gf_camera(const Object *camera_obj,
                        const pxr::GfVec2i &res,
                        const pxr::GfVec4f &border)
{
  CameraParams params;
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, camera_obj);

  pxr::GfCamera camera = gf_camera(params, res, border);
  camera.SetTransform(io::hydra::gf_matrix_from_transform(camera_obj->object_to_world().ptr()));

  return camera;
}

}  // namespace blender::render::hydra
