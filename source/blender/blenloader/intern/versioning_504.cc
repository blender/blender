/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 504, 2)) {
    for (Mesh &mesh : bmain->meshes) {
      if (mesh.radial_symmetry[0] > 1 || mesh.radial_symmetry[1] > 1 ||
          mesh.radial_symmetry[2] > 1)
      {
        mesh.symmetry |= ME_SYMMETRY_RADIAL;
      }
    }
  }

  /* Tip roundness was added to many brush types in 5.4. Some brushes of these types
   * saved before this change stored a meaningless tip_roundness.*/
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 504, 3)) {
    for (Brush &brush : bmain->brushes) {
      if (!(brush.ob_mode & OB_MODE_SCULPT)) {
        continue;
      }
      if (ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_BLOB,
               SCULPT_BRUSH_TYPE_CLAY,
               SCULPT_BRUSH_TYPE_CREASE,
               SCULPT_BRUSH_TYPE_DRAW,
               SCULPT_BRUSH_TYPE_DRAW_SHARP,
               SCULPT_BRUSH_TYPE_GRAB,
               SCULPT_BRUSH_TYPE_INFLATE,
               SCULPT_BRUSH_TYPE_LAYER,
               SCULPT_BRUSH_TYPE_NUDGE,
               SCULPT_BRUSH_TYPE_PINCH,
               SCULPT_BRUSH_TYPE_PLANE,
               SCULPT_BRUSH_TYPE_SCENE_PROJECT,
               SCULPT_BRUSH_TYPE_SLIDE_RELAX,
               SCULPT_BRUSH_TYPE_SMOOTH,
               SCULPT_BRUSH_TYPE_THUMB))
      {
        brush.tip_roundness = 1.0f;
        brush.tip_scale_x = 1.0f;
      }
    }
  }

  /* Default initialize proxy options.*/
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 504, 4)) {
    for (Scene &scene : bmain->scenes) {
      scene.r.compositor_proxy_preview_scale = 50;
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
