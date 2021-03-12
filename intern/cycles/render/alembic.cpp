/*
 * Copyright 2011-2018 Blender Foundation
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

#include "render/alembic.h"

#include "render/camera.h"
#include "render/curves.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_progress.h"
#include "util/util_transform.h"
#include "util/util_vector.h"

#ifdef WITH_ALEMBIC

using namespace Alembic::AbcGeom;

CCL_NAMESPACE_BEGIN

/* TODO(@kevindietrich): motion blur support */

void CachedData::clear()
{
  attributes.clear();
  curve_first_key.clear();
  curve_keys.clear();
  curve_radius.clear();
  curve_shader.clear();
  num_ngons.clear();
  shader.clear();
  subd_creases_edge.clear();
  subd_creases_weight.clear();
  subd_face_corners.clear();
  subd_num_corners.clear();
  subd_ptex_offset.clear();
  subd_smooth.clear();
  subd_start_corner.clear();
  transforms.clear();
  triangles.clear();
  triangles_loops.clear();
  vertices.clear();

  for (CachedAttribute &attr : attributes) {
    attr.data.clear();
  }

  attributes.clear();
}

CachedData::CachedAttribute &CachedData::add_attribute(const ustring &name,
                                                       const TimeSampling &time_sampling)
{
  for (auto &attr : attributes) {
    if (attr.name == name) {
      return attr;
    }
  }

  CachedAttribute &attr = attributes.emplace_back();
  attr.name = name;
  attr.data.set_time_sampling(time_sampling);
  return attr;
}

bool CachedData::is_constant() const
{
#  define CHECK_IF_CONSTANT(data) \
    if (!data.is_constant()) { \
      return false; \
    }

  CHECK_IF_CONSTANT(curve_first_key)
  CHECK_IF_CONSTANT(curve_keys)
  CHECK_IF_CONSTANT(curve_radius)
  CHECK_IF_CONSTANT(curve_shader)
  CHECK_IF_CONSTANT(num_ngons)
  CHECK_IF_CONSTANT(shader)
  CHECK_IF_CONSTANT(subd_creases_edge)
  CHECK_IF_CONSTANT(subd_creases_weight)
  CHECK_IF_CONSTANT(subd_face_corners)
  CHECK_IF_CONSTANT(subd_num_corners)
  CHECK_IF_CONSTANT(subd_ptex_offset)
  CHECK_IF_CONSTANT(subd_smooth)
  CHECK_IF_CONSTANT(subd_start_corner)
  CHECK_IF_CONSTANT(transforms)
  CHECK_IF_CONSTANT(triangles)
  CHECK_IF_CONSTANT(triangles_loops)
  CHECK_IF_CONSTANT(vertices)

  for (const CachedAttribute &attr : attributes) {
    if (!attr.data.is_constant()) {
      return false;
    }
  }

  return true;

#  undef CHECK_IF_CONSTANT
}

void CachedData::invalidate_last_loaded_time(bool attributes_only)
{
  if (attributes_only) {
    for (CachedAttribute &attr : attributes) {
      attr.data.invalidate_last_loaded_time();
    }

    return;
  }

  curve_first_key.invalidate_last_loaded_time();
  curve_keys.invalidate_last_loaded_time();
  curve_radius.invalidate_last_loaded_time();
  curve_shader.invalidate_last_loaded_time();
  num_ngons.invalidate_last_loaded_time();
  shader.invalidate_last_loaded_time();
  subd_creases_edge.invalidate_last_loaded_time();
  subd_creases_weight.invalidate_last_loaded_time();
  subd_face_corners.invalidate_last_loaded_time();
  subd_num_corners.invalidate_last_loaded_time();
  subd_ptex_offset.invalidate_last_loaded_time();
  subd_smooth.invalidate_last_loaded_time();
  subd_start_corner.invalidate_last_loaded_time();
  transforms.invalidate_last_loaded_time();
  triangles.invalidate_last_loaded_time();
  triangles_loops.invalidate_last_loaded_time();
  vertices.invalidate_last_loaded_time();
}

void CachedData::set_time_sampling(TimeSampling time_sampling)
{
  curve_first_key.set_time_sampling(time_sampling);
  curve_keys.set_time_sampling(time_sampling);
  curve_radius.set_time_sampling(time_sampling);
  curve_shader.set_time_sampling(time_sampling);
  num_ngons.set_time_sampling(time_sampling);
  shader.set_time_sampling(time_sampling);
  subd_creases_edge.set_time_sampling(time_sampling);
  subd_creases_weight.set_time_sampling(time_sampling);
  subd_face_corners.set_time_sampling(time_sampling);
  subd_num_corners.set_time_sampling(time_sampling);
  subd_ptex_offset.set_time_sampling(time_sampling);
  subd_smooth.set_time_sampling(time_sampling);
  subd_start_corner.set_time_sampling(time_sampling);
  transforms.set_time_sampling(time_sampling);
  triangles.set_time_sampling(time_sampling);
  triangles_loops.set_time_sampling(time_sampling);
  vertices.set_time_sampling(time_sampling);

  for (CachedAttribute &attr : attributes) {
    attr.data.set_time_sampling(time_sampling);
  }
}

/* get the sample times to load data for the given the start and end frame of the procedural */
static set<chrono_t> get_relevant_sample_times(AlembicProcedural *proc,
                                               const TimeSampling &time_sampling,
                                               size_t num_samples)
{
  set<chrono_t> result;

  if (num_samples < 2) {
    result.insert(0.0);
    return result;
  }

  double start_frame = (double)(proc->get_start_frame() / proc->get_frame_rate());
  double end_frame = (double)((proc->get_end_frame() + 1) / proc->get_frame_rate());

  size_t start_index = time_sampling.getFloorIndex(start_frame, num_samples).first;
  size_t end_index = time_sampling.getCeilIndex(end_frame, num_samples).first;

  for (size_t i = start_index; i < end_index; ++i) {
    result.insert(time_sampling.getSampleTime(i));
  }

  return result;
}

static float3 make_float3_from_yup(const V3f &v)
{
  return make_float3(v.x, -v.z, v.y);
}

static M44d convert_yup_zup(const M44d &mtx, float scale_mult)
{
  V3d scale, shear, rotation, translation;
  extractSHRT(mtx,
              scale,
              shear,
              rotation,
              translation,
              true,
              IMATH_INTERNAL_NAMESPACE::Euler<double>::XZY);

  M44d rot_mat, scale_mat, trans_mat;
  rot_mat.setEulerAngles(V3d(rotation.x, -rotation.z, rotation.y));
  scale_mat.setScale(V3d(scale.x, scale.z, scale.y));
  trans_mat.setTranslation(V3d(translation.x, -translation.z, translation.y));

  M44d temp_mat = scale_mat * rot_mat * trans_mat;

  scale_mat.setScale(static_cast<double>(scale_mult));

  return temp_mat * scale_mat;
}

static void transform_decompose(
    const M44d &mat, V3d &scale, V3d &shear, Quatd &rotation, V3d &translation)
{
  M44d mat_remainder(mat);

  /* extract scale and shear */
  Imath::extractAndRemoveScalingAndShear(mat_remainder, scale, shear);

  /* extract translation */
  translation.x = mat_remainder[3][0];
  translation.y = mat_remainder[3][1];
  translation.z = mat_remainder[3][2];

  /* extract rotation */
  rotation = extractQuat(mat_remainder);
}

static M44d transform_compose(const V3d &scale,
                              const V3d &shear,
                              const Quatd &rotation,
                              const V3d &translation)
{
  M44d scale_mat, shear_mat, rot_mat, trans_mat;

  scale_mat.setScale(scale);
  shear_mat.setShear(shear);
  rot_mat = rotation.toMatrix44();
  trans_mat.setTranslation(translation);

  return scale_mat * shear_mat * rot_mat * trans_mat;
}

