/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#include "scene/alembic_read.h"
#include "scene/alembic.h"
#include "scene/mesh.h"

#include "util/color.h"
#include "util/progress.h"

#ifdef WITH_ALEMBIC

using namespace Alembic::AbcGeom;

CCL_NAMESPACE_BEGIN

static float3 make_float3_from_yup(const V3f &v)
{
  return make_float3(v.x, -v.z, v.y);
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

  double start_frame;
  double end_frame;

  if (proc->get_use_prefetch()) {
    // load the data for the entire animation
    start_frame = static_cast<double>(proc->get_start_frame());
    end_frame = static_cast<double>(proc->get_end_frame());
  }
  else {
    // load the data for the current frame
    start_frame = static_cast<double>(proc->get_frame());
    end_frame = start_frame;
  }

  const double frame_rate = static_cast<double>(proc->get_frame_rate());
  const double start_time = start_frame / frame_rate;
  const double end_time = (end_frame + 1) / frame_rate;

  const size_t start_index = time_sampling.getFloorIndex(start_time, num_samples).first;
  const size_t end_index = time_sampling.getCeilIndex(end_time, num_samples).first;

  for (size_t i = start_index; i < end_index; ++i) {
    result.insert(time_sampling.getSampleTime(i));
  }

  return result;
}

/* Main function to read data, this will iterate over all the relevant sample times for the
 * duration of the requested animation, and call the DataReadingFunc for each of those sample time.
 */
template<typename Params, typename DataReadingFunc>
static void read_data_loop(AlembicProcedural *proc,
                           CachedData &cached_data,
                           const Params &params,
                           DataReadingFunc &&func,
                           Progress &progress)
{
  const std::set<chrono_t> times = get_relevant_sample_times(
      proc, *params.time_sampling, params.num_samples);

  cached_data.set_time_sampling(*params.time_sampling);

  for (chrono_t time : times) {
    if (progress.get_cancel()) {
      return;
    }

    func(cached_data, params, time);
  }
}

/* Polygon Mesh Geometries. */

/* Compute the vertex normals in case none are present in the IPolyMeshSchema, this is mostly used
 * to avoid computing them in the GeometryManager in order to speed up data updates. */
static void compute_vertex_normals(CachedData &cache, double current_time)
{
  if (cache.vertices.size() == 0) {
    return;
  }

  CachedData::CachedAttribute &attr_normal = cache.add_attribute(
      ustring("N"), cache.vertices.get_time_sampling());
  attr_normal.std = ATTR_STD_VERTEX_NORMAL;
  attr_normal.element = ATTR_ELEMENT_VERTEX;
  attr_normal.type_desc = TypeNormal;

  const array<float3> *vertices =
      cache.vertices.data_for_time_no_check(current_time).get_data_or_null();
  const array<int3> *triangles =
      cache.triangles.data_for_time_no_check(current_time).get_data_or_null();

  if (!vertices || !triangles) {
    attr_normal.data.add_no_data(current_time);
    return;
  }

  array<char> attr_data(vertices->size() * sizeof(float3));
  float3 *attr_ptr = reinterpret_cast<float3 *>(attr_data.data());
  memset(attr_ptr, 0, vertices->size() * sizeof(float3));

  for (size_t t = 0; t < triangles->size(); ++t) {
    const int3 tri_int3 = triangles->data()[t];
    Mesh::Triangle tri{};
    tri.v[0] = tri_int3[0];
    tri.v[1] = tri_int3[1];
    tri.v[2] = tri_int3[2];

    const float3 tri_N = tri.compute_normal(vertices->data());

    for (int v = 0; v < 3; ++v) {
      attr_ptr[tri_int3[v]] += tri_N;
    }
  }

  for (size_t v = 0; v < vertices->size(); ++v) {
    attr_ptr[v] = normalize(attr_ptr[v]);
  }

  attr_normal.data.add_data(attr_data, current_time);
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
  array<int> uv_loops;
  shader.reserve(num_triangles);
  triangles.reserve(num_triangles);
  uv_loops.reserve(num_triangles * 3);
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
      uv_loops.push_back_reserved(index_offset + j + 2);
      uv_loops.push_back_reserved(index_offset + j + 1);
      uv_loops.push_back_reserved(index_offset);
    }

    index_offset += face_counts_array[i];
  }

  cached_data.triangles.add_data(triangles, time);
  cached_data.uv_loops.add_data(uv_loops, time);
  cached_data.shader.add_data(shader, time);
}

