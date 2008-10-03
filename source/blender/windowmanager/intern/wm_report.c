
#include "MEM_guardedalloc.h"

#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#endif

static int wmReportLevel= WM_LOG_INFO;
static int wmReportPrint= 0;

static const char *wm_report_type_str(int type)
{
	switch(type) {
		case WM_LOG_DEBUG: return "Debug";
		case WM_LOG_INFO: return "Info";
		case WM_LOG_WARNING: return "Warning";
		case WM_ERROR_UNDEFINED: return "Error";
		case WM_ERROR_INVALID_INPUT: return "Invalid Input Error";
		case WM_ERROR_INVALID_CONTEXT: return "Invalid Context Error";
		case WM_ERROR_OUT_OF_MEMORY: return "Out Of Memory Error";
		default: return "Undefined Type";
	}
}

static void wm_print_report(wmReport *report)
{
	printf("%s: %s\n", report->typestr, report->message);
	fflush(stdout); /* this ensures the message is printed before a crash */
}

void WM_report(bContext *C, int type, const char *message)
{
	wmReport *report;
	int len;

	if(!C->wm) {
		fprintf(stderr, "WM_report: can't report without windowmanager.\n");
		return;
	}
	if(type < wmReportLevel)
		return;

	report= MEM_callocN(sizeof(wmReport), "wmReport");
	report->type= type;
	report->typestr= wm_report_type_str(type);

	len= strlen(message);
	report->message= MEM_callocN(sizeof(char)*(len+1), "wmReportMessage");
	memcpy(report->message, message, sizeof(char)*(len+1));

	if(wmReportPrint)
		wm_print_report(report);
	
	BLI_addtail(&C->wm->reports, report);
}

void WM_reportf(bContext *C, int type, const char *format, ...)
{
	wmReport *report;
	va_list args;
	char *message;
	int len= 256, maxlen= 65536, retval;

	if(!C->wm) {
		fprintf(stderr, "WM_report: can't report without windowmanager.\n");
		return;
	}
	if(type < wmReportLevel)
		return;

	while(1) {
		message= MEM_callocN(sizeof(char)*len+1, "wmReportMessage");

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
				fprintf(stderr, "WM_report message too long or format error.\n");
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
		report= MEM_callocN(sizeof(wmReport), "wmReport");
		report->type= type;
		report->typestr= wm_report_type_str(type);
		report->message= message;

		if(wmReportPrint)
			wm_print_report(report);

		BLI_addtail(&C->wm->reports, report);
	}
}

void wm_report_free(wmReport *report)
{
	MEM_freeN(report->message);
	MEM_freeN(report);
}

