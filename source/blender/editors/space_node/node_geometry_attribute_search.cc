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

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rect.h"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_search.h"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_undo.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"

#include "NOD_geometry_nodes_eval_log.hh"

#include "node_intern.h"

using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::StringRef;
namespace geo_log = blender::nodes::geometry_nodes_eval_log;
using geo_log::GeometryAttributeInfo;

struct AttributeSearchData {
  const bNodeTree *tree;
  const bNode *node;
  bNodeSocket *socket;
};

/* This class must not have a destructor, since it is used by buttons and freed with #MEM_freeN. */
BLI_STATIC_ASSERT(std::is_trivially_destructible_v<AttributeSearchData>, "");

static void attribute_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  AttributeSearchData *data = static_cast<AttributeSearchData *>(arg);

  SpaceNode *snode = CTX_wm_space_node(C);
  const geo_log::NodeLog *node_log = geo_log::ModifierLog::find_node_by_node_editor_context(
      *snode, *data->node);
  if (node_log == nullptr) {
    return;
  }
  blender::Vector<const GeometryAttributeInfo *> infos = node_log->lookup_available_attributes();

  blender::ui::attribute_search_add_items(str, true, infos, items, is_first);
}

static void attribute_search_exec_fn(bContext *C, void *data_v, void *item_v)
{
  if (item_v == nullptr) {
    return;
  }
  AttributeSearchData *data = static_cast<AttributeSearchData *>(data_v);
  GeometryAttributeInfo *item = (GeometryAttributeInfo *)item_v;

  bNodeSocket &socket = *data->socket;
  bNodeSocketValueString *value = static_cast<bNodeSocketValueString *>(socket.default_value);
  BLI_strncpy(value->value, item->name.c_str(), MAX_NAME);

  ED_undo_push(C, "Assign Attribute Name");
}

void node_geometry_add_attribute_search_button(const bContext *UNUSED(C),
                                               const bNodeTree *node_tree,
                                               const bNode *node,
                                               PointerRNA *socket_ptr,
                                               uiLayout *layout)
{
  uiBlock *block = uiLayoutGetBlock(layout);
  uiBut *but = uiDefIconTextButR(block,
                                 UI_BTYPE_SEARCH_MENU,
                                 0,
                                 ICON_NONE,
                                 "",
                                 0,
                                 0,
                                 10 * UI_UNIT_X, /* Dummy value, replaced by layout system. */
                                 UI_UNIT_Y,
                                 socket_ptr,
                                 "default_value",
                                 0,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 0.0f,
                                 "");

  AttributeSearchData *data = OBJECT_GUARDED_NEW(
      AttributeSearchData, {node_tree, node, (bNodeSocket *)socket_ptr->data});

  UI_but_func_search_set_results_are_suggestions(but, true);
  UI_but_func_search_set_sep_string(but, UI_MENU_ARROW_SEP);
  UI_but_func_search_set(but,
                         nullptr,
                         attribute_search_update_fn,
                         static_cast<void *>(data),
                         true,
                         nullptr,
                         attribute_search_exec_fn,
                         nullptr);
}
