/*
 * Copyright 2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef WITH_ALEMBIC

#  include <Alembic/AbcCoreFactory/All.h>
#  include <Alembic/AbcGeom/All.h>

#  include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

class AlembicProcedural;
class AttributeRequestSet;
class Progress;
struct CachedData;

/* Maps a FaceSet whose name matches that of a Shader to the index of said shader in the Geometry's
 * used_shaders list. */
struct FaceSetShaderIndexPair {
  Alembic::AbcGeom::IFaceSet face_set;
  int shader_index;
};

/* Data of an IPolyMeshSchema that we need to read. */
struct PolyMeshSchemaData {
  Alembic::AbcGeom::TimeSamplingPtr time_sampling;
  size_t num_samples;
  Alembic::AbcGeom::MeshTopologyVariance topology_variance;

  Alembic::AbcGeom::IP3fArrayProperty positions;
  Alembic::AbcGeom::IInt32ArrayProperty face_indices;
  Alembic::AbcGeom::IInt32ArrayProperty face_counts;

  Alembic::AbcGeom::IN3fGeomParam normals;

  vector<FaceSetShaderIndexPair> shader_face_sets;

  // Unsupported for now.
  Alembic::AbcGeom::IV3fArrayProperty velocities;
};

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const PolyMeshSchemaData &data,
                        Progress &progress);

/* Data of an ISubDSchema that we need to read. */
struct SubDSchemaData {
  Alembic::AbcGeom::TimeSamplingPtr time_sampling;
  size_t num_samples;
  Alembic::AbcGeom::MeshTopologyVariance topology_variance;

  Alembic::AbcGeom::IInt32ArrayProperty face_counts;
  Alembic::AbcGeom::IInt32ArrayProperty face_indices;
  Alembic::AbcGeom::IP3fArrayProperty positions;

  Alembic::AbcGeom::IInt32ArrayProperty crease_indices;
  Alembic::AbcGeom::IInt32ArrayProperty crease_lengths;
  Alembic::AbcGeom::IFloatArrayProperty crease_sharpnesses;

  vector<FaceSetShaderIndexPair> shader_face_sets;

  // Those are unsupported for now.
  Alembic::AbcGeom::IInt32ArrayProperty corner_indices;
  Alembic::AbcGeom::IFloatArrayProperty corner_sharpnesses;
  Alembic::AbcGeom::IInt32Property face_varying_interpolate_boundary;
  Alembic::AbcGeom::IInt32Property face_varying_propagate_corners;
  Alembic::AbcGeom::IInt32Property interpolate_boundary;
  Alembic::AbcGeom::IInt32ArrayProperty holes;
  Alembic::AbcGeom::IStringProperty subdivision_scheme;
  Alembic::AbcGeom::IV3fArrayProperty velocities;
};

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const SubDSchemaData &data,
                        Progress &progress);

/* Data of a ICurvesSchema that we need to read. */
struct CurvesSchemaData {
  Alembic::AbcGeom::TimeSamplingPtr time_sampling;
  size_t num_samples;
  Alembic::AbcGeom::MeshTopologyVariance topology_variance;

  Alembic::AbcGeom::IP3fArrayProperty positions;

  Alembic::AbcGeom::IInt32ArrayProperty num_vertices;

  float default_radius;
  float radius_scale;

  // Those are unsupported for now.
  Alembic::AbcGeom::IV3fArrayProperty velocities;
  // if this property is invalid then the weight for every point is 1
  Alembic::AbcGeom::IFloatArrayProperty position_weights;
  Alembic::AbcGeom::IN3fGeomParam normals;
  Alembic::AbcGeom::IFloatGeomParam widths;
  Alembic::AbcGeom::IUcharArrayProperty orders;
  Alembic::AbcGeom::IFloatArrayProperty knots;

  // TODO(@kevindietrich): type, basis, wrap
};

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const CurvesSchemaData &data,
                        Progress &progress);

void read_attributes(AlembicProcedural *proc,
                     CachedData &cache,
                     const Alembic::AbcGeom::ICompoundProperty &arb_geom_params,
                     const Alembic::AbcGeom::IV2fGeomParam &default_uvs_param,
                     const AttributeRequestSet &requested_attributes,
                     Progress &progress);

CCL_NAMESPACE_END

#endif
