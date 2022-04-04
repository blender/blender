/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "parser_string_utils.hh"

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
    r_offset.set_index_offset(global_vertices.vertices.size());
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
                            const StringRef rest_line,
                            GlobalVertices &r_global_vertices)
{
  float3 curr_vert;
  Vector<StringRef> str_vert_split;
  split_by_char(rest_line, ' ', str_vert_split);
  copy_string_to_float(str_vert_split, FLT_MAX, {curr_vert, 3});
  r_global_vertices.vertices.append(curr_vert);
  geom->vertex_indices_.append(r_global_vertices.vertices.size() - 1);
}

static void geom_add_vertex_normal(Geometry *geom,
                                   const StringRef rest_line,
                                   GlobalVertices &r_global_vertices)
{
  float3 curr_vert_normal;
  Vector<StringRef> str_vert_normal_split;
  split_by_char(rest_line, ' ', str_vert_normal_split);
  copy_string_to_float(str_vert_normal_split, FLT_MAX, {curr_vert_normal, 3});
  r_global_vertices.vertex_normals.append(curr_vert_normal);
  geom->has_vertex_normals_ = true;
}

static void geom_add_uv_vertex(const StringRef rest_line, GlobalVertices &r_global_vertices)
{
  float2 curr_uv_vert;
  Vector<StringRef> str_uv_vert_split;
  split_by_char(rest_line, ' ', str_uv_vert_split);
  copy_string_to_float(str_uv_vert_split, FLT_MAX, {curr_uv_vert, 2});
  r_global_vertices.uv_vertices.append(curr_uv_vert);
}

static void geom_add_edge(Geometry *geom,
                          const StringRef rest_line,
                          const VertexIndexOffset &offsets,
                          GlobalVertices &r_global_vertices)
{
  int edge_v1 = -1, edge_v2 = -1;
  Vector<StringRef> str_edge_split;
  split_by_char(rest_line, ' ', str_edge_split);
  copy_string_to_int(str_edge_split[0], -1, edge_v1);
  copy_string_to_int(str_edge_split[1], -1, edge_v2);
  /* Always keep stored indices non-negative and zero-based. */
  edge_v1 += edge_v1 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  edge_v2 += edge_v2 < 0 ? r_global_vertices.vertices.size() : -offsets.get_index_offset() - 1;
  BLI_assert(edge_v1 >= 0 && edge_v2 >= 0);
  geom->edges_.append({static_cast<uint>(edge_v1), static_cast<uint>(edge_v2)});
}

