/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include <sstream>

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

}  // namespace blender::bke::sim
