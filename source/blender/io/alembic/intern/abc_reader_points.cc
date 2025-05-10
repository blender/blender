/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_points.h"
#include "abc_axis_conversion.h"
#include "abc_util.h"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLT_translation.hh"

#include "BKE_geometry_set.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"

#include <algorithm>

using namespace Alembic::AbcGeom;

namespace blender::io::alembic {

AbcPointsReader::AbcPointsReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  IPoints ipoints(m_iobject, kWrapExisting);
  m_schema = ipoints.getSchema();
  get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcPointsReader::valid() const
{
  return m_schema.valid();
}

bool AbcPointsReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **r_err_str) const
{
  if (!Alembic::AbcGeom::IPoints::matches(alembic_header)) {
    *r_err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Points when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_POINTCLOUD) {
    *r_err_str = RPT_("Object type mismatch, Alembic object path points to Points.");
    return false;
  }

  return true;
}

void AbcPointsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  PointCloud *pointcloud = BKE_pointcloud_add(bmain, m_data_name.c_str());

  bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
      pointcloud, bke::GeometryOwnershipType::Editable);
  read_geometry(geometry_set, sample_sel, 0, "", 1.0f, nullptr);

  PointCloud *read_pointcloud =
      geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

  if (read_pointcloud != pointcloud) {
    BKE_pointcloud_nomain_to_pointcloud(read_pointcloud, pointcloud);
  }

  m_object = BKE_object_add_only_object(bmain, OB_POINTCLOUD, m_object_name.c_str());
  m_object->data = pointcloud;

  if (m_settings->always_add_cache_reader || has_animations(m_schema, m_settings)) {
    addCacheModifier();
  }
}

static void read_points(const P3fArraySamplePtr positions, MutableSpan<float3> r_points)
{
  for (size_t i = 0; i < positions->size(); i++) {
    copy_zup_from_yup(r_points[i], (*positions)[i].getValue());
  }
}

static N3fArraySamplePtr read_points_sample(const IPointsSchema &schema,
                                            const ISampleSelector &selector,
                                            MutableSpan<float3> r_points)
{
  Alembic::AbcGeom::IPointsSchema::Sample sample = schema.getValue(selector);

  const P3fArraySamplePtr &positions = sample.getPositions();

  ICompoundProperty prop = schema.getArbGeomParams();
  N3fArraySamplePtr vnormals;

  if (has_property(prop, "N")) {
    const Alembic::Util::uint32_t itime = static_cast<Alembic::Util::uint32_t>(
        selector.getRequestedTime());
    const IN3fArrayProperty &normals_prop = IN3fArrayProperty(prop, "N", itime);

    if (normals_prop) {
      vnormals = normals_prop.getValue(selector);
    }
  }

  read_points(positions, r_points);
  return vnormals;
}

void AbcPointsReader::read_geometry(bke::GeometrySet &geometry_set,
                                    const Alembic::Abc::ISampleSelector &sample_sel,
                                    int /*read_flag*/,
                                    const char *velocity_name,
                                    const float velocity_scale,
                                    const char **r_err_str)
{
  BLI_assert(geometry_set.has_pointcloud());

  IPointsSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    *r_err_str = RPT_("Error reading points sample; more detail on the console");
    printf("Alembic: error reading points sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return;
  }

  PointCloud *existing_pointcloud = geometry_set.get_pointcloud_for_write();
  PointCloud *pointcloud = existing_pointcloud;

  const P3fArraySamplePtr &positions = sample.getPositions();

  const IFloatGeomParam widths_param = m_schema.getWidthsParam();
  FloatArraySamplePtr widths;

  if (widths_param.valid()) {
    IFloatGeomParam::Sample wsample = widths_param.getExpandedValue(sample_sel);
    widths = wsample.getVals();
  }

  if (pointcloud->totpoint != positions->size()) {
    pointcloud = BKE_pointcloud_new_nomain(positions->size());
  }

  bke::MutableAttributeAccessor attribute_accessor = pointcloud->attributes_for_write();

  MutableSpan<float3> point_positions = pointcloud->positions_for_write();
  N3fArraySamplePtr normals = read_points_sample(m_schema, sample_sel, point_positions);
  MutableSpan<float> point_radii = pointcloud->radius_for_write();

  if (widths) {
    for (const int64_t i : IndexRange(std::min(point_radii.size(), int64_t(widths->size())))) {
      point_radii[i] = (*widths)[i] / 2.0f;
    }
  }
  else {
    point_radii.fill(0.01f);
  }

  if (normals) {
    bke::SpanAttributeWriter<float3> normals_writer =
        attribute_accessor.lookup_or_add_for_write_span<float3>("N", bke::AttrDomain::Point);
    MutableSpan<float3> point_normals = normals_writer.span;
    for (const int64_t i : IndexRange(std::min(point_normals.size(), int64_t(normals->size())))) {
      Imath::V3f nor_in = (*normals)[i];
      copy_zup_from_yup(point_normals[i], nor_in.getValue());
    }
    normals_writer.finish();
  }

  if (velocity_name != nullptr && velocity_scale != 0.0f) {
    V3fArraySamplePtr velocities = get_velocity_prop(m_schema, sample_sel, velocity_name);
    if (velocities && pointcloud->totpoint == int(velocities->size())) {
      bke::SpanAttributeWriter<float3> velocity_writer =
          attribute_accessor.lookup_or_add_for_write_span<float3>("velocity",
                                                                  bke::AttrDomain::Point);
      MutableSpan<float3> point_velocity = velocity_writer.span;
      for (const int64_t i :
           IndexRange(std::min(point_velocity.size(), int64_t(velocities->size()))))
      {
        const Imath::V3f &vel_in = (*velocities)[i];
        copy_zup_from_yup(point_velocity[i], vel_in.getValue());
        point_velocity[i] *= velocity_scale;
      }
      velocity_writer.finish();
    }
  }

  geometry_set.replace_pointcloud(pointcloud);
}

}  // namespace blender::io::alembic
