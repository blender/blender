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
#include "abc_writer_transform.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

using Alembic::AbcGeom::OCamera;
using Alembic::AbcGeom::OFloatProperty;

namespace blender {
namespace io {
namespace alembic {

AbcCameraWriter::AbcCameraWriter(Object *ob,
                                 AbcTransformWriter *parent,
                                 uint32_t time_sampling,
                                 ExportSettings &settings)
    : AbcObjectWriter(ob, time_sampling, settings, parent)
{
  OCamera camera(parent->alembicXform(), m_name, m_time_sampling);
  m_camera_schema = camera.getSchema();

  m_custom_data_container = m_camera_schema.getUserProperties();
  m_stereo_distance = OFloatProperty(m_custom_data_container, "stereoDistance", m_time_sampling);
  m_eye_separation = OFloatProperty(m_custom_data_container, "eyeSeparation", m_time_sampling);
}

void AbcCameraWriter::do_write()
{
  Camera *cam = static_cast<Camera *>(m_object->data);

  m_stereo_distance.set(cam->stereo.convergence_distance);
  m_eye_separation.set(cam->stereo.interocular_distance);

  const double apperture_x = cam->sensor_x / 10.0;
  const double apperture_y = cam->sensor_y / 10.0;
  const double film_aspect = apperture_x / apperture_y;

  m_camera_sample.setFocalLength(cam->lens);
  m_camera_sample.setHorizontalAperture(apperture_x);
  m_camera_sample.setVerticalAperture(apperture_y);
  m_camera_sample.setHorizontalFilmOffset(apperture_x * cam->shiftx);
  m_camera_sample.setVerticalFilmOffset(apperture_y * cam->shifty * film_aspect);
  m_camera_sample.setNearClippingPlane(cam->clip_start);
  m_camera_sample.setFarClippingPlane(cam->clip_end);

  if (cam->dof.focus_object) {
    Imath::V3f v(m_object->loc[0] - cam->dof.focus_object->loc[0],
                 m_object->loc[1] - cam->dof.focus_object->loc[1],
                 m_object->loc[2] - cam->dof.focus_object->loc[2]);
    m_camera_sample.setFocusDistance(v.length());
  }
  else {
    m_camera_sample.setFocusDistance(cam->dof.focus_distance);
  }

  /* Blender camera does not have an fstop param, so try to find a custom prop
   * instead. */
  m_camera_sample.setFStop(cam->dof.aperture_fstop);

  m_camera_sample.setLensSqueezeRatio(1.0);
  m_camera_schema.set(m_camera_sample);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
