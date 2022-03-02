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

#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_hash.hh"
#include "BLI_hash_mm2a.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "ED_screen.h"
#include "ED_spreadsheet.h"

#include "DEG_depsgraph.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_workspace.h"

#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"

#include "spreadsheet_context.hh"

using blender::IndexRange;
using blender::Span;
using blender::StringRef;
using blender::Vector;

namespace blender::ed::spreadsheet {

static SpreadsheetContextObject *spreadsheet_context_object_new()
{
  SpreadsheetContextObject *context = MEM_cnew<SpreadsheetContextObject>(__func__);
  context->base.type = SPREADSHEET_CONTEXT_OBJECT;
  return context;
}

static SpreadsheetContextObject *spreadsheet_context_object_copy(
    const SpreadsheetContextObject *src_context)
{
  SpreadsheetContextObject *new_context = spreadsheet_context_object_new();
  new_context->object = src_context->object;
  return new_context;
}

static void spreadsheet_context_object_hash(const SpreadsheetContextObject *context,
                                            BLI_HashMurmur2A *mm2)
{
  BLI_hash_mm2a_add(mm2, (const uchar *)&context->object, sizeof(Object *));
}

static void spreadsheet_context_object_free(SpreadsheetContextObject *context)
{
  MEM_freeN(context);
}

static SpreadsheetContextModifier *spreadsheet_context_modifier_new()
{
  SpreadsheetContextModifier *context = MEM_cnew<SpreadsheetContextModifier>(__func__);
  context->base.type = SPREADSHEET_CONTEXT_MODIFIER;
  return context;
}

static SpreadsheetContextModifier *spreadsheet_context_modifier_copy(
    const SpreadsheetContextModifier *src_context)
{
  SpreadsheetContextModifier *new_context = spreadsheet_context_modifier_new();
  if (src_context->modifier_name) {
    new_context->modifier_name = BLI_strdup(src_context->modifier_name);
  }
  return new_context;
}

static void spreadsheet_context_modifier_hash(const SpreadsheetContextModifier *context,
                                              BLI_HashMurmur2A *mm2)
{
  if (context->modifier_name) {
    BLI_hash_mm2a_add(mm2, (const uchar *)context->modifier_name, strlen(context->modifier_name));
  }
}

static void spreadsheet_context_modifier_free(SpreadsheetContextModifier *context)
{
  if (context->modifier_name) {
    MEM_freeN(context->modifier_name);
  }
  MEM_freeN(context);
}

static SpreadsheetContextNode *spreadsheet_context_node_new()
{
  SpreadsheetContextNode *context = MEM_cnew<SpreadsheetContextNode>(__func__);
  context->base.type = SPREADSHEET_CONTEXT_NODE;
  return context;
}

static SpreadsheetContextNode *spreadsheet_context_node_copy(
    const SpreadsheetContextNode *src_context)
{
  SpreadsheetContextNode *new_context = spreadsheet_context_node_new();
  if (src_context->node_name) {
    new_context->node_name = BLI_strdup(src_context->node_name);
  }
  return new_context;
}

static void spreadsheet_context_node_hash(const SpreadsheetContextNode *context,
                                          BLI_HashMurmur2A *mm2)
{
  if (context->node_name) {
    BLI_hash_mm2a_add(mm2, (const uchar *)context->node_name, strlen(context->node_name));
  }
}

static void spreadsheet_context_node_free(SpreadsheetContextNode *context)
{
  if (context->node_name) {
    MEM_freeN(context->node_name);
  }
  MEM_freeN(context);
}

SpreadsheetContext *spreadsheet_context_new(eSpaceSpreadsheet_ContextType type)
{
  switch (type) {
    case SPREADSHEET_CONTEXT_OBJECT: {
      return (SpreadsheetContext *)spreadsheet_context_object_new();
    }
    case SPREADSHEET_CONTEXT_MODIFIER: {
      return (SpreadsheetContext *)spreadsheet_context_modifier_new();
    }
    case SPREADSHEET_CONTEXT_NODE: {
      return (SpreadsheetContext *)spreadsheet_context_node_new();
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

SpreadsheetContext *spreadsheet_context_copy(const SpreadsheetContext *old_context)
{
  switch (old_context->type) {
    case SPREADSHEET_CONTEXT_OBJECT: {
      return (SpreadsheetContext *)spreadsheet_context_object_copy(
          (const SpreadsheetContextObject *)old_context);
    }
    case SPREADSHEET_CONTEXT_MODIFIER: {
      return (SpreadsheetContext *)spreadsheet_context_modifier_copy(
          (const SpreadsheetContextModifier *)old_context);
    }
    case SPREADSHEET_CONTEXT_NODE: {
      return (SpreadsheetContext *)spreadsheet_context_node_copy(
          (const SpreadsheetContextNode *)old_context);
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void spreadsheet_context_hash(const SpreadsheetContext *context, BLI_HashMurmur2A *mm2)
{
  BLI_hash_mm2a_add_int(mm2, context->type);
  switch (context->type) {
    case SPREADSHEET_CONTEXT_OBJECT: {
      spreadsheet_context_object_hash((const SpreadsheetContextObject *)context, mm2);
      break;
    }
    case SPREADSHEET_CONTEXT_MODIFIER: {
      spreadsheet_context_modifier_hash((const SpreadsheetContextModifier *)context, mm2);
      break;
    }
    case SPREADSHEET_CONTEXT_NODE: {
      spreadsheet_context_node_hash((const SpreadsheetContextNode *)context, mm2);
      break;
    }
  }
}

void spreadsheet_context_free(SpreadsheetContext *context)
{
  switch (context->type) {
    case SPREADSHEET_CONTEXT_OBJECT: {
      return spreadsheet_context_object_free((SpreadsheetContextObject *)context);
    }
    case SPREADSHEET_CONTEXT_MODIFIER: {
      return spreadsheet_context_modifier_free((SpreadsheetContextModifier *)context);
    }
    case SPREADSHEET_CONTEXT_NODE: {
      return spreadsheet_context_node_free((SpreadsheetContextNode *)context);
    }
  }
  BLI_assert_unreachable();
}

/**
 * Tag any data relevant to the spreadsheet's context for recalculation in order to collect
 * information to display in the editor, which may be cached during evaluation.
 * \return True when any data has been tagged for update.
 */
static bool spreadsheet_context_update_tag(SpaceSpreadsheet *sspreadsheet)
{
  using namespace blender;
  Vector<const SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.is_empty()) {
    return false;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return false;
  }
  SpreadsheetContextObject *object_context = (SpreadsheetContextObject *)context_path[0];
  Object *object = object_context->object;
  if (object == nullptr) {
    return false;
  }
  if (context_path.size() == 1) {
    /* No need to reevaluate, when the final or original object is viewed. */
    return false;
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
  return true;
}

}  // namespace blender::ed::spreadsheet

SpreadsheetContext *ED_spreadsheet_context_new(int type)
{
  return blender::ed::spreadsheet::spreadsheet_context_new((eSpaceSpreadsheet_ContextType)type);
}

void ED_spreadsheet_context_free(struct SpreadsheetContext *context)
{
  blender::ed::spreadsheet::spreadsheet_context_free(context);
}

void ED_spreadsheet_context_path_clear(struct SpaceSpreadsheet *sspreadsheet)
{
  LISTBASE_FOREACH_MUTABLE (SpreadsheetContext *, context, &sspreadsheet->context_path) {
    ED_spreadsheet_context_free(context);
  }
  BLI_listbase_clear(&sspreadsheet->context_path);
}

bool ED_spreadsheet_context_path_update_tag(SpaceSpreadsheet *sspreadsheet)
{
  return blender::ed::spreadsheet::spreadsheet_context_update_tag(sspreadsheet);
}

uint64_t ED_spreadsheet_context_path_hash(const SpaceSpreadsheet *sspreadsheet)
{
  BLI_HashMurmur2A mm2;
  BLI_hash_mm2a_init(&mm2, 1234);
  LISTBASE_FOREACH (const SpreadsheetContext *, context, &sspreadsheet->context_path) {
    blender::ed::spreadsheet::spreadsheet_context_hash(context, &mm2);
  }
  return BLI_hash_mm2a_end(&mm2);
}

void ED_spreadsheet_context_path_set_geometry_node(struct SpaceSpreadsheet *sspreadsheet,
                                                   struct SpaceNode *snode,
                                                   struct bNode *node)
{
  using namespace blender::ed::spreadsheet;

  Object *object = (Object *)snode->id;
  /* Try to find the modifier the node tree belongs to. */
  ModifierData *modifier = BKE_object_active_modifier(object);
  if (modifier && modifier->type != eModifierType_Nodes) {
    modifier = nullptr;
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        NodesModifierData *nmd = (NodesModifierData *)md;
        if (nmd->node_group == snode->nodetree) {
          modifier = md;
          break;
        }
      }
    }
  }
  if (modifier == nullptr) {
    return;
  }

  ED_spreadsheet_context_path_clear(sspreadsheet);

  {
    SpreadsheetContextObject *context = spreadsheet_context_object_new();
    context->object = object;
    BLI_addtail(&sspreadsheet->context_path, context);
  }
  {
    SpreadsheetContextModifier *context = spreadsheet_context_modifier_new();
    context->modifier_name = BLI_strdup(modifier->name);
    BLI_addtail(&sspreadsheet->context_path, context);
  }
  {
    int i;
    LISTBASE_FOREACH_INDEX (bNodeTreePath *, path, &snode->treepath, i) {
      if (i == 0) {
        continue;
      }
      SpreadsheetContextNode *context = spreadsheet_context_node_new();
      context->node_name = BLI_strdup(path->node_name);
      BLI_addtail(&sspreadsheet->context_path, context);
    }
  }
  {
    SpreadsheetContextNode *context = spreadsheet_context_node_new();
    context->node_name = BLI_strdup(node->name);
    BLI_addtail(&sspreadsheet->context_path, context);
  }

  sspreadsheet->object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE;
}

void ED_spreadsheet_context_paths_set_geometry_node(Main *bmain, SpaceNode *snode, bNode *node)
{
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = (SpaceLink *)area->spacedata.first;
      if (sl->spacetype == SPACE_SPREADSHEET) {
        SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;
        if ((sspreadsheet->flag & SPREADSHEET_FLAG_PINNED) == 0) {
          const uint64_t context_hash_before = ED_spreadsheet_context_path_hash(sspreadsheet);
          ED_spreadsheet_context_path_set_geometry_node(sspreadsheet, snode, node);
          const uint64_t context_hash_after = ED_spreadsheet_context_path_hash(sspreadsheet);
          if (context_hash_before != context_hash_after) {
            ED_spreadsheet_context_path_update_tag(sspreadsheet);
          }
          ED_area_tag_redraw(area);
        }
      }
    }
  }
}

void ED_spreadsheet_context_path_set_evaluated_object(SpaceSpreadsheet *sspreadsheet,
                                                      Object *object)
{
  using namespace blender::ed::spreadsheet;
  ED_spreadsheet_context_path_clear(sspreadsheet);

  SpreadsheetContextObject *context = spreadsheet_context_object_new();
  context->object = object;
  BLI_addtail(&sspreadsheet->context_path, context);
}

static bScreen *find_screen_to_search_for_context(wmWindow *window,
                                                  SpaceSpreadsheet *current_space)
{
  bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
  if (ELEM(screen->state, SCREENMAXIMIZED, SCREENFULL)) {
    /* If the spreadsheet is maximized, try to find the context in the unmaximized screen. */
    ScrArea *main_area = (ScrArea *)screen->areabase.first;
    SpaceLink *sl = (SpaceLink *)main_area->spacedata.first;
    if (sl == (SpaceLink *)current_space) {
      return main_area->full;
    }
  }
  return screen;
}

void ED_spreadsheet_context_path_guess(const bContext *C, SpaceSpreadsheet *sspreadsheet)
{
  ED_spreadsheet_context_path_clear(sspreadsheet);

  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return;
  }

  if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
    LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
      bScreen *screen = find_screen_to_search_for_context(window, sspreadsheet);
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        SpaceLink *sl = (SpaceLink *)area->spacedata.first;
        if (sl == nullptr) {
          continue;
        }
        if (sl->spacetype == SPACE_NODE) {
          SpaceNode *snode = (SpaceNode *)sl;
          if (snode->edittree != nullptr) {
            if (snode->edittree->type == NTREE_GEOMETRY) {
              LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
                if (node->type == GEO_NODE_VIEWER) {
                  if (node->flag & NODE_DO_OUTPUT) {
                    ED_spreadsheet_context_path_set_geometry_node(sspreadsheet, snode, node);
                    return;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  Object *active_object = CTX_data_active_object(C);
  if (active_object != nullptr) {
    ED_spreadsheet_context_path_set_evaluated_object(sspreadsheet, active_object);
    return;
  }
}

bool ED_spreadsheet_context_path_is_active(const bContext *C, SpaceSpreadsheet *sspreadsheet)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  if (wm == nullptr) {
    return false;
  }
  Vector<SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.is_empty()) {
    return false;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return false;
  }
  Object *object = ((SpreadsheetContextObject *)context_path[0])->object;
  if (object == nullptr) {
    return false;
  }
  if (context_path.size() == 1) {
    if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
      return false;
    }
    Object *active_object = CTX_data_active_object(C);
    return object == active_object;
  }
  if (sspreadsheet->object_eval_state != SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE) {
    return false;
  }
  if (context_path[1]->type != SPREADSHEET_CONTEXT_MODIFIER) {
    return false;
  }
  const char *modifier_name = ((SpreadsheetContextModifier *)context_path[1])->modifier_name;
  const ModifierData *modifier = BKE_modifiers_findby_name(object, modifier_name);
  if (modifier == nullptr) {
    return false;
  }
  const bool modifier_is_active = modifier->flag & eModifierFlag_Active;
  if (modifier->type != eModifierType_Nodes) {
    return false;
  }
  bNodeTree *root_node_tree = ((NodesModifierData *)modifier)->node_group;
  if (root_node_tree == nullptr) {
    return false;
  }
  const Span<SpreadsheetContext *> node_context_path = context_path.as_span().drop_front(2);
  if (node_context_path.is_empty()) {
    return false;
  }

  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    bScreen *screen = find_screen_to_search_for_context(window, sspreadsheet);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = (SpaceLink *)area->spacedata.first;
      if (sl == nullptr) {
        continue;
      }
      if (sl->spacetype != SPACE_NODE) {
        continue;
      }
      SpaceNode *snode = (SpaceNode *)sl;
      if (snode->nodetree != root_node_tree) {
        continue;
      }
      if (!modifier_is_active) {
        if (!(snode->flag & SNODE_PIN)) {
          /* Node tree has to be pinned when the modifier is not active. */
          continue;
        }
      }
      if (snode->id != &object->id) {
        continue;
      }
      Vector<bNodeTreePath *> tree_path = snode->treepath;
      if (node_context_path.size() != tree_path.size()) {
        continue;
      }
      int valid_count = 0;
      for (const int i : IndexRange(tree_path.size() - 1)) {
        if (node_context_path[i]->type != SPREADSHEET_CONTEXT_NODE) {
          break;
        }
        SpreadsheetContextNode *node_context = (SpreadsheetContextNode *)node_context_path[i];
        if (!STREQ(node_context->node_name, tree_path[i + 1]->node_name)) {
          break;
        }
        valid_count++;
      }
      if (valid_count != tree_path.size() - 1) {
        continue;
      }
      SpreadsheetContext *last_context = node_context_path.last();
      if (last_context->type != SPREADSHEET_CONTEXT_NODE) {
        return false;
      }
      const char *node_name = ((SpreadsheetContextNode *)last_context)->node_name;
      bNode *node = nodeFindNodebyName(snode->edittree, node_name);
      if (node == nullptr) {
        return false;
      }
      if (node->type != GEO_NODE_VIEWER) {
        return false;
      }
      if (!(node->flag & NODE_DO_OUTPUT)) {
        return false;
      }
      return true;
    }
  }
  return false;
}

bool ED_spreadsheet_context_path_exists(Main *UNUSED(bmain), SpaceSpreadsheet *sspreadsheet)
{
  Vector<SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.is_empty()) {
    return false;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return false;
  }
  Object *object = ((SpreadsheetContextObject *)context_path[0])->object;
  if (object == nullptr) {
    return false;
  }
  if (context_path.size() == 1) {
    return true;
  }
  if (context_path[1]->type != SPREADSHEET_CONTEXT_MODIFIER) {
    return false;
  }
  const char *modifier_name = ((SpreadsheetContextModifier *)context_path[1])->modifier_name;
  const ModifierData *modifier = BKE_modifiers_findby_name(object, modifier_name);
  if (modifier == nullptr) {
    return false;
  }
  if (modifier->type != eModifierType_Nodes) {
    return false;
  }
  bNodeTree *root_node_tree = ((NodesModifierData *)modifier)->node_group;
  if (root_node_tree == nullptr) {
    return false;
  }
  const Span<SpreadsheetContext *> node_context_path = context_path.as_span().drop_front(2);
  if (node_context_path.is_empty()) {
    return false;
  }
  bNodeTree *node_tree = root_node_tree;
  for (const int i : node_context_path.index_range()) {
    if (node_context_path[i]->type != SPREADSHEET_CONTEXT_NODE) {
      return false;
    }
    const char *node_name = ((SpreadsheetContextNode *)node_context_path[i])->node_name;
    bNode *node = nodeFindNodebyName(node_tree, node_name);
    if (node == nullptr) {
      return false;
    }
    if (node->type == GEO_NODE_VIEWER) {
      if (i == node_context_path.index_range().last()) {
        return true;
      }
      return false;
    }
    if (node->id != nullptr) {
      if (GS(node->id->name) != ID_NT) {
        return false;
      }
      node_tree = (bNodeTree *)node->id;
    }
    else {
      return false;
    }
  }
  return false;
}
