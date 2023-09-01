/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>

#include "BKE_collection.h"
#include "BKE_curves.hh"
#include "BKE_main.h"
#include "BKE_simulation_state.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_binary_search.hh"
#include "BLI_fileops.hh"
#include "BLI_hash_md5.h"
#include "BLI_path_util.h"
#include "BLI_string_utils.h"

#include "MOD_nodes.hh"

namespace blender::bke::sim {

void SimulationZoneCache::reset()
{
  this->frame_caches.clear();
  this->prev_state.reset();
  this->bdata_dir.reset();
  this->bdata_sharing.reset();
  this->failed_finding_bake = false;
  this->cache_state = CacheState::Valid;
}

void scene_simulation_states_reset(Scene &scene)
{
  FOREACH_SCENE_OBJECT_BEGIN (&scene, ob) {
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type != eModifierType_Nodes) {
        continue;
      }
      NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
      if (!nmd->runtime->simulation_cache) {
        continue;
      }
      for (auto item : nmd->runtime->simulation_cache->cache_by_zone_id.items()) {
        item.value->reset();
      }
    }
  }
  FOREACH_SCENE_OBJECT_END;
}

std::optional<std::string> get_modifier_simulation_bake_path(const Main &bmain,
                                                             const Object &object,
                                                             const NodesModifierData &nmd)
{
  const StringRefNull bmain_path = BKE_main_blendfile_path(&bmain);
  if (bmain_path.is_empty()) {
    return std::nullopt;
  }
  if (StringRef(nmd.simulation_bake_directory).is_empty()) {
    return std::nullopt;
  }
  const char *base_path = ID_BLEND_PATH(&bmain, &object.id);
  char absolute_bake_dir[FILE_MAX];
  STRNCPY(absolute_bake_dir, nmd.simulation_bake_directory);
  BLI_path_abs(absolute_bake_dir, base_path);
  return absolute_bake_dir;
}

std::optional<bake_paths::BakePath> get_simulation_zone_bake_path(const Main &bmain,
                                                                  const Object &object,
                                                                  const NodesModifierData &nmd,
                                                                  int zone_id)
{
  const std::optional<std::string> modifier_bake_path = get_modifier_simulation_bake_path(
      bmain, object, nmd);
  if (!modifier_bake_path) {
    return std::nullopt;
  }

  char zone_bake_dir[FILE_MAX];
  BLI_path_join(zone_bake_dir,
                sizeof(zone_bake_dir),
                modifier_bake_path->c_str(),
                std::to_string(zone_id).c_str());
  return bake_paths::BakePath::from_single_root(zone_bake_dir);
}

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

}  // namespace blender::bke::sim
