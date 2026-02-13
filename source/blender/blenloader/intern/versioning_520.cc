/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"

#include "BLI_listbase_iterator.hh"
#include "BLI_sys_types.h"

#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"

namespace blender {

// static CLG_LogRef LOG = {"blend.doversion"};

/* Saving file extension is now a property of the the File Output node. So inherit this
 * setting from the active scene to restore the old behavior.
 * Note: One limitation is that node groups containing file outputs that are not part of any
 * scene are not affected by versioning. */
static void do_version_file_output_use_file_extension_recursive(bNodeTree &node_tree,
                                                                const Scene &scene)
{
  for (bNode &node : node_tree.nodes) {
    if (node.type_legacy == CMP_NODE_OUTPUT_FILE) {
      NodeCompositorFileOutput *data = static_cast<NodeCompositorFileOutput *>(node.storage);
      data->use_file_extension = (scene.r.scemode & R_EXTENSION) != 0;
    }
    else if (node.type_legacy == NODE_GROUP) {
      bNodeTree *ngroup = id_cast<bNodeTree *>(node.id);
      if (ngroup) {
        do_version_file_output_use_file_extension_recursive(*ngroup, scene);
      }
    }
  }
}

void do_versions_after_linking_520(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 2)) {
    for (Scene &scene : bmain->scenes) {
      bNodeTree *node_tree = version_get_scene_compositor_node_tree(bmain, &scene);
      if (node_tree == nullptr) {
        continue;
      }
      do_version_file_output_use_file_extension_recursive(*node_tree, scene);
    }
  }
  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_520(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 1)) {
    for (Scene &scene : bmain->scenes) {
      scene.r.mode |= R_SAVE_OUTPUT;
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
