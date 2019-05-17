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

#include "abc_camera.h"

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "BKE_camera.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
}

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IFloatProperty;
using Alembic::AbcGeom::ISampleSelector;

using Alembic::AbcGeom::OCamera;
using Alembic::AbcGeom::OFloatProperty;

using Alembic::AbcGeom::CameraSample;
using Alembic::AbcGeom::kWrapExisting;

/* ************************************************************************** */

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

/* ************************************************************************** */

AbcCameraReader::AbcCameraReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  ICamera abc_cam(m_iobject, kWrapExisting);
  m_schema = abc_cam.getSchema();

  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcCameraReader::valid() const
{
  return m_schema.valid();
}

bool AbcCameraReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::ICamera::matches(alembic_header)) {
    *err_str =
        "Object type mismatch, Alembic object path pointed to Camera when importing, but not any "
        "more.";
    return false;
  }

  if (ob->type != OB_CAMERA) {
    *err_str = "Object type mismatch, Alembic object path points to Camera.";
    return false;
  }

  return true;
}

void AbcCameraReader::readObjectData(Main *bmain, const ISampleSelector &sample_sel)
{
  Camera *bcam = static_cast<Camera *>(BKE_camera_add(bmain, m_data_name.c_str()));

  CameraSample cam_sample;
  m_schema.get(cam_sample, sample_sel);

  ICompoundProperty customDataContainer = m_schema.getUserProperties();

  if (customDataContainer.valid() && customDataContainer.getPropertyHeader("stereoDistance") &&
      customDataContainer.getPropertyHeader("eyeSeparation")) {
    IFloatProperty convergence_plane(customDataContainer, "stereoDistance");
    IFloatProperty eye_separation(customDataContainer, "eyeSeparation");

    bcam->stereo.interocular_distance = eye_separation.getValue(sample_sel);
    bcam->stereo.convergence_distance = convergence_plane.getValue(sample_sel);
  }

  const float lens = static_cast<float>(cam_sample.getFocalLength());
  const float apperture_x = static_cast<float>(cam_sample.getHorizontalAperture());
  const float apperture_y = static_cast<float>(cam_sample.getVerticalAperture());
  const float h_film_offset = static_cast<float>(cam_sample.getHorizontalFilmOffset());
  const float v_film_offset = static_cast<float>(cam_sample.getVerticalFilmOffset());
  const float film_aspect = apperture_x / apperture_y;

  bcam->lens = lens;
  bcam->sensor_x = apperture_x * 10;
  bcam->sensor_y = apperture_y * 10;
  bcam->shiftx = h_film_offset / apperture_x;
  bcam->shifty = v_film_offset / apperture_y / film_aspect;
  bcam->clip_start = max_ff(0.1f, static_cast<float>(cam_sample.getNearClippingPlane()));
  bcam->clip_end = static_cast<float>(cam_sample.getFarClippingPlane());
  bcam->dof.focus_distance = static_cast<float>(cam_sample.getFocusDistance());
  bcam->dof.aperture_fstop = static_cast<float>(cam_sample.getFStop());

  m_object = BKE_object_add_only_object(bmain, OB_CAMERA, m_object_name.c_str());
  m_object->data = bcam;
}
