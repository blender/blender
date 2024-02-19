/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "DNA_windowmanager_types.h"

/**
 * Reporting Information and Errors.
 *
 * These functions are thread-safe, unless otherwise specified.
 *
 * These functions also accept nullptr in case no error reporting is needed. The message are only
 * printed to the console then.
 */

/* Report structures are stored in DNA. */

/**
 * Initialize a #ReportList struct.
 *
 * \note: Not thread-safe, should only be called from the 'owner' thread of the report list.
 */
void BKE_reports_init(ReportList *reports, int flag);
/**
 * Fully release any internal resources used by this #ReportList, as acquired by #BKE_reports_init.
 *
 * Also calls #BKE_reports_clear. The given `reports` should not be used anymore unless it is
 * re-initialized first.
 *
 * \note: Not thread-safe, should only be called from the current owner of the report list, once
 * no other concurrent access is possible.
 */
void BKE_reports_free(ReportList *reports);

/**
 * Only frees the list of reports in given \a reports. Use #BKE_reports_free to fully cleanup all
 * allocated resources.
 *
 * To make displayed reports disappear, either remove window-manager reports
 * (#wmWindowManager.reports, or #CTX_wm_reports()), or use #WM_report_banners_cancel().
 */
void BKE_reports_clear(ReportList *reports);

/** Moves all reports from `reports_src` to `reports_dst`. */
void BKE_reports_move_to_reports(ReportList *reports_dst, ReportList *reports_src);

/** (Un)lock given `reports`, in case external code needs to access its data. */
void BKE_reports_lock(ReportList *reports);
void BKE_reports_unlock(ReportList *reports);

void BKE_report(ReportList *reports, eReportType type, const char *message);
void BKE_reportf(ReportList *reports, eReportType type, const char *format, ...)
    ATTR_PRINTF_FORMAT(3, 4);

void BKE_reports_prepend(ReportList *reports, const char *prepend);
void BKE_reports_prependf(ReportList *reports, const char *prepend, ...) ATTR_PRINTF_FORMAT(2, 3);

eReportType BKE_report_print_level(ReportList *reports);
void BKE_report_print_level_set(ReportList *reports, eReportType level);

eReportType BKE_report_store_level(ReportList *reports);
void BKE_report_store_level_set(ReportList *reports, eReportType level);

char *BKE_reports_string(ReportList *reports, eReportType level);

/**
 * \return true when reports of this type will print to the `stdout`.
 */
bool BKE_reports_print_test(const ReportList *reports, eReportType type);
void BKE_reports_print(ReportList *reports, eReportType level);

Report *BKE_reports_last_displayable(ReportList *reports);

bool BKE_reports_contain(ReportList *reports, eReportType level);

const char *BKE_report_type_str(eReportType type);

bool BKE_report_write_file_fp(FILE *fp, ReportList *reports, const char *header);
bool BKE_report_write_file(const char *filepath, ReportList *reports, const char *header);
