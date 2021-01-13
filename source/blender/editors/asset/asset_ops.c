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
 * \ingroup edasset
 */

#include <string.h>

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "BLI_listbase.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "ED_asset.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* -------------------------------------------------------------------- */

struct AssetMarkResultStats {
  int tot_created;
  int tot_already_asset;
  ID *last_id;
};

static bool asset_ops_poll(bContext *UNUSED(C))
{
  return U.experimental.use_asset_browser;
}

/**
 * Return the IDs to operate on as list of #CollectionPointerLink links. Needs freeing.
 */
static ListBase /* CollectionPointerLink */ asset_operation_get_ids_from_context(const bContext *C)
{
  ListBase list = {0};

  PointerRNA idptr = CTX_data_pointer_get_type(C, "id", &RNA_ID);

  if (idptr.data) {
    CollectionPointerLink *ctx_link = MEM_callocN(sizeof(*ctx_link), __func__);
    ctx_link->ptr = idptr;
    BLI_addtail(&list, ctx_link);
  }
  else {
    CTX_data_selected_ids(C, &list);
  }

  return list;
}

static void asset_mark_for_idptr_list(const bContext *C,
                                      const ListBase /* CollectionPointerLink */ *ids,
                                      struct AssetMarkResultStats *r_stats)
{
  memset(r_stats, 0, sizeof(*r_stats));

  LISTBASE_FOREACH (CollectionPointerLink *, ctx_id, ids) {
    BLI_assert(RNA_struct_is_ID(ctx_id->ptr.type));

    ID *id = ctx_id->ptr.data;
    if (id->asset_data) {
      r_stats->tot_already_asset++;
      continue;
    }

    if (ED_asset_mark_id(C, id)) {
      r_stats->last_id = id;
      r_stats->tot_created++;
    }
  }
}

static bool asset_mark_results_report(const struct AssetMarkResultStats *stats,
                                      ReportList *reports)
{
  /* User feedback on failure. */
  if ((stats->tot_created < 1) && (stats->tot_already_asset > 0)) {
    BKE_report(reports,
               RPT_ERROR,
               "Selected data-blocks are already assets (or do not support use as assets)");
    return false;
  }
  if (stats->tot_created < 1) {
    BKE_report(reports,
               RPT_ERROR,
               "No data-blocks to create assets for found (or do not support use as assets)");
    return false;
  }

  /* User feedback on success. */
  if (stats->tot_created == 1) {
    /* If only one data-block: Give more useful message by printing asset name. */
    BKE_reportf(reports, RPT_INFO, "Data-block '%s' is now an asset", stats->last_id->name + 2);
  }
  else {
    BKE_reportf(reports, RPT_INFO, "%i data-blocks are now assets", stats->tot_created);
  }

  return true;
}

static int asset_mark_exec(bContext *C, wmOperator *op)
{
  ListBase ids = asset_operation_get_ids_from_context(C);

  struct AssetMarkResultStats stats;
  asset_mark_for_idptr_list(C, &ids, &stats);
  BLI_freelistN(&ids);

  if (!asset_mark_results_report(&stats, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  WM_main_add_notifier(NC_ID | NA_EDITED, NULL);
  WM_main_add_notifier(NC_ASSET | NA_ADDED, NULL);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_mark(wmOperatorType *ot)
{
  ot->name = "Mark Asset";
  ot->description =
      "Enable easier reuse of selected data-blocks through the Asset Browser, with the help of "
      "customizable metadata (like previews, descriptions and tags)";
  ot->idname = "ASSET_OT_mark";

  ot->exec = asset_mark_exec;
  ot->poll = asset_ops_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */

struct AssetClearResultStats {
  int tot_removed;
  ID *last_id;
};

static void asset_clear_from_idptr_list(const ListBase /* CollectionPointerLink */ *ids,
                                        struct AssetClearResultStats *r_stats)
{
  memset(r_stats, 0, sizeof(*r_stats));

  LISTBASE_FOREACH (CollectionPointerLink *, ctx_id, ids) {
    BLI_assert(RNA_struct_is_ID(ctx_id->ptr.type));

    ID *id = ctx_id->ptr.data;
    if (!id->asset_data) {
      continue;
    }

    if (ED_asset_clear_id(id)) {
      r_stats->tot_removed++;
      r_stats->last_id = id;
    }
  }
}

static bool asset_clear_result_report(const struct AssetClearResultStats *stats,
                                      ReportList *reports)

{
  if (stats->tot_removed < 1) {
    BKE_report(reports, RPT_ERROR, "No asset data-blocks selected/focused");
    return false;
  }

  if (stats->tot_removed == 1) {
    /* If only one data-block: Give more useful message by printing asset name. */
    BKE_reportf(
        reports, RPT_INFO, "Data-block '%s' is no asset anymore", stats->last_id->name + 2);
  }
  else {
    BKE_reportf(reports, RPT_INFO, "%i data-blocks are no assets anymore", stats->tot_removed);
  }

  return true;
}

static int asset_clear_exec(bContext *C, wmOperator *op)
{
  ListBase ids = asset_operation_get_ids_from_context(C);

  struct AssetClearResultStats stats;
  asset_clear_from_idptr_list(&ids, &stats);
  BLI_freelistN(&ids);

  if (!asset_clear_result_report(&stats, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  WM_main_add_notifier(NC_ID | NA_EDITED, NULL);
  WM_main_add_notifier(NC_ASSET | NA_REMOVED, NULL);

  return OPERATOR_FINISHED;
}

static void ASSET_OT_clear(wmOperatorType *ot)
{
  ot->name = "Clear Asset";
  ot->description =
      "Delete all asset metadata and turn the selected asset data-blocks back into normal "
      "data-blocks";
  ot->idname = "ASSET_OT_clear";

  ot->exec = asset_clear_exec;
  ot->poll = asset_ops_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* -------------------------------------------------------------------- */

void ED_operatortypes_asset(void)
{
  WM_operatortype_append(ASSET_OT_mark);
  WM_operatortype_append(ASSET_OT_clear);
}
