/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "BLI_math_vector_types.hh"

#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/ISampleSelector.h>
#include <Alembic/Abc/OCompoundProperty.h>
#include <Alembic/Abc/TypedArraySample.h>
#include <Alembic/AbcCoreAbstract/Foundation.h>
#include <Alembic/AbcGeom/GeometryScope.h>
#include <Alembic/AbcGeom/OGeomParam.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct CustomData;
struct Mesh;

using Alembic::Abc::ICompoundProperty;
using Alembic::Abc::OCompoundProperty;
using Alembic::Abc::V3fArraySamplePtr;
namespace blender::io::alembic {

struct UVSample {
  std::vector<Imath::V2f> uvs;
  std::vector<uint32_t> indices;
};

struct CDStreamConfig {
  int *corner_verts = nullptr;
  int totloop = 0;

  int *face_offsets = nullptr;
  int faces_num = 0;

  float3 *positions = nullptr;
  int totvert = 0;

  float2 *uv_map = nullptr;

  CustomData *loopdata = nullptr;

  bool pack_uvs = false;

  /* TODO(kevin): might need a better way to handle adding and/or updating
   * custom data such that it updates the custom data holder and its pointers properly. */
  Mesh *mesh = nullptr;
  void *(*add_customdata_cb)(Mesh *mesh, const char *name, int data_type) = nullptr;

  Alembic::Abc::chrono_t time = 0.0;
  int timesample_index = 0;

  const char **modifier_error_message = nullptr;

  /* Alembic needs Blender to keep references to C++ objects (the destructors finalize the writing
   * to ABC). The following fields are all used to keep these references. */

  /* Mapping from UV map name to its ABC property, for the 2nd and subsequent UV maps; the primary
   * UV map is kept alive by the Alembic mesh sample itself. */
  std::map<std::string, Alembic::AbcGeom::OV2fGeomParam> abc_uv_maps;

  /* ORCO coordinates, aka Generated Coordinates. */
  Alembic::AbcGeom::OV3fGeomParam abc_orco;

  /* Mapping from vertex color layer name to its Alembic color data. */
  std::map<std::string, Alembic::AbcGeom::OC4fGeomParam> abc_vertex_colors;

  CDStreamConfig() = default;
};

/* Get the UVs for the main UV property on a OSchema.
 * Returns the name of the UV layer.
 *
 * For now the active layer is used, maybe needs a better way to choose this. */
const char *get_uv_sample(UVSample &sample, const CDStreamConfig &config, const Mesh &mesh);

void write_generated_coordinates(const OCompoundProperty &prop, CDStreamConfig &config);

void read_velocity(const V3fArraySamplePtr &velocities,
                   const CDStreamConfig &config,
                   const float velocity_scale);

void read_generated_coordinates(const ICompoundProperty &prop,
                                const CDStreamConfig &config,
                                const Alembic::Abc::ISampleSelector &iss);

void write_custom_data(const OCompoundProperty &prop,
                       CDStreamConfig &config,
                       const Mesh &mesh,
                       int data_type);

void read_custom_data(const std::string &iobject_full_name,
                      const ICompoundProperty &prop,
                      const CDStreamConfig &config,
                      const Alembic::Abc::ISampleSelector &iss);

enum AbcUvScope {
  ABC_UV_SCOPE_NONE,
  ABC_UV_SCOPE_LOOP,
  ABC_UV_SCOPE_VERTEX,
};

/**
 * UVs can be defined per-loop (one value per vertex per face), or per-vertex (one value per
 * vertex). The first case is the most common, as this is the standard way of storing this data
 * given that some vertices might be on UV seams and have multiple possible UV coordinates; the
 * second case can happen when the mesh is split according to the UV islands, in which case storing
 * a single UV value per vertex allows to de-duplicate data and thus to reduce the file size since
 * vertices are guaranteed to only have a single UV coordinate.
 */
AbcUvScope get_uv_scope(const Alembic::AbcGeom::GeometryScope scope,
                        const CDStreamConfig &config,
                        const Alembic::AbcGeom::UInt32ArraySamplePtr &indices);

}  // namespace blender::io::alembic
