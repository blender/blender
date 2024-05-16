/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_report.hh"

#include "DNA_customdata_types.h"

#include "usd.hh"

#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/usd/usdGeom/primvar.h>

#include <optional>

struct Mesh;
struct ReportList;

namespace blender::io::usd {

void read_color_data_primvar(Mesh *mesh,
                             const pxr::UsdGeomPrimvar &color_primvar,
                             double motion_sample_time,
                             ReportList *reports,
                             bool is_left_handed);

}  // namespace blender::io::usd
