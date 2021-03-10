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

#include <cstring>

#include "MEM_guardedalloc.h"

#include "NOD_geometry.h"

#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_object.h"

#include "BLT_translation.h"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"

#include "node_common.h"

bNodeTreeType *ntreeType_Geometry;

static void geometry_node_tree_get_from_context(const bContext *C,
                                                bNodeTreeType *UNUSED(treetype),
                                                bNodeTree **r_ntree,
                                                ID **r_id,
                                                ID **r_from)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);

  if (ob == nullptr) {
    return;
  }

  const ModifierData *md = BKE_object_active_modifier(ob);

  if (md == nullptr) {
    return;
  }

  if (md->type == eModifierType_Nodes) {
    NodesModifierData *nmd = (NodesModifierData *)md;
    if (nmd->node_group != nullptr) {
      *r_from = &ob->id;
      *r_id = &ob->id;
      *r_ntree = nmd->node_group;
    }
  }
}

static void geometry_node_tree_update(bNodeTree *ntree)
{
  /* Needed to give correct types to reroutes. */
  ntree_update_reroute_nodes(ntree);
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_GEOMETRY, N_("Geometry"));
  func(calldata, NODE_CLASS_ATTRIBUTE, N_("Attribute"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

void register_node_tree_type_geo(void)
{
  bNodeTreeType *tt = ntreeType_Geometry = static_cast<bNodeTreeType *>(
      MEM_callocN(sizeof(bNodeTreeType), "geometry node tree type"));
  tt->type = NTREE_GEOMETRY;
  strcpy(tt->idname, "GeometryNodeTree");
  strcpy(tt->ui_name, N_("Geometry Node Editor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Geometry nodes"));
  tt->rna_ext.srna = &RNA_GeometryNodeTree;
  tt->update = geometry_node_tree_update;
  tt->get_from_context = geometry_node_tree_get_from_context;
  tt->foreach_nodeclass = foreach_nodeclass;

  ntreeTypeAdd(tt);
}
