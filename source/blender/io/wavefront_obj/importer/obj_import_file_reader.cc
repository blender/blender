/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_map.hh"
#include "BLI_math_color.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "obj_import_file_reader.hh"
#include "obj_import_string_utils.hh"

#include <charconv>

namespace blender::io::obj {

using std::string;

/**
 * Based on the properties of the given Geometry instance, create a new Geometry instance
 * or return the previous one.
 *
 * Also update index offsets which should always happen if a new Geometry instance is created.
 */
static Geometry *create_geometry(Geometry *const prev_geometry,
                                 const eGeometryType new_type,
                                 StringRef name,
                                 const GlobalVertices &global_vertices,
                                 Vector<std::unique_ptr<Geometry>> &r_all_geometries,
                                 VertexIndexOffset &r_offset)
{
  auto new_geometry = [&]() {
    r_all_geometries.append(std::make_unique<Geometry>());
    Geometry *g = r_all_geometries.last().get();
    g->geom_type_ = new_type;
    g->geometry_name_ = name.is_empty() ? "New object" : name;
    g->vertex_start_ = global_vertices.vertices.size();
    g->vertex_color_start_ = global_vertices.vertex_colors.size();
    r_offset.set_index_offset(g->vertex_start_);
    return g;
  };

  if (prev_geometry && prev_geometry->geom_type_ == GEOM_MESH) {
    /* After the creation of a Geometry instance, at least one element has been found in the OBJ
     * file that indicates that it is a mesh (basically anything but the vertex positions). */
    if (!prev_geometry->face_elements_.is_empty() || prev_geometry->has_vertex_normals_ ||
        !prev_geometry->edges_.is_empty()) {
      return new_geometry();
    }
    if (new_type == GEOM_MESH) {
      /* A Geometry created initially with a default name now found its name. */
      prev_geometry->geometry_name_ = name;
      return prev_geometry;
    }
    if (new_type == GEOM_CURVE) {
      /* The object originally created is not a mesh now that curve data
       * follows the vertex coordinates list. */
      prev_geometry->geom_type_ = GEOM_CURVE;
      return prev_geometry;
    }
  }

  if (prev_geometry && prev_geometry->geom_type_ == GEOM_CURVE) {
    return new_geometry();
  }

  return new_geometry();
}

static void geom_add_vertex(Geometry *geom,
                            const char *p,
                            const char *end,
                            GlobalVertices &r_global_vertices)
{
  float3 vert;
  p = parse_floats(p, end, 0.0f, vert, 3);
  r_global_vertices.vertices.append(vert);
  geom->vertex_count_++;
  /* OBJ extension: `xyzrgb` vertex colors, when the vertex position
   * is followed by 3 more RGB color components. See
   * http://paulbourke.net/dataformats/obj/colour.html */
  if (p < end) {
    float3 srgb;
    p = parse_floats(p, end, -1.0f, srgb, 3);
    if (srgb.x >= 0 && srgb.y >= 0 && srgb.z >= 0) {
      float3 linear;
      srgb_to_linearrgb_v3_v3(linear, srgb);
      r_global_vertices.vertex_colors.append(linear);
      geom->vertex_color_count_++;
    }
  }
}

static void geom_add_mrgb_colors(Geometry *geom,
                                 const char *p,
                                 const char *end,
                                 GlobalVertices &r_global_vertices)
{
  /* MRGB color extension, in the form of
   * "#MRGB MMRRGGBBMMRRGGBB ..."
   * http://paulbourke.net/dataformats/obj/colour.html */
  p = drop_whitespace(p, end);
  const int mrgb_length = 8;
  while (p + mrgb_length <= end) {
    uint32_t value = 0;
    std::from_chars_result res = std::from_chars(p, p + mrgb_length, value, 16);
    if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
      return;
    }
    unsigned char srgb[4];
    srgb[0] = (value >> 16) & 0xFF;
    srgb[1] = (value >> 8) & 0xFF;
    srgb[2] = value & 0xFF;
    srgb[3] = 0xFF;
    float linear[4];
    srgb_to_linearrgb_uchar4(linear, srgb);
    r_global_vertices.vertex_colors.append({linear[0], linear[1], linear[2]});
    geom->vertex_color_count_++;
    p += mrgb_length;
  }
}

