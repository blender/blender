/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_node_runtime.hh"
#include "BKE_pointcloud.h"
#include "BKE_simulation_state_serialize.hh"

#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_path_util.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

namespace blender::bke::sim {

/**
 * Turn the name into something that can be used as file name. It does not necessarily have to be
 * human readable, but it can help if it is at least partially readable.
 */
static std::string escape_name(const StringRef name)
{
  std::stringstream ss;
  for (const char c : name) {
    /* Only some letters allowed. Digits are not because they could lead to name collisions. */
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
      ss << c;
    }
    else {
      ss << int(c);
    }
  }
  return ss.str();
}

static std::string get_blend_file_name(const Main &bmain)
{
  const StringRefNull blend_file_path = BKE_main_blendfile_path(&bmain);
  char blend_name[FILE_MAX];

  BLI_path_split_file_part(blend_file_path.c_str(), blend_name, sizeof(blend_name));
  const int64_t type_start_index = StringRef(blend_name).rfind(".");
  if (type_start_index == StringRef::not_found) {
    return "";
  }
  blend_name[type_start_index] = '\0';
  return "blendcache_" + StringRef(blend_name);
}

static std::string get_modifier_sim_name(const Object &object, const ModifierData &md)
{
  const std::string object_name_escaped = escape_name(object.id.name + 2);
  const std::string modifier_name_escaped = escape_name(md.name);
  return "sim_" + object_name_escaped + "_" + modifier_name_escaped;
}

std::string get_default_modifier_bake_directory(const Main &bmain,
                                                const Object &object,
                                                const ModifierData &md)
{
  char dir[FILE_MAX];
  /* Make path that's relative to the .blend file. */
  BLI_path_join(dir,
                sizeof(dir),
                "//",
                get_blend_file_name(bmain).c_str(),
                get_modifier_sim_name(object, md).c_str());
  return dir;
}

/**
 * Version written to the baked data.
 */
static constexpr int serialize_format_version = 2;

void serialize_modifier_simulation_state(const ModifierSimulationState &state,
                                         BDataWriter &bdata_writer,
                                         BDataSharing &bdata_sharing,
                                         DictionaryValue &r_io_root)
{
  r_io_root.append_int("version", serialize_format_version);
  auto io_zones = r_io_root.append_array("zones");

  for (const auto item : state.zone_states_.items()) {
    const SimulationZoneID &zone_id = item.key;
    const SimulationZoneState &zone_state = *item.value;

    auto io_zone = io_zones->append_dict();

    io_zone->append_int("state_id", zone_id.nested_node_id);

    auto io_state_items = io_zone->append_array("state_items");
    for (const MapItem<int, std::unique_ptr<BakeItem>> &state_item_with_id :
         zone_state.item_by_identifier.items())
    {
      auto io_state_item = io_state_items->append_dict();

      io_state_item->append_int("id", state_item_with_id.key);
      serialize_bake_item(*state_item_with_id.value, bdata_writer, bdata_sharing, *io_state_item);
    }
  }
}

void deserialize_modifier_simulation_state(const bNodeTree &ntree,
                                           const DictionaryValue &io_root,
                                           const BDataReader &bdata_reader,
                                           const BDataSharing &bdata_sharing,
                                           ModifierSimulationState &r_state)
{
  io::serialize::JsonFormatter formatter;
  const std::optional<int> version = io_root.lookup_int("version");
  if (!version) {
    return;
  }
  if (*version > serialize_format_version) {
    return;
  }
  const io::serialize::ArrayValue *io_zones = io_root.lookup_array("zones");
  if (!io_zones) {
    return;
  }
  for (const auto &io_zone_value : io_zones->elements()) {
    const DictionaryValue *io_zone = io_zone_value->as_dictionary_value();
    if (!io_zone) {
      continue;
    }
    bke::sim::SimulationZoneID zone_id;
    if (const std::optional<int> state_id = io_zone->lookup_int("state_id")) {
      zone_id.nested_node_id = *state_id;
    }
    else if (const io::serialize::ArrayValue *io_zone_id = io_zone->lookup_array("zone_id")) {
      /* In the initial release of simulation nodes, the entire node id path was written to the
       * baked data. For backward compatibility the node ids are read here and then the nested node
       * id is looked up. */
      Vector<int> node_ids;
      for (const auto &io_zone_id_element : io_zone_id->elements()) {
        const io::serialize::IntValue *io_node_id = io_zone_id_element->as_int_value();
        if (!io_node_id) {
          continue;
        }
        node_ids.append(io_node_id->value());
      }
      const bNestedNodeRef *nested_node_ref = ntree.nested_node_ref_from_node_id_path(node_ids);
      if (!nested_node_ref) {
        continue;
      }
      zone_id.nested_node_id = nested_node_ref->id;
    }

    const io::serialize::ArrayValue *io_state_items = io_zone->lookup_array("state_items");
    if (!io_state_items) {
      continue;
    }

    auto zone_state = std::make_unique<bke::sim::SimulationZoneState>();

    for (const auto &io_state_item_value : io_state_items->elements()) {
      const DictionaryValue *io_state_item = io_state_item_value->as_dictionary_value();
      if (!io_state_item) {
        continue;
      }
      const std::optional<int> state_item_id = io_state_item->lookup_int("id");
      if (!state_item_id) {
        continue;
      }

      std::unique_ptr<BakeItem> new_state_item = deserialize_bake_item(
          *io_state_item, bdata_reader, bdata_sharing);

      BLI_assert(new_state_item);
      zone_state->item_by_identifier.add(*state_item_id, std::move(new_state_item));
    }

    r_state.zone_states_.add_overwrite(zone_id, std::move(zone_state));
  }
}

}  // namespace blender::bke::sim
