/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief File and directory operations.
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++ header
#endif

#include "BLI_fileops.h"
#include "BLI_string_ref.hh"

#include <fstream>
#include <string>

namespace blender {

/**
 * std::fstream subclass that handles UTF-16 encoding on Windows.
 *
 * For documentation, see https://en.cppreference.com/w/cpp/io/basic_fstream
 */
class fstream : public std::fstream {
 public:
  fstream() = default;
  explicit fstream(const char *filepath,
                   std::ios_base::openmode mode = ios_base::in | ios_base::out);
  explicit fstream(const std::string &filepath,
                   std::ios_base::openmode mode = ios_base::in | ios_base::out);

  void open(StringRefNull filepath, ios_base::openmode mode = ios_base::in | ios_base::out);
};

}  // namespace blender