/* get the matrix for the specified time, or return the identity matrix if there is no exact match
 */
static M44d get_matrix_for_time(const MatrixSampleMap &samples, chrono_t time)
{
  MatrixSampleMap::const_iterator iter = samples.find(time);
  if (iter != samples.end()) {
    return iter->second;
  }

  return M44d();
}

/* get the matrix for the specified time, or interpolate between samples if there is no exact match
 */
static M44d get_interpolated_matrix_for_time(const MatrixSampleMap &samples, chrono_t time)
{
  if (samples.empty()) {
    return M44d();
  }

  /* see if exact match */
  MatrixSampleMap::const_iterator iter = samples.find(time);
  if (iter != samples.end()) {
    return iter->second;
  }

  if (samples.size() == 1) {
    return samples.begin()->second;
  }

  if (time <= samples.begin()->first) {
    return samples.begin()->second;
  }

  if (time >= samples.rbegin()->first) {
    return samples.rbegin()->second;
  }

  /* find previous and next time sample to interpolate */
  chrono_t prev_time = samples.begin()->first;
  chrono_t next_time = samples.rbegin()->first;

  for (MatrixSampleMap::const_iterator I = samples.begin(); I != samples.end(); ++I) {
    chrono_t current_time = (*I).first;

    if (current_time > prev_time && current_time <= time) {
      prev_time = current_time;
    }

    if (current_time > next_time && current_time >= time) {
      next_time = current_time;
    }
  }

  const M44d prev_mat = get_matrix_for_time(samples, prev_time);
  const M44d next_mat = get_matrix_for_time(samples, next_time);

  V3d prev_scale, next_scale;
  V3d prev_shear, next_shear;
  V3d prev_translation, next_translation;
  Quatd prev_rotation, next_rotation;

  transform_decompose(prev_mat, prev_scale, prev_shear, prev_rotation, prev_translation);
  transform_decompose(next_mat, next_scale, next_shear, next_rotation, next_translation);

  chrono_t t = (time - prev_time) / (next_time - prev_time);

  /* ensure rotation around the shortest angle  */
  if ((prev_rotation ^ next_rotation) < 0) {
    next_rotation = -next_rotation;
  }

  return transform_compose(Imath::lerp(prev_scale, next_scale, t),
                           Imath::lerp(prev_shear, next_shear, t),
                           Imath::slerp(prev_rotation, next_rotation, t),
                           Imath::lerp(prev_translation, next_translation, t));
}

static void concatenate_xform_samples(const MatrixSampleMap &parent_samples,
                                      const MatrixSampleMap &local_samples,
                                      MatrixSampleMap &output_samples)
{
  set<chrono_t> union_of_samples;

  for (const std::pair<chrono_t, M44d> pair : parent_samples) {
    union_of_samples.insert(pair.first);
  }

  for (const std::pair<chrono_t, M44d> pair : local_samples) {
    union_of_samples.insert(pair.first);
  }

  foreach (chrono_t time, union_of_samples) {
    M44d parent_matrix = get_interpolated_matrix_for_time(parent_samples, time);
    M44d local_matrix = get_interpolated_matrix_for_time(local_samples, time);

    output_samples[time] = local_matrix * parent_matrix;
  }
}

static Transform make_transform(const M44d &a, float scale)
{
  M44d m = convert_yup_zup(a, scale);
  Transform trans;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 4; i++) {
      trans[j][i] = static_cast<float>(m[i][j]);
    }
  }
  return trans;
}

static void add_uvs(AlembicProcedural *proc,
                    const IV2fGeomParam &uvs,
                    CachedData &cached_data,
                    Progress &progress)
{
  if (uvs.getScope() != kFacevaryingScope) {
    return;
  }

  const TimeSamplingPtr time_sampling_ptr = uvs.getTimeSampling();

  TimeSampling time_sampling;
  if (time_sampling_ptr) {
    time_sampling = *time_sampling_ptr;
  }

  std::string name = Alembic::Abc::GetSourceName(uvs.getMetaData());

  /* According to the convention, primary UVs should have had their name
   * set using Alembic::Abc::SetSourceName, but you can't expect everyone
   * to follow it! :) */
  if (name.empty()) {
    name = uvs.getName();
  }

  CachedData::CachedAttribute &attr = cached_data.add_attribute(ustring(name), time_sampling);
  attr.std = ATTR_STD_UV;

  ccl::set<chrono_t> times = get_relevant_sample_times(proc, time_sampling, uvs.getNumSamples());

  foreach (chrono_t time, times) {
    if (progress.get_cancel()) {
      return;
    }

    const ISampleSelector iss = ISampleSelector(time);
    const IV2fGeomParam::Sample uvsample = uvs.getIndexedValue(iss);

    if (!uvsample.valid()) {
      continue;
    }

    const array<int3> *triangles =
        cached_data.triangles.data_for_time_no_check(time).get_data_or_null();
    const array<int3> *triangles_loops =
        cached_data.triangles_loops.data_for_time_no_check(time).get_data_or_null();

    if (!triangles || !triangles_loops) {
      continue;
    }

    array<char> data;
    data.resize(triangles->size() * 3 * sizeof(float2));

    float2 *data_float2 = reinterpret_cast<float2 *>(data.data());

    const unsigned int *indices = uvsample.getIndices()->get();
    const V2f *values = uvsample.getVals()->get();

    for (const int3 &loop : *triangles_loops) {
      unsigned int v0 = indices[loop.x];
      unsigned int v1 = indices[loop.y];
      unsigned int v2 = indices[loop.z];

      data_float2[0] = make_float2(values[v0][0], values[v0][1]);
      data_float2[1] = make_float2(values[v1][0], values[v1][1]);
      data_float2[2] = make_float2(values[v2][0], values[v2][1]);
      data_float2 += 3;
    }

    attr.data.add_data(data, time);
  }
}

static void add_normals(const Int32ArraySamplePtr face_indices,
                        const IN3fGeomParam &normals,
                        double time,
                        CachedData &cached_data)
{
  switch (normals.getScope()) {
    case kFacevaryingScope: {
      const ISampleSelector iss = ISampleSelector(time);
      const IN3fGeomParam::Sample sample = normals.getExpandedValue(iss);

      if (!sample.valid()) {
        return;
      }

      CachedData::CachedAttribute &attr = cached_data.add_attribute(ustring(normals.getName()),
                                                                    *normals.getTimeSampling());
      attr.std = ATTR_STD_VERTEX_NORMAL;

      const array<float3> *vertices =
          cached_data.vertices.data_for_time_no_check(time).get_data_or_null();

      if (!vertices) {
        return;
      }

      array<char> data;
      data.resize(vertices->size() * sizeof(float3));

      float3 *data_float3 = reinterpret_cast<float3 *>(data.data());

      const int *face_indices_array = face_indices->get();
      const N3fArraySamplePtr values = sample.getVals();

      for (size_t i = 0; i < face_indices->size(); ++i) {
        int point_index = face_indices_array[i];
        data_float3[point_index] = make_float3_from_yup(values->get()[i]);
      }

      attr.data.add_data(data, time);
      break;
    }
    case kVaryingScope:
    case kVertexScope: {
      const ISampleSelector iss = ISampleSelector(time);
      const IN3fGeomParam::Sample sample = normals.getExpandedValue(iss);

      if (!sample.valid()) {
        return;
      }

      CachedData::CachedAttribute &attr = cached_data.add_attribute(ustring(normals.getName()),
                                                                    *normals.getTimeSampling());
      attr.std = ATTR_STD_VERTEX_NORMAL;

      const array<float3> *vertices =
          cached_data.vertices.data_for_time_no_check(time).get_data_or_null();

      if (!vertices) {
        return;
      }

      array<char> data;
      data.resize(vertices->size() * sizeof(float3));

      float3 *data_float3 = reinterpret_cast<float3 *>(data.data());

      const Imath::V3f *values = sample.getVals()->get();

      for (size_t i = 0; i < vertices->size(); ++i) {
        data_float3[i] = make_float3_from_yup(values[i]);
      }

      attr.data.add_data(data, time);

      break;
    }
    default: {
      break;
    }
  }
}

