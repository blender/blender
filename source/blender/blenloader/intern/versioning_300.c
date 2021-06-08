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

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_genfile.h"
#include "DNA_listBase.h"
#include "DNA_modifier_types.h"
#include "DNA_text_types.h"

#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLO_readfile.h"
#include "readfile.h"

static void sort_linked_ids(Main *bmain)
{
  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ListBase temp_list;
    BLI_listbase_clear(&temp_list);
    LISTBASE_FOREACH_MUTABLE (ID *, id, lb) {
      if (ID_IS_LINKED(id)) {
        BLI_remlink(lb, id);
        BLI_addtail(&temp_list, id);
        id_sort_by_name(&temp_list, id, NULL);
      }
    }
    BLI_movelisttolist(lb, &temp_list);
  }
  FOREACH_MAIN_LISTBASE_END;
}

static void assert_sorted_ids(Main *bmain)
{
#ifndef NDEBUG
  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    ID *id_prev = NULL;
    LISTBASE_FOREACH (ID *, id, lb) {
      if (id_prev == NULL) {
        continue;
      }
      BLI_assert(id_prev->lib != id->lib || BLI_strcasecmp(id_prev->name, id->name) < 0);
    }
  }
  FOREACH_MAIN_LISTBASE_END;
#else
  UNUSED_VARS_NDEBUG(bmain);
#endif
}

void do_versions_after_linking_300(Main *bmain, ReportList *UNUSED(reports))
{
  if (MAIN_VERSION_ATLEAST(bmain, 300, 0) && !MAIN_VERSION_ATLEAST(bmain, 300, 1)) {
    /* Set zero user text objects to have a fake user. */
    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      if (text->id.us == 0) {
        id_fake_user_set(&text->id);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 3)) {
    /* Use new texture socket in Attribute Sample Texture node. */
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type != GEO_NODE_ATTRIBUTE_SAMPLE_TEXTURE) {
          continue;
        }
        if (node->id == NULL) {
          continue;
        }
        LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
          if (socket->type == SOCK_TEXTURE) {
            bNodeSocketValueTexture *socket_value = (bNodeSocketValueTexture *)
                                                        socket->default_value;
            socket_value->value = (Tex *)node->id;
            break;
          }
        }
        node->id = NULL;
      }
    }

    sort_linked_ids(bmain);
    assert_sorted_ids(bmain);
  }
  if (MAIN_VERSION_ATLEAST(bmain, 300, 3)) {
    assert_sorted_ids(bmain);
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_300 in this file.
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}

static void version_switch_node_input_prefix(Main *bmain)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
    if (ntree->type == NTREE_GEOMETRY) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_SWITCH) {
          LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
            /* Skip the "switch" socket. */
            if (socket == node->inputs.first) {
              continue;
            }
            strcpy(socket->name, socket->name[0] == 'A' ? "False" : "True");

            /* Replace "A" and "B", but keep the unique number suffix at the end. */
            char number_suffix[8];
            BLI_strncpy(number_suffix, socket->identifier + 1, sizeof(number_suffix));
            strcpy(socket->identifier, socket->name);
            strcat(socket->identifier, number_suffix);
          }
        }
      }
    }
  }
  FOREACH_NODETREE_END;
}

static void version_node_socket_name(bNodeTree *ntree,
                                     const int node_type,
                                     const char *old_name,
                                     const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (STREQ(socket->name, old_name)) {
          strcpy(socket->name, new_name);
        }
        if (STREQ(socket->identifier, old_name)) {
          strcpy(socket->identifier, new_name);
        }
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        if (STREQ(socket->name, old_name)) {
          strcpy(socket->name, new_name);
        }
        if (STREQ(socket->identifier, old_name)) {
          strcpy(socket->identifier, new_name);
        }
      }
    }
  }
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_300(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  if (!MAIN_VERSION_ATLEAST(bmain, 300, 1)) {
    /* Set default value for the new bisect_threshold parameter in the mirror modifier. */
    if (!DNA_struct_elem_find(fd->filesdna, "MirrorModifierData", "float", "bisect_threshold")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Mirror) {
            MirrorModifierData *mmd = (MirrorModifierData *)md;
            /* This was the previous hard-coded value. */
            mmd->bisect_threshold = 0.001f;
          }
        }
      }
    }
    /* Grease Pencil: Set default value for dilate pixels. */
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "int", "dilate_pixels")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->gpencil_settings) {
          brush->gpencil_settings->dilate_pixels = 1;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 300, 2)) {
    version_switch_node_input_prefix(bmain);

    if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "custom_scale_xyz[3]")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose == NULL) {
          continue;
        }
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          copy_v3_fl(pchan->custom_scale_xyz, pchan->custom_scale);
        }
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_BOUNDING_BOX, "Mesh", "Bounding Box");
      }
    }
    FOREACH_NODETREE_END;
  }
}
