/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <algorithm>
#include <system_error>

#include "BKE_attribute.hh"
#include "BKE_blender_version.h"
#include "BKE_mesh.hh"

#include "BLI_color.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_fileops.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_task.hh"

#include "IO_path_util.hh"

#include "obj_export_mesh.hh"
#include "obj_export_mtl.hh"
#include "obj_export_nurbs.hh"

#include "obj_export_file_writer.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.obj"};

namespace blender::io::obj {
/**
 * Per reference http://www.martinreddy.net/gfx/3d/OBJ.spec:
 * To turn off smoothing groups, use a value of 0 or off.
 * Polygonal elements use group numbers to put elements in different smoothing groups.
 * For free-form surfaces, smoothing groups are either turned on or off;
 * there is no difference between values greater than 0.
 */
const int SMOOTH_GROUP_DISABLED = 0;
const int SMOOTH_GROUP_DEFAULT = 1;

static const char *DEFORM_GROUP_DISABLED = "off";
/* There is no deform group default name. Use what the user set in the UI. */

/**
 * Per reference http://www.martinreddy.net/gfx/3d/OBJ.spec:
 * Once a material is assigned, it cannot be turned off; it can only be changed.
 * If a material name is not specified, a white material is used.
 * So an empty material name is written. */
static const char *MATERIAL_GROUP_DISABLED = "";

OBJWriter::OBJWriter(const char *filepath, const OBJExportParams &export_params) noexcept(false)
    : export_params_(export_params), outfile_path_(filepath), outfile_(nullptr)
{
  outfile_ = BLI_fopen(filepath, "wb");
  if (!outfile_) {
    throw std::system_error(errno, std::system_category(), "Cannot open file " + outfile_path_);
  }
}
OBJWriter::~OBJWriter()
{
  if (outfile_ && std::fclose(outfile_)) {
    CLOG_ERROR(&LOG,
               "Error: could not close file '%s' properly, it may be corrupted.",
               outfile_path_.c_str());
  }
}

void OBJWriter::write_vert_uv_normal_indices(FormatHandler &fh,
                                             const IndexOffsets &offsets,
                                             Span<int> vert_indices,
                                             Span<int> uv_indices,
                                             Span<int> normal_indices,
                                             bool flip) const
{
  BLI_assert(vert_indices.size() == uv_indices.size() &&
             vert_indices.size() == normal_indices.size());
  const int vertex_offset = offsets.vertex_offset + 1;
  const int uv_offset = offsets.uv_vertex_offset + 1;
  const int normal_offset = offsets.normal_offset + 1;
  const int n = vert_indices.size();
  fh.write_obj_face_begin();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write_obj_face_v_uv_normal(vert_indices[j] + vertex_offset,
                                    uv_indices[j] + uv_offset,
                                    normal_indices[j] + normal_offset);
    }
  }
  else {
    /* For a transform that is mirrored (negative scale on odd number of axes),
     * we want to flip the face index order. Start from the same index, and
     * then go backwards. Same logic in other write_*_indices functions below. */
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write_obj_face_v_uv_normal(vert_indices[j] + vertex_offset,
                                    uv_indices[j] + uv_offset,
                                    normal_indices[j] + normal_offset);
    }
  }
  fh.write_obj_face_end();
}

void OBJWriter::write_vert_normal_indices(FormatHandler &fh,
                                          const IndexOffsets &offsets,
                                          Span<int> vert_indices,
                                          Span<int> /*uv_indices*/,
                                          Span<int> normal_indices,
                                          bool flip) const
{
  BLI_assert(vert_indices.size() == normal_indices.size());
  const int vertex_offset = offsets.vertex_offset + 1;
  const int normal_offset = offsets.normal_offset + 1;
  const int n = vert_indices.size();
  fh.write_obj_face_begin();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write_obj_face_v_normal(vert_indices[j] + vertex_offset,
                                 normal_indices[j] + normal_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write_obj_face_v_normal(vert_indices[j] + vertex_offset,
                                 normal_indices[j] + normal_offset);
    }
  }
  fh.write_obj_face_end();
}