static void add_positions(const P3fArraySamplePtr positions, double time, CachedData &cached_data)
{
  if (!positions) {
    return;
  }

  array<float3> vertices;
  vertices.reserve(positions->size());

  for (size_t i = 0; i < positions->size(); i++) {
    V3f f = positions->get()[i];
    vertices.push_back_reserved(make_float3_from_yup(f));
  }

  cached_data.vertices.add_data(vertices, time);
}

static void add_triangles(const Int32ArraySamplePtr face_counts,
                          const Int32ArraySamplePtr face_indices,
                          double time,
                          CachedData &cached_data,
                          const array<int> &polygon_to_shader)
{
  if (!face_counts || !face_indices) {
    return;
  }

  const size_t num_faces = face_counts->size();
  const int *face_counts_array = face_counts->get();
  const int *face_indices_array = face_indices->get();

  size_t num_triangles = 0;
  for (size_t i = 0; i < face_counts->size(); i++) {
    num_triangles += face_counts_array[i] - 2;
  }

  array<int> shader;
  array<int3> triangles;
  array<int3> triangles_loops;
  shader.reserve(num_triangles);
  triangles.reserve(num_triangles);
  triangles_loops.reserve(num_triangles);
  int index_offset = 0;

  for (size_t i = 0; i < num_faces; i++) {
    int current_shader = 0;

    if (!polygon_to_shader.empty()) {
      current_shader = polygon_to_shader[i];
    }

    for (int j = 0; j < face_counts_array[i] - 2; j++) {
      int v0 = face_indices_array[index_offset];
      int v1 = face_indices_array[index_offset + j + 1];
      int v2 = face_indices_array[index_offset + j + 2];

      shader.push_back_reserved(current_shader);

      /* Alembic orders the loops following the RenderMan convention, so need to go in reverse. */
      triangles.push_back_reserved(make_int3(v2, v1, v0));
      triangles_loops.push_back_reserved(
          make_int3(index_offset + j + 2, index_offset + j + 1, index_offset));
    }

    index_offset += face_counts_array[i];
  }

  cached_data.triangles.add_data(triangles, time);
  cached_data.triangles_loops.add_data(triangles_loops, time);
  cached_data.shader.add_data(shader, time);
}

NODE_DEFINE(AlembicObject)
{
  NodeType *type = NodeType::add("alembic_object", create);

  SOCKET_STRING(path, "Alembic Path", ustring());
  SOCKET_NODE_ARRAY(used_shaders, "Used Shaders", &Shader::node_type);

  SOCKET_INT(subd_max_level, "Max Subdivision Level", 1);
  SOCKET_FLOAT(subd_dicing_rate, "Subdivision Dicing Rate", 1.0f);

  SOCKET_FLOAT(radius_scale, "Radius Scale", 1.0f);

  return type;
}

AlembicObject::AlembicObject() : Node(node_type)
{
  schema_type = INVALID;
}

AlembicObject::~AlembicObject()
{
}

void AlembicObject::set_object(Object *object_)
{
  object = object_;
}

Object *AlembicObject::get_object()
{
  return object;
}

bool AlembicObject::has_data_loaded() const
{
  return data_loaded;
}

void AlembicObject::update_shader_attributes(const ICompoundProperty &arb_geom_params,
                                             Progress &progress)
{
  AttributeRequestSet requested_attributes = get_requested_attributes();

  foreach (const AttributeRequest &attr, requested_attributes.requests) {
    if (progress.get_cancel()) {
      return;
    }

    bool attr_exists = false;
    foreach (CachedData::CachedAttribute &cached_attr, cached_data.attributes) {
      if (cached_attr.name == attr.name) {
        attr_exists = true;
        break;
      }
    }

    if (attr_exists) {
      continue;
    }

    read_attribute(arb_geom_params, attr.name, progress);
  }

  cached_data.invalidate_last_loaded_time(true);
  need_shader_update = false;
}

template<typename SchemaType>
void AlembicObject::read_face_sets(SchemaType &schema,
                                   array<int> &polygon_to_shader,
                                   ISampleSelector sample_sel)
{
  std::vector<std::string> face_sets;
  schema.getFaceSetNames(face_sets);

  if (face_sets.empty()) {
    return;
  }

  const Int32ArraySamplePtr face_counts = schema.getFaceCountsProperty().getValue();

  polygon_to_shader.resize(face_counts->size());

  foreach (const std::string &face_set_name, face_sets) {
    int shader_index = 0;

    foreach (Node *node, get_used_shaders()) {
      if (node->name == face_set_name) {
        break;
      }

      ++shader_index;
    }

    if (shader_index >= get_used_shaders().size()) {
      /* use the first shader instead if none was found */
      shader_index = 0;
    }

    const IFaceSet face_set = schema.getFaceSet(face_set_name);

    if (!face_set.valid()) {
      continue;
    }

    const IFaceSetSchema face_schem = face_set.getSchema();
    const IFaceSetSchema::Sample face_sample = face_schem.getValue(sample_sel);
    const Int32ArraySamplePtr group_faces = face_sample.getFaces();
    const size_t num_group_faces = group_faces->size();

    for (size_t l = 0; l < num_group_faces; l++) {
      size_t pos = (*group_faces)[l];

      if (pos >= polygon_to_shader.size()) {
        continue;
      }

      polygon_to_shader[pos] = shader_index;
    }
  }
}

void AlembicObject::load_all_data(AlembicProcedural *proc,
                                  IPolyMeshSchema &schema,
                                  Progress &progress)
{
  cached_data.clear();

  /* Only load data for the original Geometry. */
  if (instance_of) {
    return;
  }

  const TimeSamplingPtr time_sampling = schema.getTimeSampling();
  cached_data.set_time_sampling(*time_sampling);

  const IN3fGeomParam &normals = schema.getNormalsParam();

  ccl::set<chrono_t> times = get_relevant_sample_times(
      proc, *time_sampling, schema.getNumSamples());

  /* read topology */
  foreach (chrono_t time, times) {
    if (progress.get_cancel()) {
      return;
    }

    const ISampleSelector iss = ISampleSelector(time);
    const IPolyMeshSchema::Sample sample = schema.getValue(iss);

    add_positions(sample.getPositions(), time, cached_data);

    /* Only copy triangles for other frames if the topology is changing over time as well.
     *
     * TODO(@kevindietrich): even for dynamic simulations, this is a waste of memory and
     * processing time if only the positions are changing in a subsequence of frames but we
     * cannot optimize in this current system if the attributes are changing over time as well,
     * as we need valid data for each time point. This can be solved by using reference counting
     * on the ccl::array and simply share the array across frames. */
    if (schema.getTopologyVariance() != kHomogenousTopology || cached_data.triangles.size() == 0) {
      /* start by reading the face sets (per face shader), as we directly split polygons to
       * triangles
       */
      array<int> polygon_to_shader;
      read_face_sets(schema, polygon_to_shader, iss);

      add_triangles(
          sample.getFaceCounts(), sample.getFaceIndices(), time, cached_data, polygon_to_shader);
    }

    if (normals.valid()) {
      add_normals(sample.getFaceIndices(), normals, time, cached_data);
    }
  }

  if (progress.get_cancel()) {
    return;
  }

  update_shader_attributes(schema.getArbGeomParams(), progress);

  if (progress.get_cancel()) {
    return;
  }

  const IV2fGeomParam &uvs = schema.getUVsParam();

  if (uvs.valid()) {
    add_uvs(proc, uvs, cached_data, progress);
  }

  data_loaded = true;
}