static array<int> compute_polygon_to_shader_map(
    const Int32ArraySamplePtr &face_counts,
    const vector<FaceSetShaderIndexPair> &face_set_shader_index,
    ISampleSelector sample_sel)
{
  if (face_set_shader_index.empty()) {
    return {};
  }

  if (!face_counts) {
    return {};
  }

  if (face_counts->size() == 0) {
    return {};
  }

  array<int> polygon_to_shader(face_counts->size());

  for (const FaceSetShaderIndexPair &pair : face_set_shader_index) {
    const IFaceSet &face_set = pair.face_set;
    const IFaceSetSchema face_schem = face_set.getSchema();
    const IFaceSetSchema::Sample face_sample = face_schem.getValue(sample_sel);
    const Int32ArraySamplePtr group_faces = face_sample.getFaces();
    const size_t num_group_faces = group_faces->size();

    for (size_t l = 0; l < num_group_faces; l++) {
      size_t pos = (*group_faces)[l];

      if (pos >= polygon_to_shader.size()) {
        continue;
      }

      polygon_to_shader[pos] = pair.shader_index;
    }
  }

  return polygon_to_shader;
}

static void read_poly_mesh_geometry(CachedData &cached_data,
                                    const PolyMeshSchemaData &data,
                                    chrono_t time)
{
  const ISampleSelector iss = ISampleSelector(time);

  add_positions(data.positions.getValue(iss), time, cached_data);

  const Int32ArraySamplePtr face_counts = data.face_counts.getValue(iss);
  const Int32ArraySamplePtr face_indices = data.face_indices.getValue(iss);

  /* Only copy triangles for other frames if the topology is changing over time as well. */
  if (data.topology_variance != kHomogeneousTopology || cached_data.triangles.size() == 0) {
    bool do_triangles = true;

    /* Compare key with last one to check whether the topology changed. */
    if (cached_data.triangles.size() > 0) {
      const ArraySample::Key key = face_indices->getKey();

      if (key == cached_data.triangles.key1) {
        do_triangles = false;
      }

      cached_data.triangles.key1 = key;
    }

    if (do_triangles) {
      const array<int> polygon_to_shader = compute_polygon_to_shader_map(
          face_counts, data.shader_face_sets, iss);
      add_triangles(face_counts, face_indices, time, cached_data, polygon_to_shader);
    }
    else {
      cached_data.triangles.reuse_data_for_last_time(time);
      cached_data.uv_loops.reuse_data_for_last_time(time);
      cached_data.shader.reuse_data_for_last_time(time);
    }

    /* Initialize the first key. */
    if (data.topology_variance != kHomogeneousTopology && cached_data.triangles.size() == 1) {
      cached_data.triangles.key1 = face_indices->getKey();
    }
  }

  if (data.normals.valid()) {
    add_normals(face_indices, data.normals, time, cached_data);
  }
  else {
    compute_vertex_normals(cached_data, time);
  }
}

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const PolyMeshSchemaData &data,
                        Progress &progress)
{
  read_data_loop(proc, cached_data, data, read_poly_mesh_geometry, progress);
}

/* Subdivision Geometries */

