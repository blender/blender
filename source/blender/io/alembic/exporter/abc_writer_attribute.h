/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_offset_indices.hh"
#include "BLI_string_ref.hh"

#include <Alembic/AbcGeom/OGeomParam.h>

namespace blender {

struct MStringProperty;

namespace bke {
class AttributeIter;
}

namespace io::alembic {

#define ENUMERATE_EXPORTED_TYPES(X) \
  X(Alembic::AbcGeom::OBoolGeomParam, bool_params) \
  X(Alembic::AbcGeom::OCharGeomParam, int8_params) \
  X(Alembic::AbcGeom::OV2sGeomParam, short2_params) \
  X(Alembic::AbcGeom::OInt32GeomParam, int32_params) \
  X(Alembic::AbcGeom::OV2iGeomParam, int2_params) \
  X(Alembic::AbcGeom::OFloatGeomParam, float_params) \
  X(Alembic::AbcGeom::OV2fGeomParam, float2_params) \
  X(Alembic::AbcGeom::OV3fGeomParam, float3_params) \
  X(Alembic::AbcGeom::OC4fGeomParam, color_params) \
  X(Alembic::AbcGeom::OC4cGeomParam, byte_color_params) \
  X(Alembic::AbcGeom::OQuatfGeomParam, quat_params) \
  X(Alembic::AbcGeom::OM44dGeomParam, float4x4_params) \
  X(Alembic::AbcGeom::OStringGeomParam, string_params)

/* This class holds maps from string to TypedGeomParam for each attribute type supported for
 * export. We use it to keep track of exported GeomParams so that we can retrieve the proper
 * GeomParam for each attribute for each frame as the Alembic API does not really allow to get
 * a TypeGeomParam from a OCompound property after creation. */
class AttributeParamMaps {
  /* Store params in maps so we can retrieve them across frames to properly account for animated
   * data. */
#define DECLARE_MAPS(abc_type, map_name) std::map<std::string, abc_type> map_name{};
  ENUMERATE_EXPORTED_TYPES(DECLARE_MAPS)
#undef DECLARE_MAPS

 public:
#define ENSURE_PARAM_FUNC(ParamType, map) \
  void ensure_param(const Alembic::Abc::OCompoundProperty &prop, \
                    ParamType &param, \
                    const StringRefNull name, \
                    Alembic::AbcGeom::GeometryScope scope) \
  { \
    param = map[name]; \
    if (!param.valid()) { \
      param = ParamType(prop, name, false, scope, 1); \
      map[name] = param; \
    } \
  }
  ENUMERATE_EXPORTED_TYPES(ENSURE_PARAM_FUNC)
#undef ENSURE_PARAM_FUNC

  /* Write empty samples so that the current number of samples matches the number of geometry
   * samples. */
  void write_empty_samples(const size_t num_geom_samples);
};

/* Create a TypedGeomParam for the given AttributeIter.
 * `faces` are used to correct polygon winding for corner attributes.
 * `object_name` is to give useful error messages
 * `num_geom_samples` is to ensure that the attribute has the same number of samples as the
 * underlying geometry. */
void create_geom_param_for_attribute(const Alembic::Abc::OCompoundProperty &prop,
                                     AttributeParamMaps &param_maps,
                                     const bke::AttributeIter &attr,
                                     const int timesample_index,
                                     const OffsetIndices<int> faces,
                                     const StringRefNull object_name,
                                     const size_t num_geom_samples);

}  // namespace io::alembic
}  // namespace blender