void AlembicObject::load_all_data(AlembicProcedural *proc, ISubDSchema &schema, Progress &progress)
{
  cached_data.clear();

  /* Only load data for the original Geometry. */
  if (instance_of) {
    return;
  }

  AttributeRequestSet requested_attributes = get_requested_attributes();

  const TimeSamplingPtr time_sampling = schema.getTimeSampling();
  cached_data.set_time_sampling(*time_sampling);

  ccl::set<chrono_t> times = get_relevant_sample_times(
      proc, *time_sampling, schema.getNumSamples());

  /* read topology */
  foreach (chrono_t time, times) {
    if (progress.get_cancel()) {
      return;
    }

    const ISampleSelector iss = ISampleSelector(time);
    const ISubDSchema::Sample sample = schema.getValue(iss);

    add_positions(sample.getPositions(), time, cached_data);

    const Int32ArraySamplePtr face_counts = sample.getFaceCounts();
    const Int32ArraySamplePtr face_indices = sample.getFaceIndices();

    /* start by reading the face sets (per face shader) */
    array<int> polygon_to_shader;
    read_face_sets(schema, polygon_to_shader, iss);

    /* read faces */
    array<int> subd_start_corner;
    array<int> shader;
    array<int> subd_num_corners;
    array<bool> subd_smooth;
    array<int> subd_ptex_offset;
    array<int> subd_face_corners;

    const size_t num_faces = face_counts->size();
    const int *face_counts_array = face_counts->get();
    const int *face_indices_array = face_indices->get();

    int num_ngons = 0;
    int num_corners = 0;
    for (size_t i = 0; i < face_counts->size(); i++) {
      num_ngons += (face_counts_array[i] == 4 ? 0 : 1);
      num_corners += face_counts_array[i];
    }

    subd_start_corner.reserve(num_faces);
    subd_num_corners.reserve(num_faces);
    subd_smooth.reserve(num_faces);
    subd_ptex_offset.reserve(num_faces);
    shader.reserve(num_faces);
    subd_face_corners.reserve(num_corners);

    int start_corner = 0;
    int current_shader = 0;
    int ptex_offset = 0;

    for (size_t i = 0; i < face_counts->size(); i++) {
      num_corners = face_counts_array[i];

      if (!polygon_to_shader.empty()) {
        current_shader = polygon_to_shader[i];
      }

      subd_start_corner.push_back_reserved(start_corner);
      subd_num_corners.push_back_reserved(num_corners);

      for (int j = 0; j < num_corners; ++j) {
        subd_face_corners.push_back_reserved(face_indices_array[start_corner + j]);
      }

      shader.push_back_reserved(current_shader);
      subd_smooth.push_back_reserved(1);
      subd_ptex_offset.push_back_reserved(ptex_offset);

      ptex_offset += (num_corners == 4 ? 1 : num_corners);

      start_corner += num_corners;
    }

    cached_data.shader.add_data(shader, time);
    cached_data.subd_start_corner.add_data(subd_start_corner, time);
    cached_data.subd_num_corners.add_data(subd_num_corners, time);
    cached_data.subd_smooth.add_data(subd_smooth, time);
    cached_data.subd_ptex_offset.add_data(subd_ptex_offset, time);
    cached_data.subd_face_corners.add_data(subd_face_corners, time);
    cached_data.num_ngons.add_data(num_ngons, time);

    /* read creases */
    Int32ArraySamplePtr creases_length = sample.getCreaseLengths();
    Int32ArraySamplePtr creases_indices = sample.getCreaseIndices();
    FloatArraySamplePtr creases_sharpnesses = sample.getCreaseSharpnesses();

    if (creases_length && creases_indices && creases_sharpnesses) {
      array<int> creases_edge;
      array<float> creases_weight;

      creases_edge.reserve(creases_sharpnesses->size() * 2);
      creases_weight.reserve(creases_sharpnesses->size());

      int length_offset = 0;
      int weight_offset = 0;
      for (size_t c = 0; c < creases_length->size(); ++c) {
        const int crease_length = creases_length->get()[c];

        for (size_t j = 0; j < crease_length - 1; ++j) {
          creases_edge.push_back_reserved(creases_indices->get()[length_offset + j]);
          creases_edge.push_back_reserved(creases_indices->get()[length_offset + j + 1]);
          creases_weight.push_back_reserved(creases_sharpnesses->get()[weight_offset++]);
        }

        length_offset += crease_length;
      }

      cached_data.subd_creases_edge.add_data(creases_edge, time);
      cached_data.subd_creases_weight.add_data(creases_weight, time);
    }
  }

  /* TODO(@kevindietrich) : attributes, need test files */

  if (progress.get_cancel()) {
    return;
  }

  data_loaded = true;
}

void AlembicObject::load_all_data(AlembicProcedural *proc,
                                  const ICurvesSchema &schema,
                                  Progress &progress,
                                  float default_radius)
{
  cached_data.clear();

  /* Only load data for the original Geometry. */
  if (instance_of) {
    return;
  }

  const TimeSamplingPtr time_sampling = schema.getTimeSampling();
  cached_data.set_time_sampling(*time_sampling);

  ccl::set<chrono_t> times = get_relevant_sample_times(
      proc, *time_sampling, schema.getNumSamples());

  foreach (chrono_t time, times) {
    if (progress.get_cancel()) {
      return;
    }

    const ISampleSelector iss = ISampleSelector(time);
    const ICurvesSchema::Sample sample = schema.getValue(iss);

    const Int32ArraySamplePtr curves_num_vertices = sample.getCurvesNumVertices();
    const P3fArraySamplePtr position = sample.getPositions();

    const IFloatGeomParam widths_param = schema.getWidthsParam();
    FloatArraySamplePtr radiuses;

    if (widths_param.valid()) {
      IFloatGeomParam::Sample wsample = widths_param.getExpandedValue(iss);
      radiuses = wsample.getVals();
    }

    const bool do_radius = (radiuses != nullptr) && (radiuses->size() > 1);
    float radius = (radiuses && radiuses->size() == 1) ? (*radiuses)[0] : default_radius;

    array<float3> curve_keys;
    array<float> curve_radius;
    array<int> curve_first_key;
    array<int> curve_shader;

    const bool is_homogenous = schema.getTopologyVariance() == kHomogenousTopology;

    curve_keys.reserve(position->size());
    curve_radius.reserve(position->size());
    curve_first_key.reserve(curves_num_vertices->size());
    curve_shader.reserve(curves_num_vertices->size());

    int offset = 0;
    for (size_t i = 0; i < curves_num_vertices->size(); i++) {
      const int num_vertices = curves_num_vertices->get()[i];

      for (int j = 0; j < num_vertices; j++) {
        const V3f &f = position->get()[offset + j];
        curve_keys.push_back_reserved(make_float3_from_yup(f));

        if (do_radius) {
          radius = (*radiuses)[offset + j];
        }

        curve_radius.push_back_reserved(radius * radius_scale);
      }

      if (!is_homogenous || cached_data.curve_first_key.size() == 0) {
        curve_first_key.push_back_reserved(offset);
        curve_shader.push_back_reserved(0);
      }

      offset += num_vertices;
    }

    cached_data.curve_keys.add_data(curve_keys, time);
    cached_data.curve_radius.add_data(curve_radius, time);

    if (!is_homogenous || cached_data.curve_first_key.size() == 0) {
      cached_data.curve_first_key.add_data(curve_first_key, time);
      cached_data.curve_shader.add_data(curve_shader, time);
    }
  }

  // TODO(@kevindietrich): attributes, need example files

  data_loaded = true;
}