static void geom_add_polygon(Geometry *geom,
                             const StringRef rest_line,
                             const GlobalVertices &global_vertices,
                             const VertexIndexOffset &offsets,
                             const StringRef state_material_name,
                             const StringRef state_object_group,
                             const bool state_shaded_smooth)
{
  PolyElem curr_face;
  curr_face.shaded_smooth = state_shaded_smooth;
  if (!state_material_name.is_empty()) {
    curr_face.material_name = state_material_name;
  }
  if (!state_object_group.is_empty()) {
    curr_face.vertex_group = state_object_group;
    /* Yes it repeats several times, but another if-check will not reduce steps either. */
    geom->use_vertex_groups_ = true;
  }

  bool face_valid = true;
  Vector<StringRef> str_corners_split;
  split_by_char(rest_line, ' ', str_corners_split);
  for (StringRef str_corner : str_corners_split) {
    PolyCorner corner;
    const size_t n_slash = std::count(str_corner.begin(), str_corner.end(), '/');
    bool got_uv = false, got_normal = false;
    if (n_slash == 0) {
      /* Case: "f v1 v2 v3". */
      copy_string_to_int(str_corner, INT32_MAX, corner.vert_index);
    }
    else if (n_slash == 1) {
      /* Case: "f v1/vt1 v2/vt2 v3/vt3". */
      Vector<StringRef> vert_uv_split;
      split_by_char(str_corner, '/', vert_uv_split);
      if (vert_uv_split.size() != 1 && vert_uv_split.size() != 2) {
        fprintf(stderr, "Invalid face syntax '%s', ignoring\n", std::string(str_corner).c_str());
        face_valid = false;
      }
      else {
        copy_string_to_int(vert_uv_split[0], INT32_MAX, corner.vert_index);
        if (vert_uv_split.size() == 2) {
          copy_string_to_int(vert_uv_split[1], INT32_MAX, corner.uv_vert_index);
          got_uv = corner.uv_vert_index != INT32_MAX;
        }
      }
    }
    else if (n_slash == 2) {
      /* Case: "f v1//vn1 v2//vn2 v3//vn3". */
      /* Case: "f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3". */
      Vector<StringRef> vert_uv_normal_split;
      split_by_char(str_corner, '/', vert_uv_normal_split);
      if (vert_uv_normal_split.size() != 2 && vert_uv_normal_split.size() != 3) {
        fprintf(stderr, "Invalid face syntax '%s', ignoring\n", std::string(str_corner).c_str());
        face_valid = false;
      }
      else {
        copy_string_to_int(vert_uv_normal_split[0], INT32_MAX, corner.vert_index);
        if (vert_uv_normal_split.size() == 3) {
          copy_string_to_int(vert_uv_normal_split[1], INT32_MAX, corner.uv_vert_index);
          got_uv = corner.uv_vert_index != INT32_MAX;
          copy_string_to_int(vert_uv_normal_split[2], INT32_MAX, corner.vertex_normal_index);
          got_normal = corner.vertex_normal_index != INT32_MAX;
        }
        else {
          copy_string_to_int(vert_uv_normal_split[1], INT32_MAX, corner.vertex_normal_index);
          got_normal = corner.vertex_normal_index != INT32_MAX;
        }
      }
    }
    else {
      fprintf(stderr, "Invalid face syntax '%s', ignoring\n", std::string(str_corner).c_str());
      face_valid = false;
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
    curr_face.face_corners.append(corner);
  }

  if (face_valid) {
    geom->face_elements_.append(curr_face);
    geom->total_loops_ += curr_face.face_corners.size();
  }
}

static Geometry *geom_set_curve_type(Geometry *geom,
                                     const StringRef rest_line,
                                     const GlobalVertices &global_vertices,
                                     const StringRef state_object_group,
                                     VertexIndexOffset &r_offsets,
                                     Vector<std::unique_ptr<Geometry>> &r_all_geometries)
{
  if (rest_line.find("bspline") == StringRef::not_found) {
    std::cerr << "Curve type not supported:'" << rest_line << "'" << std::endl;
    return geom;
  }
  geom = create_geometry(
      geom, GEOM_CURVE, state_object_group, global_vertices, r_all_geometries, r_offsets);
  geom->nurbs_element_.group_ = state_object_group;
  return geom;
}

static void geom_set_curve_degree(Geometry *geom, const StringRef rest_line)
{
  copy_string_to_int(rest_line, 3, geom->nurbs_element_.degree);
}

static void geom_add_curve_vertex_indices(Geometry *geom,
                                          const StringRef rest_line,
                                          const GlobalVertices &global_vertices)
{
  Vector<StringRef> str_curv_split;
  split_by_char(rest_line, ' ', str_curv_split);
  /* Remove "0.0" and "1.0" from the strings. They are hardcoded. */
  str_curv_split.remove(0);
  str_curv_split.remove(0);
  geom->nurbs_element_.curv_indices.resize(str_curv_split.size());
  copy_string_to_int(str_curv_split, INT32_MAX, geom->nurbs_element_.curv_indices);
  for (int &curv_index : geom->nurbs_element_.curv_indices) {
    /* Always keep stored indices non-negative and zero-based. */
    curv_index += curv_index < 0 ? global_vertices.vertices.size() : -1;
  }
}

static void geom_add_curve_parameters(Geometry *geom, const StringRef rest_line)
{
  Vector<StringRef> str_parm_split;
  split_by_char(rest_line, ' ', str_parm_split);
  if (str_parm_split[0] != "u" && str_parm_split[0] != "v") {
    std::cerr << "Surfaces are not supported:'" << str_parm_split[0] << "'" << std::endl;
    return;
  }
  str_parm_split.remove(0);
  geom->nurbs_element_.parm.resize(str_parm_split.size());
  copy_string_to_float(str_parm_split, FLT_MAX, geom->nurbs_element_.parm);
}

static void geom_update_object_group(const StringRef rest_line, std::string &r_state_object_group)
{

  if (rest_line.find("off") != string::npos || rest_line.find("null") != string::npos ||
      rest_line.find("default") != string::npos) {
    /* Set group for future elements like faces or curves to empty. */
    r_state_object_group = "";
    return;
  }
  r_state_object_group = rest_line;
}

static void geom_update_polygon_material(Geometry *geom,
                                         const StringRef rest_line,
                                         std::string &r_state_material_name)
{
  /* Materials may repeat if faces are written without sorting. */
  geom->material_names_.add(string(rest_line));
  r_state_material_name = rest_line;
}

static void geom_update_smooth_group(const StringRef rest_line, bool &r_state_shaded_smooth)
{
  /* Some implementations use "0" and "null" too, in addition to "off". */
  if (rest_line != "0" && rest_line.find("off") == StringRef::not_found &&
      rest_line.find("null") == StringRef::not_found) {
    int smooth = 0;
    copy_string_to_int(rest_line, 0, smooth);
    r_state_shaded_smooth = smooth != 0;
  }
  else {
    /* The OBJ file explicitly set shading to off. */
    r_state_shaded_smooth = false;
  }
}

OBJParser::OBJParser(const OBJImportParams &import_params) : import_params_(import_params)
{
  obj_file_.open(import_params_.filepath);
  if (!obj_file_.good()) {
    fprintf(stderr, "Cannot read from OBJ file:'%s'.\n", import_params_.filepath);
    return;
  }
}

void OBJParser::parse(Vector<std::unique_ptr<Geometry>> &r_all_geometries,
                      GlobalVertices &r_global_vertices)
{
  if (!obj_file_.good()) {
    return;
  }

  string line;
  /* Store vertex coordinates that belong to other Geometry instances.  */
  VertexIndexOffset offsets;
  /* Non owning raw pointer to a Geometry. To be updated while creating a new Geometry. */
  Geometry *curr_geom = create_geometry(
      nullptr, GEOM_MESH, "", r_global_vertices, r_all_geometries, offsets);

  /* State-setting variables: if set, they remain the same for the remaining
   * elements in the object. */
  bool state_shaded_smooth = false;
  string state_object_group;
  string state_material_name;

  while (std::getline(obj_file_, line)) {
    /* Keep reading new lines if the last character is `\`. */
    /* Another way is to make a getline wrapper and use it in the while condition. */
    read_next_line(obj_file_, line);

    StringRef line_key, rest_line;
    split_line_key_rest(line, line_key, rest_line);
    if (line.empty() || rest_line.is_empty()) {
      continue;
    }
    switch (line_key_str_to_enum(line_key)) {
      case eOBJLineKey::V: {
        geom_add_vertex(curr_geom, rest_line, r_global_vertices);
        break;
      }
      case eOBJLineKey::VN: {
        geom_add_vertex_normal(curr_geom, rest_line, r_global_vertices);
        break;
      }
      case eOBJLineKey::VT: {
        geom_add_uv_vertex(rest_line, r_global_vertices);
        break;
      }
      case eOBJLineKey::F: {
        geom_add_polygon(curr_geom,
                         rest_line,
                         r_global_vertices,
                         offsets,
                         state_material_name,
                         state_material_name,
                         state_shaded_smooth);
        break;
      }
      case eOBJLineKey::L: {
        geom_add_edge(curr_geom, rest_line, offsets, r_global_vertices);
        break;
      }
      case eOBJLineKey::CSTYPE: {
        curr_geom = geom_set_curve_type(curr_geom,
                                        rest_line,
                                        r_global_vertices,
                                        state_object_group,
                                        offsets,
                                        r_all_geometries);
        break;
      }
      case eOBJLineKey::DEG: {
        geom_set_curve_degree(curr_geom, rest_line);
        break;
      }
      case eOBJLineKey::CURV: {
        geom_add_curve_vertex_indices(curr_geom, rest_line, r_global_vertices);
        break;
      }
      case eOBJLineKey::PARM: {
        geom_add_curve_parameters(curr_geom, rest_line);
        break;
      }
      case eOBJLineKey::O: {
        state_shaded_smooth = false;
        state_object_group = "";
        state_material_name = "";
        curr_geom = create_geometry(
            curr_geom, GEOM_MESH, rest_line, r_global_vertices, r_all_geometries, offsets);
        break;
      }
      case eOBJLineKey::G: {
        geom_update_object_group(rest_line, state_object_group);
        break;
      }
      case eOBJLineKey::S: {
        geom_update_smooth_group(rest_line, state_shaded_smooth);
        break;
      }
      case eOBJLineKey::USEMTL: {
        geom_update_polygon_material(curr_geom, rest_line, state_material_name);
        break;
      }
      case eOBJLineKey::MTLLIB: {
        mtl_libraries_.append(string(rest_line));
        break;
      }
      case eOBJLineKey::COMMENT:
        break;
      default:
        std::cout << "Element not recognised: '" << line_key << "'" << std::endl;
        break;
    }
  }
}

/**
 * Skip all texture map options and get the filepath from a "map_" line.
 */
static StringRef skip_unsupported_options(StringRef line)
{
  TextureMapOptions map_options;
  StringRef last_option;
  int64_t last_option_pos = 0;

  /* Find the last texture map option. */
  for (StringRef option : map_options.all_options()) {
    const int64_t pos{line.find(option)};
    /* Equality (>=) takes care of finding an option in the beginning of the line. Avoid messing
     * with signed-unsigned int comparison. */
    if (pos != StringRef::not_found && pos >= last_option_pos) {
      last_option = option;
      last_option_pos = pos;
    }
  }

  if (last_option.is_empty()) {
    /* No option found, line is the filepath */
    return line;
  }

  /* Remove up to start of the last option + size of the last option + space after it. */
  line = line.drop_prefix(last_option_pos + last_option.size() + 1);
  for (int i = 0; i < map_options.number_of_args(last_option); i++) {
    const int64_t pos_space{line.find_first_of(' ')};
    if (pos_space != StringRef::not_found) {
      BLI_assert(pos_space + 1 < line.size());
      line = line.drop_prefix(pos_space + 1);
    }
  }

  return line;
}

/**
 * Fix incoming texture map line keys for variations due to other exporters.
 */
static string fix_bad_map_keys(StringRef map_key)
{
  string new_map_key(map_key);
  if (map_key == "refl") {
    new_map_key = "map_refl";
  }
  if (map_key.find("bump") != StringRef::not_found) {
    /* Handles both "bump" and "map_Bump" */
    new_map_key = "map_Bump";
  }
  return new_map_key;
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
  mtl_file_.open(mtl_file_path_);
  if (!mtl_file_.good()) {
    fprintf(stderr, "Cannot read from MTL file:'%s'\n", mtl_file_path_);
    return;
  }
}

void MTLParser::parse_and_store(Map<string, std::unique_ptr<MTLMaterial>> &r_mtl_materials)
{
  if (!mtl_file_.good()) {
    return;
  }

  string line;
  MTLMaterial *current_mtlmaterial = nullptr;

  while (std::getline(mtl_file_, line)) {
    StringRef line_key, rest_line;
    split_line_key_rest(line, line_key, rest_line);
    if (line.empty() || rest_line.is_empty()) {
      continue;
    }

    /* Fix lower case/ incomplete texture map identifiers. */
    const string fixed_key = fix_bad_map_keys(line_key);
    line_key = fixed_key;

    if (line_key == "newmtl") {
      if (r_mtl_materials.remove_as(rest_line)) {
        std::cerr << "Duplicate material found:'" << rest_line
                  << "', using the last encountered Material definition." << std::endl;
      }
      current_mtlmaterial =
          r_mtl_materials.lookup_or_add(string(rest_line), std::make_unique<MTLMaterial>()).get();
    }
    else if (line_key == "Ns") {
      copy_string_to_float(rest_line, 324.0f, current_mtlmaterial->Ns);
    }
    else if (line_key == "Ka") {
      Vector<StringRef> str_ka_split;
      split_by_char(rest_line, ' ', str_ka_split);
      copy_string_to_float(str_ka_split, 0.0f, {current_mtlmaterial->Ka, 3});
    }
    else if (line_key == "Kd") {
      Vector<StringRef> str_kd_split;
      split_by_char(rest_line, ' ', str_kd_split);
      copy_string_to_float(str_kd_split, 0.8f, {current_mtlmaterial->Kd, 3});
    }
    else if (line_key == "Ks") {
      Vector<StringRef> str_ks_split;
      split_by_char(rest_line, ' ', str_ks_split);
      copy_string_to_float(str_ks_split, 0.5f, {current_mtlmaterial->Ks, 3});
    }
    else if (line_key == "Ke") {
      Vector<StringRef> str_ke_split;
      split_by_char(rest_line, ' ', str_ke_split);
      copy_string_to_float(str_ke_split, 0.0f, {current_mtlmaterial->Ke, 3});
    }
    else if (line_key == "Ni") {
      copy_string_to_float(rest_line, 1.45f, current_mtlmaterial->Ni);
    }
    else if (line_key == "d") {
      copy_string_to_float(rest_line, 1.0f, current_mtlmaterial->d);
    }
    else if (line_key == "illum") {
      copy_string_to_int(rest_line, 2, current_mtlmaterial->illum);
    }

    /* Parse image textures. */
    else if (line_key.find("map_") != StringRef::not_found) {
      /* TODO(@howardt): fix this. */
      eMTLSyntaxElement line_key_enum = mtl_line_key_str_to_enum(line_key);
      if (line_key_enum == eMTLSyntaxElement::string ||
          !current_mtlmaterial->texture_maps.contains_as(line_key_enum)) {
        /* No supported texture map found. */
        std::cerr << "Texture map type not supported:'" << line_key << "'" << std::endl;
        continue;
      }
      tex_map_XX &tex_map = current_mtlmaterial->texture_maps.lookup(line_key_enum);
      Vector<StringRef> str_map_xx_split;
      split_by_char(rest_line, ' ', str_map_xx_split);

      /* TODO(@ankitm): use `skip_unsupported_options` for parsing these options too? */
      const int64_t pos_o{str_map_xx_split.first_index_of_try("-o")};
      if (pos_o != -1 && pos_o + 3 < str_map_xx_split.size()) {
        copy_string_to_float({str_map_xx_split[pos_o + 1],
                              str_map_xx_split[pos_o + 2],
                              str_map_xx_split[pos_o + 3]},
                             0.0f,
                             {tex_map.translation, 3});
      }
      const int64_t pos_s{str_map_xx_split.first_index_of_try("-s")};
      if (pos_s != -1 && pos_s + 3 < str_map_xx_split.size()) {
        copy_string_to_float({str_map_xx_split[pos_s + 1],
                              str_map_xx_split[pos_s + 2],
                              str_map_xx_split[pos_s + 3]},
                             1.0f,
                             {tex_map.scale, 3});
      }
      /* Only specific to Normal Map node. */
      const int64_t pos_bm{str_map_xx_split.first_index_of_try("-bm")};
      if (pos_bm != -1 && pos_bm + 1 < str_map_xx_split.size()) {
        copy_string_to_float(
            str_map_xx_split[pos_bm + 1], 0.0f, current_mtlmaterial->map_Bump_strength);
      }
      const int64_t pos_projection{str_map_xx_split.first_index_of_try("-type")};
      if (pos_projection != -1 && pos_projection + 1 < str_map_xx_split.size()) {
        /* Only Sphere is supported, so whatever the type is, set it to Sphere.  */
        tex_map.projection_type = SHD_PROJ_SPHERE;
        if (str_map_xx_split[pos_projection + 1] != "sphere") {
          std::cerr << "Using projection type 'sphere', not:'"
                    << str_map_xx_split[pos_projection + 1] << "'." << std::endl;
        }
      }

      /* Skip all unsupported options and arguments. */
      tex_map.image_path = string(skip_unsupported_options(rest_line));
      tex_map.mtl_dir_path = mtl_dir_path_;
    }
  }
}
}  // namespace blender::io::obj
