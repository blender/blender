/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "IO_string_utils.hh"

#include "obj_import_file_reader.hh"

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
                            const StringRef line,
                            GlobalVertices &r_global_vertices)
{
  float3 vert;
  parse_floats(line, FLT_MAX, vert, 3);
  r_global_vertices.vertices.append(vert);
  geom->vertex_count_++;
}

static void geom_add_vertex_normal(Geometry *geom,
                                   const StringRef line,
                                   GlobalVertices &r_global_vertices)
{
  float3 normal;
  parse_floats(line, FLT_MAX, normal, 3);
  r_global_vertices.vertex_normals.append(normal);
  geom->has_vertex_normals_ = true;
}

static void geom_add_uv_vertex(const StringRef line, GlobalVertices &r_global_vertices)
{
  float2 uv;
  parse_floats(line, FLT_MAX, uv, 2);
  r_global_vertices.uv_vertices.append(uv);
}

static void geom_add_edge(Geometry *geom,
                          StringRef line,
                          const VertexIndexOffset &offsets,
                          GlobalVertices &r_global_vertices)
{
  int edge_v1, edge_v2;
  line = parse_int(line, -1, edge_v1);
  line = parse_int(line, -1, edge_v2);
  /* Always keep stored indices non-negative and zero-based. */
  edge_v1 += edge_v1 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  edge_v2 += edge_v2 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  BLI_assert(edge_v1 >= 0 && edge_v2 >= 0);
  geom->edges_.append({static_cast<uint>(edge_v1), static_cast<uint>(edge_v2)});
}

