/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief String utilities
 */

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "BLI_path_util.h"
#include "BLI_string.h"

using namespace std;

namespace Freestyle {

namespace StringUtils {

void getPathName(const string &path, const string &base, vector<string> &pathnames);

// STL related
struct ltstr {
  bool operator()(const char *s1, const char *s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};

}  // end of namespace StringUtils

} /* namespace Freestyle */