void OBJWriter::write_vert_uv_indices(FormatHandler &fh,
                                      const IndexOffsets &offsets,
                                      Span<int> vert_indices,
                                      Span<int> uv_indices,
                                      Span<int> /*normal_indices*/,
                                      bool flip) const
{
  BLI_assert(vert_indices.size() == uv_indices.size());
  const int vertex_offset = offsets.vertex_offset + 1;
  const int uv_offset = offsets.uv_vertex_offset + 1;
  const int n = vert_indices.size();
  fh.write_obj_face_begin();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write_obj_face_v_uv(vert_indices[j] + vertex_offset, uv_indices[j] + uv_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write_obj_face_v_uv(vert_indices[j] + vertex_offset, uv_indices[j] + uv_offset);
    }
  }
  fh.write_obj_face_end();
}

void OBJWriter::write_vert_indices(FormatHandler &fh,
                                   const IndexOffsets &offsets,
                                   Span<int> vert_indices,
                                   Span<int> /*uv_indices*/,
                                   Span<int> /*normal_indices*/,
                                   bool flip) const
{
  const int vertex_offset = offsets.vertex_offset + 1;
  const int n = vert_indices.size();
  fh.write_obj_face_begin();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write_obj_face_v(vert_indices[j] + vertex_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write_obj_face_v(vert_indices[j] + vertex_offset);
    }
  }
  fh.write_obj_face_end();
}

void OBJWriter::write_header() const
{
  using namespace std::string_literals;
  FormatHandler fh;
  fh.write_string("# Blender "s + BKE_blender_version_string());
  fh.write_string("# www.blender.org");
  fh.write_to_file(outfile_);
}

void OBJWriter::write_mtllib_name(const StringRefNull mtl_filepath) const
{
  /* Split `.MTL` file path into parent directory and filename. */
  char mtl_file_name[FILE_MAXFILE];
  char mtl_dir_name[FILE_MAXDIR];
  BLI_path_split_dir_file(mtl_filepath.data(),
                          mtl_dir_name,
                          sizeof(mtl_dir_name),
                          mtl_file_name,
                          sizeof(mtl_file_name));
  FormatHandler fh;
  fh.write_obj_mtllib(mtl_file_name);
  fh.write_to_file(outfile_);
}

static void spaces_to_underscores(std::string &r_name)
{
  std::replace(r_name.begin(), r_name.end(), ' ', '_');
}

void OBJWriter::write_object_name(FormatHandler &fh, const OBJMesh &obj_mesh_data) const
{
  std::string object_name = obj_mesh_data.get_object_name();
  spaces_to_underscores(object_name);
  if (export_params_.export_object_groups) {
    std::string mesh_name = obj_mesh_data.get_object_mesh_name();
    spaces_to_underscores(mesh_name);
    fh.write_obj_group(object_name + "_" + mesh_name);
    return;
  }
  fh.write_obj_object(object_name);
}

/* Split up large meshes into multi-threaded jobs; each job processes
 * this amount of items. */
static const int chunk_size = 32768;
static int calc_chunk_count(int count)
{
  return (count + chunk_size - 1) / chunk_size;
}

/* Write /tot_count/ items to OBJ file output. Each item is written
 * by a /function/ that should be independent from other items.
 * If the amount of items is large enough (> chunk_size), then writing
 * will be done in parallel, into temporary FormatHandler buffers that
 * will be written into the final /fh/ buffer at the end.
 */
template<typename Function>
void obj_parallel_chunked_output(FormatHandler &fh, int tot_count, const Function &function)
{
  if (tot_count <= 0) {
    return;
  }
  /* If we have just one chunk, process it directly into the output
   * buffer - avoids all the job scheduling and temporary vector allocation
   * overhead. */
  const int chunk_count = calc_chunk_count(tot_count);
  if (chunk_count == 1) {
    for (int i = 0; i < tot_count; i++) {
      function(fh, i);
    }
    return;
  }
  /* Give each chunk its own temporary output buffer, and process them in parallel. */
  Array<FormatHandler> buffers(chunk_count);
  threading::parallel_for(IndexRange(chunk_count), 1, [&](IndexRange range) {
    for (const int r : range) {
      int i_start = r * chunk_size;
      int i_end = std::min(i_start + chunk_size, tot_count);
      auto &buf = buffers[r];
      for (int i = i_start; i < i_end; i++) {
        function(buf, i);
      }
    }
  });
  /* Emit all temporary output buffers into the destination buffer. */
  for (auto &buf : buffers) {
    fh.append_from(buf);
  }
}

