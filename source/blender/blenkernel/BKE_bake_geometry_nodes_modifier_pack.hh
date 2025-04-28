/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_modifier_types.h"

#include "BKE_bake_items_paths.hh"
#include "BKE_packedFile.hh"

struct ReportList;
struct Main;

namespace blender::bke::bake {

NodesModifierPackedBake *pack_bake_from_disk(const BakePath &bake_path, ReportList *reports);

[[nodiscard]] bool unpack_bake_to_disk(const NodesModifierPackedBake &packed_bake,
                                       const BakePath &bake_path,
                                       ReportList *reports);

enum class PackGeometryNodesBakeResult {
  NoDataFound,
  PackedAlready,
  Success,
};

PackGeometryNodesBakeResult pack_geometry_nodes_bake(Main &bmain,
                                                     ReportList *reports,
                                                     Object &object,
                                                     NodesModifierData &nmd,
                                                     NodesModifierBake &bake);

enum class UnpackGeometryNodesBakeResult {
  BlendFileNotSaved,
  NoPackedData,
  Error,
  Success,
};

UnpackGeometryNodesBakeResult unpack_geometry_nodes_bake(Main &bmain,
                                                         ReportList *reports,
                                                         Object &object,
                                                         NodesModifierData &nmd,
                                                         NodesModifierBake &bake,
                                                         ePF_FileStatus how);

}  // namespace blender::bke::bake