static void geom_add_vertex_normal(Geometry *geom,
                                   const char *p,
                                   const char *end,
                                   GlobalVertices &r_global_vertices)
{
  float3 normal;
  parse_floats(p, end, 0.0f, normal, 3);
  r_global_vertices.vertex_normals.append(normal);
  geom->has_vertex_normals_ = true;
}

static void geom_add_uv_vertex(const char *p, const char *end, GlobalVertices &r_global_vertices)
{
  float2 uv;
  parse_floats(p, end, 0.0f, uv, 2);
  r_global_vertices.uv_vertices.append(uv);
}

static void geom_add_edge(Geometry *geom,
                          const char *p,
                          const char *end,
                          const VertexIndexOffset &offsets,
                          GlobalVertices &r_global_vertices)
{
  int edge_v1, edge_v2;
  p = parse_int(p, end, -1, edge_v1);
  p = parse_int(p, end, -1, edge_v2);
  /* Always keep stored indices non-negative and zero-based. */
  edge_v1 += edge_v1 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  edge_v2 += edge_v2 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  BLI_assert(edge_v1 >= 0 && edge_v2 >= 0);
  geom->edges_.append({static_cast<uint>(edge_v1), static_cast<uint>(edge_v2)});
}

static void geom_add_polygon(Geometry *geom,
                             const char *p,
                             const char *end,
                             const GlobalVertices &global_vertices,
                             const VertexIndexOffset &offsets,
                             const int material_index,
                             const int group_index,
                             const bool shaded_smooth)
{
  PolyElem curr_face;
  curr_face.shaded_smooth = shaded_smooth;
  curr_face.material_index = material_index;
  if (group_index >= 0) {
    curr_face.vertex_group_index = group_index;
    geom->use_vertex_groups_ = true;
  }

  const int orig_corners_size = geom->face_corners_.size();
  curr_face.start_index_ = orig_corners_size;

  bool face_valid = true;
  p = drop_whitespace(p, end);
  while (p < end && face_valid) {
    PolyCorner corner;
    bool got_uv = false, got_normal = false;
    /* Parse vertex index. */
    p = parse_int(p, end, INT32_MAX, corner.vert_index, false);
    face_valid &= corner.vert_index != INT32_MAX;
    if (p < end && *p == '/') {
      /* Parse UV index. */
      ++p;
      if (p < end && *p != '/') {
        p = parse_int(p, end, INT32_MAX, corner.uv_vert_index, false);
        got_uv = corner.uv_vert_index != INT32_MAX;
      }
      /* Parse normal index. */
      if (p < end && *p == '/') {
        ++p;
        p = parse_int(p, end, INT32_MAX, corner.vertex_normal_index, false);
        got_normal = corner.vertex_normal_index != INT32_MAX;
      }
    }
    /* Always keep stored indices non-negative and zero-based. */
    corner.vert_index += corner.vert_index < 0 ? global_vertices.vertices.size() :
                                                 -offsets.get_index_offset() - 1;
    if (corner.vert_index < 0 || corner.vert_index >= global_vertices.vertices.size()) {
      fprintf(stderr,
              "Invalid vertex index %i (valid range [0, %zu)), ignoring face\n",
              corner.vert_index,
              (size_t)global_vertices.vertices.size());
      face_valid = false;
    }
    if (got_uv) {
      corner.uv_vert_index += corner.uv_vert_index < 0 ? global_vertices.uv_vertices.size() : -1;
      if (corner.uv_vert_index < 0 || corner.uv_vert_index >= global_vertices.uv_vertices.size()) {
        fprintf(stderr,
                "Invalid UV index %i (valid range [0, %zu)), ignoring face\n",
                corner.uv_vert_index,
                (size_t)global_vertices.uv_vertices.size());
        face_valid = false;
      }
    }
    /* Ignore corner normal index, if the geometry does not have any normals.
     * Some obj files out there do have face definitions that refer to normal indices,
     * without any normals being present (T98782). */
    if (got_normal && geom->has_vertex_normals_) {
      corner.vertex_normal_index += corner.vertex_normal_index < 0 ?
                                        global_vertices.vertex_normals.size() :
                                        -1;
      if (corner.vertex_normal_index < 0 ||
          corner.vertex_normal_index >= global_vertices.vertex_normals.size()) {
        fprintf(stderr,
                "Invalid normal index %i (valid range [0, %zu)), ignoring face\n",
                corner.vertex_normal_index,
                (size_t)global_vertices.vertex_normals.size());
        face_valid = false;
      }
    }
    geom->face_corners_.append(corner);
    curr_face.corner_count_++;

    /* Skip whitespace to get to the next face corner. */
    p = drop_whitespace(p, end);
  }

  if (face_valid) {
    geom->face_elements_.append(curr_face);
    geom->total_loops_ += curr_face.corner_count_;
  }
  else {
    /* Remove just-added corners for the invalid face. */
    geom->face_corners_.resize(orig_corners_size);
    geom->has_invalid_polys_ = true;
  }
}

