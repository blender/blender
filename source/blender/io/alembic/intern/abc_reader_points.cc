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

#include "BLI_color_types.hh"

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

static void read_points_sample(const IPointsSchema &schema,
                               const ISampleSelector &selector,
                               MutableSpan<float3> r_points)
{
  Alembic::AbcGeom::IPointsSchema::Sample sample = schema.getValue(selector);

  const P3fArraySamplePtr &positions = sample.getPositions();
  read_points(positions, r_points);
}

template<typename TOut, typename TIn> static TOut convert_abc_value(const TIn &in)
{
  static_assert(std::is_same_v<TIn, TOut>,
                "convert_abc_value needs to be explicitly specialized for each pair of types");
  return in;
}

template<> float3 convert_abc_value(const V3f &in)
{
  float3 out;
  copy_zup_from_yup(out, in.getValue());
  return out;
}

template<> ColorGeometry4f convert_abc_value(const C3f &in)
{
  return ColorGeometry4f(in[0], in[1], in[2], 1.0f);
}

template<> float2 convert_abc_value(const V2f &in)
{
  return in.getValue();
}

template<typename TArrayProperty, typename TWriteValue>
static void read_typed_property_sample(const ICompoundProperty &parent,
                                       const ISampleSelector &selector,
                                       const std::string &name,
                                       bke::MutableAttributeAccessor &attribute_accessor)
{
  const TArrayProperty &array_prop = TArrayProperty(parent, name);
  if (array_prop) {
    using SamplePtr = typename TArrayProperty::sample_ptr_type;
    using ValueType = typename TArrayProperty::value_type;

    const SamplePtr sample_ptr = array_prop.getValue(selector);
    bke::SpanAttributeWriter<TWriteValue> writer =
        attribute_accessor.lookup_or_add_for_write_span<TWriteValue>(name, bke::AttrDomain::Point);
    MutableSpan<TWriteValue> span = writer.span;
    for (const int64_t i : IndexRange(std::min(span.size(), int64_t(sample_ptr->size())))) {
      ValueType value = (*sample_ptr)[i];
      span[i] = convert_abc_value<TWriteValue>(value);
    }
    writer.finish();
  }
}

static void read_point_arb_geom_params(const IPointsSchema &schema,
                                       const ISampleSelector &selector,
                                       bke::MutableAttributeAccessor &attribute_accessor)
{
  const ICompoundProperty prop = schema.getArbGeomParams();
  if (!prop.valid()) {
    return;
  }

  for (size_t i = 0; i < prop.getNumProperties(); i++) {
    const PropertyHeader header = prop.getPropertyHeader(i);
    const PropertyType property_type = header.getPropertyType();
    if (property_type != kArrayProperty) {
      // currently unsupported
      continue;
    }

    const DataType data_type = header.getDataType();
    const MetaData metadata = header.getMetaData();
    const std::string interpretation = metadata.get("interpretation");
    const std::string name = header.getName();

    if (data_type == DataType(kFloat32POD, 3)) {
      if (interpretation == C3fTPTraits::interpretation()) {
        read_typed_property_sample<IC3fArrayProperty, ColorGeometry4f>(
            prop, selector, name, attribute_accessor);
      }
      else if (interpretation == N3fTPTraits::interpretation()) {
        read_typed_property_sample<IN3fArrayProperty, float3>(
            prop, selector, name, attribute_accessor);
      }
      else {
        read_typed_property_sample<IV3fArrayProperty, float3>(
            prop, selector, name, attribute_accessor);
      }
    }
    else if (data_type == DataType(kFloat32POD, 2)) {
      read_typed_property_sample<IV2fArrayProperty, float2>(
          prop, selector, name, attribute_accessor);
    }
    else if (data_type == DataType(kFloat32POD, 1)) {
      read_typed_property_sample<IFloatArrayProperty, float>(
          prop, selector, name, attribute_accessor);
    }
  }
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
  read_points_sample(m_schema, sample_sel, point_positions);

  MutableSpan<float> point_radii = pointcloud->radius_for_write();

  if (widths) {
    for (const int64_t i : IndexRange(std::min(point_radii.size(), int64_t(widths->size())))) {
      point_radii[i] = (*widths)[i] / 2.0f;
    }
  }
  else {
    point_radii.fill(0.01f);
  }

  read_point_arb_geom_params(m_schema, sample_sel, attribute_accessor);

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
