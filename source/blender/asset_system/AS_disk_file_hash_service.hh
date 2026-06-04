/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "BLI_string_ref.hh"

#include <memory>
#include <string>

namespace blender {
struct bContext;
}

namespace blender::asset_system {

/**
 * C++ wrapper around the DiskFileHashService class implemented in Python.
 *
 * Run the following to see which hash algorithms are supported:
 *
 * `blender -b --python-expr "import hashlib; print(hashlib.algorithms_available)"`
 */
class DiskFileHashService {
 private:
  std::string storage_path_;

 public:
  explicit DiskFileHashService(StringRef storage_path);
  ~DiskFileHashService();

  /** Return the hash of a file on disk. */
  std::string get_hash(StringRef filepath, StringRef hash_algorithm);

  /** Check the file on disk, to see if it matches the given properties. */
  bool file_matches(StringRef filepath,
                    StringRef hash_algorithm,
                    StringRef hexhash,
                    int64_t size_in_bytes);

 private:
  /** Release the Python instance associated with this DFHS. */
  void release_python();
};

/**
 * Obtain a DiskFileHashService, which stores its cache at the given location.
 */
std::unique_ptr<DiskFileHashService> disk_file_hash_service_get(StringRef storage_path);

}  // namespace blender::asset_system
