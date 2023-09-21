/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>
#include <string>

#include "BLI_string_ref.hh"
#include "BLI_sub_frame.hh"
#include "BLI_vector.hh"

namespace blender::bke::bake {

struct MetaFile {
  SubFrame frame;
  std::string path;
};

struct BakePath {
  /** Path to the directory containing the meta data per frame. */
  std::string meta_dir;
  /**
   * Path to the directory that contains the binary data. Could be shared between multiple bakes
   * to reduce memory consumption.
   */
  std::string blobs_dir;
  /**
   * Folder that is allowed to be deleted when the bake is deleted and it doesn't contain anything
   * else. Typically, this contains the meta and blob directories.
   */
  std::optional<std::string> bake_dir;

  static BakePath from_single_root(StringRefNull root_dir);
};

std::string frame_to_file_name(const SubFrame &frame);
std::optional<SubFrame> file_name_to_frame(const StringRefNull file_name);

Vector<MetaFile> find_sorted_meta_files(const StringRefNull meta_dir);

}  // namespace blender::bke::bake
