/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 */

#pragma once

/* internal exports only */

struct ReportList;
struct SpaceInfo;
struct wmOperatorType;

void FILE_OT_autopack_toggle(wmOperatorType *ot);
void FILE_OT_pack_all(wmOperatorType *ot);
void FILE_OT_unpack_all(wmOperatorType *ot);
void FILE_OT_unpack_item(wmOperatorType *ot);
void FILE_OT_pack_libraries(wmOperatorType *ot);
void FILE_OT_unpack_libraries(wmOperatorType *ot);

void FILE_OT_make_paths_relative(wmOperatorType *ot);
void FILE_OT_make_paths_absolute(wmOperatorType *ot);
void FILE_OT_report_missing_files(wmOperatorType *ot);
void FILE_OT_find_missing_files(wmOperatorType *ot);

void INFO_OT_reports_display_update(wmOperatorType *ot);

/* `info_draw.cc` */

void *info_text_pick(const SpaceInfo *sinfo,
                     const ARegion *region,
                     const ReportList *reports,
                     int mouse_y);
int info_textview_height(const SpaceInfo *sinfo, const ARegion *region, const ReportList *reports);
void info_textview_main(const SpaceInfo *sinfo, const ARegion *region, const ReportList *reports);

/* `info_report.cc` */

int info_report_mask(const SpaceInfo *sinfo);
void INFO_OT_select_pick(wmOperatorType *ot); /* report selection */
void INFO_OT_select_all(wmOperatorType *ot);
void INFO_OT_select_box(wmOperatorType *ot);

void INFO_OT_report_replay(wmOperatorType *ot);
void INFO_OT_report_delete(wmOperatorType *ot);
void INFO_OT_report_copy(wmOperatorType *ot);