static Geometry *geom_set_curve_type(Geometry *geom,
                                     const char *p,
                                     const char *end,
                                     const GlobalVertices &global_vertices,
                                     const StringRef group_name,
                                     VertexIndexOffset &r_offsets,
                                     Vector<std::unique_ptr<Geometry>> &r_all_geometries)
{
  p = drop_whitespace(p, end);
  if (!StringRef(p, end).startswith("bspline")) {
    std::cerr << "Curve type not supported: '" << std::string(p, end) << "'" << std::endl;
    return geom;
  }
  geom = create_geometry(
      geom, GEOM_CURVE, group_name, global_vertices, r_all_geometries, r_offsets);
  geom->nurbs_element_.group_ = group_name;
  return geom;
}

static void geom_set_curve_degree(Geometry *geom, const char *p, const char *end)
{
  parse_int(p, end, 3, geom->nurbs_element_.degree);
}

static void geom_add_curve_vertex_indices(Geometry *geom,
                                          const char *p,
                                          const char *end,
                                          const GlobalVertices &global_vertices)
{
  /* Curve lines always have "0.0" and "1.0", skip over them. */
  float dummy[2];
  p = parse_floats(p, end, 0, dummy, 2);
  /* Parse indices. */
  while (p < end) {
    int index;
    p = parse_int(p, end, INT32_MAX, index);
    if (index == INT32_MAX) {
      return;
    }
    /* Always keep stored indices non-negative and zero-based. */
    index += index < 0 ? global_vertices.vertices.size() : -1;
    geom->nurbs_element_.curv_indices.append(index);
  }
}

static void geom_add_curve_parameters(Geometry *geom, const char *p, const char *end)
{
  p = drop_whitespace(p, end);
  if (p == end) {
    std::cerr << "Invalid OBJ curve parm line" << std::endl;
    return;
  }
  if (*p != 'u') {
    std::cerr << "OBJ curve surfaces are not supported: '" << *p << "'" << std::endl;
    return;
  }
  ++p;

  while (p < end) {
    float val;
    p = parse_float(p, end, FLT_MAX, val);
    if (val != FLT_MAX) {
      geom->nurbs_element_.parm.append(val);
    }
    else {
      std::cerr << "OBJ curve parm line has invalid number" << std::endl;
      return;
    }
  }
}

static void geom_update_group(const StringRef rest_line, std::string &r_group_name)
{
  if (rest_line.find("off") != string::npos || rest_line.find("null") != string::npos ||
      rest_line.find("default") != string::npos) {
    /* Set group for future elements like faces or curves to empty. */
    r_group_name = "";
    return;
  }
  r_group_name = rest_line;
}

static void geom_update_smooth_group(const char *p, const char *end, bool &r_state_shaded_smooth)
{
  p = drop_whitespace(p, end);
  /* Some implementations use "0" and "null" too, in addition to "off". */
  const StringRef line = StringRef(p, end);
  if (line == "0" || line.startswith("off") || line.startswith("null")) {
    r_state_shaded_smooth = false;
    return;
  }

  int smooth = 0;
  parse_int(p, end, 0, smooth);
  r_state_shaded_smooth = smooth != 0;
}

