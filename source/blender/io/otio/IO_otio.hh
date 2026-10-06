/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup otio
 */

#pragma once

#include "BLI_path_utils.hh"

namespace blender {

struct bContext;
struct ReportList;
struct Scene;

struct OTIOExportParams {
  char filepath[FILE_MAX] = "";
  bool use_resolve_metadata = false;

  ReportList *reports = nullptr;
};

struct OTIOImportParams {
  char filepath[FILE_MAX] = "";

  ReportList *reports = nullptr;
};

void OTIO_export(bContext *C, const OTIOExportParams &params);
Scene *OTIO_import(bContext *C, const OTIOImportParams &params);

}  // namespace blender
