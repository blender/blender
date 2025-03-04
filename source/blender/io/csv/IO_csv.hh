/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup csv
 */

#pragma once

#include "BLI_path_utils.hh"

struct PointCloud;
struct ReportList;

namespace blender::io::csv {

struct CSVImportParams {
  /** Full path to the source CSV file to import. */
  char filepath[FILE_MAX];
  char delimiter = ',';

  ReportList *reports = nullptr;
};

PointCloud *import_csv_as_pointcloud(const CSVImportParams &import_params);

}  // namespace blender::io::csv