OBJParser::OBJParser(const OBJImportParams &import_params, size_t read_buffer_size = 64 * 1024)
    : import_params_(import_params), read_buffer_size_(read_buffer_size)
{
  obj_file_ = BLI_fopen(import_params_.filepath, "rb");
  if (!obj_file_) {
    fprintf(stderr, "Cannot read from OBJ file:'%s'.\n", import_params_.filepath);
    return;
  }
}

OBJParser::~OBJParser()
{
  if (obj_file_) {
    fclose(obj_file_);
  }
}

/* If line starts with keyword followed by whitespace, returns true and drops it from the line. */
static bool parse_keyword(const char *&p, const char *end, StringRef keyword)
{
  const size_t keyword_len = keyword.size();
  if (end - p < keyword_len + 1) {
    return false;
  }
  if (memcmp(p, keyword.data(), keyword_len) != 0) {
    return false;
  }
  /* Treat any ASCII control character as white-space;
   * don't use `isspace()` for performance reasons. */
  if (p[keyword_len] > ' ') {
    return false;
  }
  p += keyword_len + 1;
  return true;
}

void OBJParser::parse(Vector<std::unique_ptr<Geometry>> &r_all_geometries,
                      GlobalVertices &r_global_vertices)
{
  if (!obj_file_) {
    return;
  }

  /* Use the filename as the default name given to the initial object. */
  char ob_name[FILE_MAXFILE];
  BLI_strncpy(ob_name, BLI_path_basename(import_params_.filepath), FILE_MAXFILE);
  BLI_path_extension_replace(ob_name, FILE_MAXFILE, "");

  VertexIndexOffset offsets;
  Geometry *curr_geom = create_geometry(
      nullptr, GEOM_MESH, ob_name, r_global_vertices, r_all_geometries, offsets);

  /* State variables: once set, they remain the same for the remaining
   * elements in the object. */
  bool state_shaded_smooth = false;
  string state_group_name;
  int state_group_index = -1;
  string state_material_name;
  int state_material_index = -1;

  /* Read the input file in chunks. We need up to twice the possible chunk size,
   * to possibly store remainder of the previous input line that got broken mid-chunk. */
  Array<char> buffer(read_buffer_size_ * 2);

  size_t buffer_offset = 0;
  size_t line_number = 0;
  while (true) {
    /* Read a chunk of input from the file. */
    size_t bytes_read = fread(buffer.data() + buffer_offset, 1, read_buffer_size_, obj_file_);
    if (bytes_read == 0 && buffer_offset == 0) {
      break; /* No more data to read. */
    }

    /* Ensure buffer ends in a newline. */
    if (bytes_read < read_buffer_size_) {
      if (bytes_read == 0 || buffer[buffer_offset + bytes_read - 1] != '\n') {
        buffer[buffer_offset + bytes_read] = '\n';
        bytes_read++;
      }
    }

    size_t buffer_end = buffer_offset + bytes_read;
    if (buffer_end == 0) {
      break;
    }

    /* Find last newline. */
    size_t last_nl = buffer_end;
    while (last_nl > 0) {
      --last_nl;
      if (buffer[last_nl] == '\n') {
        if (last_nl < 1 || buffer[last_nl - 1] != '\\') {
          break;
        }
      }
    }
    if (buffer[last_nl] != '\n') {
      /* Whole line did not fit into our read buffer. Warn and exit. */
      fprintf(stderr,
              "OBJ file contains a line #%zu that is too long (max. length %zu)\n",
              line_number,
              read_buffer_size_);
      break;
    }
    ++last_nl;

    /* Parse the buffer (until last newline) that we have so far,
     * line by line. */
    StringRef buffer_str{buffer.data(), (int64_t)last_nl};
    while (!buffer_str.is_empty()) {
      StringRef line = read_next_line(buffer_str);
      const char *p = line.begin(), *end = line.end();
      p = drop_whitespace(p, end);
      ++line_number;
      if (p == end) {
        continue;
      }
      /* Most common things that start with 'v': vertices, normals, UVs. */
      if (*p == 'v') {
        if (parse_keyword(p, end, "v")) {
          geom_add_vertex(curr_geom, p, end, r_global_vertices);
        }
        else if (parse_keyword(p, end, "vn")) {
          geom_add_vertex_normal(curr_geom, p, end, r_global_vertices);
        }
        else if (parse_keyword(p, end, "vt")) {
          geom_add_uv_vertex(p, end, r_global_vertices);
        }
      }
      /* Faces. */
      else if (parse_keyword(p, end, "f")) {
        geom_add_polygon(curr_geom,
                         p,
                         end,
                         r_global_vertices,
                         offsets,
                         state_material_index,
                         state_group_index, /* TODO was wrongly material name! */
                         state_shaded_smooth);
      }
      /* Faces. */
      else if (parse_keyword(p, end, "l")) {
        geom_add_edge(curr_geom, p, end, offsets, r_global_vertices);
      }
      /* Objects. */
      else if (parse_keyword(p, end, "o")) {
        state_shaded_smooth = false;
        state_group_name = "";
        state_material_name = "";
        curr_geom = create_geometry(curr_geom,
                                    GEOM_MESH,
                                    StringRef(p, end).trim(),
                                    r_global_vertices,
                                    r_all_geometries,
                                    offsets);
      }
      /* Groups. */
      else if (parse_keyword(p, end, "g")) {
        geom_update_group(StringRef(p, end).trim(), state_group_name);
        int new_index = curr_geom->group_indices_.size();
        state_group_index = curr_geom->group_indices_.lookup_or_add(state_group_name, new_index);
        if (new_index == state_group_index) {
          curr_geom->group_order_.append(state_group_name);
        }
      }
      /* Smoothing groups. */
      else if (parse_keyword(p, end, "s")) {
        geom_update_smooth_group(p, end, state_shaded_smooth);
      }
      /* Materials and their libraries. */
      else if (parse_keyword(p, end, "usemtl")) {
        state_material_name = StringRef(p, end).trim();
        int new_mat_index = curr_geom->material_indices_.size();
        state_material_index = curr_geom->material_indices_.lookup_or_add(state_material_name,
                                                                          new_mat_index);
        if (new_mat_index == state_material_index) {
          curr_geom->material_order_.append(state_material_name);
        }
      }
      else if (parse_keyword(p, end, "mtllib")) {
        add_mtl_library(StringRef(p, end).trim());
      }
      else if (parse_keyword(p, end, "#MRGB")) {
        geom_add_mrgb_colors(curr_geom, p, end, r_global_vertices);
      }
      /* Comments. */
      else if (*p == '#') {
        /* Nothing to do. */
      }
      /* Curve related things. */
      else if (parse_keyword(p, end, "cstype")) {
        curr_geom = geom_set_curve_type(
            curr_geom, p, end, r_global_vertices, state_group_name, offsets, r_all_geometries);
      }
      else if (parse_keyword(p, end, "deg")) {
        geom_set_curve_degree(curr_geom, p, end);
      }
      else if (parse_keyword(p, end, "curv")) {
        geom_add_curve_vertex_indices(curr_geom, p, end, r_global_vertices);
      }
      else if (parse_keyword(p, end, "parm")) {
        geom_add_curve_parameters(curr_geom, p, end);
      }
      else if (StringRef(p, end).startswith("end")) {
        /* End of curve definition, nothing else to do. */
      }
      else {
        std::cout << "OBJ element not recognized: '" << std::string(p, end) << "'" << std::endl;
      }
    }

    /* We might have a line that was cut in the middle by the previous buffer;
     * copy it over for next chunk reading. */
    size_t left_size = buffer_end - last_nl;
    memmove(buffer.data(), buffer.data() + last_nl, left_size);
    buffer_offset = left_size;
  }

  add_default_mtl_library();
}

