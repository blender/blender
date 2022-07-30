/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <algorithm>
#include <cstdio>

#include "BKE_blender_version.h"
#include "BKE_geometry_set.hh"

#include "BLI_color.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_path_util.h"
#include "BLI_task.hh"

#include "IO_path_util.hh"

#include "obj_export_mesh.hh"
#include "obj_export_mtl.hh"
#include "obj_export_nurbs.hh"

#include "obj_export_file_writer.hh"

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

void OBJWriter::write_vert_uv_normal_indices(FormatHandler<eFileType::OBJ> &fh,
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
  fh.write<eOBJSyntaxElement::poly_element_begin>();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write<eOBJSyntaxElement::vertex_uv_normal_indices>(vert_indices[j] + vertex_offset,
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
      fh.write<eOBJSyntaxElement::vertex_uv_normal_indices>(vert_indices[j] + vertex_offset,
                                                            uv_indices[j] + uv_offset,
                                                            normal_indices[j] + normal_offset);
    }
  }
  fh.write<eOBJSyntaxElement::poly_element_end>();
}

void OBJWriter::write_vert_normal_indices(FormatHandler<eFileType::OBJ> &fh,
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
  fh.write<eOBJSyntaxElement::poly_element_begin>();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write<eOBJSyntaxElement::vertex_normal_indices>(vert_indices[j] + vertex_offset,
                                                         normal_indices[j] + normal_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write<eOBJSyntaxElement::vertex_normal_indices>(vert_indices[j] + vertex_offset,
                                                         normal_indices[j] + normal_offset);
    }
  }
  fh.write<eOBJSyntaxElement::poly_element_end>();
}

void OBJWriter::write_vert_uv_indices(FormatHandler<eFileType::OBJ> &fh,
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
  fh.write<eOBJSyntaxElement::poly_element_begin>();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write<eOBJSyntaxElement::vertex_uv_indices>(vert_indices[j] + vertex_offset,
                                                     uv_indices[j] + uv_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write<eOBJSyntaxElement::vertex_uv_indices>(vert_indices[j] + vertex_offset,
                                                     uv_indices[j] + uv_offset);
    }
  }
  fh.write<eOBJSyntaxElement::poly_element_end>();
}

void OBJWriter::write_vert_indices(FormatHandler<eFileType::OBJ> &fh,
                                   const IndexOffsets &offsets,
                                   Span<int> vert_indices,
                                   Span<int> /*uv_indices*/,
                                   Span<int> /*normal_indices*/,
                                   bool flip) const
{
  const int vertex_offset = offsets.vertex_offset + 1;
  const int n = vert_indices.size();
  fh.write<eOBJSyntaxElement::poly_element_begin>();
  if (!flip) {
    for (int j = 0; j < n; ++j) {
      fh.write<eOBJSyntaxElement::vertex_indices>(vert_indices[j] + vertex_offset);
    }
  }
  else {
    for (int k = 0; k < n; ++k) {
      int j = k == 0 ? 0 : n - k;
      fh.write<eOBJSyntaxElement::vertex_indices>(vert_indices[j] + vertex_offset);
    }
  }
  fh.write<eOBJSyntaxElement::poly_element_end>();
}

void OBJWriter::write_header() const
{
  using namespace std::string_literals;
  FormatHandler<eFileType::OBJ> fh;
  fh.write<eOBJSyntaxElement::string>("# Blender "s + BKE_blender_version_string() + "\n");
  fh.write<eOBJSyntaxElement::string>("# www.blender.org\n");
  fh.write_to_file(outfile_);
}

void OBJWriter::write_mtllib_name(const StringRefNull mtl_filepath) const
{
  /* Split .MTL file path into parent directory and filename. */
  char mtl_file_name[FILE_MAXFILE];
  char mtl_dir_name[FILE_MAXDIR];
  BLI_split_dirfile(mtl_filepath.data(), mtl_dir_name, mtl_file_name, FILE_MAXDIR, FILE_MAXFILE);
  FormatHandler<eFileType::OBJ> fh;
  fh.write<eOBJSyntaxElement::mtllib>(mtl_file_name);
  fh.write_to_file(outfile_);
}

