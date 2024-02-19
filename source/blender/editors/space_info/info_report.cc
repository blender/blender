/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#include <climits>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_select_utils.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "info_intern.hh"

static void reports_select_all(ReportList *reports, int report_mask, int action)
{
  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    LISTBASE_FOREACH_BACKWARD (const Report *, report, &reports->list) {
      if ((report->type & report_mask) && (report->flag & SELECT)) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  LISTBASE_FOREACH_BACKWARD (Report *, report, &reports->list) {
    if (report->type & report_mask) {
      switch (action) {
        case SEL_SELECT:
          report->flag = SELECT;
          break;
        case SEL_DESELECT:
          report->flag = ~SELECT;
          break;
        case SEL_INVERT:
          report->flag ^= SELECT;
          break;
        default:
          BLI_assert(0);
      }
    }
  }
}

int info_report_mask(const SpaceInfo * /*sinfo*/)
{
#if 0
  int report_mask = 0;

  if (sinfo->rpt_mask & INFO_RPT_DEBUG) {
    report_mask |= RPT_DEBUG_ALL;
  }
  if (sinfo->rpt_mask & INFO_RPT_INFO) {
    report_mask |= RPT_INFO_ALL;
  }
  if (sinfo->rpt_mask & INFO_RPT_OP) {
    report_mask |= RPT_OPERATOR_ALL;
  }
  if (sinfo->rpt_mask & INFO_RPT_WARN) {
    report_mask |= RPT_WARNING_ALL;
  }
  if (sinfo->rpt_mask & INFO_RPT_ERR) {
    report_mask |= RPT_ERROR_ALL;
  }

  return report_mask;
#endif

  return RPT_DEBUG_ALL | RPT_INFO_ALL | RPT_OPERATOR_ALL | RPT_PROPERTY_ALL | RPT_WARNING_ALL |
         RPT_ERROR_ALL;
}

static int report_replay_exec(bContext *C, wmOperator * /*op*/)
{
  /* TODO: get this working again! */
#if 0
  SpaceInfo *sc = CTX_wm_space_info(C);
  ReportList *reports = CTX_wm_reports(C);
  int report_mask = info_report_mask(sc);
  Report *report;

  sc->type = CONSOLE_TYPE_PYTHON;

  for (report = reports->list.last; report; report = report->prev) {
    if ((report->type & report_mask) && (report->type & RPT_OPERATOR_ALL | RPT_PROPERTY_ALL) &&
        (report->flag & SELECT))
    {
      console_history_add_str(sc, report->message, 0);
      WM_operator_name_call(C, "CONSOLE_OT_execute", WM_OP_EXEC_DEFAULT, nullptr, nullptr);

      ED_area_tag_redraw(CTX_wm_area(C));
    }
  }

  sc->type = CONSOLE_TYPE_REPORT;
#endif
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void INFO_OT_report_replay(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Replay Operators";
  ot->description = "Replay selected reports";
  ot->idname = "INFO_OT_report_replay";

  /* api callbacks */
  ot->poll = ED_operator_info_active;
  ot->exec = report_replay_exec;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;

  /* properties */
}

static int select_report_pick_exec(bContext *C, wmOperator *op)
{
  int report_index = RNA_int_get(op->ptr, "report_index");
  bool extend = RNA_boolean_get(op->ptr, "extend");

  Report *report = static_cast<Report *>(BLI_findlink(&CTX_wm_reports(C)->list, report_index));

  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ReportList *reports = CTX_wm_reports(C);
  const int report_mask = info_report_mask(sinfo);
  if (!report) {
    return OPERATOR_CANCELLED;
  }

  if (!extend) {
    reports_select_all(reports, report_mask, SEL_DESELECT);
  }
  report->flag ^= SELECT; /* toggle */

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

static int select_report_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ARegion *region = CTX_wm_region(C);
  ReportList *reports = CTX_wm_reports(C);
  Report *report;

  report = static_cast<Report *>(info_text_pick(sinfo, region, reports, event->mval[1]));

  RNA_int_set(op->ptr, "report_index", BLI_findindex(&reports->list, report));

  return select_report_pick_exec(C, op);
}

void INFO_OT_select_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Report";
  ot->description = "Select reports by index";
  ot->idname = "INFO_OT_select_pick";

  /* api callbacks */
  ot->poll = ED_operator_info_active;
  ot->invoke = select_report_pick_invoke;
  ot->exec = select_report_pick_exec;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;

  /* properties */
  PropertyRNA *prop;
  RNA_def_int(
      ot->srna, "report_index", 0, 0, INT_MAX, "Report", "Index of the report", 0, INT_MAX);
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend report selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int report_select_all_exec(bContext *C, wmOperator *op)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ReportList *reports = CTX_wm_reports(C);
  const int report_mask = info_report_mask(sinfo);

  int action = RNA_enum_get(op->ptr, "action");
  reports_select_all(reports, report_mask, action);

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void INFO_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all visible reports";
  ot->idname = "INFO_OT_select_all";

  /* api callbacks */
  ot->poll = ED_operator_info_active;
  ot->exec = report_select_all_exec;

  /* properties */
  WM_operator_properties_select_action(ot, SEL_SELECT, true);
}

/* box_select operator */
static int box_select_exec(bContext *C, wmOperator *op)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ARegion *region = CTX_wm_region(C);
  ReportList *reports = CTX_wm_reports(C);
  int report_mask = info_report_mask(sinfo);
  Report *report_min, *report_max;
  rcti rect;

  WM_operator_properties_border_to_rcti(op, &rect);

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const int select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    LISTBASE_FOREACH (Report *, report, &reports->list) {
      if ((report->type & report_mask) == 0) {
        continue;
      }
      report->flag &= ~SELECT;
    }
  }

  report_min = static_cast<Report *>(info_text_pick(sinfo, region, reports, rect.ymax));
  report_max = static_cast<Report *>(info_text_pick(sinfo, region, reports, rect.ymin));

  /* get the first report if none found */
  if (report_min == nullptr) {
    // printf("find_min\n");
    LISTBASE_FOREACH (Report *, report, &reports->list) {
      if (report->type & report_mask) {
        report_min = report;
        break;
      }
    }
  }

  if (report_max == nullptr) {
    // printf("find_max\n");
    LISTBASE_FOREACH_BACKWARD (Report *, report, &reports->list) {
      if (report->type & report_mask) {
        report_max = report;
        break;
      }
    }
  }

  if (report_min == nullptr || report_max == nullptr) {
    return OPERATOR_CANCELLED;
  }

  for (Report *report = report_min; (report != report_max->next); report = report->next) {
    if ((report->type & report_mask) == 0) {
      continue;
    }
    SET_FLAG_FROM_TEST(report->flag, select, SELECT);
  }

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

