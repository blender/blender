/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_bake_items_serialize.hh"
#include "BKE_simulation_state.hh"

#include "BLI_serialize.hh"

struct Main;
struct ModifierData;
struct Object;

namespace blender::bke::sim {

/**
 * Get the directory that contains all baked simulation data for the given modifier.
 */
std::string get_default_modifier_bake_directory(const Main &bmain,
                                                const Object &object,
                                                const ModifierData &md);

}  // namespace blender::bke::sim
