/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BLI_timeit.hh"

#include "DNA_windowmanager_types.h"
#include "IO_ply.h"
#include "ply_export.hh"
#include "ply_import.hh"

using namespace blender::timeit;

static void report_duration(const char *job, const TimePoint &start_time, const char *path)
{
  Nanoseconds duration = Clock::now() - start_time;
  std::cout << "PLY " << job << " of '" << BLI_path_basename(path) << "' took ";
  print_duration(duration);
  std::cout << '\n';
}

void PLY_export(bContext *C, const PLYExportParams *export_params)
{
  TimePoint start_time = Clock::now();
  blender::io::ply::exporter_main(C, *export_params);
  report_duration("export", start_time, export_params->filepath);
}

void PLY_import(bContext *C, const PLYImportParams *import_params, wmOperator *op)
{
  TimePoint start_time = Clock::now();
  blender::io::ply::importer_main(C, *import_params, op);
  report_duration("import", start_time, import_params->filepath);
}