static eMTLSyntaxElement mtl_line_start_to_enum(const char *&p, const char *end)
{
  if (parse_keyword(p, end, "map_Kd")) {
    return eMTLSyntaxElement::map_Kd;
  }
  if (parse_keyword(p, end, "map_Ks")) {
    return eMTLSyntaxElement::map_Ks;
  }
  if (parse_keyword(p, end, "map_Ns")) {
    return eMTLSyntaxElement::map_Ns;
  }
  if (parse_keyword(p, end, "map_d")) {
    return eMTLSyntaxElement::map_d;
  }
  if (parse_keyword(p, end, "refl")) {
    return eMTLSyntaxElement::map_refl;
  }
  if (parse_keyword(p, end, "map_refl")) {
    return eMTLSyntaxElement::map_refl;
  }
  if (parse_keyword(p, end, "map_Ke")) {
    return eMTLSyntaxElement::map_Ke;
  }
  if (parse_keyword(p, end, "bump")) {
    return eMTLSyntaxElement::map_Bump;
  }
  if (parse_keyword(p, end, "map_Bump") || parse_keyword(p, end, "map_bump")) {
    return eMTLSyntaxElement::map_Bump;
  }
  return eMTLSyntaxElement::string;
}

static const std::pair<StringRef, int> unsupported_texture_options[] = {
    {"-blendu", 1},
    {"-blendv", 1},
    {"-boost", 1},
    {"-cc", 1},
    {"-clamp", 1},
    {"-imfchan", 1},
    {"-mm", 2},
    {"-t", 3},
    {"-texres", 1},
};

