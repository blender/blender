/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <cstdio>
#include <memory>
#include <system_error>

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_scene_types.h"

#include "ED_object.hh"

#include "obj_export_mesh.hh"
#include "obj_export_nurbs.hh"
#include "obj_exporter.hh"

#include "obj_export_file_writer.hh"

namespace blender::io::obj {

OBJDepsgraph::OBJDepsgraph(const bContext *C, const eEvaluationMode eval_mode)
{
  Scene *scene = CTX_data_scene(C);
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (eval_mode == DAG_EVAL_RENDER) {
    depsgraph_ = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_RENDER);
    needs_free_ = true;
    DEG_graph_build_for_all_objects(depsgraph_);
    BKE_scene_graph_evaluated_ensure(depsgraph_, bmain);
  }
  else {
    depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C);
    needs_free_ = false;
  }
}

OBJDepsgraph::~OBJDepsgraph()
{
  if (needs_free_) {
    DEG_graph_free(depsgraph_);
  }
}

Depsgraph *OBJDepsgraph::get()
{
  return depsgraph_;
}

void OBJDepsgraph::update_for_newframe()
{
  BKE_scene_graph_update_for_newframe(depsgraph_);
}

static void print_exception_error(const std::system_error &ex)
{
  std::cerr << ex.code().category().name() << ": " << ex.what() << ": " << ex.code().message()
            << std::endl;
}

static bool is_curve_nurbs_compatible(const Nurb *nurb)
{
  while (nurb) {
    if (nurb->type == CU_BEZIER || nurb->pntsv != 1) {
      return false;
    }
    nurb = nurb->next;
  }
  return true;
}

/**
 * Filter supported objects from the Scene.
 *
 * \note Curves are also stored with Meshes if export settings specify so.
 */
std::pair<Vector<std::unique_ptr<OBJMesh>>, Vector<std::unique_ptr<OBJCurve>>>
filter_supported_objects(Depsgraph *depsgraph, const OBJExportParams &export_params)
{
  Vector<std::unique_ptr<OBJMesh>> r_exportable_meshes;
  Vector<std::unique_ptr<OBJCurve>> r_exportable_nurbs;
  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    if (export_params.export_selected_objects && !(object->base_flag & BASE_SELECTED)) {
      continue;
    }
    switch (object->type) {
      case OB_SURF:
        /* Evaluated surface objects appear as mesh objects from the iterator. */
        break;
      case OB_MESH:
        r_exportable_meshes.append(std::make_unique<OBJMesh>(depsgraph, export_params, object));
        break;
      case OB_CURVES_LEGACY: {
        Curve *curve = static_cast<Curve *>(object->data);
        Nurb *nurb{static_cast<Nurb *>(curve->nurb.first)};
        if (!nurb) {
          /* An empty curve. Not yet supported to export these as meshes. */
          if (export_params.export_curves_as_nurbs) {
            r_exportable_nurbs.append(
                std::make_unique<OBJCurve>(depsgraph, export_params, object));
          }
          break;
        }
        if (export_params.export_curves_as_nurbs && is_curve_nurbs_compatible(nurb)) {
          /* Export in parameter form: control points. */
          r_exportable_nurbs.append(std::make_unique<OBJCurve>(depsgraph, export_params, object));
        }
        else {
          /* Export in mesh form: edges and vertices. */
          r_exportable_meshes.append(std::make_unique<OBJMesh>(depsgraph, export_params, object));
        }
        break;
      }
      default:
        /* Other object types are not supported. */
        break;
    }
  }
  DEG_OBJECT_ITER_END;
  return {std::move(r_exportable_meshes), std::move(r_exportable_nurbs)};
}

