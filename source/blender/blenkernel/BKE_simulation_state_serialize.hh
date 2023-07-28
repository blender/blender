/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_bake_items_serialize.hh"
#include "BKE_simulation_state.hh"

#include "BLI_serialize.hh"

struct Main;
struct ModifierData;

namespace blender {
class fstream;
}

namespace blender::bke::sim {

using DictionaryValue = io::serialize::DictionaryValue;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

/**
 * Get the directory that contains all baked simulation data for the given modifier.
 */
std::string get_default_modifier_bake_directory(const Main &bmain,
                                                const Object &object,
                                                const ModifierData &md);

/**
 * Encode the simulation state in a #DictionaryValue which also contains references to external
 * binary data that has been written using #bdata_writer.
 */
void serialize_modifier_simulation_state(const ModifierSimulationState &state,
                                         BDataWriter &bdata_writer,
                                         BDataSharing &bdata_sharing,
                                         DictionaryValue &r_io_root);
/**
 * Fill the simulation state by parsing the provided #DictionaryValue which also contains
 * references to external binary data that is read using #bdata_reader.
 */
void deserialize_modifier_simulation_state(const bNodeTree &ntree,
                                           const DictionaryValue &io_root,
                                           const BDataReader &bdata_reader,
                                           const BDataSharing &bdata_sharing,
                                           ModifierSimulationState &r_state);

}  // namespace blender::bke::sim