static bool parse_texture_option(const char *&p,
                                 const char *end,
                                 MTLMaterial *material,
                                 tex_map_XX &tex_map)
{
  p = drop_whitespace(p, end);
  if (parse_keyword(p, end, "-o")) {
    p = parse_floats(p, end, 0.0f, tex_map.translation, 3);
    return true;
  }
  if (parse_keyword(p, end, "-s")) {
    p = parse_floats(p, end, 1.0f, tex_map.scale, 3);
    return true;
  }
  if (parse_keyword(p, end, "-bm")) {
    p = parse_float(p, end, 1.0f, material->map_Bump_strength);
    return true;
  }
  if (parse_keyword(p, end, "-type")) {
    p = drop_whitespace(p, end);
    /* Only sphere is supported. */
    tex_map.projection_type = SHD_PROJ_SPHERE;
    const StringRef line = StringRef(p, end);
    if (!line.startswith("sphere")) {
      std::cerr << "OBJ import: only sphere MTL projection type is supported: '" << line << "'"
                << std::endl;
    }
    p = drop_non_whitespace(p, end);
    return true;
  }
  /* Check for unsupported options and skip them. */
  for (const auto &opt : unsupported_texture_options) {
    if (parse_keyword(p, end, opt.first)) {
      /* Drop the arguments. */
      for (int i = 0; i < opt.second; ++i) {
        p = drop_whitespace(p, end);
        p = drop_non_whitespace(p, end);
      }
      return true;
    }
  }

  return false;
}

static void parse_texture_map(const char *p,
                              const char *end,
                              MTLMaterial *material,
                              const char *mtl_dir_path)
{
  const StringRef line = StringRef(p, end);
  bool is_map = line.startswith("map_");
  bool is_refl = line.startswith("refl");
  bool is_bump = line.startswith("bump");
  if (!is_map && !is_refl && !is_bump) {
    return;
  }
  eMTLSyntaxElement key = mtl_line_start_to_enum(p, end);
  if (key == eMTLSyntaxElement::string || !material->texture_maps.contains(key)) {
    /* No supported texture map found. */
    std::cerr << "OBJ import: MTL texture map type not supported: '" << line << "'" << std::endl;
    return;
  }
  tex_map_XX &tex_map = material->texture_maps.lookup(key);
  tex_map.mtl_dir_path = mtl_dir_path;

  /* Parse texture map options. */
  while (parse_texture_option(p, end, material, tex_map)) {
  }

  /* What remains is the image path. */
  tex_map.image_path = StringRef(p, end).trim();
}

Span<std::string> OBJParser::mtl_libraries() const
{
  return mtl_libraries_;
}

