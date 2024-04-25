/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#include <cstring>
#include <memory>

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "IO_stl.hh"

#include "stl_export.hh"
#include "stl_export_writer.hh"

namespace blender::io::stl {

void export_frame(Depsgraph *depsgraph,
                  float scene_unit_scale,
                  const STLExportParams &export_params)
{
  std::unique_ptr<FileWriter> writer;

  /* If not exporting in batch, create single writer for all objects. */
  if (!export_params.use_batch) {
    try {
      writer = std::make_unique<FileWriter>(export_params.filepath, export_params.ascii_format);
    }
    catch (const std::runtime_error &ex) {
      fprintf(stderr, "%s\n", ex.what());
      BKE_reportf(export_params.reports,
                  RPT_ERROR,
                  "STL Export: Cannot open file '%s'",
                  export_params.filepath);
      return;
    }
  }

  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    if (object->type != OB_MESH) {
      continue;
    }

    if (export_params.export_selected_objects && !(object->base_flag & BASE_SELECTED)) {
      continue;
    }

    /* If exporting in batch, create writer for each iteration over objects. */
    if (export_params.use_batch) {
      /* Get object name by skipping initial "OB" prefix. */
      char object_name[sizeof(object->id.name) - 2];
      STRNCPY(object_name, object->id.name + 2);
      BLI_path_make_safe_filename(object_name);
      /* Replace spaces with underscores. */
      BLI_string_replace_char(object_name, ' ', '_');

      /* Include object name in the exported file name. */
      char filepath[FILE_MAX];
      STRNCPY(filepath, export_params.filepath);
      BLI_path_suffix(filepath, FILE_MAX, object_name, "");
      /* Make sure we have .stl extension (case insensitive). */
      if (!BLI_path_extension_check(filepath, ".stl")) {
        BLI_path_extension_ensure(filepath, FILE_MAX, ".stl");
      }

      try {
        writer = std::make_unique<FileWriter>(filepath, export_params.ascii_format);
      }
      catch (const std::runtime_error &ex) {
        fprintf(stderr, "%s\n", ex.what());
        BKE_reportf(
            export_params.reports, RPT_ERROR, "STL Export: Cannot open file '%s'", filepath);
        return;
      }
    }

    Object *obj_eval = DEG_get_evaluated_object(depsgraph, object);
    Mesh *mesh = export_params.apply_modifiers ? BKE_object_get_evaluated_mesh(obj_eval) :
                                                 BKE_object_get_pre_modified_mesh(obj_eval);

    /* Calculate transform. */
    float global_scale = export_params.global_scale * scene_unit_scale;
    float axes_transform[3][3];
    unit_m3(axes_transform);
    float xform[4][4];
    /* +Y-forward and +Z-up are the default Blender axis settings. */
    mat3_from_axis_conversion(
        export_params.forward_axis, export_params.up_axis, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
    mul_m4_m3m4(xform, axes_transform, obj_eval->object_to_world().ptr());
    /* mul_m4_m3m4 does not transform last row of obmat, i.e. location data. */
    mul_v3_m3v3(xform[3], axes_transform, obj_eval->object_to_world().location());
    xform[3][3] = obj_eval->object_to_world()[3][3];

    /* Write triangles. */
    const Span<float3> positions = mesh->vert_positions();
    const blender::Span<int> corner_verts = mesh->corner_verts();
    for (const int3 &tri : mesh->corner_tris()) {
      Triangle t;
      for (int i = 0; i < 3; i++) {
        float3 pos = positions[corner_verts[tri[i]]];
        mul_m4_v3(xform, pos);
        pos *= global_scale;
        t.vertices[i] = pos;
      }
      t.normal = math::normal_tri(t.vertices[0], t.vertices[1], t.vertices[2]);
      writer->write_triangle(t);
    }
  }
  DEG_OBJECT_ITER_END;
}

void exporter_main(bContext *C, const STLExportParams &export_params)
{
  Depsgraph *depsgraph = nullptr;
  bool needs_free = false;

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (export_params.collection[0]) {
    Collection *collection = reinterpret_cast<Collection *>(
        BKE_libblock_find_name(bmain, ID_GR, export_params.collection));
    if (!collection) {
      BKE_reportf(export_params.reports,
                  RPT_ERROR,
                  "STL Export: Unable to find collection '%s'",
                  export_params.collection);
      return;
    }

    ViewLayer *view_layer = CTX_data_view_layer(C);

    depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
    needs_free = true;
    DEG_graph_build_from_collection(depsgraph, collection);
    BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  }
  else {
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  float scene_unit_scale = 1.0f;
  if ((scene->unit.system != USER_UNIT_NONE) && export_params.use_scene_unit) {
    scene_unit_scale = scene->unit.scale_length;
  }

  export_frame(depsgraph, scene_unit_scale, export_params);

  if (needs_free) {
    DEG_graph_free(depsgraph);
  }
}

}  // namespace blender::io::stl
