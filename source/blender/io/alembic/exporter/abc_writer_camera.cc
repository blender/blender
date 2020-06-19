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
 */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_camera.h"
#include "abc_hierarchy_iterator.h"

#include "BKE_camera.h"

#include "BLI_assert.h"

#include "DNA_camera_types.h"
#include "DNA_scene_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

namespace blender {
namespace io {
namespace alembic {

using Alembic::AbcGeom::CameraSample;
using Alembic::AbcGeom::OCamera;
using Alembic::AbcGeom::OFloatProperty;

ABCCameraWriter::ABCCameraWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args)
{
}

bool ABCCameraWriter::is_supported(const HierarchyContext *context) const
{
  Camera *camera = static_cast<Camera *>(context->object->data);
  return camera->type == CAM_PERSP;
}

void ABCCameraWriter::create_alembic_objects(const HierarchyContext * /*context*/)
{
  CLOG_INFO(&LOG, 2, "exporting %s", args_.abc_path.c_str());
  abc_camera_ = OCamera(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_camera_schema_ = abc_camera_.getSchema();

  abc_custom_data_container_ = abc_camera_schema_.getUserProperties();
  abc_stereo_distance_ = OFloatProperty(
      abc_custom_data_container_, "stereoDistance", timesample_index_);
  abc_eye_separation_ = OFloatProperty(
      abc_custom_data_container_, "eyeSeparation", timesample_index_);
}

const Alembic::Abc::OObject ABCCameraWriter::get_alembic_object() const
{
  return abc_camera_;
}

void ABCCameraWriter::do_write(HierarchyContext &context)
{
  Camera *cam = static_cast<Camera *>(context.object->data);

  abc_stereo_distance_.set(cam->stereo.convergence_distance);
  abc_eye_separation_.set(cam->stereo.interocular_distance);

  const double apperture_x = cam->sensor_x / 10.0;
  const double apperture_y = cam->sensor_y / 10.0;
  const double film_aspect = apperture_x / apperture_y;

  CameraSample camera_sample;
  camera_sample.setFocalLength(cam->lens);
  camera_sample.setHorizontalAperture(apperture_x);
  camera_sample.setVerticalAperture(apperture_y);
  camera_sample.setHorizontalFilmOffset(apperture_x * cam->shiftx);
  camera_sample.setVerticalFilmOffset(apperture_y * cam->shifty * film_aspect);
  camera_sample.setNearClippingPlane(cam->clip_start);
  camera_sample.setFarClippingPlane(cam->clip_end);

  if (cam->dof.focus_object) {
    Imath::V3f v(context.object->loc[0] - cam->dof.focus_object->loc[0],
                 context.object->loc[1] - cam->dof.focus_object->loc[1],
                 context.object->loc[2] - cam->dof.focus_object->loc[2]);
    camera_sample.setFocusDistance(v.length());
  }
  else {
    camera_sample.setFocusDistance(cam->dof.focus_distance);
  }

  /* Blender camera does not have an fstop param, so try to find a custom prop
   * instead. */
  camera_sample.setFStop(cam->dof.aperture_fstop);

  camera_sample.setLensSqueezeRatio(1.0);
  abc_camera_schema_.set(camera_sample);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
