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

#include "BLI_math_vector.h"

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
    const char **err_str) const
{
  if (!Alembic::AbcGeom::IPoints::matches(alembic_header)) {
    *err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Points when importing, but not any "
        "more");
    return false;
  }

  if (ob->type != OB_POINTCLOUD) {
    *err_str = RPT_("Object type mismatch, Alembic object path points to Points.");
    return false;
  }

  return true;
}

void AbcPointsReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  PointCloud *point_cloud = static_cast<PointCloud *>(
      BKE_pointcloud_add_default(bmain, m_data_name.c_str()));

  bke::GeometrySet geometry_set = bke::GeometrySet::from_pointcloud(
      point_cloud, bke::GeometryOwnershipType::Editable);
  read_geometry(geometry_set, sample_sel, 0, "", 1.0f, nullptr);

  PointCloud *read_point_cloud =
      geometry_set.get_component_for_write<bke::PointCloudComponent>().release();

  if (read_point_cloud != point_cloud) {
    BKE_pointcloud_nomain_to_pointcloud(read_point_cloud, point_cloud);
  }

  m_object = BKE_object_add_only_object(bmain, OB_POINTCLOUD, m_object_name.c_str());
  m_object->data = point_cloud;

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
                                    const char * /*velocity_name*/,
                                    const float /*velocity_scale*/,
                                    const char **err_str)
{
  BLI_assert(geometry_set.has_pointcloud());

  IPointsSchema::Sample sample;
  try {
    sample = m_schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    *err_str = RPT_("Error reading points sample; more detail on the console");
    printf("Alembic: error reading points sample for '%s/%s' at time %f: %s\n",
           m_iobject.getFullName().c_str(),
           m_schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return;
  }

  PointCloud *existing_point_cloud = geometry_set.get_pointcloud_for_write();
  PointCloud *point_cloud = existing_point_cloud;

  const P3fArraySamplePtr &positions = sample.getPositions();

  const IFloatGeomParam widths_param = m_schema.getWidthsParam();
  FloatArraySamplePtr radii;

  if (widths_param.valid()) {
    IFloatGeomParam::Sample wsample = widths_param.getExpandedValue(sample_sel);
    radii = wsample.getVals();
  }

  if (point_cloud->totpoint != positions->size()) {
    point_cloud = BKE_pointcloud_new_nomain(positions->size());
  }

  bke::MutableAttributeAccessor attribute_accessor = point_cloud->attributes_for_write();

  bke::SpanAttributeWriter<float3> positions_writer =
      attribute_accessor.lookup_or_add_for_write_span<float3>("position", bke::AttrDomain::Point);
  MutableSpan<float3> point_positions = positions_writer.span;
  N3fArraySamplePtr normals = read_points_sample(m_schema, sample_sel, point_positions);
  positions_writer.finish();

  bke::SpanAttributeWriter<float> point_radii_writer =
      attribute_accessor.lookup_or_add_for_write_span<float>("radius", bke::AttrDomain::Point);
  MutableSpan<float> point_radii = point_radii_writer.span;

  if (radii) {
    for (size_t i = 0; i < radii->size(); i++) {
      point_radii[i] = (*radii)[i];
    }
  }
  else {
    point_radii.fill(0.01f);
  }
  point_radii_writer.finish();

  if (normals) {
    bke::SpanAttributeWriter<float3> normals_writer =
        attribute_accessor.lookup_or_add_for_write_span<float3>("N", bke::AttrDomain::Point);
    MutableSpan<float3> point_normals = normals_writer.span;
    for (size_t i = 0; i < normals->size(); i++) {
      Imath::V3f nor_in = (*normals)[i];
      copy_zup_from_yup(point_normals[i], nor_in.getValue());
    }
    normals_writer.finish();
  }

  geometry_set.replace_pointcloud(point_cloud);
}

}  // namespace blender::io::alembic
