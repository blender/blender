/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BLI_path_util.h"
#include "BLI_timeit.hh"

#include "IO_wavefront_obj.h"

#include "obj_exporter.hh"
#include "obj_importer.hh"

using namespace blender::timeit;

static void report_duration(const char *job, const TimePoint &start_time, const char *path)
{
  Nanoseconds duration = Clock::now() - start_time;
  std::cout << "OBJ " << job << " of '" << BLI_path_basename(path) << "' took ";
  print_duration(duration);
  std::cout << '\n';
}

void OBJ_export(bContext *C, const OBJExportParams *export_params)
{
  TimePoint start_time = Clock::now();
  blender::io::obj::exporter_main(C, *export_params);
  report_duration("export", start_time, export_params->filepath);
}

void OBJ_import(bContext *C, const OBJImportParams *import_params)
{
  TimePoint start_time = Clock::now();
  blender::io::obj::importer_main(C, *import_params);
  report_duration("import", start_time, import_params->filepath);
}