void OBJParser::add_mtl_library(StringRef path)
{
  /* Remove any quotes from start and end (T67266, T97794). */
  if (path.size() > 2 && path.startswith("\"") && path.endswith("\"")) {
    path = path.drop_prefix(1).drop_suffix(1);
  }

  if (!mtl_libraries_.contains(path)) {
    mtl_libraries_.append(path);
  }
}

void OBJParser::add_default_mtl_library()
{
  /* Add any existing .mtl file that's with the same base name as the .obj file
   * into candidate .mtl files to search through. This is not technically following the
   * spec, but the old python importer was doing it, and there are user files out there
   * that contain "mtllib bar.mtl" for a foo.obj, and depend on finding materials
   * from foo.mtl (see T97757). */
  char mtl_file_path[FILE_MAX];
  BLI_strncpy(mtl_file_path, import_params_.filepath, sizeof(mtl_file_path));
  BLI_path_extension_replace(mtl_file_path, sizeof(mtl_file_path), ".mtl");
  if (BLI_exists(mtl_file_path)) {
    char mtl_file_base[FILE_MAX];
    BLI_split_file_part(mtl_file_path, mtl_file_base, sizeof(mtl_file_base));
    add_mtl_library(mtl_file_base);
  }
}

MTLParser::MTLParser(StringRefNull mtl_library, StringRefNull obj_filepath)
{
  char obj_file_dir[FILE_MAXDIR];
  BLI_split_dir_part(obj_filepath.data(), obj_file_dir, FILE_MAXDIR);
  BLI_path_join(mtl_file_path_, FILE_MAX, obj_file_dir, mtl_library.data(), NULL);
  BLI_split_dir_part(mtl_file_path_, mtl_dir_path_, FILE_MAXDIR);
}

void MTLParser::parse_and_store(Map<string, std::unique_ptr<MTLMaterial>> &r_materials)
{
  size_t buffer_len;
  void *buffer = BLI_file_read_text_as_mem(mtl_file_path_, 0, &buffer_len);
  if (buffer == nullptr) {
    fprintf(stderr, "OBJ import: cannot read from MTL file: '%s'\n", mtl_file_path_);
    return;
  }

  MTLMaterial *material = nullptr;

  StringRef buffer_str{(const char *)buffer, (int64_t)buffer_len};
  while (!buffer_str.is_empty()) {
    const StringRef line = read_next_line(buffer_str);
    const char *p = line.begin(), *end = line.end();
    p = drop_whitespace(p, end);
    if (p == end) {
      continue;
    }

    if (parse_keyword(p, end, "newmtl")) {
      StringRef mat_name = StringRef(p, end).trim();
      if (r_materials.contains(mat_name)) {
        material = nullptr;
      }
      else {
        material =
            r_materials.lookup_or_add(string(mat_name), std::make_unique<MTLMaterial>()).get();
      }
    }
    else if (material != nullptr) {
      if (parse_keyword(p, end, "Ns")) {
        parse_float(p, end, 324.0f, material->Ns);
      }
      else if (parse_keyword(p, end, "Ka")) {
        parse_floats(p, end, 0.0f, material->Ka, 3);
      }
      else if (parse_keyword(p, end, "Kd")) {
        parse_floats(p, end, 0.8f, material->Kd, 3);
      }
      else if (parse_keyword(p, end, "Ks")) {
        parse_floats(p, end, 0.5f, material->Ks, 3);
      }
      else if (parse_keyword(p, end, "Ke")) {
        parse_floats(p, end, 0.0f, material->Ke, 3);
      }
      else if (parse_keyword(p, end, "Ni")) {
        parse_float(p, end, 1.45f, material->Ni);
      }
      else if (parse_keyword(p, end, "d")) {
        parse_float(p, end, 1.0f, material->d);
      }
      else if (parse_keyword(p, end, "illum")) {
        /* Some files incorrectly use a float (T60135). */
        float val;
        parse_float(p, end, 1.0f, val);
        material->illum = val;
      }
      else {
        parse_texture_map(p, end, material, mtl_dir_path_);
      }
    }
  }

  MEM_freeN(buffer);
}
}  // namespace blender::io::obj