void OBJWriter::write_vertex_coords(FormatHandler &fh,
                                    const OBJMesh &obj_mesh_data,
                                    bool write_colors) const
{
  const int tot_count = obj_mesh_data.tot_vertices();

  const Mesh *mesh = obj_mesh_data.get_mesh();
  const StringRef name = mesh->active_color_attribute;

  const float4x4 transform = obj_mesh_data.get_world_axes_transform();
  const Span<float3> positions = obj_mesh_data.get_mesh()->vert_positions();

  if (write_colors && !name.is_empty()) {
    const bke::AttributeAccessor attributes = mesh->attributes();
    const VArray<ColorGeometry4f> attribute = *attributes.lookup_or_default<ColorGeometry4f>(
        name, bke::AttrDomain::Point, {0.0f, 0.0f, 0.0f, 0.0f});

    BLI_assert(tot_count == attribute.size());
    obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler &buf, int i) {
      const float3 vertex = math::transform_point(transform, positions[i]);
      ColorGeometry4f linear = attribute.get(i);
      float srgb[3];
      linearrgb_to_srgb_v3_v3(srgb, linear);
      buf.write_obj_vertex_color(vertex[0], vertex[1], vertex[2], srgb[0], srgb[1], srgb[2]);
    });
  }
  else {
    obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler &buf, int i) {
      const float3 vertex = math::transform_point(transform, positions[i]);
      buf.write_obj_vertex(vertex[0], vertex[1], vertex[2]);
    });
  }
}

void OBJWriter::write_uv_coords(FormatHandler &fh, OBJMesh &r_obj_mesh_data) const
{
  const Span<float2> uv_coords = r_obj_mesh_data.get_uv_coords();
  obj_parallel_chunked_output(fh, uv_coords.size(), [&](FormatHandler &buf, int i) {
    const float2 &uv_vertex = uv_coords[i];
    buf.write_obj_uv(uv_vertex[0], uv_vertex[1]);
  });
}

void OBJWriter::write_normals(FormatHandler &fh, OBJMesh &obj_mesh_data)
{
  /* Poly normals should be calculated earlier via store_normal_coords_and_indices. */
  const Span<float3> normal_coords = obj_mesh_data.get_normal_coords();
  obj_parallel_chunked_output(fh, normal_coords.size(), [&](FormatHandler &buf, int i) {
    const float3 &normal = normal_coords[i];
    buf.write_obj_normal(normal[0], normal[1], normal[2]);
  });
}

OBJWriter::func_vert_uv_normal_indices OBJWriter::get_face_element_writer(
    const int total_uv_vertices) const
{
  if (export_params_.export_normals) {
    if (export_params_.export_uv && (total_uv_vertices > 0)) {
      /* Write both normals and UV indices. */
      return &OBJWriter::write_vert_uv_normal_indices;
    }
    /* Write normals indices. */
    return &OBJWriter::write_vert_normal_indices;
  }
  /* Write UV indices. */
  if (export_params_.export_uv && (total_uv_vertices > 0)) {
    return &OBJWriter::write_vert_uv_indices;
  }
  /* Write neither normals nor UV indices. */
  return &OBJWriter::write_vert_indices;
}

static int get_smooth_group(const OBJMesh &mesh, const OBJExportParams &params, int face_idx)
{
  if (face_idx < 0) {
    return NEGATIVE_INIT;
  }
  int group = SMOOTH_GROUP_DISABLED;
  if (mesh.is_ith_face_smooth(face_idx)) {
    group = !params.export_smooth_groups ? SMOOTH_GROUP_DEFAULT : mesh.ith_smooth_group(face_idx);
  }
  return group;
}

