/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "camera.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "hydra/object.h"

namespace blender::io::hydra {

CameraData::CameraData(const View3D *v3d, const ARegion *region)
{
  const RegionView3D *region_data = (const RegionView3D *)region->regiondata;

  /* TODO: refactor use BKE_camera_params API. */
  float VIEWPORT_SENSOR_SIZE = DEFAULT_SENSOR_WIDTH * 2.0f;

  pxr::GfVec2i res(region->winx, region->winy);
  float ratio = (float)res[0] / res[1];
  transform_ = gf_matrix_from_transform(region_data->viewmat).GetInverse();

  switch (region_data->persp) {
    case RV3D_PERSP: {
      mode_ = CAM_PERSP;
      clip_range_ = pxr::GfRange1f(v3d->clip_start, v3d->clip_end);
      lens_shift_ = pxr::GfVec2f(0.0, 0.0);
      focal_length_ = v3d->lens;

      if (ratio > 1.0) {
        sensor_size_ = pxr::GfVec2f(VIEWPORT_SENSOR_SIZE, VIEWPORT_SENSOR_SIZE / ratio);
      }
      else {
        sensor_size_ = pxr::GfVec2f(VIEWPORT_SENSOR_SIZE * ratio, VIEWPORT_SENSOR_SIZE);
      }
      break;
    }

    case RV3D_ORTHO: {
      mode_ = CAM_ORTHO;
      lens_shift_ = pxr::GfVec2f(0.0f, 0.0f);

      float o_size = region_data->dist * VIEWPORT_SENSOR_SIZE / v3d->lens;
      float o_depth = v3d->clip_end;

      clip_range_ = pxr::GfRange1f(-o_depth * 0.5, o_depth * 0.5);

      if (ratio > 1.0f) {
        ortho_size_ = pxr::GfVec2f(o_size, o_size / ratio);
      }
      else {
        ortho_size_ = pxr::GfVec2f(o_size * ratio, o_size);
      }
      break;
    }

    case RV3D_CAMOB: {
      pxr::GfMatrix4d mat = transform_;
      *this = CameraData(v3d->camera, res, pxr::GfVec4f(0, 0, 1, 1));
      transform_ = mat;

      /* This formula was taken from previous plugin with corresponded comment.
       * See blender/intern/cycles/blender/blender_camera.cpp:blender_camera_from_view (look
       * for 1.41421f). */
      float zoom = 4.0 / pow((pow(2.0, 0.5) + region_data->camzoom / 50.0), 2);

      /* Updating l_shift due to viewport zoom and view_camera_offset
       * view_camera_offset should be multiplied by 2. */
      lens_shift_ = pxr::GfVec2f((lens_shift_[0] + region_data->camdx * 2) / zoom,
                                 (lens_shift_[1] + region_data->camdy * 2) / zoom);

      if (mode_ == CAM_ORTHO) {
        ortho_size_ *= zoom;
      }
      else {
        sensor_size_ *= zoom;
      }
      break;
    }

    default:
      break;
  }
}

CameraData::CameraData(const Object *camera_obj, pxr::GfVec2i res, pxr::GfVec4f tile)
{
  const Camera *camera = (const Camera *)camera_obj->data;

  float t_pos[2] = {tile[0], tile[1]};
  float t_size[2] = {tile[2], tile[3]};
  transform_ = gf_matrix_from_transform(camera_obj->object_to_world);
  clip_range_ = pxr::GfRange1f(camera->clip_start, camera->clip_end);
  mode_ = camera->type;

  if (camera->dof.flag & CAM_DOF_ENABLED) {
    float focus_distance;
    if (!camera->dof.focus_object) {
      focus_distance = camera->dof.focus_distance;
    }
    else {
      pxr::GfVec3f obj_pos(camera->dof.focus_object->object_to_world[0][3],
                           camera->dof.focus_object->object_to_world[1][3],
                           camera->dof.focus_object->object_to_world[2][3]);
      pxr::GfVec3f cam_pos(transform_[0][3], transform_[1][3], transform_[2][3]);
      focus_distance = (obj_pos - cam_pos).GetLength();
    }

    dof_data_ = std::tuple(
        std::max(focus_distance, 0.001f), camera->dof.aperture_fstop, camera->dof.aperture_blades);
  }

  float ratio = float(res[0]) / res[1];

  switch (camera->sensor_fit) {
    case CAMERA_SENSOR_FIT_VERT:
      lens_shift_ = pxr::GfVec2f(camera->shiftx / ratio, camera->shifty);
      break;
    case CAMERA_SENSOR_FIT_HOR:
      lens_shift_ = pxr::GfVec2f(camera->shiftx, camera->shifty * ratio);
      break;
    case CAMERA_SENSOR_FIT_AUTO:
      if (ratio > 1.0f) {
        lens_shift_ = pxr::GfVec2f(camera->shiftx, camera->shifty * ratio);
      }
      else {
        lens_shift_ = pxr::GfVec2f(camera->shiftx / ratio, camera->shifty);
      }
      break;
    default:
      lens_shift_ = pxr::GfVec2f(camera->shiftx, camera->shifty);
      break;
  }

  lens_shift_ = pxr::GfVec2f(
      lens_shift_[0] / t_size[0] + (t_pos[0] + t_size[0] * 0.5 - 0.5) / t_size[0],
      lens_shift_[1] / t_size[1] + (t_pos[1] + t_size[1] * 0.5 - 0.5) / t_size[1]);

  switch (camera->type) {
    case CAM_PERSP: {
      focal_length_ = camera->lens;

      switch (camera->sensor_fit) {
        case CAMERA_SENSOR_FIT_VERT:
          sensor_size_ = pxr::GfVec2f(camera->sensor_y * ratio, camera->sensor_y);
          break;
        case CAMERA_SENSOR_FIT_HOR:
          sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_x / ratio);
          break;
        case CAMERA_SENSOR_FIT_AUTO:
          if (ratio > 1.0f) {
            sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_x / ratio);
          }
          else {
            sensor_size_ = pxr::GfVec2f(camera->sensor_x * ratio, camera->sensor_x);
          }
          break;
        default:
          sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_y);
          break;
      }
      sensor_size_ = pxr::GfVec2f(sensor_size_[0] * t_size[0], sensor_size_[1] * t_size[1]);
      break;
    }

    case CAM_ORTHO: {
      focal_length_ = 0.0f;
      switch (camera->sensor_fit) {
        case CAMERA_SENSOR_FIT_VERT:
          ortho_size_ = pxr::GfVec2f(camera->ortho_scale * ratio, camera->ortho_scale);
          break;
        case CAMERA_SENSOR_FIT_HOR:
          ortho_size_ = pxr::GfVec2f(camera->ortho_scale, camera->ortho_scale / ratio);
          break;
        case CAMERA_SENSOR_FIT_AUTO:
          if (ratio > 1.0f) {
            ortho_size_ = pxr::GfVec2f(camera->ortho_scale, camera->ortho_scale / ratio);
          }
          else {
            ortho_size_ = pxr::GfVec2f(camera->ortho_scale * ratio, camera->ortho_scale);
          }
          break;
        default:
          ortho_size_ = pxr::GfVec2f(camera->ortho_scale, camera->ortho_scale);
          break;
      }
      ortho_size_ = pxr::GfVec2f(ortho_size_[0] * t_size[0], ortho_size_[1] * t_size[1]);
      break;
    }

    case CAM_PANO: {
      /* TODO: Recheck parameters for PANO camera */
      focal_length_ = camera->lens;

      switch (camera->sensor_fit) {
        case CAMERA_SENSOR_FIT_VERT:
          sensor_size_ = pxr::GfVec2f(camera->sensor_y * ratio, camera->sensor_y);
          break;
        case CAMERA_SENSOR_FIT_HOR:
          sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_x / ratio);
          break;
        case CAMERA_SENSOR_FIT_AUTO:
          if (ratio > 1.0f) {
            sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_x / ratio);
          }
          else {
            sensor_size_ = pxr::GfVec2f(camera->sensor_x * ratio, camera->sensor_x);
          }
          break;
        default:
          sensor_size_ = pxr::GfVec2f(camera->sensor_x, camera->sensor_y);
          break;
      }
      sensor_size_ = pxr::GfVec2f(sensor_size_[0] * t_size[0], sensor_size_[1] * t_size[1]);
      break;
    }

    default: {
      focal_length_ = camera->lens;
      sensor_size_ = pxr::GfVec2f(camera->sensor_y * ratio, camera->sensor_y);
      break;
    }
  }
}

