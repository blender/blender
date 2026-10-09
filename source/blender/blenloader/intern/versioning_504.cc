/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"

#include "BLI_listbase.hh"
#include "BLI_sys_types.hh"

#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"

namespace blender {

// static CLG_LogRef LOG = {"blend.doversion"};

void do_versions_after_linking_504(FileData * /*fd*/, Main * /*bmain*/)
{
  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_504(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  /* Disable "Set Background Value" input to keep old behavior of Deactivate Voxels node. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 504, 1)) {
    for (bNodeTree &tree : bmain->nodetrees) {
      if (tree.type != NTREE_GEOMETRY) {
        continue;
      }
      for (bNode &node : tree.nodes) {
        if (STREQ(node.idname, "GeometryNodeGridDeactivateVoxels")) {
          if (!bke::node_find_socket(node, SOCK_IN, "Set Background Value"_ustr)) {
            bNodeSocket &input = version_node_add_socket(
                tree, node, SOCK_IN, "NodeSocketBool", "Set Background Value");
            input.default_value_typed<bNodeSocketValueBoolean>()->value = false;
          }
        }
      }
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

}  // namespace blender
