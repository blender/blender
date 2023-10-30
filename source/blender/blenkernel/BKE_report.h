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

#ifdef __cplusplus
extern "C" {
#endif

/* Reporting Information and Errors
 *
 * These functions also accept NULL in case no error reporting
 * is needed. */

/* Report structures are stored in DNA. */

void BKE_reports_init(ReportList *reports, int flag);
/**
 * Only frees the list \a reports.
 * To make displayed reports disappear, either remove window-manager reports
 * (#wmWindowManager.reports, or #CTX_wm_reports()), or use #WM_report_banners_cancel().
 */
void BKE_reports_clear(ReportList *reports);

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

#ifdef __cplusplus
}
#endif
