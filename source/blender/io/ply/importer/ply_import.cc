/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"
#include "BLI_memory_utils.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ply_data.hh"
#include "ply_import.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"
#include "ply_import_mesh.hh"

namespace blender::io::ply {

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
  while (!str.is_empty() && str[0] <= ' ')
    str = str.drop_front(1);
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
             line.first() == '-') {
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

void importer_main(bContext *C, const PLYImportParams &import_params, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  importer_main(bmain, scene, view_layer, import_params, op);
}

void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params,
                   wmOperator *op)
{
  /* File base name used for both mesh and object. */
  char ob_name[FILE_MAX];
  STRNCPY(ob_name, BLI_path_basename(import_params.filepath));
  BLI_path_extension_strip(ob_name);

  /* Parse header. */
  PlyReadBuffer file(import_params.filepath, 64 * 1024);

  PlyHeader header;
  const char *err = read_header(file, header);
  if (err != nullptr) {
    fprintf(stderr, "PLY Importer: %s: %s\n", ob_name, err);
    BKE_reportf(op->reports, RPT_ERROR, "PLY Importer: %s: %s", ob_name, err);
    return;
  }

  /* Parse actual file data. */
  std::unique_ptr<PlyData> data = import_ply_data(file, header);
  if (data == nullptr) {
    fprintf(stderr, "PLY Importer: failed importing %s, unknown error\n", ob_name);
    BKE_report(op->reports, RPT_ERROR, "PLY Importer: failed importing, unknown error");
    return;
  }
  if (!data->error.empty()) {
    fprintf(stderr, "PLY Importer: failed importing %s: %s\n", ob_name, data->error.c_str());
    BKE_report(op->reports, RPT_ERROR, "PLY Importer: failed importing, unknown error");
    return;
  }
  if (data->vertices.is_empty()) {
    fprintf(stderr, "PLY Importer: file %s contains no vertices\n", ob_name);
    BKE_report(op->reports, RPT_ERROR, "PLY Importer: failed importing, no vertices");
    return;
  }

  /* Create mesh and do all prep work. */
  Mesh *mesh_in_main = BKE_mesh_add(bmain, ob_name);
  BKE_view_layer_base_deselect_all(scene, view_layer);
  LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
  Object *obj = BKE_object_add_only_object(bmain, OB_MESH, ob_name);
  BKE_mesh_assign_object(bmain, obj, mesh_in_main);
  BKE_collection_object_add(bmain, lc->collection, obj);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, obj);
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  /* Stuff ply data into the mesh. */
  Mesh *mesh = convert_ply_to_mesh(*data, import_params);
  BKE_mesh_nomain_to_mesh(mesh, mesh_in_main, obj);

  /* Object matrix and finishing up. */
  float global_scale = import_params.global_scale;
  if ((scene->unit.system != USER_UNIT_NONE) && import_params.use_scene_unit) {
    global_scale *= scene->unit.scale_length;
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

  DEG_id_tag_update(&lc->collection->id, ID_RECALC_COPY_ON_WRITE);
  int flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
              ID_RECALC_BASE_FLAGS;
  DEG_id_tag_update_ex(bmain, &obj->id, flags);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
}
}  // namespace blender::io::ply
