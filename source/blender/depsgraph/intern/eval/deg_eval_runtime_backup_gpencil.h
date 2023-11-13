/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

struct bGPdata;

namespace blender::deg {

struct Depsgraph;

/* Backup of volume datablocks runtime data. */
class GPencilBackup {
 public:
  GPencilBackup(const Depsgraph *depsgraph);

  void init_from_gpencil(bGPdata *gpd);
  void restore_to_gpencil(bGPdata *gpd);

  const Depsgraph *depsgraph;
};

}  // namespace blender::deg
