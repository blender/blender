/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "OCIO_config.hh"

#include "fallback/fallback_config.hh"

#if defined(WITH_OPENCOLORIO)
#  include "libocio/libocio_config.hh"
#endif

namespace blender::ocio {

std::unique_ptr<Config> Config::create_from_environment()
{
#if defined(WITH_OPENCOLORIO)
  return LibOCIOConfig::create_from_environment();
#endif

  return nullptr;
}

std::unique_ptr<Config> Config::create_from_file(const StringRefNull filename)
{
#if defined(WITH_OPENCOLORIO)
  return LibOCIOConfig::create_from_file(filename);
#else
  (void)filename;
#endif

  return nullptr;
}

std::unique_ptr<Config> Config::create_fallback()
{
  return std::make_unique<FallbackConfig>();
}

}  // namespace blender::ocio
