/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "DNA_view3d_types.h"

#pragma once

/** \file
 * \ingroup bgrease_pencil
 */

struct ARegion;
struct View3D;
struct bContext;
struct Scene;
struct ReportList;
struct Depsgraph;

namespace blender::io::grease_pencil {

struct IOContext {
  ReportList *reports;
  bContext &C;
  const ARegion *region;
  const View3D *v3d;
  const RegionView3D *rv3d;
  Scene *scene;
  Depsgraph *depsgraph;

  IOContext(bContext &C,
            const ARegion *region,
            const View3D *v3d,
            const RegionView3D *rv3d,
            ReportList *reports);
};

struct ImportParams {
  float scale = 1.0f;
  int frame_number = 1;
  int resolution = 10;
  bool use_scene_unit = false;
  bool recenter_bounds = false;
};

struct ExportParams {
  /* Object to be exported. */
  enum class SelectMode {
    Active = 0,
    Selected = 1,
    Visible = 2,
  };

  /** Frame-range to be exported. */
  enum class FrameMode {
    Active = 0,
    Selected = 1,
    Scene = 2,
  };

  Object *object = nullptr;
  SelectMode select_mode = SelectMode::Active;
  FrameMode frame_mode = FrameMode::Active;
  bool export_stroke_materials = true;
  bool export_fill_materials = true;
  /* Clip drawings to camera size when exporting in camera view. */
  bool use_clip_camera = false;
  /* Enforce uniform stroke width by averaging radius. */
  bool use_uniform_width = false;
  /* Distance for resampling outline curves before export, disabled if zero. */
  float outline_resample_length = 0.0f;
};

bool import_svg(const IOContext &context, const ImportParams &params, StringRefNull filepath);
bool export_svg(const IOContext &context,
                const ExportParams &params,
                Scene &scene,
                StringRefNull filepath);
bool export_pdf(const IOContext &context,
                const ExportParams &params,
                Scene &scene,
                StringRefNull filepath);

}  // namespace blender::io::grease_pencil