void OBJWriter::write_object_name(FormatHandler<eFileType::OBJ> &fh,
                                  const OBJMesh &obj_mesh_data) const
{
  const char *object_name = obj_mesh_data.get_object_name();
  if (export_params_.export_object_groups) {
    const std::string object_name = obj_mesh_data.get_object_name();
    const char *mesh_name = obj_mesh_data.get_object_mesh_name();
    fh.write<eOBJSyntaxElement::object_group>(object_name + "_" + mesh_name);
    return;
  }
  fh.write<eOBJSyntaxElement::object_name>(object_name);
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
void obj_parallel_chunked_output(FormatHandler<eFileType::OBJ> &fh,
                                 int tot_count,
                                 const Function &function)
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
  std::vector<FormatHandler<eFileType::OBJ>> buffers(chunk_count);
  blender::threading::parallel_for(IndexRange(chunk_count), 1, [&](IndexRange range) {
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

void OBJWriter::write_vertex_coords(FormatHandler<eFileType::OBJ> &fh,
                                    const OBJMesh &obj_mesh_data,
                                    bool write_colors) const
{
  const int tot_count = obj_mesh_data.tot_vertices();

  Mesh *mesh = obj_mesh_data.get_mesh();
  CustomDataLayer *colors_layer = nullptr;
  if (write_colors) {
    colors_layer = BKE_id_attributes_active_color_get(&mesh->id);
  }
  if (write_colors && (colors_layer != nullptr)) {
    const bke::AttributeAccessor attributes = bke::mesh_attributes(*mesh);
    const VArray<ColorGeometry4f> attribute = attributes.lookup_or_default<ColorGeometry4f>(
        colors_layer->name, ATTR_DOMAIN_POINT, {0.0f, 0.0f, 0.0f, 0.0f});

    BLI_assert(tot_count == attribute.size());
    obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler<eFileType::OBJ> &buf, int i) {
      float3 vertex = obj_mesh_data.calc_vertex_coords(i, export_params_.scaling_factor);
      ColorGeometry4f linear = attribute.get(i);
      float srgb[3];
      linearrgb_to_srgb_v3_v3(srgb, linear);
      buf.write<eOBJSyntaxElement::vertex_coords_color>(
          vertex[0], vertex[1], vertex[2], srgb[0], srgb[1], srgb[2]);
    });
  }
  else {
    obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler<eFileType::OBJ> &buf, int i) {
      float3 vertex = obj_mesh_data.calc_vertex_coords(i, export_params_.scaling_factor);
      buf.write<eOBJSyntaxElement::vertex_coords>(vertex[0], vertex[1], vertex[2]);
    });
  }
}

void OBJWriter::write_uv_coords(FormatHandler<eFileType::OBJ> &fh, OBJMesh &r_obj_mesh_data) const
{
  const Vector<float2> &uv_coords = r_obj_mesh_data.get_uv_coords();
  const int tot_count = uv_coords.size();
  obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler<eFileType::OBJ> &buf, int i) {
    const float2 &uv_vertex = uv_coords[i];
    buf.write<eOBJSyntaxElement::uv_vertex_coords>(uv_vertex[0], uv_vertex[1]);
  });
}

void OBJWriter::write_poly_normals(FormatHandler<eFileType::OBJ> &fh, OBJMesh &obj_mesh_data)
{
  /* Poly normals should be calculated earlier via store_normal_coords_and_indices. */
  const Vector<float3> &normal_coords = obj_mesh_data.get_normal_coords();
  const int tot_count = normal_coords.size();
  obj_parallel_chunked_output(fh, tot_count, [&](FormatHandler<eFileType::OBJ> &buf, int i) {
    const float3 &normal = normal_coords[i];
    buf.write<eOBJSyntaxElement::normal>(normal[0], normal[1], normal[2]);
  });
}

