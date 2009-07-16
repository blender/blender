/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_REPORT_H
#define BKE_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"

/* Reporting Information and Errors
 *
 * These functions also accept NULL in case no error reporting
 * is needed. */

typedef enum ReportType {
	RPT_DEBUG					= 1<<0,
	RPT_INFO					= 1<<1,
	RPT_OPERATOR				= 1<<2,
	RPT_WARNING					= 1<<3,
	RPT_ERROR					= 1<<4,
	RPT_ERROR_INVALID_INPUT		= 1<<5,
	RPT_ERROR_INVALID_CONTEXT	= 1<<6,
	RPT_ERROR_OUT_OF_MEMORY		= 1<<7
} ReportType;

#define RPT_DEBUG_ALL		(RPT_DEBUG)
#define RPT_INFO_ALL		(RPT_INFO)
#define RPT_OPERATOR_ALL	(RPT_OPERATOR)
#define RPT_WARNING_ALL		(RPT_WARNING)
#define RPT_ERROR_ALL		(RPT_ERROR|RPT_ERROR_INVALID_INPUT|RPT_ERROR_INVALID_CONTEXT|RPT_ERROR_OUT_OF_MEMORY)

enum ReportListFlags {
	RPT_PRINT = 1,
	RPT_STORE = 2,
};

typedef struct Report {
	struct Report *next, *prev;
	ReportType type;
	char *typestr;
	char *message;
} Report;

typedef struct ReportList {
	ListBase list;
	ReportType printlevel;
	ReportType storelevel;
	int flag;
} ReportList;

void BKE_reports_init(ReportList *reports, int flag);
void BKE_reports_clear(ReportList *reports);

void BKE_report(ReportList *reports, ReportType type, const char *message);
void BKE_reportf(ReportList *reports, ReportType type, const char *format, ...);

void BKE_reports_prepend(ReportList *reports, const char *prepend);
void BKE_reports_prependf(ReportList *reports, const char *prepend, ...);

ReportType BKE_report_print_level(ReportList *reports);
void BKE_report_print_level_set(ReportList *reports, ReportType level);

ReportType BKE_report_store_level(ReportList *reports);
void BKE_report_store_level_set(ReportList *reports, ReportType level);

char *BKE_reports_string(ReportList *reports, ReportType level);
void BKE_reports_print(ReportList *reports, ReportType level);

#ifdef __cplusplus
}
#endif
	
#endif