static void add_subd_polygons(CachedData &cached_data, const SubDSchemaData &data, chrono_t time)
{
  const ISampleSelector iss = ISampleSelector(time);

  const Int32ArraySamplePtr face_counts = data.face_counts.getValue(iss);
  const Int32ArraySamplePtr face_indices = data.face_indices.getValue(iss);

  array<int> subd_start_corner;
  array<int> shader;
  array<int> subd_num_corners;
  array<bool> subd_smooth;
  array<int> subd_ptex_offset;
  array<int> subd_face_corners;
  array<int> uv_loops;

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
  uv_loops.reserve(num_corners);

  int start_corner = 0;
  int current_shader = 0;
  int ptex_offset = 0;

  const array<int> polygon_to_shader = compute_polygon_to_shader_map(
      face_counts, data.shader_face_sets, iss);

  for (size_t i = 0; i < face_counts->size(); i++) {
    num_corners = face_counts_array[i];

    if (!polygon_to_shader.empty()) {
      current_shader = polygon_to_shader[i];
    }

    subd_start_corner.push_back_reserved(start_corner);
    subd_num_corners.push_back_reserved(num_corners);

    for (int j = 0; j < num_corners; ++j) {
      subd_face_corners.push_back_reserved(face_indices_array[start_corner + j]);
      uv_loops.push_back_reserved(start_corner + j);
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
  cached_data.uv_loops.add_data(uv_loops, time);
}

static void add_subd_edge_creases(CachedData &cached_data,
                                  const SubDSchemaData &data,
                                  chrono_t time)
{
  if (!(data.crease_indices.valid() && data.crease_lengths.valid() &&
        data.crease_sharpnesses.valid())) {
    return;
  }

  const ISampleSelector iss = ISampleSelector(time);

  const Int32ArraySamplePtr creases_length = data.crease_lengths.getValue(iss);
  const Int32ArraySamplePtr creases_indices = data.crease_indices.getValue(iss);
  const FloatArraySamplePtr creases_sharpnesses = data.crease_sharpnesses.getValue(iss);

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

static void add_subd_vertex_creases(CachedData &cached_data,
                                    const SubDSchemaData &data,
                                    chrono_t time)
{
  if (!(data.corner_indices.valid() && data.crease_sharpnesses.valid())) {
    return;
  }

  const ISampleSelector iss = ISampleSelector(time);
  const Int32ArraySamplePtr creases_indices = data.crease_indices.getValue(iss);
  const FloatArraySamplePtr creases_sharpnesses = data.crease_sharpnesses.getValue(iss);

  if (!(creases_indices && creases_sharpnesses) ||
      creases_indices->size() != creases_sharpnesses->size()) {
    return;
  }

  array<float> sharpnesses;
  sharpnesses.reserve(creases_indices->size());
  array<int> indices;
  indices.reserve(creases_indices->size());

  for (size_t i = 0; i < creases_indices->size(); i++) {
    indices.push_back_reserved((*creases_indices)[i]);
    sharpnesses.push_back_reserved((*creases_sharpnesses)[i]);
  }

  cached_data.subd_vertex_crease_indices.add_data(indices, time);
  cached_data.subd_vertex_crease_weights.add_data(sharpnesses, time);
}

static void read_subd_geometry(CachedData &cached_data, const SubDSchemaData &data, chrono_t time)
{
  const ISampleSelector iss = ISampleSelector(time);

  add_positions(data.positions.getValue(iss), time, cached_data);

  if (data.topology_variance != kHomogeneousTopology || cached_data.shader.size() == 0) {
    add_subd_polygons(cached_data, data, time);
    add_subd_edge_creases(cached_data, data, time);
    add_subd_vertex_creases(cached_data, data, time);
  }
}

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const SubDSchemaData &data,
                        Progress &progress)
{
  read_data_loop(proc, cached_data, data, read_subd_geometry, progress);
}

/* Curve Geometries. */

static void read_curves_data(CachedData &cached_data, const CurvesSchemaData &data, chrono_t time)
{
  const ISampleSelector iss = ISampleSelector(time);

  const Int32ArraySamplePtr curves_num_vertices = data.num_vertices.getValue(iss);
  const P3fArraySamplePtr position = data.positions.getValue(iss);

  FloatArraySamplePtr radiuses;

  if (data.widths.valid()) {
    IFloatGeomParam::Sample wsample = data.widths.getExpandedValue(iss);
    radiuses = wsample.getVals();
  }

  const bool do_radius = (radiuses != nullptr) && (radiuses->size() > 1);
  float radius = (radiuses && radiuses->size() == 1) ? (*radiuses)[0] : data.default_radius;

  array<float3> curve_keys;
  array<float> curve_radius;
  array<int> curve_first_key;
  array<int> curve_shader;

  const bool is_homogeneous = data.topology_variance == kHomogeneousTopology;

  curve_keys.reserve(position->size());
  curve_radius.reserve(position->size());
  curve_first_key.reserve(curves_num_vertices->size());
  curve_shader.reserve(curves_num_vertices->size());

  int offset = 0;
  for (size_t i = 0; i < curves_num_vertices->size(); i++) {
    const int num_vertices = curves_num_vertices->get()[i];

    for (int j = 0; j < num_vertices; j++) {
      const V3f &f = position->get()[offset + j];
      // todo(@kevindietrich): we are reading too much data?
      curve_keys.push_back_slow(make_float3_from_yup(f));

      if (do_radius) {
        radius = (*radiuses)[offset + j];
      }

      curve_radius.push_back_slow(radius * data.radius_scale);
    }

    if (!is_homogeneous || cached_data.curve_first_key.size() == 0) {
      curve_first_key.push_back_reserved(offset);
      curve_shader.push_back_reserved(0);
    }

    offset += num_vertices;
  }

  cached_data.curve_keys.add_data(curve_keys, time);
  cached_data.curve_radius.add_data(curve_radius, time);

  if (!is_homogeneous || cached_data.curve_first_key.size() == 0) {
    cached_data.curve_first_key.add_data(curve_first_key, time);
    cached_data.curve_shader.add_data(curve_shader, time);
  }
}

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const CurvesSchemaData &data,
                        Progress &progress)
{
  read_data_loop(proc, cached_data, data, read_curves_data, progress);
}

/* Points Geometries. */

static void read_points_data(CachedData &cached_data, const PointsSchemaData &data, chrono_t time)
{
  const ISampleSelector iss = ISampleSelector(time);

  const P3fArraySamplePtr position = data.positions.getValue(iss);
  FloatArraySamplePtr radiuses;

  array<float3> a_positions;
  array<float> a_radius;
  array<int> a_shader;
  a_positions.reserve(position->size());
  a_radius.reserve(position->size());
  a_shader.reserve(position->size());

  if (data.radiuses.valid()) {
    IFloatGeomParam::Sample wsample = data.radiuses.getExpandedValue(iss);
    radiuses = wsample.getVals();
  }

  const bool do_radius = (radiuses != nullptr) && (radiuses->size() > 1);
  float radius = (radiuses && radiuses->size() == 1) ? (*radiuses)[0] : data.default_radius;

  int offset = 0;
  for (size_t i = 0; i < position->size(); i++) {
    const V3f &f = position->get()[offset + i];
    a_positions.push_back_slow(make_float3_from_yup(f));

    if (do_radius) {
      radius = (*radiuses)[offset + i];
      a_radius.push_back_slow(radius);
    }

    a_shader.push_back_slow((int)0);
  }

  cached_data.points.add_data(a_positions, time);
  cached_data.radiuses.add_data(a_radius, time);
  cached_data.points_shader.add_data(a_shader, time);
}

void read_geometry_data(AlembicProcedural *proc,
                        CachedData &cached_data,
                        const PointsSchemaData &data,
                        Progress &progress)
{
  read_data_loop(proc, cached_data, data, read_points_data, progress);
}
/* Attributes conversions. */

/* Type traits for converting between Alembic and Cycles types.
 */

template<typename T> struct value_type_converter {
  using cycles_type = float;
  /* Use `TypeDesc::FLOAT` instead of `TypeFloat` to work around a compiler bug in gcc 11. */
  static constexpr TypeDesc type_desc = TypeDesc::FLOAT;
  static constexpr const char *type_name = "float (default)";

  static cycles_type convert_value(T value)
  {
    return static_cast<float>(value);
  }
};

template<> struct value_type_converter<Imath::V2f> {
  using cycles_type = float2;
  static constexpr TypeDesc type_desc = TypeFloat2;
  static constexpr const char *type_name = "float2";

  static cycles_type convert_value(Imath::V2f value)
  {
    return make_float2(value.x, value.y);
  }
};

template<> struct value_type_converter<Imath::V3f> {
  using cycles_type = float3;
  static constexpr TypeDesc type_desc = TypeVector;
  static constexpr const char *type_name = "float3";

  static cycles_type convert_value(Imath::V3f value)
  {
    return make_float3_from_yup(value);
  }
};

template<> struct value_type_converter<Imath::C3f> {
  using cycles_type = uchar4;
  static constexpr TypeDesc type_desc = TypeRGBA;
  static constexpr const char *type_name = "rgb";

  static cycles_type convert_value(Imath::C3f value)
  {
    return color_float_to_byte(make_float3(value.x, value.y, value.z));
  }
};

template<> struct value_type_converter<Imath::C4f> {
  using cycles_type = uchar4;
  static constexpr TypeDesc type_desc = TypeRGBA;
  static constexpr const char *type_name = "rgba";

  static cycles_type convert_value(Imath::C4f value)
  {
    return color_float4_to_uchar4(make_float4(value.r, value.g, value.b, value.a));
  }
};

/* Main function used to read attributes of any type. */
template<typename TRAIT>
static void process_attribute(CachedData &cache,
                              CachedData::CachedAttribute &attribute,
                              GeometryScope scope,
                              const typename ITypedGeomParam<TRAIT>::Sample &sample,
                              double time)
{
  using abc_type = typename TRAIT::value_type;
  using cycles_type = typename value_type_converter<abc_type>::cycles_type;

  const TypedArraySample<TRAIT> &values = *sample.getVals();

  switch (scope) {
    case kConstantScope:
    case kVertexScope: {
      const array<float3> *vertices =
          cache.vertices.data_for_time_no_check(time).get_data_or_null();

      if (!vertices) {
        attribute.data.add_no_data(time);
        return;
      }

      if (vertices->size() != values.size()) {
        attribute.data.add_no_data(time);
        return;
      }

      array<char> data(vertices->size() * sizeof(cycles_type));

      cycles_type *pod_typed_data = reinterpret_cast<cycles_type *>(data.data());

      for (size_t i = 0; i < values.size(); ++i) {
        *pod_typed_data++ = value_type_converter<abc_type>::convert_value(values[i]);
      }

      attribute.data.add_data(data, time);
      break;
    }
    case kVaryingScope: {
      const array<int3> *triangles =
          cache.triangles.data_for_time_no_check(time).get_data_or_null();

      if (!triangles) {
        attribute.data.add_no_data(time);
        return;
      }

      array<char> data(triangles->size() * 3 * sizeof(cycles_type));

      cycles_type *pod_typed_data = reinterpret_cast<cycles_type *>(data.data());

      for (const int3 &tri : *triangles) {
        *pod_typed_data++ = value_type_converter<abc_type>::convert_value(values[tri.x]);
        *pod_typed_data++ = value_type_converter<abc_type>::convert_value(values[tri.y]);
        *pod_typed_data++ = value_type_converter<abc_type>::convert_value(values[tri.z]);
      }

      attribute.data.add_data(data, time);
      break;
    }
    default: {
      break;
    }
  }
}

/* UVs are processed separately as their indexing is based on loops, instead of vertices or
 * corners. */
static void process_uvs(CachedData &cache,
                        CachedData::CachedAttribute &attribute,
                        GeometryScope scope,
                        const IV2fGeomParam::Sample &sample,
                        double time)
{
  if (scope != kFacevaryingScope && scope != kVaryingScope && scope != kVertexScope) {
    return;
  }

  const array<int> *uv_loops = cache.uv_loops.data_for_time_no_check(time).get_data_or_null();

  /* It's ok to not have loop indices, as long as the scope is not face-varying. */
  if (!uv_loops && scope == kFacevaryingScope) {
    return;
  }

  const array<int3> *triangles = cache.triangles.data_for_time_no_check(time).get_data_or_null();
  const array<int> *corners =
      cache.subd_face_corners.data_for_time_no_check(time).get_data_or_null();

  array<char> data;
  if (triangles) {
    data.resize(triangles->size() * 3 * sizeof(float2));
  }
  else if (corners) {
    data.resize(corners->size() * sizeof(float2));
  }
  else {
    return;
  }

  float2 *data_float2 = reinterpret_cast<float2 *>(data.data());

  const uint32_t *indices = sample.getIndices()->get();
  const V2f *values = sample.getVals()->get();

  if (scope == kFacevaryingScope) {
    for (const int uv_loop_index : *uv_loops) {
      const uint32_t index = indices[uv_loop_index];
      *data_float2++ = make_float2(values[index][0], values[index][1]);
    }
  }
  else if (scope == kVaryingScope || scope == kVertexScope) {
    if (triangles) {
      for (size_t i = 0; i < triangles->size(); i++) {
        const int3 t = (*triangles)[i];
        *data_float2++ = make_float2(values[t.x][0], values[t.x][1]);
        *data_float2++ = make_float2(values[t.y][0], values[t.y][1]);
        *data_float2++ = make_float2(values[t.z][0], values[t.z][1]);
      }
    }
    else if (corners) {
      for (size_t i = 0; i < corners->size(); i++) {
        const int c = (*corners)[i];
        *data_float2++ = make_float2(values[c][0], values[c][1]);
      }
    }
  }

  attribute.data.add_data(data, time);
}

/* Type of the function used to parse one time worth of data, either process_uvs or
 * process_attribute_generic. */
template<typename TRAIT>
using process_callback_type = void (*)(CachedData &,
                                       CachedData::CachedAttribute &,
                                       GeometryScope,
                                       const typename ITypedGeomParam<TRAIT>::Sample &,
                                       double);

/* Main loop to process the attributes, this will look at the given param's TimeSampling and
 * extract data based on which frame time is requested by the procedural and execute the callback
 * for each of those requested time. */
template<typename TRAIT>
static void read_attribute_loop(AlembicProcedural *proc,
                                CachedData &cache,
                                const ITypedGeomParam<TRAIT> &param,
                                process_callback_type<TRAIT> callback,
                                Progress &progress,
                                AttributeStandard std = ATTR_STD_NONE)
{
  const std::set<chrono_t> times = get_relevant_sample_times(
      proc, *param.getTimeSampling(), param.getNumSamples());

  if (times.empty()) {
    return;
  }

  std::string name = param.getName();

  if (std == ATTR_STD_UV) {
    std::string uv_source_name = Alembic::Abc::GetSourceName(param.getMetaData());

    /* According to the convention, primary UVs should have had their name
     * set using Alembic::Abc::SetSourceName, but you can't expect everyone
     * to follow it! :) */
    if (!uv_source_name.empty()) {
      name = uv_source_name;
    }
  }

  CachedData::CachedAttribute &attribute = cache.add_attribute(ustring(name),
                                                               *param.getTimeSampling());

  using abc_type = typename TRAIT::value_type;

  attribute.data.set_time_sampling(*param.getTimeSampling());
  attribute.std = std;
  attribute.type_desc = value_type_converter<abc_type>::type_desc;

  if (attribute.type_desc == TypeRGBA) {
    attribute.element = ATTR_ELEMENT_CORNER_BYTE;
  }
  else {
    if (param.getScope() == kVaryingScope || param.getScope() == kFacevaryingScope) {
      attribute.element = ATTR_ELEMENT_CORNER;
    }
    else {
      attribute.element = ATTR_ELEMENT_VERTEX;
    }
  }

  for (const chrono_t time : times) {
    if (progress.get_cancel()) {
      return;
    }

    ISampleSelector iss = ISampleSelector(time);
    typename ITypedGeomParam<TRAIT>::Sample sample;
    param.getIndexed(sample, iss);

    if (!sample.valid()) {
      continue;
    }

    if (!sample.getVals()) {
      attribute.data.add_no_data(time);
      continue;
    }

    /* Check whether we already loaded constant data. */
    if (attribute.data.size() != 0) {
      if (param.isConstant()) {
        return;
      }

      const ArraySample::Key indices_key = sample.getIndices()->getKey();
      const ArraySample::Key values_key = sample.getVals()->getKey();

      const bool is_same_as_last_time = (indices_key == attribute.data.key1 &&
                                         values_key == attribute.data.key2);

      attribute.data.key1 = indices_key;
      attribute.data.key2 = values_key;

      if (is_same_as_last_time) {
        attribute.data.reuse_data_for_last_time(time);
        continue;
      }
    }

    callback(cache, attribute, param.getScope(), sample, time);
  }
}

/* Attributes requests. */

/* This structure is used to tell which ICoumpoundProperty the PropertyHeader comes from, as we
 * need the parent when downcasting to the proper type. */
struct PropHeaderAndParent {
  const PropertyHeader *prop;
  ICompoundProperty parent;
};

/* Parse the ICompoundProperty to look for properties whose names appear in the
 * AttributeRequestSet. This also looks into any child ICompoundProperty of the given
 * ICompoundProperty. If no property of the given name is found, let it be that way, Cycles will
 * use a zero value for the missing attribute. */
static void parse_requested_attributes_recursive(const AttributeRequestSet &requested_attributes,
                                                 const ICompoundProperty &arb_geom_params,
                                                 vector<PropHeaderAndParent> &requested_properties)
{
  if (!arb_geom_params.valid()) {
    return;
  }

  for (const AttributeRequest &req : requested_attributes.requests) {
    const PropertyHeader *property_header = arb_geom_params.getPropertyHeader(req.name.c_str());

    if (!property_header) {
      continue;
    }

    requested_properties.push_back({property_header, arb_geom_params});
  }

  /* Look into children compound properties. */
  for (size_t i = 0; i < arb_geom_params.getNumProperties(); ++i) {
    const PropertyHeader &property_header = arb_geom_params.getPropertyHeader(i);

    if (property_header.isCompound()) {
      ICompoundProperty compound_property = ICompoundProperty(arb_geom_params,
                                                              property_header.getName());
      parse_requested_attributes_recursive(
          requested_attributes, compound_property, requested_properties);
    }
  }
}

/* Main entry point for parsing requested attributes from an ICompoundProperty, this exists so that
 * we can simply return the list of properties instead of allocating it on the stack and passing it
 * as a parameter. */
static vector<PropHeaderAndParent> parse_requested_attributes(
    const AttributeRequestSet &requested_attributes, const ICompoundProperty &arb_geom_params)
{
  vector<PropHeaderAndParent> requested_properties;
  parse_requested_attributes_recursive(
      requested_attributes, arb_geom_params, requested_properties);
  return requested_properties;
}

/* Read the attributes requested by the shaders from the archive. This will recursively find named
 * attributes from the AttributeRequestSet in the ICompoundProperty and any of its compound child.
 * The attributes are added to the CachedData's attribute list. For each attribute we will try to
 * deduplicate data across consecutive frames. */
void read_attributes(AlembicProcedural *proc,
                     CachedData &cache,
                     const ICompoundProperty &arb_geom_params,
                     const IV2fGeomParam &default_uvs_param,
                     const AttributeRequestSet &requested_attributes,
                     Progress &progress)
{
  if (default_uvs_param.valid()) {
    /* Only the default UVs should be treated as the standard UV attribute. */
    read_attribute_loop(proc, cache, default_uvs_param, process_uvs, progress, ATTR_STD_UV);
  }

  vector<PropHeaderAndParent> requested_properties = parse_requested_attributes(
      requested_attributes, arb_geom_params);

  for (const PropHeaderAndParent &prop_and_parent : requested_properties) {
    if (progress.get_cancel()) {
      return;
    }

    const PropertyHeader *prop = prop_and_parent.prop;
    const ICompoundProperty &parent = prop_and_parent.parent;

    if (IBoolGeomParam::matches(*prop)) {
      const IBoolGeomParam &param = IBoolGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<BooleanTPTraits>, progress);
    }
    else if (IInt32GeomParam::matches(*prop)) {
      const IInt32GeomParam &param = IInt32GeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<Int32TPTraits>, progress);
    }
    else if (IFloatGeomParam::matches(*prop)) {
      const IFloatGeomParam &param = IFloatGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<Float32TPTraits>, progress);
    }
    else if (IV2fGeomParam::matches(*prop)) {
      const IV2fGeomParam &param = IV2fGeomParam(parent, prop->getName());
      if (Alembic::AbcGeom::isUV(*prop)) {
        read_attribute_loop(proc, cache, param, process_uvs, progress);
      }
      else {
        read_attribute_loop(proc, cache, param, process_attribute<V2fTPTraits>, progress);
      }
    }
    else if (IV3fGeomParam::matches(*prop)) {
      const IV3fGeomParam &param = IV3fGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<V3fTPTraits>, progress);
    }
    else if (IN3fGeomParam::matches(*prop)) {
      const IN3fGeomParam &param = IN3fGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<N3fTPTraits>, progress);
    }
    else if (IC3fGeomParam::matches(*prop)) {
      const IC3fGeomParam &param = IC3fGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<C3fTPTraits>, progress);
    }
    else if (IC4fGeomParam::matches(*prop)) {
      const IC4fGeomParam &param = IC4fGeomParam(parent, prop->getName());
      read_attribute_loop(proc, cache, param, process_attribute<C4fTPTraits>, progress);
    }
  }

  cache.invalidate_last_loaded_time(true);
}

CCL_NAMESPACE_END

#endif