static void write_mesh_objects(const Span<std::unique_ptr<OBJMesh>> exportable_as_mesh,
                               OBJWriter &obj_writer,
                               MTLWriter *mtl_writer,
                               const OBJExportParams &export_params)
{
  /* Parallelization is over meshes/objects, which means
   * we have to have the output text buffer for each object,
   * and write them all into the file at the end. */
  size_t count = exportable_as_mesh.size();
  Array<FormatHandler> buffers(count);

  /* Serial: gather material indices, ensure normals & edges. */
  Vector<Vector<int>> mtlindices;
  if (mtl_writer) {
    obj_writer.write_mtllib_name(mtl_writer->mtl_file_path());
    mtlindices.reserve(count);
  }
  for (auto &obj_mesh : exportable_as_mesh) {
    OBJMesh &obj = *obj_mesh;
    if (mtl_writer) {
      mtlindices.append(mtl_writer->add_materials(obj));
    }
  }

  /* Parallel over meshes: store normal coords & indices, uv coords and indices. */
  threading::parallel_for(IndexRange(count), 1, [&](IndexRange range) {
    for (const int i : range) {
      OBJMesh &obj = *exportable_as_mesh[i];
      if (export_params.export_normals) {
        obj.store_normal_coords_and_indices();
      }
      if (export_params.export_uv) {
        obj.store_uv_coords_and_indices();
      }
    }
  });

  /* Serial: calculate index offsets; these are sequentially added
   * over all meshes, and requite normal/uv indices to be calculated. */
  Vector<IndexOffsets> index_offsets;
  index_offsets.reserve(count);
  IndexOffsets offsets{0, 0, 0};
  for (auto &obj_mesh : exportable_as_mesh) {
    OBJMesh &obj = *obj_mesh;
    index_offsets.append(offsets);
    offsets.vertex_offset += obj.tot_vertices();
    offsets.uv_vertex_offset += obj.tot_uv_vertices();
    offsets.normal_offset += obj.get_normal_coords().size();
  }

  /* Parallel over meshes: main result writing. */
  threading::parallel_for(IndexRange(count), 1, [&](IndexRange range) {
    for (const int i : range) {
      OBJMesh &obj = *exportable_as_mesh[i];
      auto &fh = buffers[i];

      obj_writer.write_object_name(fh, obj);
      obj_writer.write_vertex_coords(fh, obj, export_params.export_colors);

      if (obj.tot_faces() > 0) {
        if (export_params.export_smooth_groups) {
          obj.calc_smooth_groups(export_params.smooth_groups_bitflags);
        }
        if (export_params.export_materials) {
          obj.calc_face_order();
        }
        if (export_params.export_normals) {
          obj_writer.write_normals(fh, obj);
        }
        if (export_params.export_uv) {
          obj_writer.write_uv_coords(fh, obj);
        }
        /* This function takes a 0-indexed slot index for the obj_mesh object and
         * returns the material name that we are using in the .obj file for it. */
        const auto *obj_mtlindices = mtlindices.is_empty() ? nullptr : &mtlindices[i];
        auto matname_fn = [&](int s) -> const char * {
          if (!obj_mtlindices || s < 0 || s >= obj_mtlindices->size()) {
            return nullptr;
          }
          return mtl_writer->mtlmaterial_name((*obj_mtlindices)[s]);
        };
        obj_writer.write_face_elements(fh, index_offsets[i], obj, matname_fn);
      }
      obj_writer.write_edges_indices(fh, index_offsets[i], obj);

      /* Nothing will need this object's data after this point, release
       * various arrays here. */
      obj.clear();
    }
  });

  /* Write all the object text buffers into the output file. */
  FILE *f = obj_writer.get_outfile();
  for (auto &b : buffers) {
    b.write_to_file(f);
  }
}

/**
 * Export NURBS Curves in parameter form, not as vertices and edges.
 */
static void write_nurbs_curve_objects(const Span<std::unique_ptr<OBJCurve>> exportable_as_nurbs,
                                      const OBJWriter &obj_writer)
{
  FormatHandler fh;
  /* #OBJCurve doesn't have any dynamically allocated memory, so it's fine
   * to wait for #blender::Vector to clean the objects up. */
  for (const std::unique_ptr<OBJCurve> &obj_curve : exportable_as_nurbs) {
    obj_writer.write_nurbs_curve(fh, *obj_curve);
  }
  fh.write_to_file(obj_writer.get_outfile());
}

