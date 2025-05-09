/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "OCIO_version.hh"

#include "opencolorio.hh"

namespace blender::ocio {

Version get_version()
{
#ifdef WITH_OPENCOLORIO
  const int version_hex = OCIO_NAMESPACE::GetVersionHex();
#else
  const int version_hex = 0;
#endif
  return {version_hex >> 24, (version_hex >> 16) & 0xff, (version_hex >> 8) & 0xff};
}

}  // namespace blender::ocio
