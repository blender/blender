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
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_trackpos_cc {

static void cmp_node_trackpos_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("X"));
  b.add_output<decl::Float>(N_("Y"));
  b.add_output<decl::Vector>(N_("Speed")).subtype(PROP_VELOCITY);
}

static void init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTrackPosData *data = MEM_cnew<NodeTrackPosData>(__func__);

  node->storage = data;
}

static void node_composit_buts_trackpos(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               "CLIP_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *object;
    uiLayout *col;
    PointerRNA tracking_ptr;
    NodeTrackPosData *data = (NodeTrackPosData *)node->storage;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, false);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (object) {
      PointerRNA object_ptr;

      RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, object, &object_ptr);

      uiItemPointerR(col, ptr, "track_name", &object_ptr, "tracks", "", ICON_ANIM_DATA);
    }
    else {
      uiItemR(layout, ptr, "track_name", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_ANIM_DATA);
    }

    uiItemR(layout, ptr, "position", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

    if (ELEM(node->custom1, CMP_TRACKPOS_RELATIVE_FRAME, CMP_TRACKPOS_ABSOLUTE_FRAME)) {
      uiItemR(layout, ptr, "frame_relative", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    }
  }
}

}  // namespace blender::nodes::node_composite_trackpos_cc

void register_node_type_cmp_trackpos()
{
  namespace file_ns = blender::nodes::node_composite_trackpos_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TRACKPOS, "Track Position", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_trackpos_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_trackpos;
  node_type_init(&ntype, file_ns::init);
  node_type_storage(
      &ntype, "NodeTrackPosData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