void export_frame(Depsgraph *depsgraph, const OBJExportParams &export_params, const char *filepath)
{
  std::unique_ptr<OBJWriter> frame_writer = nullptr;
  try {
    frame_writer = std::make_unique<OBJWriter>(filepath, export_params);
  }
  catch (const std::system_error &ex) {
    print_exception_error(ex);
    BKE_reportf(export_params.reports, RPT_ERROR, "OBJ Export: Cannot open file '%s'", filepath);
    return;
  }
  if (!frame_writer) {
    BLI_assert(!"File should be writable by now.");
    return;
  }
  std::unique_ptr<MTLWriter> mtl_writer = nullptr;
  if (export_params.export_materials) {
    try {
      mtl_writer = std::make_unique<MTLWriter>(filepath);
    }
    catch (const std::system_error &ex) {
      print_exception_error(ex);
      BKE_reportf(export_params.reports,
                  RPT_WARNING,
                  "OBJ Export: Cannot create mtl file for '%s'",
                  filepath);
    }
  }

  frame_writer->write_header();

  auto [exportable_as_mesh, exportable_as_nurbs] = filter_supported_objects(depsgraph,
                                                                            export_params);

  write_mesh_objects(exportable_as_mesh, *frame_writer, mtl_writer.get(), export_params);
  if (mtl_writer) {
    mtl_writer->write_header(export_params.blen_filepath);
    char dest_dir[FILE_MAX];
    if (export_params.file_base_for_tests[0] == '\0') {
      BLI_path_split_dir_part(export_params.filepath, dest_dir, sizeof(dest_dir));
    }
    else {
      STRNCPY(dest_dir, export_params.file_base_for_tests);
    }
    BLI_path_slash_native(dest_dir);
    BLI_path_normalize(dest_dir);
    mtl_writer->write_materials(export_params.blen_filepath,
                                export_params.path_mode,
                                dest_dir,
                                export_params.export_pbr_extensions);
  }
  write_nurbs_curve_objects(exportable_as_nurbs, *frame_writer);
}

bool append_frame_to_filename(const char *filepath, const int frame, char *r_filepath_with_frames)
{
  BLI_strncpy(r_filepath_with_frames, filepath, FILE_MAX);
  BLI_path_extension_strip(r_filepath_with_frames);
  BLI_path_frame(r_filepath_with_frames, FILE_MAX, frame, 4);
  return BLI_path_extension_replace(r_filepath_with_frames, FILE_MAX, ".obj");
}

void exporter_main(bContext *C, const OBJExportParams &export_params)
{
  ED_object_mode_set(C, OB_MODE_OBJECT);
  OBJDepsgraph obj_depsgraph(C, export_params.export_eval_mode);
  Scene *scene = DEG_get_input_scene(obj_depsgraph.get());
  const char *filepath = export_params.filepath;

  /* Single frame export, i.e. no animation. */
  if (!export_params.export_animation) {
    fprintf(stderr, "Writing to %s\n", filepath);
    export_frame(obj_depsgraph.get(), export_params, filepath);
    return;
  }

  char filepath_with_frames[FILE_MAX];
  /* Used to reset the Scene to its original state. */
  const int original_frame = scene->r.cfra;

  for (int frame = export_params.start_frame; frame <= export_params.end_frame; frame++) {
    const bool filepath_ok = append_frame_to_filename(filepath, frame, filepath_with_frames);
    if (!filepath_ok) {
      fprintf(stderr, "Error: File Path too long.\n%s\n", filepath_with_frames);
      return;
    }

    scene->r.cfra = frame;
    obj_depsgraph.update_for_newframe();
    fprintf(stderr, "Writing to %s\n", filepath_with_frames);
    export_frame(obj_depsgraph.get(), export_params, filepath_with_frames);
  }
  scene->r.cfra = original_frame;
}
}  // namespace blender::io::obj