OBJWriter::func_vert_uv_normal_indices OBJWriter::get_poly_element_writer(
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

static int get_smooth_group(const OBJMesh &mesh, const OBJExportParams &params, int poly_idx)
{
  if (poly_idx < 0) {
    return NEGATIVE_INIT;
  }
  int group = SMOOTH_GROUP_DISABLED;
  if (mesh.is_ith_poly_smooth(poly_idx)) {
    group = !params.export_smooth_groups ? SMOOTH_GROUP_DEFAULT : mesh.ith_smooth_group(poly_idx);
  }
  return group;
}

void OBJWriter::write_poly_elements(FormatHandler<eFileType::OBJ> &fh,
                                    const IndexOffsets &offsets,
                                    const OBJMesh &obj_mesh_data,
                                    std::function<const char *(int)> matname_fn)
{
  const func_vert_uv_normal_indices poly_element_writer = get_poly_element_writer(
      obj_mesh_data.tot_uv_vertices());

  const int tot_polygons = obj_mesh_data.tot_polygons();
  const int tot_deform_groups = obj_mesh_data.tot_deform_groups();
  threading::EnumerableThreadSpecific<Vector<float>> group_weights;

  obj_parallel_chunked_output(fh, tot_polygons, [&](FormatHandler<eFileType::OBJ> &buf, int idx) {
    /* Polygon order for writing into the file is not necessarily the same
     * as order in the mesh; it will be sorted by material indices. Remap current
     * and previous indices here according to the order. */
    int prev_i = obj_mesh_data.remap_poly_index(idx - 1);
    int i = obj_mesh_data.remap_poly_index(idx);

    Vector<int> poly_vertex_indices = obj_mesh_data.calc_poly_vertex_indices(i);
    Span<int> poly_uv_indices = obj_mesh_data.calc_poly_uv_indices(i);
    Vector<int> poly_normal_indices = obj_mesh_data.calc_poly_normal_indices(i);

    /* Write smoothing group if different from previous. */
    {
      const int prev_group = get_smooth_group(obj_mesh_data, export_params_, prev_i);
      const int group = get_smooth_group(obj_mesh_data, export_params_, i);
      if (group != prev_group) {
        buf.write<eOBJSyntaxElement::smooth_group>(group);
      }
    }

    /* Write vertex group if different from previous. */
    if (export_params_.export_vertex_groups) {
      Vector<float> &local_weights = group_weights.local();
      local_weights.resize(tot_deform_groups);
      const int16_t prev_group = idx == 0 ? NEGATIVE_INIT :
                                            obj_mesh_data.get_poly_deform_group_index(
                                                prev_i, local_weights);
      const int16_t group = obj_mesh_data.get_poly_deform_group_index(i, local_weights);
      if (group != prev_group) {
        buf.write<eOBJSyntaxElement::object_group>(
            group == NOT_FOUND ? DEFORM_GROUP_DISABLED :
                                 obj_mesh_data.get_poly_deform_group_name(group));
      }
    }

    /* Write material name and material group if different from previous. */
    if (export_params_.export_materials && obj_mesh_data.tot_materials() > 0) {
      const int16_t prev_mat = idx == 0 ? NEGATIVE_INIT : obj_mesh_data.ith_poly_matnr(prev_i);
      const int16_t mat = obj_mesh_data.ith_poly_matnr(i);
      if (mat != prev_mat) {
        if (mat == NOT_FOUND) {
          buf.write<eOBJSyntaxElement::poly_usemtl>(MATERIAL_GROUP_DISABLED);
        }
        else {
          const char *mat_name = matname_fn(mat);
          if (!mat_name) {
            mat_name = MATERIAL_GROUP_DISABLED;
          }
          if (export_params_.export_material_groups) {
            const std::string object_name = obj_mesh_data.get_object_name();
            fh.write<eOBJSyntaxElement::object_group>(object_name + "_" + mat_name);
          }
          buf.write<eOBJSyntaxElement::poly_usemtl>(mat_name);
        }
      }
    }

    /* Write polygon elements. */
    (this->*poly_element_writer)(buf,
                                 offsets,
                                 poly_vertex_indices,
                                 poly_uv_indices,
                                 poly_normal_indices,
                                 obj_mesh_data.is_mirrored_transform());
  });
}

void OBJWriter::write_edges_indices(FormatHandler<eFileType::OBJ> &fh,
                                    const IndexOffsets &offsets,
                                    const OBJMesh &obj_mesh_data) const
{
  /* NOTE: ensure_mesh_edges should be called before. */
  const int tot_edges = obj_mesh_data.tot_edges();
  for (int edge_index = 0; edge_index < tot_edges; edge_index++) {
    const std::optional<std::array<int, 2>> vertex_indices =
        obj_mesh_data.calc_loose_edge_vert_indices(edge_index);
    if (!vertex_indices) {
      continue;
    }
    fh.write<eOBJSyntaxElement::edge>((*vertex_indices)[0] + offsets.vertex_offset + 1,
                                      (*vertex_indices)[1] + offsets.vertex_offset + 1);
  }
}

void OBJWriter::write_nurbs_curve(FormatHandler<eFileType::OBJ> &fh,
                                  const OBJCurve &obj_nurbs_data) const
{
  const int total_splines = obj_nurbs_data.total_splines();
  for (int spline_idx = 0; spline_idx < total_splines; spline_idx++) {
    const int total_vertices = obj_nurbs_data.total_spline_vertices(spline_idx);
    for (int vertex_idx = 0; vertex_idx < total_vertices; vertex_idx++) {
      const float3 vertex_coords = obj_nurbs_data.vertex_coordinates(
          spline_idx, vertex_idx, export_params_.scaling_factor);
      fh.write<eOBJSyntaxElement::vertex_coords>(
          vertex_coords[0], vertex_coords[1], vertex_coords[2]);
    }

    const char *nurbs_name = obj_nurbs_data.get_curve_name();
    const int nurbs_degree = obj_nurbs_data.get_nurbs_degree(spline_idx);
    fh.write<eOBJSyntaxElement::object_group>(nurbs_name);
    fh.write<eOBJSyntaxElement::cstype>();
    fh.write<eOBJSyntaxElement::nurbs_degree>(nurbs_degree);
    /**
     * The numbers written here are indices into the vertex coordinates written
     * earlier, relative to the line that is going to be written.
     * [0.0 - 1.0] is the curve parameter range.
     * 0.0 1.0 -1 -2 -3 -4 for a non-cyclic curve with 4 vertices.
     * 0.0 1.0 -1 -2 -3 -4 -1 -2 -3 for a cyclic curve with 4 vertices.
     */
    const int total_control_points = obj_nurbs_data.total_spline_control_points(spline_idx);
    fh.write<eOBJSyntaxElement::curve_element_begin>();
    for (int i = 0; i < total_control_points; i++) {
      /* "+1" to keep indices one-based, even if they're negative: i.e., -1 refers to the
       * last vertex coordinate, -2 second last. */
      fh.write<eOBJSyntaxElement::vertex_indices>(-((i % total_vertices) + 1));
    }
    fh.write<eOBJSyntaxElement::curve_element_end>();

    /**
     * In `parm u 0 0.1 ..` line:, (total control points + 2) equidistant numbers in the
     * parameter range are inserted. However for curves with endpoint flag,
     * first degree+1 numbers are zeroes, and last degree+1 numbers are ones
     */

    const short flagsu = obj_nurbs_data.get_nurbs_flagu(spline_idx);
    const bool cyclic = flagsu & CU_NURB_CYCLIC;
    const bool endpoint = !cyclic && (flagsu & CU_NURB_ENDPOINT);
    fh.write<eOBJSyntaxElement::nurbs_parameter_begin>();
    for (int i = 1; i <= total_control_points + 2; i++) {
      float parm = 1.0f * i / (total_control_points + 2 + 1);
      if (endpoint) {
        if (i <= nurbs_degree) {
          parm = 0;
        }
        else if (i > total_control_points + 2 - nurbs_degree) {
          parm = 1;
        }
      }
      fh.write<eOBJSyntaxElement::nurbs_parameters>(parm);
    }
    fh.write<eOBJSyntaxElement::nurbs_parameter_end>();

    fh.write<eOBJSyntaxElement::nurbs_group_end>();
  }
}

/* -------------------------------------------------------------------- */
/** \name .MTL writers.
 * \{ */

/**
 * Convert #float3 to string of space-separated numbers, with no leading or trailing space.
 * Only to be used in NON-performance-critical code.
 */
static std::string float3_to_string(const float3 &numbers)
{
  std::ostringstream r_string;
  r_string << numbers[0] << " " << numbers[1] << " " << numbers[2];
  return r_string.str();
};

MTLWriter::MTLWriter(const char *obj_filepath) noexcept(false)
{
  mtl_filepath_ = obj_filepath;
  const bool ok = BLI_path_extension_replace(mtl_filepath_.data(), FILE_MAX, ".mtl");
  if (!ok) {
    throw std::system_error(ENAMETOOLONG, std::system_category(), "");
  }
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
      std::cerr << "Error: could not close the file '" << mtl_filepath_
                << "' properly, it may be corrupted." << std::endl;
    }
  }
}

