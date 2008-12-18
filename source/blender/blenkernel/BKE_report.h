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

/* Reporting Information and Errors */

typedef enum ReportType {
	RPT_DEBUG					= 0,
	RPT_INFO					= 1000,
	RPT_WARNING					= 2000,
	RPT_ERROR					= 3000,
	RPT_ERROR_INVALID_INPUT		= 3001,
	RPT_ERROR_INVALID_CONTEXT	= 3002,
	RPT_ERROR_OUT_OF_MEMORY		= 3003
} ReportType;

enum ReportListFlags {
	RPT_PRINT = 1,
	RPT_STORE = 2
};

typedef struct Report {
	struct Report *next, *prev;
	ReportType type;
	char *typestr;
	char *message;
} Report;

typedef struct ReportList {
	ListBase list;
	ReportType level;
	int flags;
} ReportList;

void BKE_report_list_init(ReportList *reports, int flag);
void BKE_report_list_clear(ReportList *reports);

void BKE_report(ReportList *reports, ReportType type, const char *message);
void BKE_reportf(ReportList *reports, ReportType type, const char *format, ...);

ReportType BKE_report_level(ReportList *reports);
void BKE_report_level_set(ReportList *reports, ReportType level);

int BKE_report_has_error(ReportList *reports);

#ifdef __cplusplus
}
#endif
	
#endif

