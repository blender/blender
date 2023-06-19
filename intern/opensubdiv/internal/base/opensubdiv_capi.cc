/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "opensubdiv_capi.h"
#include "opensubdiv/version.h"
#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include "internal/base/util.h"

void openSubdiv_init() {}

void openSubdiv_cleanup() {}

int openSubdiv_getVersionHex()
{
#if defined(OPENSUBDIV_VERSION_NUMBER)
  return OPENSUBDIV_VERSION_NUMBER;
#elif defined(OPENSUBDIV_VERSION_MAJOR)
  return OPENSUBDIV_VERSION_MAJOR * 10000 + OPENSUBDIV_VERSION_MINOR * 100 +
         OPENSUBDIV_VERSION_PATCH;
#elif defined(OPENSUBDIV_VERSION)
  const char *version = STRINGIFY(OPENSUBDIV_VERSION);
  if (version[0] == 'v') {
    version += 1;
  }
  int major = 0, minor = 0, patch = 0;
  vector<string> tokens;
  blender::opensubdiv::stringSplit(&tokens, version, "_", true);
  if (tokens.size() == 3) {
    major = atoi(tokens[0].c_str());
    minor = atoi(tokens[1].c_str());
    patch = atoi(tokens[2].c_str());
  }
  return major * 10000 + minor * 100 + patch;
#else
  return 0;
#endif
}