void MTLWriter::write_header(const char *blen_filepath)
{
  using namespace std::string_literals;
  const char *blen_basename = (blen_filepath && blen_filepath[0] != '\0') ?
                                  BLI_path_basename(blen_filepath) :
                                  "None";
  fmt_handler_.write<eMTLSyntaxElement::string>("# Blender "s + BKE_blender_version_string() +
                                                " MTL File: '" + blen_basename + "'\n");
  fmt_handler_.write<eMTLSyntaxElement::string>("# www.blender.org\n");
}

StringRefNull MTLWriter::mtl_file_path() const
{
  return mtl_filepath_;
}

void MTLWriter::write_bsdf_properties(const MTLMaterial &mtl_material)
{
  fmt_handler_.write<eMTLSyntaxElement::Ns>(mtl_material.Ns);
  fmt_handler_.write<eMTLSyntaxElement::Ka>(
      mtl_material.Ka.x, mtl_material.Ka.y, mtl_material.Ka.z);
  fmt_handler_.write<eMTLSyntaxElement::Kd>(
      mtl_material.Kd.x, mtl_material.Kd.y, mtl_material.Kd.z);
  fmt_handler_.write<eMTLSyntaxElement::Ks>(
      mtl_material.Ks.x, mtl_material.Ks.y, mtl_material.Ks.z);
  fmt_handler_.write<eMTLSyntaxElement::Ke>(
      mtl_material.Ke.x, mtl_material.Ke.y, mtl_material.Ke.z);
  fmt_handler_.write<eMTLSyntaxElement::Ni>(mtl_material.Ni);
  fmt_handler_.write<eMTLSyntaxElement::d>(mtl_material.d);
  fmt_handler_.write<eMTLSyntaxElement::illum>(mtl_material.illum);
}

