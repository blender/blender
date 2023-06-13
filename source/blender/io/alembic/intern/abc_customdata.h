/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup balembic
 */

#include "BLI_math_vector_types.hh"

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#include <map>

#include "BLI_math_vector_types.hh"

struct CustomData;
struct Mesh;

using Alembic::Abc::ICompoundProperty;
using Alembic::Abc::OCompoundProperty;
namespace blender::io::alembic {

struct UVSample {
  std::vector<Imath::V2f> uvs;
  std::vector<uint32_t> indices;
};

struct CDStreamConfig {
  int *corner_verts;
  int totloop;

  int *poly_offsets;
  int totpoly;

  float3 *positions;
  int totvert;

  float2 *mloopuv;

  CustomData *loopdata;

  bool pack_uvs;

  /* TODO(kevin): might need a better way to handle adding and/or updating
   * custom data such that it updates the custom data holder and its pointers properly. */
  Mesh *mesh;
  void *(*add_customdata_cb)(Mesh *mesh, const char *name, int data_type);

  double weight;
  Alembic::Abc::chrono_t time;
  int timesample_index;
  bool use_vertex_interpolation;
  Alembic::AbcGeom::index_t index;
  Alembic::AbcGeom::index_t ceil_index;

  const char **modifier_error_message;

  /* Alembic needs Blender to keep references to C++ objects (the destructors finalize the writing
   * to ABC). The following fields are all used to keep these references. */

  /* Mapping from UV map name to its ABC property, for the 2nd and subsequent UV maps; the primary
   * UV map is kept alive by the Alembic mesh sample itself. */
  std::map<std::string, Alembic::AbcGeom::OV2fGeomParam> abc_uv_maps;

  /* ORCO coordinates, aka Generated Coordinates. */
  Alembic::AbcGeom::OV3fGeomParam abc_orco;

  /* Mapping from vertex color layer name to its Alembic color data. */
  std::map<std::string, Alembic::AbcGeom::OC4fGeomParam> abc_vertex_colors;

  CDStreamConfig()
      : corner_verts(NULL),
        totloop(0),
        poly_offsets(NULL),
        totpoly(0),
        totvert(0),
        pack_uvs(false),
        mesh(NULL),
        add_customdata_cb(NULL),
        weight(0.0),
        time(0.0),
        index(0),
        ceil_index(0),
        modifier_error_message(NULL)
  {
  }
};

/* Get the UVs for the main UV property on a OSchema.
 * Returns the name of the UV layer.
 *
 * For now the active layer is used, maybe needs a better way to choose this. */
const char *get_uv_sample(UVSample &sample, const CDStreamConfig &config, CustomData *data);

void write_generated_coordinates(const OCompoundProperty &prop, CDStreamConfig &config);

void read_generated_coordinates(const ICompoundProperty &prop,
                                const CDStreamConfig &config,
                                const Alembic::Abc::ISampleSelector &iss);

void write_custom_data(const OCompoundProperty &prop,
                       CDStreamConfig &config,
                       CustomData *data,
                       int data_type);

void read_custom_data(const std::string &iobject_full_name,
                      const ICompoundProperty &prop,
                      const CDStreamConfig &config,
                      const Alembic::Abc::ISampleSelector &iss);

typedef enum {
  ABC_UV_SCOPE_NONE,
  ABC_UV_SCOPE_LOOP,
  ABC_UV_SCOPE_VERTEX,
} AbcUvScope;

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