void OBJWriter::write_face_elements(FormatHandler &fh,
                                    const IndexOffsets &offsets,
                                    const OBJMesh &obj_mesh_data,
                                    FunctionRef<const char *(int)> matname_fn)
{
  const func_vert_uv_normal_indices face_element_writer = get_face_element_writer(
      obj_mesh_data.tot_uv_vertices());

  const int tot_faces = obj_mesh_data.tot_faces();
  const int tot_deform_groups = obj_mesh_data.tot_deform_groups();
  threading::EnumerableThreadSpecific<Vector<float>> group_weights;
  const bke::AttributeAccessor attributes = obj_mesh_data.get_mesh()->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);

  obj_parallel_chunked_output(fh, tot_faces, [&](FormatHandler &buf, int idx) {
    /* Polygon order for writing into the file is not necessarily the same
     * as order in the mesh; it will be sorted by material indices. Remap current
     * and previous indices here according to the order. */
    int prev_i = obj_mesh_data.remap_face_index(idx - 1);
    int i = obj_mesh_data.remap_face_index(idx);

    const Span<int> face_vertex_indices = obj_mesh_data.calc_face_vert_indices(i);
    const Span<int> face_uv_indices = obj_mesh_data.get_face_uv_indices(i);
    const Span<int> face_normal_indices = obj_mesh_data.get_face_normal_indices(i);

    /* Write smoothing group if different from previous. */
    {
      const int prev_group = get_smooth_group(obj_mesh_data, export_params_, prev_i);
      const int group = get_smooth_group(obj_mesh_data, export_params_, i);
      if (group != prev_group) {
        buf.write_obj_smooth(group);
      }
    }

    /* Write vertex group if different from previous. */
    if (export_params_.export_vertex_groups) {
      Vector<float> &local_weights = group_weights.local();
      local_weights.resize(tot_deform_groups);
      const int16_t prev_group = idx == 0 ? NEGATIVE_INIT :
                                            obj_mesh_data.get_face_deform_group_index(
                                                prev_i, local_weights);
      const int16_t group = obj_mesh_data.get_face_deform_group_index(i, local_weights);
      if (group != prev_group) {
        buf.write_obj_group(group == NOT_FOUND ? DEFORM_GROUP_DISABLED :
                                                 obj_mesh_data.get_face_deform_group_name(group));
      }
    }

    /* Write material name and material group if different from previous. */
    if ((export_params_.export_materials || export_params_.export_material_groups) &&
        obj_mesh_data.tot_materials() > 0)
    {
      const int16_t prev_mat = idx == 0 ? NEGATIVE_INIT : std::max(0, material_indices[prev_i]);
      const int16_t mat = std::max(0, material_indices[i]);
      if (mat != prev_mat) {
        if (mat == NOT_FOUND) {
          if (export_params_.export_materials) {
            buf.write_obj_usemtl(MATERIAL_GROUP_DISABLED);
          }
        }
        else {
          const char *mat_name = matname_fn(mat);
          if (!mat_name) {
            mat_name = MATERIAL_GROUP_DISABLED;
          }
          if (export_params_.export_material_groups) {
            std::string object_name = obj_mesh_data.get_object_name();
            spaces_to_underscores(object_name);
            buf.write_obj_group(object_name + "_" + mat_name);
          }
          if (export_params_.export_materials) {
            buf.write_obj_usemtl(mat_name);
          }
        }
      }
    }

    /* Write face elements. */
    (this->*face_element_writer)(buf,
                                 offsets,
                                 face_vertex_indices,
                                 face_uv_indices,
                                 face_normal_indices,
                                 obj_mesh_data.is_mirrored_transform());
  });
}

void OBJWriter::write_edges_indices(FormatHandler &fh,
                                    const IndexOffsets &offsets,
                                    const OBJMesh &obj_mesh_data) const
{
  const Mesh &mesh = *obj_mesh_data.get_mesh();
  const bke::LooseEdgeCache &loose_edges = mesh.loose_edges();
  if (loose_edges.count == 0) {
    return;
  }

  const Span<int2> edges = mesh.edges();
  for (const int64_t i : edges.index_range()) {
    if (loose_edges.is_loose_bits[i]) {
      const int2 obj_edge = edges[i] + offsets.vertex_offset + 1;
      fh.write_obj_edge(obj_edge[0], obj_edge[1]);
    }
  }
}

