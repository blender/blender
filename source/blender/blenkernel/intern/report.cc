/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_global.h" /* G.background only */
#include "BKE_report.h"

const char *BKE_report_type_str(eReportType type)
{
  switch (type) {
    case RPT_DEBUG:
      return TIP_("Debug");
    case RPT_INFO:
      return TIP_("Info");
    case RPT_OPERATOR:
      return TIP_("Operator");
    case RPT_PROPERTY:
      return TIP_("Property");
    case RPT_WARNING:
      return TIP_("Warning");
    case RPT_ERROR:
      return TIP_("Error");
    case RPT_ERROR_INVALID_INPUT:
      return TIP_("Invalid Input Error");
    case RPT_ERROR_INVALID_CONTEXT:
      return TIP_("Invalid Context Error");
    case RPT_ERROR_OUT_OF_MEMORY:
      return TIP_("Out Of Memory Error");
    default:
      return TIP_("Undefined Type");
  }
}

void BKE_reports_init(ReportList *reports, int flag)
{
  if (!reports) {
    return;
  }

  memset(reports, 0, sizeof(ReportList));

  reports->storelevel = RPT_INFO;
  reports->printlevel = RPT_ERROR;
  reports->flag = flag;
}

void BKE_reports_clear(ReportList *reports)
{
  Report *report, *report_next;

  if (!reports) {
    return;
  }

  report = static_cast<Report *>(reports->list.first);

  while (report) {
    report_next = report->next;
    MEM_freeN((void *)report->message);
    MEM_freeN(report);
    report = report_next;
  }

  BLI_listbase_clear(&reports->list);
}

void BKE_report(ReportList *reports, eReportType type, const char *_message)
{
  Report *report;
  int len;
  const char *message = TIP_(_message);

  if (BKE_reports_print_test(reports, type)) {
    printf("%s: %s\n", BKE_report_type_str(type), message);
    fflush(stdout); /* this ensures the message is printed before a crash */
  }

  if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
    char *message_alloc;
    report = static_cast<Report *>(MEM_callocN(sizeof(Report), "Report"));
    report->type = type;
    report->typestr = BKE_report_type_str(type);

    len = strlen(message);
    message_alloc = static_cast<char *>(MEM_mallocN(sizeof(char) * (len + 1), "ReportMessage"));
    memcpy(message_alloc, message, sizeof(char) * (len + 1));
    report->message = message_alloc;
    report->len = len;
    BLI_addtail(&reports->list, report);
  }
}

void BKE_reportf(ReportList *reports, eReportType type, const char *_format, ...)
{
  Report *report;
  va_list args;
  const char *format = TIP_(_format);

  if (BKE_reports_print_test(reports, type)) {
    printf("%s: ", BKE_report_type_str(type));
    va_start(args, _format);
    vprintf(format, args);
    va_end(args);
    fprintf(stdout, "\n"); /* otherwise each report needs to include a \n */
    fflush(stdout);        /* this ensures the message is printed before a crash */
  }

  if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
    report = static_cast<Report *>(MEM_callocN(sizeof(Report), "Report"));

    va_start(args, _format);
    report->message = BLI_vsprintfN(format, args);
    va_end(args);

    report->len = strlen(report->message);
    report->type = type;
    report->typestr = BKE_report_type_str(type);

    BLI_addtail(&reports->list, report);
  }
}

/**
 * Shared logic behind #BKE_reports_prepend & #BKE_reports_prependf.
 */
static void reports_prepend_impl(ReportList *reports, const char *prepend)
{
  /* Caller must ensure. */
  BLI_assert(reports && reports->list.first);
  const size_t prefix_len = strlen(prepend);
  for (Report *report = static_cast<Report *>(reports->list.first); report; report = report->next)
  {
    char *message = BLI_string_joinN(prepend, report->message);
    MEM_freeN((void *)report->message);
    report->message = message;
    report->len += prefix_len;
    BLI_assert(report->len == strlen(message));
  }
}

