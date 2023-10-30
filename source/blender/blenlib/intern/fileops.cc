/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_fileops.hh"

#ifdef WIN32
#  include "utfconv.h"
#endif

namespace blender {
fstream::fstream(const char *filepath, std::ios_base::openmode mode)
{
  this->open(filepath, mode);
}

fstream::fstream(const std::string &filepath, std::ios_base::openmode mode)
{
  this->open(filepath, mode);
}

void fstream::open(StringRefNull filepath, ios_base::openmode mode)
{
#ifdef WIN32
  const char *filepath_cstr = filepath.c_str();
  UTF16_ENCODE(filepath_cstr);
  std::wstring filepath_wstr(filepath_cstr_16);
  std::fstream::open(filepath_wstr.c_str(), mode);
  UTF16_UN_ENCODE(filepath_cstr);
#else
  std::fstream::open(filepath, mode);
#endif
}

}  // namespace blender
