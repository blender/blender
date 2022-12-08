/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "IO_ply.h"

namespace blender::io::ply {

struct PLYExportParamsDefault : public PLYExportParams {
  PLYExportParamsDefault()
  {
    filepath[0] = '\0';
    file_base_for_tests[0] = '\0';
    blen_filepath = "";

    forward_axis = IO_AXIS_NEGATIVE_Z;
    up_axis = IO_AXIS_Y;
    global_scale = 1.f;

    apply_modifiers = true;
    export_selected_objects = false;
    export_uv = true;
    export_normals = true;
    export_colors = false;
    export_triangulated_mesh = false;

    ascii_format = false;
  }
};

}  // namespace blender::io::PLY