/* ****** Box Select ****** */

void INFO_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Toggle box selection";
  ot->idname = "INFO_OT_select_box";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_info_active;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;

  /* properties */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

static int report_delete_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ReportList *reports = CTX_wm_reports(C);
  int report_mask = info_report_mask(sinfo);

  Report *report, *report_next;

  for (report = static_cast<Report *>(reports->list.first); report;) {
    report_next = report->next;

    if ((report->type & report_mask) && (report->flag & SELECT)) {
      BLI_remlink(&reports->list, report);
      MEM_freeN((void *)report->message);
      MEM_freeN(report);
    }

    report = report_next;
  }

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void INFO_OT_report_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Reports";
  ot->description = "Delete selected reports";
  ot->idname = "INFO_OT_report_delete";

  /* api callbacks */
  ot->poll = ED_operator_info_active;
  ot->exec = report_delete_exec;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;

  /* properties */
}

static int report_copy_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceInfo *sinfo = CTX_wm_space_info(C);
  ReportList *reports = CTX_wm_reports(C);
  int report_mask = info_report_mask(sinfo);

  DynStr *buf_dyn = BLI_dynstr_new();
  char *buf_str;

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    if ((report->type & report_mask) && (report->flag & SELECT)) {
      BLI_dynstr_append(buf_dyn, report->message);
      BLI_dynstr_append(buf_dyn, "\n");
    }
  }

  buf_str = BLI_dynstr_get_cstring(buf_dyn);
  BLI_dynstr_free(buf_dyn);

  WM_clipboard_text_set(buf_str, false);

  MEM_freeN(buf_str);
  return OPERATOR_FINISHED;
}

void INFO_OT_report_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Reports to Clipboard";
  ot->description = "Copy selected reports to clipboard";
  ot->idname = "INFO_OT_report_copy";

  /* api callbacks */
  ot->poll = ED_operator_info_active;
  ot->exec = report_copy_exec;

  /* flags */
  // ot->flag = OPTYPE_REGISTER;

  /* properties */
}
