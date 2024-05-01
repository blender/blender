/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_utility_mixins.hh"

#include "BLI_vector.hh"

#include "IO_wavefront_obj.hh"

struct bContext;
struct Collection;

namespace blender::io::obj {

/**
 * Behaves like `std::unique_ptr<Depsgraph, custom_deleter>`.
 * Needed to free a new Depsgraph created for #DAG_EVAL_RENDER.
 */
class OBJDepsgraph : NonMovable, NonCopyable {
 private:
  Depsgraph *depsgraph_ = nullptr;
  bool needs_free_ = false;

 public:
  OBJDepsgraph(const bContext *C, eEvaluationMode eval_mode, Collection *collection);
  ~OBJDepsgraph();

  Depsgraph *get();
  void update_for_newframe();
};

/**
 * The main function for exporting a .obj file according to the given `export_parameters`.
 * It uses the context `C` to get the dependency graph, and from that, the `Scene`.
 * Depending on whether or not `export_params.export_animation` is set, it writes
 * either one file per animation frame, or just one file.
 */
/**
 * Central internal function to call Scene update & writer functions.
 */
void exporter_main(bContext *C, const OBJExportParams &export_params);

class OBJMesh;
class OBJCurve;

/**
 * Export a single frame of a .obj file, according to the given `export_parameters`.
 * The frame state is given in `depsgraph`.
 * The output file name is given by `filepath`.
 * This function is normally called from `exporter_main`, but is exposed here for testing purposes.
 */
/**
 * Export a single frame to a .OBJ file.
 *
 * Conditionally write a .MTL file also.
 */
void export_frame(Depsgraph *depsgraph,
                  const OBJExportParams &export_params,
                  const char *filepath);

/**
 * Find the objects to be exported in the `view_layer` of the dependency graph`depsgraph`,
 * and return them in vectors `unique_ptr`s of `OBJMesh` and `OBJCurve`.
 * If `export_params.export_selected_objects` is set, then only selected objects are to be
 * exported, else all objects are to be exported. But only objects of type `OB_MESH`,
 * `OB_CURVES_LEGACY`, and `OB_SURF` are supported; the rest will be ignored. If
 * `export_params.export_curves_as_nurbs` is set, then curves of type `CU_NURBS` are exported in
 * curve form in the .obj file, otherwise they are converted to mesh and returned in the `OBJMesh`
 * vector. All other exportable types are always converted to mesh and returned in the `OBJMesh`
 * vector.
 */
std::pair<Vector<std::unique_ptr<OBJMesh>>, Vector<std::unique_ptr<OBJCurve>>>
filter_supported_objects(Depsgraph *depsgraph, const OBJExportParams &export_params);

/**
 * Makes `r_filepath_with_frames` (which should point at a character array of size `FILE_MAX`)
 * be `filepath` with its "#" characters replaced by the number representing `frame`, and with
 * a .obj extension.
 */
/**
 * Append the current frame number in the .OBJ file name.
 *
 * \return Whether the filepath is in #FILE_MAX limits.
 */
bool append_frame_to_filename(const char *filepath, int frame, char *r_filepath_with_frames);
}  // namespace blender::io::obj
