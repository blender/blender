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

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_blenlib.h"

#include "BKE_report.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif

static char *report_type_str(int type)
{
	switch(type) {
		case RPT_DEBUG: return "Debug";
		case RPT_INFO: return "Info";
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
	memset(reports, 0, sizeof(ReportList));

	reports->storelevel= RPT_WARNING;
	reports->printlevel= RPT_WARNING;
	reports->flag= flag;
}

void BKE_reports_clear(ReportList *reports)
{
	Report *report;

	for(report=reports->list.first; report; report=report->next)
		MEM_freeN(report->message);

	BLI_freelistN(&reports->list);
}

void BKE_report(ReportList *reports, ReportType type, const char *message)
{
	Report *report;
	int len;

	if(!reports)
		return;
	
	if(type >= RPT_ERROR)
		reports->flag |= RPT_HAS_ERROR;

	if((reports->flag & RPT_PRINT) && (type >= reports->printlevel)) {
		printf("%s: %s\n", report_type_str(type), message);
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if((reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
		report= MEM_callocN(sizeof(Report), "Report");
		report->type= type;
		report->typestr= report_type_str(type);

		len= strlen(message);
		report->message= MEM_callocN(sizeof(char)*(len+1), "ReportMessage");
		memcpy(report->message, message, sizeof(char)*(len+1));
		
		BLI_addtail(&reports->list, report);
	}
}

void BKE_reportf(ReportList *reports, ReportType type, const char *format, ...)
{
	Report *report;
	va_list args;
	char *message;
	int len= 256, maxlen= 65536, retval;

	if(!reports)
		return;

	if(type >= RPT_ERROR)
		reports->flag |= RPT_HAS_ERROR;

	if((reports->flag & RPT_PRINT) && (type >= reports->printlevel)) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if((reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
		while(1) {
			message= MEM_callocN(sizeof(char)*len+1, "ReportMessage");

			va_start(args, format);
			retval= vsnprintf(message, len, format, args);
			va_end(args);

			if(retval == -1) {
				/* -1 means not enough space, but on windows it may also mean
				 * there is a formatting error, so we impose a maximum length */
				MEM_freeN(message);
				message= NULL;

				len *= 2;
				if(len > maxlen) {
					fprintf(stderr, "BKE_reportf message too long or format error.\n");
					break;
				}
			}
			else if(retval > len) {
				/* in C99 the actual length required is returned */
				MEM_freeN(message);
				message= NULL;

				len= retval;
			}
			else
				break;
		}

		if(message) {
			report= MEM_callocN(sizeof(Report), "Report");
			report->type= type;
			report->typestr= report_type_str(type);
			report->message= message;

			BLI_addtail(&reports->list, report);
		}
	}
}

ReportType BKE_report_print_level(ReportList *reports)
{
	return reports->printlevel;
}

void BKE_report_print_level_set(ReportList *reports, ReportType level)
{
	reports->printlevel= level;
}

ReportType BKE_report_store_level(ReportList *reports)
{
	return reports->storelevel;
}

void BKE_report_store_level_set(ReportList *reports, ReportType level)
{
	reports->storelevel= level;
}

void BKE_reports_print(ReportList *reports, ReportType level)
{
	Report *report;

	if(!reports)
		return;
	
	for(report=reports->list.first; report; report=report->next)
		if(report->type >= level)
			printf("%s: %s\n", report->typestr, report->message);

	fflush(stdout);
}

