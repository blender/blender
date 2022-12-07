/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "IO_ply.h"

namespace blender::io::ply {

struct PLYExportParamsDefault {
  PLYExportParams params;
  PLYExportParamsDefault()
  {
    params.filepath[0] = '\0';
    params.file_base_for_tests[0] = '\0';
    params.blen_filepath = "";

    params.forward_axis = IO_AXIS_NEGATIVE_Z;
    params.up_axis = IO_AXIS_Y;
    params.global_scale = 1.f;

    params.apply_modifiers = true;
    params.export_selected_objects = false;
    params.export_uv = true;
    params.export_normals = true;
    params.export_colors = false;
    params.export_triangulated_mesh = false;

    params.ascii_format = false;
  }
};

}  // namespace blender::io::PLY