void AlembicObject::setup_transform_cache(float scale)
{
  cached_data.transforms.clear();
  cached_data.transforms.invalidate_last_loaded_time();

  if (scale == 0.0f) {
    scale = 1.0f;
  }

  if (xform_time_sampling) {
    cached_data.transforms.set_time_sampling(*xform_time_sampling);
  }

  if (xform_samples.size() == 0) {
    Transform tfm = transform_scale(make_float3(scale));
    cached_data.transforms.add_data(tfm, 0.0);
  }
  else {
    /* It is possible for a leaf node of the hierarchy to have multiple samples for its transforms
     * if a sibling has animated transforms. So check if we indeed have animated transformations.
     */
    M44d first_matrix = xform_samples.begin()->first;
    bool has_animation = false;
    for (const std::pair<chrono_t, M44d> pair : xform_samples) {
      if (pair.second != first_matrix) {
        has_animation = true;
        break;
      }
    }

    if (!has_animation) {
      Transform tfm = make_transform(first_matrix, scale);
      cached_data.transforms.add_data(tfm, 0.0);
    }
    else {
      for (const std::pair<chrono_t, M44d> pair : xform_samples) {
        Transform tfm = make_transform(pair.second, scale);
        cached_data.transforms.add_data(tfm, pair.first);
      }
    }
  }
}

AttributeRequestSet AlembicObject::get_requested_attributes()
{
  AttributeRequestSet requested_attributes;

  Geometry *geometry = object->get_geometry();
  assert(geometry);

  foreach (Node *node, geometry->get_used_shaders()) {
    Shader *shader = static_cast<Shader *>(node);

    foreach (const AttributeRequest &attr, shader->attributes.requests) {
      if (attr.name != "") {
        requested_attributes.add(attr.name);
      }
    }
  }

  return requested_attributes;
}

void AlembicObject::read_attribute(const ICompoundProperty &arb_geom_params,
                                   const ustring &attr_name,
                                   Progress &progress)
{
  const PropertyHeader *prop = arb_geom_params.getPropertyHeader(attr_name.c_str());

  if (prop == nullptr) {
    return;
  }

  if (IV2fProperty::matches(prop->getMetaData()) && Alembic::AbcGeom::isUV(*prop)) {
    const IV2fGeomParam &param = IV2fGeomParam(arb_geom_params, prop->getName());

    CachedData::CachedAttribute &attribute = cached_data.add_attribute(attr_name,
                                                                       *param.getTimeSampling());

    for (size_t i = 0; i < param.getNumSamples(); ++i) {
      if (progress.get_cancel()) {
        return;
      }

      ISampleSelector iss = ISampleSelector(index_t(i));

      IV2fGeomParam::Sample sample;
      param.getIndexed(sample, iss);

      const chrono_t time = param.getTimeSampling()->getSampleTime(index_t(i));

      if (param.getScope() == kFacevaryingScope) {
        V2fArraySamplePtr values = sample.getVals();
        UInt32ArraySamplePtr indices = sample.getIndices();

        attribute.std = ATTR_STD_NONE;
        attribute.element = ATTR_ELEMENT_CORNER;
        attribute.type_desc = TypeFloat2;

        const array<int3> *triangles =
            cached_data.triangles.data_for_time_no_check(time).get_data_or_null();
        const array<int3> *triangles_loops =
            cached_data.triangles_loops.data_for_time_no_check(time).get_data_or_null();

        if (!triangles || !triangles_loops) {
          return;
        }

        array<char> data;
        data.resize(triangles->size() * 3 * sizeof(float2));

        float2 *data_float2 = reinterpret_cast<float2 *>(data.data());

        for (const int3 &loop : *triangles_loops) {
          unsigned int v0 = (*indices)[loop.x];
          unsigned int v1 = (*indices)[loop.y];
          unsigned int v2 = (*indices)[loop.z];

          data_float2[0] = make_float2((*values)[v0][0], (*values)[v0][1]);
          data_float2[1] = make_float2((*values)[v1][0], (*values)[v1][1]);
          data_float2[2] = make_float2((*values)[v2][0], (*values)[v2][1]);
          data_float2 += 3;
        }

        attribute.data.set_time_sampling(*param.getTimeSampling());
        attribute.data.add_data(data, time);
      }
    }
  }
  else if (IC3fProperty::matches(prop->getMetaData())) {
    const IC3fGeomParam &param = IC3fGeomParam(arb_geom_params, prop->getName());

    CachedData::CachedAttribute &attribute = cached_data.add_attribute(attr_name,
                                                                       *param.getTimeSampling());

    for (size_t i = 0; i < param.getNumSamples(); ++i) {
      if (progress.get_cancel()) {
        return;
      }

      ISampleSelector iss = ISampleSelector(index_t(i));

      IC3fGeomParam::Sample sample;
      param.getIndexed(sample, iss);

      const chrono_t time = param.getTimeSampling()->getSampleTime(index_t(i));

      C3fArraySamplePtr values = sample.getVals();

      attribute.std = ATTR_STD_NONE;

      if (param.getScope() == kVaryingScope) {
        attribute.element = ATTR_ELEMENT_CORNER_BYTE;
        attribute.type_desc = TypeRGBA;

        const array<int3> *triangles =
            cached_data.triangles.data_for_time_no_check(time).get_data_or_null();

        if (!triangles) {
          return;
        }

        array<char> data;
        data.resize(triangles->size() * 3 * sizeof(uchar4));

        uchar4 *data_uchar4 = reinterpret_cast<uchar4 *>(data.data());

        int offset = 0;
        for (const int3 &tri : *triangles) {
          Imath::C3f v = (*values)[tri.x];
          data_uchar4[offset + 0] = color_float_to_byte(make_float3(v.x, v.y, v.z));

          v = (*values)[tri.y];
          data_uchar4[offset + 1] = color_float_to_byte(make_float3(v.x, v.y, v.z));

          v = (*values)[tri.z];
          data_uchar4[offset + 2] = color_float_to_byte(make_float3(v.x, v.y, v.z));

          offset += 3;
        }

        attribute.data.set_time_sampling(*param.getTimeSampling());
        attribute.data.add_data(data, time);
      }
    }
  }
  else if (IC4fProperty::matches(prop->getMetaData())) {
    const IC4fGeomParam &param = IC4fGeomParam(arb_geom_params, prop->getName());

    CachedData::CachedAttribute &attribute = cached_data.add_attribute(attr_name,
                                                                       *param.getTimeSampling());

    for (size_t i = 0; i < param.getNumSamples(); ++i) {
      if (progress.get_cancel()) {
        return;
      }

      ISampleSelector iss = ISampleSelector(index_t(i));

      IC4fGeomParam::Sample sample;
      param.getIndexed(sample, iss);

      const chrono_t time = param.getTimeSampling()->getSampleTime(index_t(i));

      C4fArraySamplePtr values = sample.getVals();

      attribute.std = ATTR_STD_NONE;

      if (param.getScope() == kVaryingScope) {
        attribute.element = ATTR_ELEMENT_CORNER_BYTE;
        attribute.type_desc = TypeRGBA;

        const array<int3> *triangles =
            cached_data.triangles.data_for_time_no_check(time).get_data_or_null();

        if (!triangles) {
          return;
        }

        array<char> data;
        data.resize(triangles->size() * 3 * sizeof(uchar4));

        uchar4 *data_uchar4 = reinterpret_cast<uchar4 *>(data.data());

        int offset = 0;
        for (const int3 &tri : *triangles) {
          Imath::C4f v = (*values)[tri.x];
          data_uchar4[offset + 0] = color_float4_to_uchar4(make_float4(v.r, v.g, v.b, v.a));

          v = (*values)[tri.y];
          data_uchar4[offset + 1] = color_float4_to_uchar4(make_float4(v.r, v.g, v.b, v.a));

          v = (*values)[tri.z];
          data_uchar4[offset + 2] = color_float4_to_uchar4(make_float4(v.r, v.g, v.b, v.a));

          offset += 3;
        }

        attribute.data.set_time_sampling(*param.getTimeSampling());
        attribute.data.add_data(data, time);
      }
    }
  }
}

