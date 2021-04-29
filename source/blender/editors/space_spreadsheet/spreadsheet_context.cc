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

#include "ED_spreadsheet.h"

#include "DEG_depsgraph.h"

#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "spreadsheet_context.hh"

namespace blender::ed::spreadsheet {

static SpreadsheetContextObject *spreadsheet_context_object_new()
{
  SpreadsheetContextObject *context = (SpreadsheetContextObject *)MEM_callocN(
      sizeof(SpreadsheetContextObject), __func__);
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
  SpreadsheetContextModifier *context = (SpreadsheetContextModifier *)MEM_callocN(
      sizeof(SpreadsheetContextModifier), __func__);
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
  SpreadsheetContextNode *context = (SpreadsheetContextNode *)MEM_callocN(
      sizeof(SpreadsheetContextNode), __func__);
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
 */
static void spreadsheet_context_update_tag(SpaceSpreadsheet *sspreadsheet)
{
  using namespace blender;
  Vector<const SpreadsheetContext *> context_path = sspreadsheet->context_path;
  if (context_path.is_empty()) {
    return;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return;
  }
  SpreadsheetContextObject *object_context = (SpreadsheetContextObject *)context_path[0];
  Object *object = object_context->object;
  if (object == nullptr) {
    return;
  }
  if (context_path.size() == 1) {
    /* No need to reevaluate, when the final or original object is viewed. */
    return;
  }

  DEG_id_tag_update(&object->id, ID_RECALC_GEOMETRY);
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

void ED_spreadsheet_context_path_update_tag(SpaceSpreadsheet *sspreadsheet)
{
  blender::ed::spreadsheet::spreadsheet_context_update_tag(sspreadsheet);
}

uint64_t ED_spreadsheet_context_path_hash(SpaceSpreadsheet *sspreadsheet)
{
  BLI_HashMurmur2A mm2;
  BLI_hash_mm2a_init(&mm2, 1234);
  LISTBASE_FOREACH (SpreadsheetContext *, context, &sspreadsheet->context_path) {
    blender::ed::spreadsheet::spreadsheet_context_hash(context, &mm2);
  }
  return BLI_hash_mm2a_end(&mm2);
}

void ED_spreadsheet_set_geometry_node_context(struct SpaceSpreadsheet *sspreadsheet,
                                              struct SpaceNode *snode,
                                              struct bNode *node)
{
  using namespace blender::ed::spreadsheet;
  ED_spreadsheet_context_path_clear(sspreadsheet);

  Object *object = (Object *)snode->id;
  ModifierData *modifier = BKE_object_active_modifier(object);

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

  sspreadsheet->object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED;
}
