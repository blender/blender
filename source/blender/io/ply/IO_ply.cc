/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <fmt/core.h>

#include "BLI_timeit.hh"
#include "DNA_windowmanager_types.h"
#include "IO_ply.hh"
#include "ply_export.hh"
#include "ply_import.hh"

using namespace blender::timeit;

static void report_duration(const char *job, const TimePoint &start_time, const char *path)
{
  Nanoseconds duration = Clock::now() - start_time;
  fmt::print("PLY {} of '{}' took ", job, BLI_path_basename(path));
  print_duration(duration);
  fmt::print("\n");
}

void PLY_export(bContext *C, const PLYExportParams &params)
{
  TimePoint start_time = Clock::now();
  blender::io::ply::exporter_main(C, params);
  report_duration("export", start_time, params.filepath);
}

void PLY_import(bContext *C, const PLYImportParams &params)
{
  TimePoint start_time = Clock::now();
  blender::io::ply::importer_main(C, params);
  report_duration("import", start_time, params.filepath);
}

Mesh *PLY_import_mesh(const PLYImportParams &params)
{
  return blender::io::ply::import_mesh(params);
}
