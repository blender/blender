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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/report.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "BKE_report.h"
#include "BKE_global.h" /* G.background only */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *report_type_str(int type)
{
	switch(type) {
		case RPT_DEBUG: return "Debug";
		case RPT_INFO: return "Info";
		case RPT_OPERATOR: return "Operator";
		case RPT_WARNING: return "Warning";
		case RPT_ERROR: return "Error";
		case RPT_ERROR_INVALID_INPUT: return "Invalid Input Error";
		case RPT_ERROR_INVALID_CONTEXT: return "Invalid Context Error";
		case RPT_ERROR_OUT_OF_MEMORY: return "Out Of Memory Error";
		default: return "Undefined Type";
	}
}

void BKE_reports_init(ReportList *reports, int flag)
{
	if (!reports)
		return;

	memset(reports, 0, sizeof(ReportList));

	reports->storelevel= RPT_INFO;
	reports->printlevel= RPT_ERROR;
	reports->flag= flag;
}

void BKE_reports_clear(ReportList *reports)
{
	Report *report, *report_next;

	if (!reports)
		return;

	report= reports->list.first;

	while (report) {
		report_next= report->next;
		MEM_freeN((void *)report->message);
		MEM_freeN(report);
		report= report_next;
	}

	reports->list.first= reports->list.last= NULL;
}

void BKE_report(ReportList *reports, ReportType type, const char *message)
{
	Report *report;
	int len;

	/* in background mode always print otherwise there are cases the errors wont be displayed,
	 * but still add to the report list since this is used for python exception handling */
	if (G.background || !reports || ((reports->flag & RPT_PRINT) && (type >= reports->printlevel))) {
		printf("%s: %s\n", report_type_str(type), message);
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
		char *message_alloc;
		report= MEM_callocN(sizeof(Report), "Report");
		report->type= type;
		report->typestr= report_type_str(type);

		len= strlen(message);
		message_alloc= MEM_callocN(sizeof(char)*(len+1), "ReportMessage");
		memcpy(message_alloc, message, sizeof(char)*(len+1));
		report->message= message_alloc;
		report->len= len;
		BLI_addtail(&reports->list, report);
	}
}

void BKE_reportf(ReportList *reports, ReportType type, const char *format, ...)
{
	DynStr *ds;
	Report *report;
	va_list args;

	if (G.background || !reports || ((reports->flag & RPT_PRINT) && (type >= reports->printlevel))) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		fprintf(stdout, "\n"); /* otherise each report needs to include a \n */
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
		report= MEM_callocN(sizeof(Report), "Report");

		ds= BLI_dynstr_new();
		va_start(args, format);
		BLI_dynstr_vappendf(ds, format, args);
		va_end(args);

		report->message= BLI_dynstr_get_cstring(ds);
		report->len= BLI_dynstr_get_len(ds);
		BLI_dynstr_free(ds);

		report->type= type;
		report->typestr= report_type_str(type);

		BLI_addtail(&reports->list, report);
	}
}

void BKE_reports_prepend(ReportList *reports, const char *prepend)
{
	Report *report;
	DynStr *ds;

	if (!reports)
		return;

	for (report=reports->list.first; report; report=report->next) {
		ds= BLI_dynstr_new();

		BLI_dynstr_append(ds, prepend);
		BLI_dynstr_append(ds, report->message);
		MEM_freeN((void *)report->message);

		report->message= BLI_dynstr_get_cstring(ds);
		report->len= BLI_dynstr_get_len(ds);

		BLI_dynstr_free(ds);
	}
}

void BKE_reports_prependf(ReportList *reports, const char *prepend, ...)
{
	Report *report;
	DynStr *ds;
	va_list args;

	if (!reports)
		return;

	for (report=reports->list.first; report; report=report->next) {
		ds= BLI_dynstr_new();
		va_start(args, prepend);
		BLI_dynstr_vappendf(ds, prepend, args);
		va_end(args);

		BLI_dynstr_append(ds, report->message);
		MEM_freeN((void *)report->message);

		report->message= BLI_dynstr_get_cstring(ds);
		report->len= BLI_dynstr_get_len(ds);

		BLI_dynstr_free(ds);
	}
}

ReportType BKE_report_print_level(ReportList *reports)
{
	if (!reports)
		return RPT_ERROR;

	return reports->printlevel;
}

void BKE_report_print_level_set(ReportList *reports, ReportType level)
{
	if (!reports)
		return;

	reports->printlevel= level;
}

ReportType BKE_report_store_level(ReportList *reports)
{
	if (!reports)
		return RPT_ERROR;

	return reports->storelevel;
}

void BKE_report_store_level_set(ReportList *reports, ReportType level)
{
	if (!reports)
		return;

	reports->storelevel= level;
}

char *BKE_reports_string(ReportList *reports, ReportType level)
{
	Report *report;
	DynStr *ds;
	char *cstring;

	if (!reports || !reports->list.first)
		return NULL;

	ds= BLI_dynstr_new();
	for (report=reports->list.first; report; report=report->next)
		if (report->type >= level)
			BLI_dynstr_appendf(ds, "%s: %s\n", report->typestr, report->message);

	if (BLI_dynstr_get_len(ds))
		cstring= BLI_dynstr_get_cstring(ds);
	else
		cstring= NULL;

	BLI_dynstr_free(ds);
	return cstring;
}

void BKE_reports_print(ReportList *reports, ReportType level)
{
	char *cstring = BKE_reports_string(reports, level);
	
	if (cstring == NULL)
		return;
	
	puts(cstring);
	fflush(stdout);
	MEM_freeN(cstring);
}

Report *BKE_reports_last_displayable(ReportList *reports)
{
	Report *report;
	
	for (report= reports->list.last; report; report=report->prev) {
		if (ELEM3(report->type, RPT_ERROR, RPT_WARNING, RPT_INFO))
			return report;
	}
	
	return NULL;
}

int BKE_reports_contain(ReportList *reports, ReportType level)
{
	Report *report;
	if (reports != NULL) {
		for (report=reports->list.first; report; report=report->next)
			if (report->type >= level)
				return TRUE;
	}
	return FALSE;
}

