/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "OCIO_config.hh"

#include "fallback/fallback_config.hh"

#include "libocio/libocio_config.hh"

namespace blender::ocio {

std::unique_ptr<Config> Config::create_from_environment()
{
  return LibOCIOConfig::create_from_environment();
}

std::unique_ptr<Config> Config::create_fallback()
{
  return std::make_unique<FallbackConfig>();
}

}  // namespace blender::ocio
