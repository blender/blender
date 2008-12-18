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

void BKE_report_list_init(ReportList *reports, int flags)
{
	memset(reports, 0, sizeof(ReportList));

	reports->level= RPT_WARNING;
	reports->flags= flags;
}

void BKE_report_list_clear(ReportList *reports)
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

	if(!reports || type < reports->level)
		return;

	if(reports->flags & RPT_PRINT) {
		printf("%s: %s\n", report_type_str(type), message);
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if(reports->flags & RPT_STORE) {
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

	if(!reports || type < reports->level)
		return;

	if(reports->flags & RPT_PRINT) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		fflush(stdout); /* this ensures the message is printed before a crash */
	}

	if(reports->flags & RPT_STORE) {
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

ReportType BKE_report_level(ReportList *reports)
{
	return reports->level;
}

void BKE_report_level_set(ReportList *reports, ReportType level)
{
	reports->level= level;
}

int BKE_report_has_error(ReportList *reports)
{
	Report *report;

	if(!reports)
		return 0;

	for(report=reports->list.first; report; report=report->next)
		if(report->type >= RPT_ERROR)
			return 1;
	
	return 0;
}

