/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "abc_writer_attribute.h"

#include "DNA_meshdata_types.h"  // For MStringProperty

#include "BLI_color.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"

#include "intern/abc_axis_conversion.h"
#include "intern/abc_util.h"

#include "CLG_log.h"

using namespace Alembic::AbcGeom;

static CLG_LogRef LOG = {"io.alembic"};

namespace blender::io::alembic {

template<typename BlenderDataType> struct TypeTraitConverter;

template<> struct TypeTraitConverter<bool> {
  using AbcTraitType = Alembic::Abc::BooleanTPTraits;

  static Alembic::Abc::bool_t convert(bool v)
  {
    return v;
  }
};

template<> struct TypeTraitConverter<int8_t> {
  using AbcTraitType = Alembic::Abc::Int8TPTraits;

  static int8_t convert(int8_t v)
  {
    return static_cast<int8_t>(v);
  }
};

template<> struct TypeTraitConverter<short2> {
  using AbcTraitType = Alembic::Abc::V2sTPTraits;

  static Imath::V2s convert(short2 v)
  {
    return {v.x, v.y};
  }
};

template<> struct TypeTraitConverter<int> {
  using AbcTraitType = Alembic::Abc::Int32TPTraits;

  static int convert(int v)
  {
    return v;
  }
};

template<> struct TypeTraitConverter<int2> {
  using AbcTraitType = Alembic::Abc::V2iTPTraits;

  static Imath::V2i convert(int2 v)
  {
    return {v.x, v.y};
  }
};

template<> struct TypeTraitConverter<float> {
  using AbcTraitType = Alembic::Abc::Float32TPTraits;

  static float convert(float v)
  {
    return v;
  }
};

template<> struct TypeTraitConverter<float2> {
  using AbcTraitType = Alembic::Abc::V2fTPTraits;

  static Imath::V2f convert(float2 v)
  {
    return {v.x, v.y};
  }
};

template<> struct TypeTraitConverter<float3> {
  using AbcTraitType = Alembic::Abc::V3fTPTraits;

  static Imath::V3f convert(float3 v)
  {
    copy_yup_from_zup(v, v);
    return {v.x, v.y, v.z};
  }
};

template<> struct TypeTraitConverter<float4x4> {
  /* Although matrices are stored as floats in Blender, we convert to double as this is what most
   * other software use. */
  using AbcTraitType = Alembic::Abc::M44dTPTraits;

  static Imath::M44d convert(float4x4 v)
  {
    copy_m44_axis_swap(v.ptr(), v.ptr(), ABC_YUP_FROM_ZUP);
    return convert_matrix_datatype(v.ptr());
  }
};

template<> struct TypeTraitConverter<ColorGeometry4f> {
  using AbcTraitType = Alembic::Abc::C4fTPTraits;

  static Imath::C4f convert(ColorGeometry4f v)
  {
    return {v.r, v.g, v.b, v.a};
  }
};

template<> struct TypeTraitConverter<ColorGeometry4b> {
  /* TODO(kevindietrich) : the importer only cares about float colors. */
  using AbcTraitType = Alembic::Abc::C4fTPTraits;

  static Imath::C4f convert(ColorGeometry4b v)
  {
    static constexpr float float_scale = 1.0f / 255.0f;
    return {float(v.r) * float_scale,
            float(v.g) * float_scale,
            float(v.b) * float_scale,
            float(v.a) * float_scale};
  }
};

template<> struct TypeTraitConverter<math::Quaternion> {
  using AbcTraitType = Alembic::Abc::QuatfTPTraits;

  static Imath::Quatf convert(math::Quaternion quat)
  {
    float3 v = quat.imaginary_part();
    copy_yup_from_zup(v, v);
    return {quat.w, v.x, v.y, v.z};
  }
};

template<> struct TypeTraitConverter<MStringProperty> {
  using AbcTraitType = Alembic::Abc::StringTPTraits;

  static std::string convert(const MStringProperty &v)
  {
    return std::string(v.s, size_t(v.s_len));
  }
};

static GeometryScope get_scope_for_attribute_domain(bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Instance:
    case bke::AttrDomain::Edge:
    case bke::AttrDomain::Layer: {
      /* Not supported */
      break;
    }
    case bke::AttrDomain::Point: {
      return kVertexScope;
    }
    case bke::AttrDomain::Curve:
    case bke::AttrDomain::Face: {
      return kUniformScope;
    }
    case bke::AttrDomain::Corner: {
      return kFacevaryingScope;
    }
  }

  return kUnknownScope;
}

template<typename GeomParamType>
static void write_empty_samples(GeomParamType &param, const size_t num_geom_samples)
{
  /* If we were to write the empty sample as a default constructed TypedArray, Alembic would
   * duplicate the last known sample. This could pose problem if, for example, the number of points
   * changes but the attribute is missing on the same frame (it would get a number of points which
   * might be bigger or smaller). Therefore, we need to write an array of size 0, but which still
   * has a valid (non-null) address, as a zero-size array should be understood by everyone as "the
   * attribute is missing". */
  for (size_t i = param.getNumSamples(); i < num_geom_samples; i++) {
    typename GeomParamType::Sample sample;
    static typename GeomParamType::value_type address_for_array{};
    sample.setVals({&address_for_array, 0ul});
    param.set(sample);
  }
}