/* Update existing attributes and remove any attribute not in the cached_data, those attributes
 * were added by Cycles (e.g. face normals) */
static void update_attributes(AttributeSet &attributes, CachedData &cached_data, double frame_time)
{
  set<Attribute *> cached_attributes;

  for (CachedData::CachedAttribute &attribute : cached_data.attributes) {
    const array<char> *attr_data = attribute.data.data_for_time(frame_time).get_data_or_null();

    Attribute *attr = nullptr;
    if (attribute.std != ATTR_STD_NONE) {
      attr = attributes.add(attribute.std, attribute.name);
    }
    else {
      attr = attributes.add(attribute.name, attribute.type_desc, attribute.element);
    }
    assert(attr);

    cached_attributes.insert(attr);

    if (!attr_data) {
      /* no new data */
      continue;
    }

    /* weak way of detecting if the topology has changed
     * todo: reuse code from device_update patch */
    if (attr->buffer.size() != attr_data->size()) {
      attr->buffer.resize(attr_data->size());
    }

    memcpy(attr->data(), attr_data->data(), attr_data->size());
    attr->modified = true;
  }

  /* remove any attributes not in cached_attributes */
  list<Attribute>::iterator it;
  for (it = attributes.attributes.begin(); it != attributes.attributes.end();) {
    if (cached_attributes.find(&(*it)) == cached_attributes.end()) {
      attributes.attributes.erase(it++);
      attributes.modified = true;
      continue;
    }

    it++;
  }
}

NODE_DEFINE(AlembicProcedural)
{
  NodeType *type = NodeType::add("alembic", create);

  SOCKET_STRING(filepath, "Filename", ustring());
  SOCKET_FLOAT(frame, "Frame", 1.0f);
  SOCKET_FLOAT(start_frame, "Start Frame", 1.0f);
  SOCKET_FLOAT(end_frame, "End Frame", 1.0f);
  SOCKET_FLOAT(frame_rate, "Frame Rate", 24.0f);
  SOCKET_FLOAT(frame_offset, "Frame Offset", 0.0f);
  SOCKET_FLOAT(default_radius, "Default Radius", 0.01f);
  SOCKET_FLOAT(scale, "Scale", 1.0f);

  SOCKET_NODE_ARRAY(objects, "Objects", &AlembicObject::node_type);

  return type;
}

AlembicProcedural::AlembicProcedural() : Procedural(node_type)
{
  objects_loaded = false;
  scene_ = nullptr;
}

AlembicProcedural::~AlembicProcedural()
{
  ccl::set<Geometry *> geometries_set;
  ccl::set<Object *> objects_set;
  ccl::set<AlembicObject *> abc_objects_set;

  foreach (Node *node, objects) {
    AlembicObject *abc_object = static_cast<AlembicObject *>(node);

    if (abc_object->get_object()) {
      objects_set.insert(abc_object->get_object());

      if (abc_object->get_object()->get_geometry()) {
        geometries_set.insert(abc_object->get_object()->get_geometry());
      }
    }

    delete_node(abc_object);
  }

  /* We may delete a Procedural before rendering started, so scene_ can be null. */
  if (!scene_) {
    assert(geometries_set.empty());
    assert(objects_set.empty());
    return;
  }

  scene_->delete_nodes(geometries_set, this);
  scene_->delete_nodes(objects_set, this);
}

void AlembicProcedural::generate(Scene *scene, Progress &progress)
{
  assert(scene_ == nullptr || scene_ == scene);
  scene_ = scene;

  if (frame < start_frame || frame > end_frame) {
    clear_modified();
    return;
  }

  bool need_shader_updates = false;
  bool need_data_updates = false;

  foreach (Node *object_node, objects) {
    AlembicObject *object = static_cast<AlembicObject *>(object_node);

    if (object->is_modified()) {
      need_data_updates = true;
    }

    /* Check for changes in shaders (e.g. newly requested attributes). */
    foreach (Node *shader_node, object->get_used_shaders()) {
      Shader *shader = static_cast<Shader *>(shader_node);

      if (shader->need_update_geometry()) {
        object->need_shader_update = true;
        need_shader_updates = true;
      }
    }
  }

  if (!is_modified() && !need_shader_updates && !need_data_updates) {
    return;
  }

  if (!archive.valid()) {
    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
    archive = factory.getArchive(filepath.c_str());

    if (!archive.valid()) {
      /* avoid potential infinite update loops in viewport synchronization */
      filepath.clear();
      clear_modified();
      return;
    }
  }

  if (!objects_loaded || objects_is_modified()) {
    load_objects(progress);
    objects_loaded = true;
  }

  const chrono_t frame_time = (chrono_t)((frame - frame_offset) / frame_rate);

  build_caches(progress);

  foreach (Node *node, objects) {
    AlembicObject *object = static_cast<AlembicObject *>(node);

    if (progress.get_cancel()) {
      return;
    }

    /* skip constant objects */
    if (object->is_constant() && !object->is_modified() && !object->need_shader_update &&
        !scale_is_modified()) {
      continue;
    }

    if (object->schema_type == AlembicObject::POLY_MESH) {
      read_mesh(object, frame_time);
    }
    else if (object->schema_type == AlembicObject::CURVES) {
      read_curves(object, frame_time);
    }
    else if (object->schema_type == AlembicObject::SUBD) {
      read_subd(object, frame_time);
    }

    object->clear_modified();
  }

  clear_modified();
}

void AlembicProcedural::add_object(AlembicObject *object)
{
  objects.push_back_slow(object);
  tag_objects_modified();
}

void AlembicProcedural::tag_update(Scene *scene)
{
  scene->procedural_manager->tag_update();
}

AlembicObject *AlembicProcedural::get_or_create_object(const ustring &path)
{
  foreach (Node *node, objects) {
    AlembicObject *object = static_cast<AlembicObject *>(node);

    if (object->get_path() == path) {
      return object;
    }
  }

  AlembicObject *object = create_node<AlembicObject>();
  object->set_path(path);

  add_object(object);

  return object;
}

