/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include "BLI_timeit.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"

#include "IO_fbx.hh"
#include "fbx_import.hh"

#include <fmt/core.h>

using namespace blender::timeit;

static void report_duration(const char *job, const TimePoint &start_time, const char *path)
{
  Nanoseconds duration = Clock::now() - start_time;
  fmt::print("FBX {} of '{}' took ", job, BLI_path_basename(path));
  print_duration(duration);
  fmt::print("\n");
}

void FBX_import(bContext *C, const FBXImportParams &params)
{
  TimePoint start_time = Clock::now();
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  blender::io::fbx::importer_main(bmain, scene, view_layer, params);
  report_duration("import", start_time, params.filepath);
}
