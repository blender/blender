/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup spinfo
 */

#pragma once

/* internal exports only */

struct ReportList;
struct SpaceInfo;
struct wmOperatorType;

void FILE_OT_autopack_toggle(struct wmOperatorType *ot);
void FILE_OT_pack_all(struct wmOperatorType *ot);
void FILE_OT_unpack_all(struct wmOperatorType *ot);
void FILE_OT_unpack_item(struct wmOperatorType *ot);
void FILE_OT_pack_libraries(struct wmOperatorType *ot);
void FILE_OT_unpack_libraries(struct wmOperatorType *ot);

void FILE_OT_make_paths_relative(struct wmOperatorType *ot);
void FILE_OT_make_paths_absolute(struct wmOperatorType *ot);
void FILE_OT_report_missing_files(struct wmOperatorType *ot);
void FILE_OT_find_missing_files(struct wmOperatorType *ot);

void INFO_OT_reports_display_update(struct wmOperatorType *ot);

/* info_draw.c */

void *info_text_pick(const struct SpaceInfo *sinfo,
                     const struct ARegion *region,
                     const struct ReportList *reports,
                     int mouse_y);
int info_textview_height(const struct SpaceInfo *sinfo,
                         const struct ARegion *region,
                         const struct ReportList *reports);
void info_textview_main(const struct SpaceInfo *sinfo,
                        const struct ARegion *region,
                        const struct ReportList *reports);

/* info_report.c */

int info_report_mask(const struct SpaceInfo *sinfo);
void INFO_OT_select_pick(struct wmOperatorType *ot); /* report selection */
void INFO_OT_select_all(struct wmOperatorType *ot);
void INFO_OT_select_box(struct wmOperatorType *ot);

void INFO_OT_report_replay(struct wmOperatorType *ot);
void INFO_OT_report_delete(struct wmOperatorType *ot);
void INFO_OT_report_copy(struct wmOperatorType *ot);
