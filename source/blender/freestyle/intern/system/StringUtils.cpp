/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief String utilities
 */

// soc #include <qfileinfo.h>

#include "StringUtils.h"
#include "FreestyleConfig.h"

namespace Freestyle::StringUtils {

void getPathName(const string &path, const string &base, vector<string> &pathnames)
{
  string dir;
  string res;
  char cleaned[FILE_MAX];
  unsigned size = path.size();

  pathnames.push_back(base);

  for (unsigned int pos = 0, sep = path.find(Config::PATH_SEP, pos); pos < size;
       pos = sep + 1, sep = path.find(Config::PATH_SEP, pos)) {
    if (sep == (unsigned)string::npos) {
      sep = size;
    }

    dir = path.substr(pos, sep - pos);

    BLI_strncpy(cleaned, dir.c_str(), FILE_MAX);
    BLI_path_normalize(nullptr, cleaned);
    res = string(cleaned);

    if (!base.empty()) {
      res += Config::DIR_SEP + base;
    }

    pathnames.push_back(res);
  }
}

}  // namespace Freestyle::StringUtils
