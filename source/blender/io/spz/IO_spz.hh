/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#pragma once

#include <string>

namespace blender {

struct bContext;
struct PointCloud;
struct ReportList;

struct SPZImportParams {
  /* Full path to the source SPZ file to import. */
  std::string filepath;

  ReportList *reports = nullptr;
};

void SPZ_import(bContext *C, const SPZImportParams *import_params);
PointCloud *SPZ_import_point_cloud(const SPZImportParams *import_params);

}  // namespace blender
