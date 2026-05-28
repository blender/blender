/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

namespace blender::asset_system {

/**
 * Status of the asset's file(s) on disk, compared to the remote asset listing.
 */
enum class RemoteAssetFileStatus {
  /** Just so you can recognize a zero-initialized field of this type. */
  UNSET = 0,
  /** The asset's main file does not exist on disk. */
  NOT_ON_DISK = 1,
  /** All the asset's files exist on disk, and match the listing's hashes. */
  MATCH = 2,
  /** At least one of the asset's files exists on disk, but doesn't match the listing's hash. */
  NO_MATCH = 3,
  /* In the future there will likely be another option here: INCOMPLETE. It will indicate that the
   * asset's main file, which contains the asset datablock, exists, but the asset's other files do
   * not. As such, this will only be added when Blender supports multi-file assets. */
};

}  // namespace blender::asset_system