void AttributeParamMaps::write_empty_samples(const size_t num_geom_samples)
{
#define WRITE_EMPTY_SAMPLE(abc_type, map_name) \
  for (auto &iter : this->map_name) { \
    io::alembic::write_empty_samples(iter.second, num_geom_samples); \
  }
  ENUMERATE_EXPORTED_TYPES(WRITE_EMPTY_SAMPLE)
#undef WRITE_EMPTY_SAMPLE
}

template<typename BlenderDataType>
static void create_geom_param_for_attribute(const Alembic::Abc::OCompoundProperty &prop,
                                            AttributeParamMaps &param_maps,
                                            const VArray<BlenderDataType> &buffer,
                                            const StringRefNull name,
                                            const GeometryScope scope,
                                            const int timesample_index,
                                            const OffsetIndices<int> faces,
                                            const StringRefNull object_name,
                                            const size_t num_geom_samples)
{
  using TypeConverter = TypeTraitConverter<BlenderDataType>;
  using AlembicTraitType = typename TypeConverter::AbcTraitType;
  using AlembicType = typename AlembicTraitType::value_type;

  std::vector<AlembicType> values;

  GeometryScope corrected_scope = scope;

  if (buffer.is_single()) {
    corrected_scope = GeometryScope::kConstantScope;
    values.push_back(TypeConverter::convert(buffer[0]));
  }
  else {
    values.resize(static_cast<size_t>(buffer.size()));
    AlembicType *values_ptr = values.data();
    if (scope == kFacevaryingScope) {
      /* NOTE: data needs to be written in the reverse order. */
      for (const int i : faces.index_range()) {
        const IndexRange face = faces[i];
        for (int j = face.size() - 1; j >= 0; j--) {
          const int blender_index = face[j];
          *values_ptr++ = TypeConverter::convert(buffer[blender_index]);
        }
      }
    }
    else {
      for (int i = 0; i < buffer.size(); i++) {
        *values_ptr++ = TypeConverter::convert(buffer[i]);
      }
    }
  }

  using ParamType = OTypedGeomParam<AlembicTraitType>;
  using SampleType = typename ParamType::Sample;

  try {
    ParamType param;
    param_maps.ensure_param(prop, param, name, corrected_scope);
    param.setTimeSampling(timesample_index);

    write_empty_samples(param, num_geom_samples);

    SampleType sample(values, corrected_scope);
    param.set(sample);
  }
  catch (const Alembic::Util::Exception &ex) {
    CLOG_WARN(&LOG,
              "On object '%s', error writing attribute '%s' : %s",
              object_name.c_str(),
              name.c_str(),
              ex.what());
  }
}

void create_geom_param_for_attribute(const Alembic::Abc::OCompoundProperty &prop,
                                     AttributeParamMaps &param_maps,
                                     const bke::AttributeIter &attr,
                                     const int timesample_index,
                                     const OffsetIndices<int> faces,
                                     const StringRefNull object_name,
                                     const size_t num_geom_samples)
{
  const GeometryScope scope = get_scope_for_attribute_domain(attr.domain);
  if (scope == kUnknownScope) {
    CLOG_WARN(&LOG,
              "On object '%s', attribute '%s' (Blender domain %d, type %d) cannot be converted "
              "to Alembic",
              object_name.c_str(),
              attr.name.c_str(),
              int8_t(attr.domain),
              int(attr.data_type));
    return;
  }

  const GVArray attribute = *attr.get();

  /* #bke::attribute_math::to_static_type does not yet support strings. */
  if (attr.data_type == bke::AttrType::String) {
    create_geom_param_for_attribute(prop,
                                    param_maps,
                                    attribute.typed<MStringProperty>(),
                                    attr.name,
                                    scope,
                                    timesample_index,
                                    faces,
                                    object_name,
                                    num_geom_samples);
    return;
  }

  bke::attribute_math::to_static_type(attr.data_type, [&]<typename BlenderT>() {
    if constexpr (!std::is_same_v<BlenderT, float4>) {
      create_geom_param_for_attribute(prop,
                                      param_maps,
                                      attribute.typed<BlenderT>(),
                                      attr.name,
                                      scope,
                                      timesample_index,
                                      faces,
                                      object_name,
                                      num_geom_samples);
    }
    else {
      /* Alembic does not have an equivalent to float4. If needed we could either flatten it to a
       * float, export it as a float2 with an array extent of 2, or even export as quaternion or
       * box2. */
      CLOG_WARN(&LOG,
                "On object '%s', attribute '%s' of type %d is not supported by Alembic",
                object_name.c_str(),
                attr.name.c_str(),
                int(attr.data_type));
    }
  });
}

}  // namespace blender::io::alembic