void AlembicProcedural::load_objects(Progress &progress)
{
  unordered_map<string, AlembicObject *> object_map;

  foreach (Node *node, objects) {
    AlembicObject *object = static_cast<AlembicObject *>(node);

    /* only consider newly added objects */
    if (object->get_object() == nullptr) {
      object_map.insert({object->get_path().c_str(), object});
    }
  }

  IObject root = archive.getTop();

  for (size_t i = 0; i < root.getNumChildren(); ++i) {
    walk_hierarchy(root, root.getChildHeader(i), {}, object_map, progress);
  }

  /* Create nodes in the scene. */
  for (std::pair<string, AlembicObject *> pair : object_map) {
    AlembicObject *abc_object = pair.second;

    Geometry *geometry = nullptr;

    if (!abc_object->instance_of) {
      if (abc_object->schema_type == AlembicObject::CURVES) {
        geometry = scene_->create_node<Hair>();
      }
      else if (abc_object->schema_type == AlembicObject::POLY_MESH ||
               abc_object->schema_type == AlembicObject::SUBD) {
        geometry = scene_->create_node<Mesh>();
      }
      else {
        continue;
      }

      geometry->set_owner(this);
      geometry->name = abc_object->iobject.getName();

      array<Node *> used_shaders = abc_object->get_used_shaders();
      geometry->set_used_shaders(used_shaders);
    }

    Object *object = scene_->create_node<Object>();
    object->set_owner(this);
    object->set_geometry(geometry);
    object->name = abc_object->iobject.getName();

    abc_object->set_object(object);
  }

  /* Share geometries between instances. */
  foreach (Node *node, objects) {
    AlembicObject *abc_object = static_cast<AlembicObject *>(node);

    if (abc_object->instance_of) {
      abc_object->get_object()->set_geometry(
          abc_object->instance_of->get_object()->get_geometry());
      abc_object->schema_type = abc_object->instance_of->schema_type;
    }
  }
}

void AlembicProcedural::read_mesh(AlembicObject *abc_object, Abc::chrono_t frame_time)
{
  CachedData &cached_data = abc_object->get_cached_data();

  /* update sockets */

  Object *object = abc_object->get_object();
  cached_data.transforms.copy_to_socket(frame_time, object, object->get_tfm_socket());

  if (object->is_modified()) {
    object->tag_update(scene_);
  }

  /* Only update sockets for the original Geometry. */
  if (abc_object->instance_of) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(object->get_geometry());

  cached_data.vertices.copy_to_socket(frame_time, mesh, mesh->get_verts_socket());

  cached_data.shader.copy_to_socket(frame_time, mesh, mesh->get_shader_socket());

  array<int3> *triangle_data = cached_data.triangles.data_for_time(frame_time).get_data_or_null();
  if (triangle_data) {
    array<int> triangles;
    array<bool> smooth;

    triangles.reserve(triangle_data->size() * 3);
    smooth.reserve(triangle_data->size());

    for (size_t i = 0; i < triangle_data->size(); ++i) {
      int3 tri = (*triangle_data)[i];
      triangles.push_back_reserved(tri.x);
      triangles.push_back_reserved(tri.y);
      triangles.push_back_reserved(tri.z);
      smooth.push_back_reserved(1);
    }

    mesh->set_triangles(triangles);
    mesh->set_smooth(smooth);
  }

  /* update attributes */

  update_attributes(mesh->attributes, cached_data, frame_time);

  /* we don't yet support arbitrary attributes, for now add vertex
   * coordinates as generated coordinates if requested */
  if (mesh->need_attribute(scene_, ATTR_STD_GENERATED)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);
    memcpy(
        attr->data_float3(), mesh->get_verts().data(), sizeof(float3) * mesh->get_verts().size());
  }

  if (mesh->is_modified()) {
    bool need_rebuild = mesh->triangles_is_modified();
    mesh->tag_update(scene_, need_rebuild);
  }
}

void AlembicProcedural::read_subd(AlembicObject *abc_object, Abc::chrono_t frame_time)
{
  CachedData &cached_data = abc_object->get_cached_data();

  if (abc_object->subd_max_level_is_modified() || abc_object->subd_dicing_rate_is_modified()) {
    /* need to reset the current data is something changed */
    cached_data.invalidate_last_loaded_time();
  }

  /* Update sockets. */

  Object *object = abc_object->get_object();
  cached_data.transforms.copy_to_socket(frame_time, object, object->get_tfm_socket());

  if (object->is_modified()) {
    object->tag_update(scene_);
  }

  /* Only update sockets for the original Geometry. */
  if (abc_object->instance_of) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(object->get_geometry());

  /* Cycles overwrites the original triangles when computing displacement, so we always have to
   * repass the data if something is animated (vertices most likely) to avoid buffer overflows. */
  if (!cached_data.is_constant()) {
    cached_data.invalidate_last_loaded_time();

    /* remove previous triangles, if any */
    array<int> triangles;
    mesh->set_triangles(triangles);
  }

  mesh->clear_non_sockets();

  /* Alembic is OpenSubDiv compliant, there is no option to set another subdivision type. */
  mesh->set_subdivision_type(Mesh::SubdivisionType::SUBDIVISION_CATMULL_CLARK);
  mesh->set_subd_max_level(abc_object->get_subd_max_level());
  mesh->set_subd_dicing_rate(abc_object->get_subd_dicing_rate());

  cached_data.vertices.copy_to_socket(frame_time, mesh, mesh->get_verts_socket());

  /* cached_data.shader is also used for subd_shader */
  cached_data.shader.copy_to_socket(frame_time, mesh, mesh->get_subd_shader_socket());

  cached_data.subd_start_corner.copy_to_socket(
      frame_time, mesh, mesh->get_subd_start_corner_socket());

  cached_data.subd_num_corners.copy_to_socket(
      frame_time, mesh, mesh->get_subd_num_corners_socket());

  cached_data.subd_smooth.copy_to_socket(frame_time, mesh, mesh->get_subd_smooth_socket());

  cached_data.subd_ptex_offset.copy_to_socket(
      frame_time, mesh, mesh->get_subd_ptex_offset_socket());

  cached_data.subd_face_corners.copy_to_socket(
      frame_time, mesh, mesh->get_subd_face_corners_socket());

  cached_data.num_ngons.copy_to_socket(frame_time, mesh, mesh->get_num_ngons_socket());

  cached_data.subd_creases_edge.copy_to_socket(
      frame_time, mesh, mesh->get_subd_creases_edge_socket());

  cached_data.subd_creases_weight.copy_to_socket(
      frame_time, mesh, mesh->get_subd_creases_weight_socket());

  mesh->set_num_subd_faces(mesh->get_subd_shader().size());

  /* Update attributes. */

  update_attributes(mesh->subd_attributes, cached_data, frame_time);

  /* we don't yet support arbitrary attributes, for now add vertex
   * coordinates as generated coordinates if requested */
  if (mesh->need_attribute(scene_, ATTR_STD_GENERATED)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);
    memcpy(
        attr->data_float3(), mesh->get_verts().data(), sizeof(float3) * mesh->get_verts().size());
  }

  if (mesh->is_modified()) {
    bool need_rebuild = (mesh->triangles_is_modified()) ||
                        (mesh->subd_num_corners_is_modified()) ||
                        (mesh->subd_shader_is_modified()) || (mesh->subd_smooth_is_modified()) ||
                        (mesh->subd_ptex_offset_is_modified()) ||
                        (mesh->subd_start_corner_is_modified()) ||
                        (mesh->subd_face_corners_is_modified());

    mesh->tag_update(scene_, need_rebuild);
  }
}