static void geom_add_polygon(Geometry *geom,
                             StringRef line,
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
  while (!line.is_empty() && face_valid) {
    PolyCorner corner;
    bool got_uv = false, got_normal = false;
    /* Parse vertex index. */
    line = parse_int(line, INT32_MAX, corner.vert_index, false);
    face_valid &= corner.vert_index != INT32_MAX;
    if (!line.is_empty() && line[0] == '/') {
      /* Parse UV index. */
      line = line.drop_prefix(1);
      if (!line.is_empty() && line[0] != '/') {
        line = parse_int(line, INT32_MAX, corner.uv_vert_index, false);
        got_uv = corner.uv_vert_index != INT32_MAX;
      }
      /* Parse normal index. */
      if (!line.is_empty() && line[0] == '/') {
        line = line.drop_prefix(1);
        line = parse_int(line, INT32_MAX, corner.vertex_normal_index, false);
        got_normal = corner.uv_vert_index != INT32_MAX;
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
    if (got_normal) {
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
    line = drop_whitespace(line);
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
                                     const StringRef rest_line,
                                     const GlobalVertices &global_vertices,
                                     const StringRef group_name,
                                     VertexIndexOffset &r_offsets,
                                     Vector<std::unique_ptr<Geometry>> &r_all_geometries)
{
  if (rest_line.find("bspline") == StringRef::not_found) {
    std::cerr << "Curve type not supported:'" << rest_line << "'" << std::endl;
    return geom;
  }
  geom = create_geometry(
      geom, GEOM_CURVE, group_name, global_vertices, r_all_geometries, r_offsets);
  geom->nurbs_element_.group_ = group_name;
  return geom;
}

static void geom_set_curve_degree(Geometry *geom, const StringRef line)
{
  parse_int(line, 3, geom->nurbs_element_.degree);
}

static void geom_add_curve_vertex_indices(Geometry *geom,
                                          StringRef line,
                                          const GlobalVertices &global_vertices)
{
  /* Curve lines always have "0.0" and "1.0", skip over them. */
  float dummy[2];
  line = parse_floats(line, 0, dummy, 2);
  /* Parse indices. */
  while (!line.is_empty()) {
    int index;
    line = parse_int(line, INT32_MAX, index);
    if (index == INT32_MAX) {
      return;
    }
    /* Always keep stored indices non-negative and zero-based. */
    index += index < 0 ? global_vertices.vertices.size() : -1;
    geom->nurbs_element_.curv_indices.append(index);
  }
}

static void geom_add_curve_parameters(Geometry *geom, StringRef line)
{
  line = drop_whitespace(line);
  if (line.is_empty()) {
    std::cerr << "Invalid OBJ curve parm line: '" << line << "'" << std::endl;
    return;
  }
  if (line[0] != 'u') {
    std::cerr << "OBJ curve surfaces are not supported: '" << line[0] << "'" << std::endl;
    return;
  }
  line = line.drop_prefix(1);

  while (!line.is_empty()) {
    float val;
    line = parse_float(line, FLT_MAX, val);
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

static void geom_update_smooth_group(StringRef line, bool &r_state_shaded_smooth)
{
  line = drop_whitespace(line);
  /* Some implementations use "0" and "null" too, in addition to "off". */
  if (line == "0" || line.startswith("off") || line.startswith("null")) {
    r_state_shaded_smooth = false;
    return;
  }

  int smooth = 0;
  parse_int(line, 0, smooth);
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

void OBJParser::parse(Vector<std::unique_ptr<Geometry>> &r_all_geometries,
                      GlobalVertices &r_global_vertices)
{
  if (!obj_file_) {
    return;
  }

  VertexIndexOffset offsets;
  Geometry *curr_geom = create_geometry(
      nullptr, GEOM_MESH, "", r_global_vertices, r_all_geometries, offsets);

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
      ++line_number;
      if (line.is_empty()) {
        continue;
      }
      /* Most common things that start with 'v': vertices, normals, UVs. */
      if (line[0] == 'v') {
        if (line.startswith("v ")) {
          line = line.drop_prefix(2);
          geom_add_vertex(curr_geom, line, r_global_vertices);
        }
        else if (line.startswith("vn ")) {
          line = line.drop_prefix(3);
          geom_add_vertex_normal(curr_geom, line, r_global_vertices);
        }
        else if (line.startswith("vt ")) {
          line = line.drop_prefix(3);
          geom_add_uv_vertex(line, r_global_vertices);
        }
      }
      /* Faces. */
      else if (line.startswith("f ")) {
        line = line.drop_prefix(2);
        geom_add_polygon(curr_geom,
                         line,
                         r_global_vertices,
                         offsets,
                         state_material_index,
                         state_group_index, /* TODO was wrongly material name! */
                         state_shaded_smooth);
      }
      /* Faces. */
      else if (line.startswith("l ")) {
        line = line.drop_prefix(2);
        geom_add_edge(curr_geom, line, offsets, r_global_vertices);
      }
      /* Objects. */
      else if (line.startswith("o ")) {
        line = line.drop_prefix(2);
        state_shaded_smooth = false;
        state_group_name = "";
        state_material_name = "";
        curr_geom = create_geometry(
            curr_geom, GEOM_MESH, line, r_global_vertices, r_all_geometries, offsets);
      }
      /* Groups. */
      else if (line.startswith("g ")) {
        line = line.drop_prefix(2);
        geom_update_group(line, state_group_name);
        int new_index = curr_geom->group_indices_.size();
        state_group_index = curr_geom->group_indices_.lookup_or_add(state_group_name, new_index);
        if (new_index == state_group_index) {
          curr_geom->group_order_.append(state_group_name);
        }
      }
      /* Smoothing groups. */
      else if (line.startswith("s ")) {
        line = line.drop_prefix(2);
        geom_update_smooth_group(line, state_shaded_smooth);
      }
      /* Materials and their libraries. */
      else if (line.startswith("usemtl ")) {
        line = line.drop_prefix(7);
        state_material_name = line;
        int new_mat_index = curr_geom->material_indices_.size();
        state_material_index = curr_geom->material_indices_.lookup_or_add(state_material_name,
                                                                          new_mat_index);
        if (new_mat_index == state_material_index) {
          curr_geom->material_order_.append(state_material_name);
        }
      }
      else if (line.startswith("mtllib ")) {
        line = line.drop_prefix(7);
        mtl_libraries_.append(string(line));
      }
      /* Comments. */
      else if (line.startswith("#")) {
        /* Nothing to do. */
      }
      /* Curve related things. */
      else if (line.startswith("cstype ")) {
        line = line.drop_prefix(7);
        curr_geom = geom_set_curve_type(
            curr_geom, line, r_global_vertices, state_group_name, offsets, r_all_geometries);
      }
      else if (line.startswith("deg ")) {
        line = line.drop_prefix(4);
        geom_set_curve_degree(curr_geom, line);
      }
      else if (line.startswith("curv ")) {
        line = line.drop_prefix(5);
        geom_add_curve_vertex_indices(curr_geom, line, r_global_vertices);
      }
      else if (line.startswith("parm ")) {
        line = line.drop_prefix(5);
        geom_add_curve_parameters(curr_geom, line);
      }
      else if (line.startswith("end")) {
        /* End of curve definition, nothing else to do. */
      }
      else {
        std::cout << "OBJ element not recognised: '" << line << "'" << std::endl;
      }
    }

    /* We might have a line that was cut in the middle by the previous buffer;
     * copy it over for next chunk reading. */
    size_t left_size = buffer_end - last_nl;
    memmove(buffer.data(), buffer.data() + last_nl, left_size);
    buffer_offset = left_size;
  }
}

static eMTLSyntaxElement mtl_line_start_to_enum(StringRef &line)
{
  if (line.startswith("map_Kd")) {
    line = line.drop_prefix(6);
    return eMTLSyntaxElement::map_Kd;
  }
  if (line.startswith("map_Ks")) {
    line = line.drop_prefix(6);
    return eMTLSyntaxElement::map_Ks;
  }
  if (line.startswith("map_Ns")) {
    line = line.drop_prefix(6);
    return eMTLSyntaxElement::map_Ns;
  }
  if (line.startswith("map_d")) {
    line = line.drop_prefix(5);
    return eMTLSyntaxElement::map_d;
  }
  if (line.startswith("refl")) {
    line = line.drop_prefix(4);
    return eMTLSyntaxElement::map_refl;
  }
  if (line.startswith("map_refl")) {
    line = line.drop_prefix(8);
    return eMTLSyntaxElement::map_refl;
  }
  if (line.startswith("map_Ke")) {
    line = line.drop_prefix(6);
    return eMTLSyntaxElement::map_Ke;
  }
  if (line.startswith("bump")) {
    line = line.drop_prefix(4);
    return eMTLSyntaxElement::map_Bump;
  }
  if (line.startswith("map_Bump") || line.startswith("map_bump")) {
    line = line.drop_prefix(8);
    return eMTLSyntaxElement::map_Bump;
  }
  return eMTLSyntaxElement::string;
}

static const std::pair<const char *, int> unsupported_texture_options[] = {
    {"-blendu ", 1},
    {"-blendv ", 1},
    {"-boost ", 1},
    {"-cc ", 1},
    {"-clamp ", 1},
    {"-imfchan ", 1},
    {"-mm ", 2},
    {"-t ", 3},
    {"-texres ", 1},
};

static bool parse_texture_option(StringRef &line, MTLMaterial *material, tex_map_XX &tex_map)
{
  line = drop_whitespace(line);
  if (line.startswith("-o ")) {
    line = line.drop_prefix(3);
    line = parse_floats(line, 0.0f, tex_map.translation, 3);
    return true;
  }
  if (line.startswith("-s ")) {
    line = line.drop_prefix(3);
    line = parse_floats(line, 1.0f, tex_map.scale, 3);
    return true;
  }
  if (line.startswith("-bm ")) {
    line = line.drop_prefix(4);
    line = parse_float(line, 0.0f, material->map_Bump_strength);
    return true;
  }
  if (line.startswith("-type ")) {
    line = line.drop_prefix(6);
    line = drop_whitespace(line);
    /* Only sphere is supported. */
    tex_map.projection_type = SHD_PROJ_SPHERE;
    if (!line.startswith("sphere")) {
      std::cerr << "OBJ import: only sphere MTL projection type is supported: '" << line << "'"
                << std::endl;
    }
    line = drop_non_whitespace(line);
    return true;
  }
  /* Check for unsupported options and skip them. */
  for (const auto &opt : unsupported_texture_options) {
    if (line.startswith(opt.first)) {
      /* Drop the option name. */
      line = line.drop_known_prefix(opt.first);
      /* Drop the arguments. */
      for (int i = 0; i < opt.second; ++i) {
        line = drop_whitespace(line);
        line = drop_non_whitespace(line);
      }
      return true;
    }
  }

  return false;
}

static void parse_texture_map(StringRef line, MTLMaterial *material, const char *mtl_dir_path)
{
  bool is_map = line.startswith("map_");
  bool is_refl = line.startswith("refl");
  bool is_bump = line.startswith("bump");
  if (!is_map && !is_refl && !is_bump) {
    return;
  }
  eMTLSyntaxElement key = mtl_line_start_to_enum(line);
  if (key == eMTLSyntaxElement::string || !material->texture_maps.contains(key)) {
    /* No supported texture map found. */
    std::cerr << "OBJ import: MTL texture map type not supported: '" << line << "'" << std::endl;
    return;
  }
  tex_map_XX &tex_map = material->texture_maps.lookup(key);
  tex_map.mtl_dir_path = mtl_dir_path;

  /* Parse texture map options. */
  while (parse_texture_option(line, material, tex_map)) {
  }

  /* What remains is the image path. */
  line = line.trim();
  tex_map.image_path = line;
}

Span<std::string> OBJParser::mtl_libraries() const
{
  return mtl_libraries_;
}

MTLParser::MTLParser(StringRef mtl_library, StringRefNull obj_filepath)
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
    StringRef line = read_next_line(buffer_str);
    if (line.is_empty()) {
      continue;
    }

    if (line.startswith("newmtl ")) {
      line = line.drop_prefix(7);
      if (r_materials.remove_as(line)) {
        std::cerr << "Duplicate material found:'" << line
                  << "', using the last encountered Material definition." << std::endl;
      }
      material = r_materials.lookup_or_add(string(line), std::make_unique<MTLMaterial>()).get();
    }
    else if (material != nullptr) {
      if (line.startswith("Ns ")) {
        line = line.drop_prefix(3);
        parse_float(line, 324.0f, material->Ns);
      }
      else if (line.startswith("Ka ")) {
        line = line.drop_prefix(3);
        parse_floats(line, 0.0f, material->Ka, 3);
      }
      else if (line.startswith("Kd ")) {
        line = line.drop_prefix(3);
        parse_floats(line, 0.8f, material->Kd, 3);
      }
      else if (line.startswith("Ks ")) {
        line = line.drop_prefix(3);
        parse_floats(line, 0.5f, material->Ks, 3);
      }
      else if (line.startswith("Ke ")) {
        line = line.drop_prefix(3);
        parse_floats(line, 0.0f, material->Ke, 3);
      }
      else if (line.startswith("Ni ")) {
        line = line.drop_prefix(3);
        parse_float(line, 1.45f, material->Ni);
      }
      else if (line.startswith("d ")) {
        line = line.drop_prefix(2);
        parse_float(line, 1.0f, material->d);
      }
      else if (line.startswith("illum ")) {
        line = line.drop_prefix(6);
        parse_int(line, 2, material->illum);
      }
      else {
        parse_texture_map(line, material, mtl_dir_path_);
      }
    }
  }

  MEM_freeN(buffer);
}
}  // namespace blender::io::obj
