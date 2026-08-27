/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <array>

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_matrix_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ply_data.hh"
#include "ply_import.hh"

#include "BKE_pointcloud.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"
#include "ply_import_gsplat.hh"
#include "ply_import_mesh.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.ply"};

namespace io::ply {

/* If line starts with keyword, returns true and drops it from the line. */
static bool parse_keyword(Span<char> &str, StringRef keyword)
{
  const size_t keyword_len = keyword.size();
  if (str.size() < keyword_len) {
    return false;
  }
  if (memcmp(str.data(), keyword.data(), keyword_len) != 0) {
    return false;
  }
  str = str.drop_front(keyword_len);
  return true;
}

static Span<char> parse_word(Span<char> &str)
{
  size_t len = 0;
  while (len < str.size() && str[len] > ' ') {
    ++len;
  }
  Span<char> word(str.begin(), len);
  str = str.drop_front(len);
  return word;
}

static void skip_space(Span<char> &str)
{
  while (!str.is_empty() && str[0] <= ' ') {
    str = str.drop_front(1);
  }
}

static PlyDataTypes type_from_string(Span<char> word)
{
  StringRef input(word.data(), word.size());
  if (ELEM(input, "uchar", "uint8")) {
    return PlyDataTypes::UCHAR;
  }
  if (ELEM(input, "char", "int8")) {
    return PlyDataTypes::CHAR;
  }
  if (ELEM(input, "ushort", "uint16")) {
    return PlyDataTypes::USHORT;
  }
  if (ELEM(input, "short", "int16")) {
    return PlyDataTypes::SHORT;
  }
  if (ELEM(input, "uint", "uint32")) {
    return PlyDataTypes::UINT;
  }
  if (ELEM(input, "int", "int32")) {
    return PlyDataTypes::INT;
  }
  if (ELEM(input, "float", "float32")) {
    return PlyDataTypes::FLOAT;
  }
  if (ELEM(input, "double", "float64")) {
    return PlyDataTypes::DOUBLE;
  }
  return PlyDataTypes::NONE;
}

const char *read_header(PlyReadBuffer &file, PlyHeader &r_header)
{
  Span<char> word, line;
  line = file.read_line();
  if (StringRef(line.data(), line.size()) != "ply") {
    return "Invalid PLY header.";
  }

  while (true) { /* We break when end_header is encountered. */
    line = file.read_line();

    if (parse_keyword(line, "format")) {
      skip_space(line);
      if (parse_keyword(line, "ascii")) {
        r_header.type = PlyFormatType::ASCII;
      }
      else if (parse_keyword(line, "binary_big_endian")) {
        r_header.type = PlyFormatType::BINARY_BE;
      }
      else if (parse_keyword(line, "binary_little_endian")) {
        r_header.type = PlyFormatType::BINARY_LE;
      }
    }
    else if (parse_keyword(line, "element")) {
      PlyElement element;

      skip_space(line);
      word = parse_word(line);
      element.name = std::string(word.data(), word.size());
      skip_space(line);
      word = parse_word(line);
      element.count = std::stoi(std::string(word.data(), word.size()));
      r_header.elements.append(element);
    }
    else if (parse_keyword(line, "property")) {
      PlyProperty property;
      skip_space(line);
      if (parse_keyword(line, "list")) {
        skip_space(line);
        property.count_type = type_from_string(parse_word(line));
      }
      skip_space(line);
      property.type = type_from_string(parse_word(line));
      skip_space(line);
      word = parse_word(line);
      property.name = std::string(word.data(), word.size());
      r_header.elements.last().properties.append(property);
    }
    else if (parse_keyword(line, "end_header")) {
      break;
    }
    else if (line.is_empty() || (line.first() >= '0' && line.first() <= '9') ||
             line.first() == '-')
    {
      /* A value was found before we broke out of the loop. No end_header. */
      return "No end_header.";
    }
  }

  file.after_header(r_header.type != PlyFormatType::ASCII);
  for (PlyElement &el : r_header.elements) {
    el.calc_stride();
  }
  return nullptr;
}

static std::unique_ptr<PlyData> read_ply_to_data(const PLYImportParams &import_params,
                                                 const char *ob_name)
{
  /* Parse header. */
  PlyReadBuffer file(import_params.filepath, 64 * 1024);

  PlyHeader header;
  const char *err = read_header(file, header);
  if (err != nullptr) {
    CLOG_ERROR(&LOG, "PLY Importer: %s: %s", ob_name, err);
    BKE_reportf(import_params.reports, RPT_ERROR, "PLY Importer: %s: %s", ob_name, err);
    return nullptr;
  }

  /* Parse actual file data. */
  std::unique_ptr<PlyData> data = import_ply_data(file, header);
  if (data == nullptr) {
    CLOG_ERROR(&LOG, "PLY Importer: failed importing %s, unknown error", ob_name);
    BKE_report(import_params.reports, RPT_ERROR, "PLY Importer: failed importing, unknown error");
    return nullptr;
  }
  if (!data->error.empty()) {
    CLOG_ERROR(&LOG, "PLY Importer: failed importing %s: %s", ob_name, data->error.c_str());
    BKE_report(import_params.reports, RPT_ERROR, "PLY Importer: failed importing, unknown error");
    return nullptr;
  }
  if (data->vertices.is_empty()) {
    CLOG_ERROR(&LOG, "PLY Importer: file %s contains no vertices", ob_name);
    BKE_report(import_params.reports, RPT_ERROR, "PLY Importer: failed importing, no vertices");
    return nullptr;
  }

  return data;
}

static bool is_data_gaussian_splat(const PlyData &data)
{
  /* If PLY is not just vertices, it is not a gaussian splat.
   *
   * Ignore possible vertex normals, as some gaussian splats in the PLY format include vertex
   * normals. For example, spz_to_ply writes vertex normals explaining that some applications
   * require them to present (they are written as (0, 0, 0)).
   */
  if (!data.edges.is_empty() || !data.face_vertices.is_empty() || !data.face_sizes.is_empty() ||
      !data.uv_coordinates.is_empty())
  {
    return false;
  }

  Set<std::string> attr_names;
  for (const PlyCustomAttribute &attr : data.vertex_custom_attr) {
    attr_names.add(attr.name);
  }

  constexpr auto required_attr_names = std::to_array<const char *>({"f_dc_0",
                                                                    "f_dc_1",
                                                                    "f_dc_2",
                                                                    "opacity",
                                                                    "scale_0",
                                                                    "scale_1",
                                                                    "scale_2",
                                                                    "rot_0",
                                                                    "rot_1",
                                                                    "rot_2",
                                                                    "rot_3"});
  for (const char *required_attr_name : required_attr_names) {
    if (!attr_names.contains(required_attr_name)) {
      return false;
    }
  }

  // TODO(sergey): Check the f_rest_<i> attributes are consistent?

  return true;
}

Mesh *import_mesh(const PLYImportParams &import_params)
{
  /* File base name used for both mesh and object. */
  char ob_name[FILE_MAX];
  STRNCPY(ob_name, BLI_path_basename(import_params.filepath));
  BLI_path_extension_strip(ob_name);

  /* Stuff ply data into the mesh. */
  std::unique_ptr<PlyData> data = read_ply_to_data(import_params, ob_name);
  if (!data) {
    return nullptr;
  }
  return convert_ply_to_mesh(*data, import_params);
}

PointCloud *import_point_cloud(const PLYImportParams &import_params)
{
  /* File base name used for both mesh and object. */
  char ob_name[FILE_MAX];
  STRNCPY(ob_name, BLI_path_basename(import_params.filepath));
  BLI_path_extension_strip(ob_name);

  /* Stuff ply data into the mesh. */
  std::unique_ptr<PlyData> data = read_ply_to_data(import_params, ob_name);
  if (!data) {
    return nullptr;
  }
  if (!is_data_gaussian_splat(*data)) {
    return nullptr;
  }
  return convert_gsplat_ply_to_point_cloud(*data, import_params);
}

void importer_main(bContext *C, const PLYImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  importer_main(bmain, scene, view_layer, import_params);
}

void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params)
{
  /* File base name used for both mesh and object. */
  char ob_name[FILE_MAX];
  STRNCPY(ob_name, BLI_path_basename(import_params.filepath));
  BLI_path_extension_strip(ob_name);

  /* Stuff ply data into the mesh. */
  std::unique_ptr<PlyData> data = read_ply_to_data(import_params, ob_name);
  if (!data) {
    return;
  }

  ObjectType ob_type = OB_EMPTY;
  ID *ob_data = nullptr;
  Mesh *mesh = nullptr;
  Mesh *mesh_in_main = nullptr;
  if (is_data_gaussian_splat(*data)) {
    PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(*data, import_params);
    if (!point_cloud) {
      return;
    }
    PointCloud *point_cloud_in_main = BKE_pointcloud_add(bmain, ob_name);
    ob_type = OB_POINTCLOUD;
    ob_data = id_cast<ID *>(point_cloud_in_main);
    BKE_pointcloud_nomain_to_pointcloud(point_cloud, point_cloud_in_main);
  }
  else {
    mesh = convert_ply_to_mesh(*data, import_params);
    if (!mesh) {
      return;
    }
    ob_type = OB_MESH;
    mesh_in_main = BKE_mesh_add(bmain, ob_name);
    ob_data = id_cast<ID *>(mesh_in_main);
    /* Delay conversion of mesh to mesh_in_main until the object is know. */
  }

  BLI_assert(ob_data);
  BLI_assert(ob_type != OB_EMPTY);

  /* Create mesh and do all prep work. */
  BKE_view_layer_base_deselect_all(*bmain, scene, view_layer);
  LayerCollection *lc = BKE_layer_collection_get_active_editable(view_layer);
  if (!ID_IS_EDITABLE(lc->collection)) {
    BKE_report(import_params.reports,
               RPT_WARNING,
               "Could not find an editable collection in current scene, imported data will not be "
               "instantiated");
  }
  Object *obj = BKE_object_add_only_object(bmain, ob_type, ob_name);
  obj->data = ob_data;
  BKE_collection_object_add(bmain, lc->collection, obj);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, obj)) {
    /* `base` will be nullptr if the Object could not be instantiated in the current viewlayer. */
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }

  if (ob_type == OB_MESH) {
    BLI_assert(mesh);
    BLI_assert(mesh_in_main);
    BKE_mesh_nomain_to_mesh(mesh, mesh_in_main, obj);
  }

  /* Object matrix and finishing up. */
  float global_scale = import_params.global_scale;
  if ((scene->unit.system != USER_UNIT_NONE) && import_params.use_scene_unit) {
    global_scale /= scene->unit.scale_length;
  }
  float scale_vec[3] = {global_scale, global_scale, global_scale};
  float obmat3x3[3][3];
  unit_m3(obmat3x3);
  float obmat4x4[4][4];
  unit_m4(obmat4x4);
  /* +Y-forward and +Z-up are the Blender's default axis settings. */
  mat3_from_axis_conversion(
      IO_AXIS_Y, IO_AXIS_Z, import_params.forward_axis, import_params.up_axis, obmat3x3);
  copy_m4_m3(obmat4x4, obmat3x3);
  rescale_m4(obmat4x4, scale_vec);
  BKE_object_apply_mat4(obj, obmat4x4, true, false);

  DEG_id_tag_update(&lc->collection->id, ID_RECALC_SYNC_TO_EVAL);
  int flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
              ID_RECALC_BASE_FLAGS;
  DEG_id_tag_update_ex(bmain, &obj->id, flags);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
}
}  // namespace io::ply
}  // namespace blender
