/* SPDX-FileCopyrightText: 2008-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief String utilities
 */

// soc #include <qfileinfo.h>

#include "StringUtils.h"
#include "FreestyleConfig.h"

#include "BLI_sys_types.h"

namespace Freestyle::StringUtils {

void getPathName(const string &path, const string &base, vector<string> &pathnames)
{
  string dir;
  string res;
  char cleaned[FILE_MAX];
  uint size = path.size();

  pathnames.push_back(base);

  for (uint pos = 0, sep = path.find(Config::PATH_SEP, pos); pos < size;
       pos = sep + 1, sep = path.find(Config::PATH_SEP, pos))
  {
    if (sep == uint(string::npos)) {
      sep = size;
    }

    dir = path.substr(pos, sep - pos);

    STRNCPY(cleaned, dir.c_str());
    BLI_path_normalize(cleaned);
    res = string(cleaned);

    if (!base.empty()) {
      res += Config::DIR_SEP + base;
    }

    pathnames.push_back(res);
  }
}

}  // namespace Freestyle::StringUtils