static float4x4 compute_world_axes_transform(const OBJExportParams &export_params,
                                             const blender::float4x4 &object_to_world)
{
  float4x4 world_axes_transform;
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the Blender's default axis settings. */
  mat3_from_axis_conversion(
      export_params.forward_axis, export_params.up_axis, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
  mul_m4_m3m4(world_axes_transform.ptr(), axes_transform, object_to_world.ptr());
  /* #mul_m4_m3m4 does not transform last row of #Object.object_to_world, i.e. location data. */
  mul_v3_m3v3(world_axes_transform[3], axes_transform, object_to_world.location());
  world_axes_transform[3][3] = object_to_world[3][3];

  /* Apply global scale transform. */
  mul_v3_fl(world_axes_transform[0], export_params.global_scale);
  mul_v3_fl(world_axes_transform[1], export_params.global_scale);
  mul_v3_fl(world_axes_transform[2], export_params.global_scale);
  mul_v3_fl(world_axes_transform[3], export_params.global_scale);

  return world_axes_transform;
}

void OBJWriter::write_nurbs_curve(FormatHandler &fh, const IOBJCurve &obj_nurbs_data) const
{
  const int total_splines = obj_nurbs_data.total_splines();
  for (int spline_idx = 0; spline_idx < total_splines; spline_idx++) {
    /* Double check no surface is passed in as they are no supported (this is filtered when parsed)
     */
    BLI_assert(obj_nurbs_data.num_control_points_v(spline_idx) == 1);

    const float4x4 world_axes_transform = compute_world_axes_transform(
        export_params_, obj_nurbs_data.object_transform());

    const char *nurbs_name = obj_nurbs_data.get_curve_name();
    const int degree_u = obj_nurbs_data.get_nurbs_degree_u(spline_idx);
    fh.write_obj_group(nurbs_name);
    fh.write_obj_cstype();
    fh.write_obj_nurbs_degree(degree_u);

    const int num_points_u = obj_nurbs_data.num_control_points_u(spline_idx);

    Vector<float> knot_buffer;
    Span<float> knots_u = obj_nurbs_data.get_knots_u(spline_idx, knot_buffer);
    IndexRange point_range(0, num_points_u);
    knots_u = valid_nurb_control_point_range(degree_u + 1, knots_u, point_range);

    /* Write coords */
    Vector<float3> dynamic_point_buffer;
    Span<float3> vertex_coords = obj_nurbs_data.vertex_coordinates(spline_idx,
                                                                   dynamic_point_buffer);

    /* Write only unique points. */
    IndexRange point_loop_range = point_range.size() > vertex_coords.size() ?
                                      point_range.drop_back(point_range.size() -
                                                            vertex_coords.size()) :
                                      point_range;
    for (const int64_t index : point_loop_range) {
      /* Modulo will loop back to the 0:th point, not the start of the point range! */
      float3 co = vertex_coords[index % vertex_coords.size()];
      mul_m4_v3(world_axes_transform.ptr(), &co.x);
      fh.write_obj_vertex(co[0], co[1], co[2]);
    }

    fh.write_obj_curve_begin();
    fh.write_obj_nurbs_parm(knots_u[degree_u]);
    fh.write_obj_nurbs_parm(knots_u.last(degree_u));

    /* Loop over the [0, N) range, not its actual interval [x, N + x).
     * For cyclic curves, up to [0, order) will be repeated.
     */
    for (int64_t index : point_range.index_range()) {
      /* Write one based (1 ==> coords[0]) relative/negative indices.
       * TODO: Write positive indices...
       */
      index = -(point_loop_range.size() - (index % point_loop_range.size()));
      fh.write_obj_face_v(index);
    }
    fh.write_obj_curve_end();

    /* Write knot vector. */
    fh.write_obj_nurbs_parm_begin();
    for (const float &u : knots_u) {
      fh.write_obj_nurbs_parm(u);
    }
    fh.write_obj_nurbs_parm_end();
    fh.write_obj_nurbs_group_end();
  }
}

/* -------------------------------------------------------------------- */
/** \name `.MTL` writers.
 * \{ */

static const char *tex_map_type_to_string[] = {
    "map_Kd",
    "map_Pm",
    "map_Ks",
    "map_Ns",
    "map_Pr",
    "map_Ps",
    "map_refl",
    "map_Ke",
    "map_d",
    "map_Bump",
};
BLI_STATIC_ASSERT(ARRAY_SIZE(tex_map_type_to_string) == int(MTLTexMapType::Count),
                  "array size mismatch");

/**
 * Convert #float3 to string of space-separated numbers, with no leading or trailing space.
 * Only to be used in NON-performance-critical code.
 */
static std::string float3_to_string(const float3 &numbers)
{
  return fmt::format("{} {} {}", numbers[0], numbers[1], numbers[2]);
}

MTLWriter::MTLWriter(const char *obj_filepath, bool write_file) noexcept(false)
{
  if (!write_file) {
    return;
  }
  char mtl_path[FILE_MAX];
  STRNCPY(mtl_path, obj_filepath);

  const bool ok = BLI_path_extension_replace(mtl_path, sizeof(mtl_path), ".mtl");
  if (!ok) {
    throw std::system_error(ENAMETOOLONG, std::system_category(), "");
  }

  mtl_filepath_ = mtl_path;
  outfile_ = BLI_fopen(mtl_filepath_.c_str(), "wb");
  if (!outfile_) {
    throw std::system_error(errno, std::system_category(), "Cannot open file " + mtl_filepath_);
  }
}
MTLWriter::~MTLWriter()
{
  if (outfile_) {
    fmt_handler_.write_to_file(outfile_);
    if (std::fclose(outfile_)) {
      CLOG_ERROR(&LOG,
                 "Error: could not close file '%s' properly, it may be corrupted.",
                 mtl_filepath_.c_str());
    }
  }
}

void MTLWriter::write_header(const char *blen_filepath)
{
  using namespace std::string_literals;
  const char *blen_basename = (blen_filepath && blen_filepath[0] != '\0') ?
                                  BLI_path_basename(blen_filepath) :
                                  "None";
  fmt_handler_.write_string("# Blender "s + BKE_blender_version_string() + " MTL File: '" +
                            blen_basename + "'");
  fmt_handler_.write_string("# www.blender.org");
}

StringRefNull MTLWriter::mtl_file_path() const
{
  return mtl_filepath_;
}

void MTLWriter::write_bsdf_properties(const MTLMaterial &mtl, bool write_pbr)
{
  /* For various material properties, we only capture information
   * coming from the texture, or the default value of the socket.
   * When the texture is present, do not emit the default value. */

  /* Do not write Ns & Ka when writing in PBR mode. */
  if (!write_pbr) {
    if (!mtl.tex_map_of_type(MTLTexMapType::SpecularExponent).is_valid()) {
      fmt_handler_.write_mtl_float("Ns", mtl.spec_exponent);
    }
    fmt_handler_.write_mtl_float3(
        "Ka", mtl.ambient_color.x, mtl.ambient_color.y, mtl.ambient_color.z);
  }
  if (!mtl.tex_map_of_type(MTLTexMapType::Color).is_valid()) {
    fmt_handler_.write_mtl_float3("Kd", mtl.color.x, mtl.color.y, mtl.color.z);
  }
  if (!mtl.tex_map_of_type(MTLTexMapType::Specular).is_valid()) {
    fmt_handler_.write_mtl_float3("Ks", mtl.spec_color.x, mtl.spec_color.y, mtl.spec_color.z);
  }
  if (!mtl.tex_map_of_type(MTLTexMapType::Emission).is_valid()) {
    fmt_handler_.write_mtl_float3(
        "Ke", mtl.emission_color.x, mtl.emission_color.y, mtl.emission_color.z);
  }
  fmt_handler_.write_mtl_float("Ni", mtl.ior);
  if (!mtl.tex_map_of_type(MTLTexMapType::Alpha).is_valid()) {
    fmt_handler_.write_mtl_float("d", mtl.alpha);
  }
  fmt_handler_.write_mtl_illum(mtl.illum_mode);

  if (write_pbr) {
    if (!mtl.tex_map_of_type(MTLTexMapType::Roughness).is_valid() && mtl.roughness >= 0.0f) {
      fmt_handler_.write_mtl_float("Pr", mtl.roughness);
    }
    if (!mtl.tex_map_of_type(MTLTexMapType::Metallic).is_valid() && mtl.metallic >= 0.0f) {
      fmt_handler_.write_mtl_float("Pm", mtl.metallic);
    }
    if (!mtl.tex_map_of_type(MTLTexMapType::Sheen).is_valid() && mtl.sheen >= 0.0f) {
      fmt_handler_.write_mtl_float("Ps", mtl.sheen);
    }
    if (mtl.cc_thickness >= 0.0f) {
      fmt_handler_.write_mtl_float("Pc", mtl.cc_thickness);
    }
    if (mtl.cc_roughness >= 0.0f) {
      fmt_handler_.write_mtl_float("Pcr", mtl.cc_roughness);
    }
    if (mtl.aniso >= 0.0f) {
      fmt_handler_.write_mtl_float("aniso", mtl.aniso);
    }
    if (mtl.aniso_rot >= 0.0f) {
      fmt_handler_.write_mtl_float("anisor", mtl.aniso_rot);
    }
    if (mtl.transmit_color.x > 0.0f || mtl.transmit_color.y > 0.0f || mtl.transmit_color.z > 0.0f)
    {
      fmt_handler_.write_mtl_float3(
          "Tf", mtl.transmit_color.x, mtl.transmit_color.y, mtl.transmit_color.z);
    }
  }
}

void MTLWriter::write_texture_map(const MTLMaterial &mtl_material,
                                  MTLTexMapType texture_key,
                                  const MTLTexMap &texture_map,
                                  const char *blen_filedir,
                                  const char *dest_dir,
                                  ePathReferenceMode path_mode,
                                  Set<std::pair<std::string, std::string>> &copy_set)
{
  std::string options;
  /* Option strings should have their own leading spaces. */
  if (texture_map.translation != float3{0.0f, 0.0f, 0.0f}) {
    options.append(" -o ").append(float3_to_string(texture_map.translation));
  }
  if (texture_map.scale != float3{1.0f, 1.0f, 1.0f}) {
    options.append(" -s ").append(float3_to_string(texture_map.scale));
  }
  if (texture_key == MTLTexMapType::Normal && mtl_material.normal_strength > 0.0001f) {
    options.append(" -bm ").append(std::to_string(mtl_material.normal_strength));
  }

  std::string path = path_reference(
      texture_map.image_path.c_str(), blen_filedir, dest_dir, path_mode, &copy_set);
  /* Always emit forward slashes for cross-platform compatibility. */
  std::replace(path.begin(), path.end(), '\\', '/');

  fmt_handler_.write_mtl_map(tex_map_type_to_string[int(texture_key)], options, path);
}

static bool is_pbr_map(MTLTexMapType type)
{
  return type == MTLTexMapType::Metallic || type == MTLTexMapType::Roughness ||
         type == MTLTexMapType::Sheen;
}

static bool is_non_pbr_map(MTLTexMapType type)
{
  return type == MTLTexMapType::SpecularExponent || type == MTLTexMapType::Reflection;
}

void MTLWriter::write_materials(const char *blen_filepath,
                                ePathReferenceMode path_mode,
                                const char *dest_dir,
                                bool write_pbr)
{
  if (mtlmaterials_.is_empty()) {
    return;
  }

  char blen_filedir[FILE_MAX];
  BLI_path_split_dir_part(blen_filepath, blen_filedir, sizeof(blen_filedir));
  BLI_path_slash_native(blen_filedir);
  BLI_path_normalize(blen_filedir);

  std::sort(mtlmaterials_.begin(),
            mtlmaterials_.end(),
            [](const MTLMaterial &a, const MTLMaterial &b) { return a.name < b.name; });
  Set<std::pair<std::string, std::string>> copy_set;
  for (const MTLMaterial &mtlmat : mtlmaterials_) {
    fmt_handler_.write_string("");
    fmt_handler_.write_mtl_newmtl(mtlmat.name);
    write_bsdf_properties(mtlmat, write_pbr);
    for (int key = 0; key < int(MTLTexMapType::Count); key++) {
      const MTLTexMap &tex = mtlmat.texture_maps[key];
      if (!tex.is_valid()) {
        continue;
      }
      if (!write_pbr && is_pbr_map((MTLTexMapType)key)) {
        continue;
      }
      if (write_pbr && is_non_pbr_map((MTLTexMapType)key)) {
        continue;
      }
      write_texture_map(
          mtlmat, (MTLTexMapType)key, tex, blen_filedir, dest_dir, path_mode, copy_set);
    }
  }
  path_reference_copy(copy_set);
}

Vector<int> MTLWriter::add_materials(const OBJMesh &mesh_to_export)
{
  Vector<int> mtl_indices;
  mtl_indices.resize(mesh_to_export.tot_materials());
  for (int16_t i = 0; i < mesh_to_export.tot_materials(); i++) {
    const Material *material = mesh_to_export.materials[i];
    if (!material) {
      mtl_indices[i] = -1;
      continue;
    }
    int mtlmat_index = material_map_.lookup_default(material, -1);
    if (mtlmat_index != -1) {
      mtl_indices[i] = mtlmat_index;
    }
    else {
      mtlmaterials_.append(mtlmaterial_for_material(material));
      mtl_indices[i] = mtlmaterials_.size() - 1;
      material_map_.add_new(material, mtl_indices[i]);
    }
  }
  return mtl_indices;
}

const char *MTLWriter::mtlmaterial_name(int index)
{
  if (index < 0 || index >= mtlmaterials_.size()) {
    return nullptr;
  }
  return mtlmaterials_[index].name.c_str();
}
/** \} */

}  // namespace blender::io::obj