pxr::GfCamera CameraData::gf_camera()
{
  return gf_camera(pxr::GfVec4f(0, 0, 1, 1));
}

pxr::GfCamera CameraData::gf_camera(pxr::GfVec4f tile)
{
  float t_pos[2] = {tile[0], tile[1]}, t_size[2] = {tile[2], tile[3]};

  pxr::GfCamera gf_camera = pxr::GfCamera();

  gf_camera.SetClippingRange(clip_range_);

  float l_shift[2] = {(lens_shift_[0] + t_pos[0] + t_size[0] * 0.5f - 0.5f) / t_size[0],
                      (lens_shift_[1] + t_pos[1] + t_size[1] * 0.5f - 0.5f) / t_size[1]};

  switch (mode_) {
    case CAM_PERSP:
    case CAM_PANO: {
      /* TODO: store panoramic camera settings */
      gf_camera.SetProjection(pxr::GfCamera::Projection::Perspective);
      gf_camera.SetFocalLength(focal_length_);

      float s_size[2] = {sensor_size_[0] * t_size[0], sensor_size_[1] * t_size[1]};

      gf_camera.SetHorizontalAperture(s_size[0]);
      gf_camera.SetVerticalAperture(s_size[1]);

      gf_camera.SetHorizontalApertureOffset(l_shift[0] * s_size[0]);
      gf_camera.SetVerticalApertureOffset(l_shift[1] * s_size[1]);
      break;
    }
    case CAM_ORTHO: {
      gf_camera.SetProjection(pxr::GfCamera::Projection::Orthographic);

      /* Use tenths of a world unit according to USD docs
       * https://graphics.pixar.com/usd/docs/api/class_gf_camera.html */
      float o_size[2] = {ortho_size_[0] * t_size[0] * 10, ortho_size_[1] * t_size[1] * 10};

      gf_camera.SetHorizontalAperture(o_size[0]);
      gf_camera.SetVerticalAperture(o_size[1]);

      gf_camera.SetHorizontalApertureOffset(l_shift[0] * o_size[0]);
      gf_camera.SetVerticalApertureOffset(l_shift[1] * o_size[1]);
      break;
    }
    default:
      break;
  }

  gf_camera.SetTransform(transform_);
  return gf_camera;
}

}  // namespace blender::io::hydra