void AlembicProcedural::read_curves(AlembicObject *abc_object, Abc::chrono_t frame_time)
{
  CachedData &cached_data = abc_object->get_cached_data();

  /* update sockets */

  Object *object = abc_object->get_object();
  cached_data.transforms.copy_to_socket(frame_time, object, object->get_tfm_socket());

  if (object->is_modified()) {
    object->tag_update(scene_);
  }

  /* Only update sockets for the original Geometry. */
  if (abc_object->instance_of) {
    return;
  }

  Hair *hair = static_cast<Hair *>(object->get_geometry());

  cached_data.curve_keys.copy_to_socket(frame_time, hair, hair->get_curve_keys_socket());

  cached_data.curve_radius.copy_to_socket(frame_time, hair, hair->get_curve_radius_socket());

  cached_data.curve_shader.copy_to_socket(frame_time, hair, hair->get_curve_shader_socket());

  cached_data.curve_first_key.copy_to_socket(frame_time, hair, hair->get_curve_first_key_socket());

  /* update attributes */

  update_attributes(hair->attributes, cached_data, frame_time);

  /* we don't yet support arbitrary attributes, for now add first keys as generated coordinates if
   * requested */
  if (hair->need_attribute(scene_, ATTR_STD_GENERATED)) {
    Attribute *attr_generated = hair->attributes.add(ATTR_STD_GENERATED);
    float3 *generated = attr_generated->data_float3();

    for (size_t i = 0; i < hair->num_curves(); i++) {
      generated[i] = hair->get_curve_keys()[hair->get_curve(i).first_key];
    }
  }

  const bool rebuild = (hair->curve_keys_is_modified() || hair->curve_radius_is_modified());
  hair->tag_update(scene_, rebuild);
}

void AlembicProcedural::walk_hierarchy(
    IObject parent,
    const ObjectHeader &header,
    MatrixSamplesData matrix_samples_data,
    const unordered_map<std::string, AlembicObject *> &object_map,
    Progress &progress)
{
  if (progress.get_cancel()) {
    return;
  }

  IObject next_object;

  MatrixSampleMap concatenated_xform_samples;

  if (IXform::matches(header)) {
    IXform xform(parent, header.getName());

    IXformSchema &xs = xform.getSchema();

    if (xs.getNumOps() > 0) {
      TimeSamplingPtr ts = xs.getTimeSampling();
      MatrixSampleMap local_xform_samples;

      MatrixSampleMap *temp_xform_samples = nullptr;
      if (matrix_samples_data.samples == nullptr) {
        /* If there is no parent transforms, fill the map directly. */
        temp_xform_samples = &concatenated_xform_samples;
      }
      else {
        /* use a temporary map */
        temp_xform_samples = &local_xform_samples;
      }

      for (size_t i = 0; i < xs.getNumSamples(); ++i) {
        chrono_t sample_time = ts->getSampleTime(index_t(i));
        XformSample sample = xs.getValue(ISampleSelector(sample_time));
        temp_xform_samples->insert({sample_time, sample.getMatrix()});
      }

      if (matrix_samples_data.samples != nullptr) {
        concatenate_xform_samples(
            *matrix_samples_data.samples, local_xform_samples, concatenated_xform_samples);
      }

      matrix_samples_data.samples = &concatenated_xform_samples;
      matrix_samples_data.time_sampling = ts;
    }

    next_object = xform;
  }
  else if (ISubD::matches(header)) {
    ISubD subd(parent, header.getName());

    unordered_map<std::string, AlembicObject *>::const_iterator iter;
    iter = object_map.find(subd.getFullName());

    if (iter != object_map.end()) {
      AlembicObject *abc_object = iter->second;
      abc_object->iobject = subd;
      abc_object->schema_type = AlembicObject::SUBD;

      if (matrix_samples_data.samples) {
        abc_object->xform_samples = *matrix_samples_data.samples;
        abc_object->xform_time_sampling = matrix_samples_data.time_sampling;
      }
    }

    next_object = subd;
  }
  else if (IPolyMesh::matches(header)) {
    IPolyMesh mesh(parent, header.getName());

    unordered_map<std::string, AlembicObject *>::const_iterator iter;
    iter = object_map.find(mesh.getFullName());

    if (iter != object_map.end()) {
      AlembicObject *abc_object = iter->second;
      abc_object->iobject = mesh;
      abc_object->schema_type = AlembicObject::POLY_MESH;

      if (matrix_samples_data.samples) {
        abc_object->xform_samples = *matrix_samples_data.samples;
        abc_object->xform_time_sampling = matrix_samples_data.time_sampling;
      }
    }

    next_object = mesh;
  }
  else if (ICurves::matches(header)) {
    ICurves curves(parent, header.getName());

    unordered_map<std::string, AlembicObject *>::const_iterator iter;
    iter = object_map.find(curves.getFullName());

    if (iter != object_map.end()) {
      AlembicObject *abc_object = iter->second;
      abc_object->iobject = curves;
      abc_object->schema_type = AlembicObject::CURVES;

      if (matrix_samples_data.samples) {
        abc_object->xform_samples = *matrix_samples_data.samples;
        abc_object->xform_time_sampling = matrix_samples_data.time_sampling;
      }
    }

    next_object = curves;
  }
  else if (IFaceSet::matches(header)) {
    // ignore the face set, it will be read along with the data
  }
  else if (IPoints::matches(header)) {
    // unsupported for now
  }
  else if (INuPatch::matches(header)) {
    // unsupported for now
  }
  else {
    next_object = parent.getChild(header.getName());

    if (next_object.isInstanceRoot()) {
      unordered_map<std::string, AlembicObject *>::const_iterator iter;

      /* Was this object asked to be rendered? */
      iter = object_map.find(next_object.getFullName());

      if (iter != object_map.end()) {
        AlembicObject *abc_object = iter->second;

        /* Only try to render an instance if the original object is also rendered. */
        iter = object_map.find(next_object.instanceSourcePath());

        if (iter != object_map.end()) {
          abc_object->iobject = next_object;
          abc_object->instance_of = iter->second;

          if (matrix_samples_data.samples) {
            abc_object->xform_samples = *matrix_samples_data.samples;
            abc_object->xform_time_sampling = matrix_samples_data.time_sampling;
          }
        }
      }
    }
  }

  if (next_object.valid()) {
    for (size_t i = 0; i < next_object.getNumChildren(); ++i) {
      walk_hierarchy(
          next_object, next_object.getChildHeader(i), matrix_samples_data, object_map, progress);
    }
  }
}

void AlembicProcedural::build_caches(Progress &progress)
{
  for (Node *node : objects) {
    AlembicObject *object = static_cast<AlembicObject *>(node);

    if (progress.get_cancel()) {
      return;
    }

    if (object->schema_type == AlembicObject::POLY_MESH) {
      if (!object->has_data_loaded()) {
        IPolyMesh polymesh(object->iobject, Alembic::Abc::kWrapExisting);
        IPolyMeshSchema schema = polymesh.getSchema();
        object->load_all_data(this, schema, progress);
      }
      else if (object->need_shader_update) {
        IPolyMesh polymesh(object->iobject, Alembic::Abc::kWrapExisting);
        IPolyMeshSchema schema = polymesh.getSchema();
        object->update_shader_attributes(schema.getArbGeomParams(), progress);
      }
    }
    else if (object->schema_type == AlembicObject::CURVES) {
      if (!object->has_data_loaded() || default_radius_is_modified() ||
          object->radius_scale_is_modified()) {
        ICurves curves(object->iobject, Alembic::Abc::kWrapExisting);
        ICurvesSchema schema = curves.getSchema();
        object->load_all_data(this, schema, progress, default_radius);
      }
    }
    else if (object->schema_type == AlembicObject::SUBD) {
      if (!object->has_data_loaded()) {
        ISubD subd_mesh(object->iobject, Alembic::Abc::kWrapExisting);
        ISubDSchema schema = subd_mesh.getSchema();
        object->load_all_data(this, schema, progress);
      }
      else if (object->need_shader_update) {
        ISubD subd_mesh(object->iobject, Alembic::Abc::kWrapExisting);
        ISubDSchema schema = subd_mesh.getSchema();
        object->update_shader_attributes(schema.getArbGeomParams(), progress);
      }
    }

    if (scale_is_modified() || object->get_cached_data().transforms.size() == 0) {
      object->setup_transform_cache(scale);
    }
  }
}

CCL_NAMESPACE_END

#endif
