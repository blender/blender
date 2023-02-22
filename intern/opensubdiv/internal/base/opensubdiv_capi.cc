// Copyright 2013 Blender Foundation
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

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