void BKE_reports_prepend(ReportList *reports, const char *prepend)
{
  if (!reports || !reports->list.first) {
    return;
  }
  reports_prepend_impl(reports, TIP_(prepend));
}

void BKE_reports_prependf(ReportList *reports, const char *prepend_format, ...)
{
  if (!reports || !reports->list.first) {
    return;
  }
  va_list args;
  va_start(args, prepend_format);
  char *prepend = BLI_vsprintfN(TIP_(prepend_format), args);
  va_end(args);

  reports_prepend_impl(reports, prepend);

  MEM_freeN(prepend);
}

eReportType BKE_report_print_level(ReportList *reports)
{
  if (!reports) {
    return RPT_ERROR;
  }

  return eReportType(reports->printlevel);
}

void BKE_report_print_level_set(ReportList *reports, eReportType level)
{
  if (!reports) {
    return;
  }

  reports->printlevel = level;
}

eReportType BKE_report_store_level(ReportList *reports)
{
  if (!reports) {
    return RPT_ERROR;
  }

  return eReportType(reports->storelevel);
}

void BKE_report_store_level_set(ReportList *reports, eReportType level)
{
  if (!reports) {
    return;
  }

  reports->storelevel = level;
}

char *BKE_reports_string(ReportList *reports, eReportType level)
{
  Report *report;
  DynStr *ds;
  char *cstring;

  if (!reports || !reports->list.first) {
    return nullptr;
  }

  ds = BLI_dynstr_new();
  for (report = static_cast<Report *>(reports->list.first); report; report = report->next) {
    if (report->type >= level) {
      BLI_dynstr_appendf(ds, "%s: %s\n", report->typestr, report->message);
    }
  }

  if (BLI_dynstr_get_len(ds)) {
    cstring = BLI_dynstr_get_cstring(ds);
  }
  else {
    cstring = nullptr;
  }

  BLI_dynstr_free(ds);
  return cstring;
}

bool BKE_reports_print_test(const ReportList *reports, eReportType type)
{
  if (reports == nullptr) {
    return true;
  }
  if (reports->flag & RPT_PRINT_HANDLED_BY_OWNER) {
    return false;
  }
  /* In background mode always print otherwise there are cases the errors won't be displayed,
   * but still add to the report list since this is used for Python exception handling. */
  if (G.background) {
    return true;
  }

  /* Common case. */
  return (reports->flag & RPT_PRINT) && (type >= reports->printlevel);
}

void BKE_reports_print(ReportList *reports, eReportType level)
{
  char *cstring = BKE_reports_string(reports, level);

  if (cstring == nullptr) {
    return;
  }

  puts(cstring);
  fflush(stdout);
  MEM_freeN(cstring);
}

Report *BKE_reports_last_displayable(ReportList *reports)
{
  Report *report;

  for (report = static_cast<Report *>(reports->list.last); report; report = report->prev) {
    if (ELEM(report->type, RPT_ERROR, RPT_WARNING, RPT_INFO)) {
      return report;
    }
  }

  return nullptr;
}

bool BKE_reports_contain(ReportList *reports, eReportType level)
{
  Report *report;
  if (reports != nullptr) {
    for (report = static_cast<Report *>(reports->list.first); report; report = report->next) {
      if (report->type >= level) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_report_write_file_fp(FILE *fp, ReportList *reports, const char *header)
{
  Report *report;

  if (header) {
    fputs(header, fp);
  }

  for (report = static_cast<Report *>(reports->list.first); report; report = report->next) {
    fprintf((FILE *)fp, "%s  # %s\n", report->message, report->typestr);
  }

  return true;
}

bool BKE_report_write_file(const char *filepath, ReportList *reports, const char *header)
{
  FILE *fp;

  errno = 0;
  fp = BLI_fopen(filepath, "wb");
  if (fp == nullptr) {
    fprintf(stderr,
            "Unable to save '%s': %s\n",
            filepath,
            errno ? strerror(errno) : "Unknown error opening file");
    return false;
  }

  BKE_report_write_file_fp(fp, reports, header);

  fclose(fp);

  return true;
}