void MTLWriter::write_texture_map(
    const MTLMaterial &mtl_material,
    const Map<const eMTLSyntaxElement, tex_map_XX>::Item &texture_map,
    const char *blen_filedir,
    const char *dest_dir,
    ePathReferenceMode path_mode,
    Set<std::pair<std::string, std::string>> &copy_set)
{
  std::string options;
  /* Option strings should have their own leading spaces. */
  if (texture_map.value.translation != float3{0.0f, 0.0f, 0.0f}) {
    options.append(" -o ").append(float3_to_string(texture_map.value.translation));
  }
  if (texture_map.value.scale != float3{1.0f, 1.0f, 1.0f}) {
    options.append(" -s ").append(float3_to_string(texture_map.value.scale));
  }
  if (texture_map.key == eMTLSyntaxElement::map_Bump && mtl_material.map_Bump_strength > 0.0001f) {
    options.append(" -bm ").append(std::to_string(mtl_material.map_Bump_strength));
  }

#define SYNTAX_DISPATCH(eMTLSyntaxElement) \
  if (texture_map.key == eMTLSyntaxElement) { \
    std::string path = path_reference( \
        texture_map.value.image_path.c_str(), blen_filedir, dest_dir, path_mode, &copy_set); \
    /* Always emit forward slashes for cross-platform compatibility. */ \
    std::replace(path.begin(), path.end(), '\\', '/'); \
    fmt_handler_.write<eMTLSyntaxElement>(options, path.c_str()); \
    return; \
  }

  SYNTAX_DISPATCH(eMTLSyntaxElement::map_Kd);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_Ks);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_Ns);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_d);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_refl);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_Ke);
  SYNTAX_DISPATCH(eMTLSyntaxElement::map_Bump);

  BLI_assert(!"This map type was not written to the file.");
}

void MTLWriter::write_materials(const char *blen_filepath,
                                ePathReferenceMode path_mode,
                                const char *dest_dir)
{
  if (mtlmaterials_.size() == 0) {
    return;
  }

  char blen_filedir[PATH_MAX];
  BLI_split_dir_part(blen_filepath, blen_filedir, PATH_MAX);
  BLI_path_slash_native(blen_filedir);
  BLI_path_normalize(nullptr, blen_filedir);

  std::sort(mtlmaterials_.begin(),
            mtlmaterials_.end(),
            [](const MTLMaterial &a, const MTLMaterial &b) { return a.name < b.name; });
  Set<std::pair<std::string, std::string>> copy_set;
  for (const MTLMaterial &mtlmat : mtlmaterials_) {
    fmt_handler_.write<eMTLSyntaxElement::string>("\n");
    fmt_handler_.write<eMTLSyntaxElement::newmtl>(mtlmat.name);
    write_bsdf_properties(mtlmat);
    for (const auto &tex : mtlmat.texture_maps.items()) {
      if (tex.value.image_path.empty()) {
        continue;
      }
      write_texture_map(mtlmat, tex, blen_filedir, dest_dir, path_mode, copy_set);
    }
  }
  path_reference_copy(copy_set);
}

Vector<int> MTLWriter::add_materials(const OBJMesh &mesh_to_export)
{
  Vector<int> r_mtl_indices;
  r_mtl_indices.resize(mesh_to_export.tot_materials());
  for (int16_t i = 0; i < mesh_to_export.tot_materials(); i++) {
    const Material *material = mesh_to_export.get_object_material(i);
    if (!material) {
      r_mtl_indices[i] = -1;
      continue;
    }
    int mtlmat_index = material_map_.lookup_default(material, -1);
    if (mtlmat_index != -1) {
      r_mtl_indices[i] = mtlmat_index;
    }
    else {
      mtlmaterials_.append(mtlmaterial_for_material(material));
      r_mtl_indices[i] = mtlmaterials_.size() - 1;
      material_map_.add_new(material, r_mtl_indices[i]);
    }
  }
  return r_mtl_indices;
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
