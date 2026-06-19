/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_camera.h"
#include "abc_keyframing.h"
#include "abc_util.h"

/* Silence warnings from copying deprecated fields. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "ANIM_action.hh"
#include "ANIM_fcurve.hh"

#include "BLI_math_base_c.hh"

#include "BKE_camera.h"
#include "BKE_object.hh"

#include "BLT_translation.hh"

namespace blender {

using Alembic::AbcGeom::CameraSample;
using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IFloatProperty;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::kWrapExisting;

namespace io::alembic {

AbcCameraReader::AbcCameraReader(const AbcReaderConstructorArgs &args) : AbcObjectReader(args)
{
  ICamera abc_cam(m_iobject, kWrapExisting);
  m_schema = abc_cam.getSchema();
}

bool AbcCameraReader::valid() const
{
  return m_schema.valid();
}

bool AbcCameraReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **r_err_str) const
{
  if (!Alembic::AbcGeom::ICamera::matches(alembic_header)) {
    *r_err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Camera when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_CAMERA) {
    *r_err_str = RPT_("Object type mismatch, Alembic object path points to Camera");
    return false;
  }

  return true;
}

static void read_camera_sample(Camera *bcam,
                               const ICamera::schema_type &schema,
                               const ISampleSelector &sample_sel)
{
  CameraSample cam_sample;
  schema.get(cam_sample, sample_sel);

  ICompoundProperty customDataContainer = schema.getUserProperties();

  if (customDataContainer.valid() && customDataContainer.getPropertyHeader("stereoDistance") &&
      customDataContainer.getPropertyHeader("eyeSeparation"))
  {
    IFloatProperty convergence_plane(customDataContainer, "stereoDistance");
    IFloatProperty eye_separation(customDataContainer, "eyeSeparation");

    bcam->stereo.interocular_distance = eye_separation.getValue(sample_sel);
    bcam->stereo.convergence_distance = convergence_plane.getValue(sample_sel);
  }

  const float lens = float(cam_sample.getFocalLength());
  const float apperture_x = float(cam_sample.getHorizontalAperture());
  const float apperture_y = float(cam_sample.getVerticalAperture());
  const float h_film_offset = float(cam_sample.getHorizontalFilmOffset());
  const float v_film_offset = float(cam_sample.getVerticalFilmOffset());
  const float film_aspect = apperture_x / apperture_y;

  bcam->lens = lens;
  bcam->sensor_x = apperture_x * 10;
  bcam->sensor_y = apperture_y * 10;
  bcam->shiftx = h_film_offset / apperture_x;
  bcam->shifty = v_film_offset / apperture_y / film_aspect;
  bcam->clip_start = max_ff(0.1f, float(cam_sample.getNearClippingPlane()));
  bcam->clip_end = float(cam_sample.getFarClippingPlane());
  bcam->dof.focus_distance = float(cam_sample.getFocusDistance());
  bcam->dof.aperture_fstop = float(cam_sample.getFStop());
}

void AbcCameraReader::readObjectData(Main *bmain, const ISampleSelector &sample_sel)
{
  Camera *bcam = BKE_camera_add(bmain, m_data_name.c_str());
  read_camera_sample(bcam, m_schema, sample_sel);
  m_object = BKE_object_add_only_object(bmain, OB_CAMERA, m_object_name.c_str());
  m_object->data = id_cast<ID *>(bcam);
}

/* The macro that needs to be passed should have arguments :
 * (short_name, rna_path, member_accessor) */
#define ENUMERATE_CAMERA_PROPERTIES(X) \
  X(lens, lens, lens) \
  X(sensor_width, sensor_width, sensor_x) \
  X(sensor_height, sensor_height, sensor_y) \
  X(clip_start, clip_start, clip_start) \
  X(clip_end, clip_end, clip_end) \
  X(shift_x, shift_x, shiftx) \
  X(shift_y, shift_y, shifty) \
  X(focus_distance, dof.focus_distance, dof.focus_distance) \
  X(aperture_fstop, dof.aperture_fstop, dof.aperture_fstop) \
  X(interocular_distance, stereo.interocular_distance, stereo.interocular_distance) \
  X(convergence_distance, stereo.convergence_distance, stereo.convergence_distance)

class CameraFCurveCreationHelper : public FCurveCreationHelper {
  Camera *camera_ = nullptr;
  const Alembic::AbcGeom::ICameraSchema &schema_{};

  /* Keep track of what has been modified to remove unnecessary fcurves at the end as Alembic
   * seemingly does not have per property information. */
  struct MemberModified {
#define DECLARE_MEMBER(short_name, rna_path, member_accessor) bool short_name = false;
    ENUMERATE_CAMERA_PROPERTIES(DECLARE_MEMBER)
#undef DECLARE_MEMBER
  };

  MemberModified member_modified_{};

#define DECLARE_FCURVES(short_name, rna_path, member_accessor) \
  FCurve *short_name##_fcurve = nullptr;
  ENUMERATE_CAMERA_PROPERTIES(DECLARE_FCURVES)
#undef DECLARE_FCURVES

 public:
  CameraFCurveCreationHelper(Camera *camera, const Alembic::AbcGeom::ICameraSchema &schema)
      : FCurveCreationHelper(&camera->id), camera_(camera), schema_(schema)
  {
  }

  void create_fcurves(const int sample_count) override
  {
#define CREATE_FCURVE(short_name, rna_path, member_accessor) \
  short_name##_fcurve = create_fcurve({#rna_path, 0}, sample_count);
    ENUMERATE_CAMERA_PROPERTIES(CREATE_FCURVE)
#undef CREATE_FCURVE
  }

  void set_fcurves_sample(const FrameSampleInfo &sample_info) override
  {
    /* To detect what has been modified. */
    Camera last_camera = *camera_;
    read_camera_sample(camera_, schema_, sample_info.selector);

#define SET_FCURVE_SAMPLE(short_name, rna_path, member_accessor) \
  set_fcurve_sample(short_name##_fcurve, \
                    sample_info.sample_index, \
                    sample_info.frame, \
                    camera_->member_accessor); \
  member_modified_.short_name |= last_camera.member_accessor != camera_->member_accessor;
    ENUMERATE_CAMERA_PROPERTIES(SET_FCURVE_SAMPLE)
#undef SET_FCURVE_SAMPLE
  }

  void remove_unnecessary_fcurves() override
  {
#define REMOVE_UNNECESSARY_FCURVE(short_name, rna_path, member_accessor) \
  if (member_modified_.short_name == false) { \
    channelbag->fcurve_remove(*short_name##_fcurve); \
  }
    ENUMERATE_CAMERA_PROPERTIES(REMOVE_UNNECESSARY_FCURVE)
#undef REMOVE_UNNECESSARY_FCURVE
  }
};

std::unique_ptr<FCurveCreationHelper> AbcCameraReader::getKeyFramingHelper()
{
  if (m_schema.isConstant()) {
    return nullptr;
  }

  Camera *camera = id_cast<Camera *>(m_object->data);
  return std::make_unique<CameraFCurveCreationHelper>(camera, m_schema);
}

}  // namespace io::alembic
}  // namespace blender
