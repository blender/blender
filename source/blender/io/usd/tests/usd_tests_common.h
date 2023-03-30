/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */
#pragma once

#include <string>

namespace blender::io::usd {

/* Calls the function to load the USD plugins from the
 * USD data directory under the Blender bin directory
 * that was supplied as the --test-release-dir flag to `ctest`.
 * Thus function must be called before instantiating a USD
 * stage to avoid errors.  The returned string is the path to
 * the USD data files directory from which the plugins were
 * loaded. If the USD data files directory can't be determined,
 * plugin registration is skipped and the empty string is
 * returned. */
std::string register_usd_plugins_for_tests();

}  // namespace blender::io::usd
