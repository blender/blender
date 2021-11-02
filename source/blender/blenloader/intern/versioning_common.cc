/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup blenloader
 */
/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <cstring>

#include "DNA_node_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "versioning_common.h"

ARegion *do_versions_add_region_if_not_found(ListBase *regionbase,
                                             int region_type,
                                             const char *name,
                                             int link_after_region_type)
{
  ARegion *link_after_region = nullptr;
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == region_type) {
      return nullptr;
    }
    if (region->regiontype == link_after_region_type) {
      link_after_region = region;
    }
  }

  ARegion *new_region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), name));
  new_region->regiontype = region_type;
  BLI_insertlinkafter(regionbase, link_after_region, new_region);
  return new_region;
}

/**
 * Rename if the ID doesn't exist.
 *
 * \return the ID (if found).
 */
ID *do_versions_rename_id(Main *bmain,
                          const short id_type,
                          const char *name_src,
                          const char *name_dst)
{
  /* We can ignore libraries */
  ListBase *lb = which_libbase(bmain, id_type);
  ID *id = nullptr;
  LISTBASE_FOREACH (ID *, idtest, lb) {
    if (idtest->lib == nullptr) {
      if (STREQ(idtest->name + 2, name_src)) {
        id = idtest;
      }
      if (STREQ(idtest->name + 2, name_dst)) {
        return nullptr;
      }
    }
  }
  if (id != nullptr) {
    BLI_strncpy(id->name + 2, name_dst, sizeof(id->name) - 2);
    /* We know it's unique, this just sorts. */
    BLI_libblock_ensure_unique_name(bmain, id->name);
  }
  return id;
}

static void change_node_socket_name(ListBase *sockets, const char *old_name, const char *new_name)
{
  LISTBASE_FOREACH (bNodeSocket *, socket, sockets) {
    if (STREQ(socket->name, old_name)) {
      BLI_strncpy(socket->name, new_name, sizeof(socket->name));
    }
    if (STREQ(socket->identifier, old_name)) {
      BLI_strncpy(socket->identifier, new_name, sizeof(socket->name));
    }
  }
}

void version_node_socket_name(bNodeTree *ntree,
                              const int node_type,
                              const char *old_name,
                              const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      change_node_socket_name(&node->inputs, old_name, new_name);
      change_node_socket_name(&node->outputs, old_name, new_name);
    }
  }
}

void version_node_input_socket_name(bNodeTree *ntree,
                                    const int node_type,
                                    const char *old_name,
                                    const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      change_node_socket_name(&node->inputs, old_name, new_name);
    }
  }
}

void version_node_output_socket_name(bNodeTree *ntree,
                                    const int node_type,
                                    const char *old_name,
                                    const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      change_node_socket_name(&node->outputs, old_name, new_name);
    }
  }
}

/**
 * Replace the ID name of all nodes in the tree with the given type with the new name.
 */
void version_node_id(bNodeTree *ntree, const int node_type, const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      if (!STREQ(node->idname, new_name)) {
        strcpy(node->idname, new_name);
      }
    }
  }
}

/**
 * Adjust animation data for newly added node sockets.
 *
 * Node sockets are addressed by their index (in their RNA path, and thus FCurves/drivers), and
 * thus when a new node is added in the middle of the list, existing animation data needs to be
 * adjusted.
 *
 * Since this is about animation data, it only concerns input sockets.
 *
 * \param node_tree_type node tree type that has these nodes, for example NTREE_SHADER.
 * \param node_type node type to adjust, for example SH_NODE_BSDF_PRINCIPLED.
 * \param socket_index_orig the original index of the moved socket; when socket 4 moved to 6,
 * pass 4 here.
 * \param socket_index_offset the offset of the nodes, so when socket 4 moved to 6,
 * pass 2 here.
 * \param total_number_of_sockets the total number of sockets in the node.
 */
void version_node_socket_index_animdata(Main *bmain,
                                        const int node_tree_type,
                                        const int node_type,
                                        const int socket_index_orig,
                                        const int socket_index_offset,
                                        const int total_number_of_sockets)
{

  /* The for loop for the input ids is at the top level otherwise we lose the animation
   * keyframe data. Not sure what causes that, so I (Sybren) moved the code here from
   * versioning_290.c as-is (structure-wise). */
  for (int input_index = total_number_of_sockets - 1; input_index >= socket_index_orig;
       input_index--) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, owner_id) {
      if (ntree->type != node_tree_type) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type != node_type) {
          continue;
        }

        const size_t node_name_length = strlen(node->name);
        const size_t node_name_escaped_max_length = (node_name_length * 2);
        char *node_name_escaped = (char *)MEM_mallocN(node_name_escaped_max_length + 1,
                                                      "escaped name");
        BLI_str_escape(node_name_escaped, node->name, node_name_escaped_max_length);
        char *rna_path_prefix = BLI_sprintfN("nodes[\"%s\"].inputs", node_name_escaped);

        const int new_index = input_index + socket_index_offset;
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, owner_id, rna_path_prefix, nullptr, nullptr, input_index, new_index, false);
        MEM_freeN(rna_path_prefix);
        MEM_freeN(node_name_escaped);
      }
    }
    FOREACH_NODETREE_END;
  }
}
