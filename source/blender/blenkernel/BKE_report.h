/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_REPORT_H__
#define __BKE_REPORT_H__

/** \file BKE_report.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "DNA_windowmanager_types.h"
#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

/* Reporting Information and Errors
 *
 * These functions also accept NULL in case no error reporting
 * is needed. */

/* report structures are stored in DNA */

void BKE_reports_init(ReportList *reports, int flag);
void BKE_reports_clear(ReportList *reports);

void BKE_report(ReportList *reports, ReportType type, const char *message);
void BKE_reportf(ReportList *reports, ReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(3, 4);

void BKE_reports_prepend(ReportList *reports, const char *prepend);
void BKE_reports_prependf(ReportList *reports, const char *prepend, ...) ATTR_PRINTF_FORMAT(2, 3);

ReportType BKE_report_print_level(ReportList *reports);
void BKE_report_print_level_set(ReportList *reports, ReportType level);

ReportType BKE_report_store_level(ReportList *reports);
void BKE_report_store_level_set(ReportList *reports, ReportType level);

char *BKE_reports_string(ReportList *reports, ReportType level);
void BKE_reports_print(ReportList *reports, ReportType level);

Report *BKE_reports_last_displayable(ReportList *reports);

bool BKE_reports_contain(ReportList *reports, ReportType level);

const char *BKE_report_type_str(ReportType type);

bool BKE_report_write_file_fp(FILE *fp, ReportList *reports, const char *header);
bool BKE_report_write_file(const char *filepath, ReportList *reports, const char *header);

#ifdef __cplusplus
}
#endif
	
#endif

