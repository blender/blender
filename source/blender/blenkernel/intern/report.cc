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
#include <mutex>

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_global.hh" /* G.background only */
#include "BKE_report.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"reports"};

void BKE_report_log(eReportType type, const char *message, CLG_LogRef *log)
{
  switch (type) {
    case RPT_DEBUG:
      CLOG_STR_DEBUG(log, message);
      break;
    case RPT_INFO:
    case RPT_OPERATOR:
    case RPT_PROPERTY:
      CLOG_INFO_NOCHECK(log, "%s", message);
      break;
    case RPT_WARNING:
      CLOG_STR_WARN(log, message);
      break;
    case RPT_ERROR:
    case RPT_ERROR_INVALID_INPUT:
    case RPT_ERROR_INVALID_CONTEXT:
    case RPT_ERROR_OUT_OF_MEMORY:
      CLOG_STR_ERROR(log, message);
      break;
  }
}

const char *BKE_report_type_str(eReportType type)
{
  switch (type) {
    case RPT_DEBUG:
      return RPT_("Debug");
    case RPT_INFO:
      return RPT_("Info");
    case RPT_OPERATOR:
      return RPT_("Operator");
    case RPT_PROPERTY:
      return RPT_("Property");
    case RPT_WARNING:
      return RPT_("Warning");
    case RPT_ERROR:
      return RPT_("Error");
    case RPT_ERROR_INVALID_INPUT:
      return RPT_("Invalid Input Error");
    case RPT_ERROR_INVALID_CONTEXT:
      return RPT_("Invalid Context Error");
    case RPT_ERROR_OUT_OF_MEMORY:
      return RPT_("Out Of Memory Error");
    default:
      return RPT_("Undefined Type");
  }
}

void BKE_reports_init(ReportList *reports, int flag)
{
  if (!reports) {
    return;
  }

  *reports = ReportList{};

  reports->storelevel = RPT_INFO;
  reports->printlevel = RPT_ERROR;
  reports->flag = flag;

  reports->lock = MEM_new<std::mutex>(__func__);
}

void BKE_reports_free(ReportList *reports)
{
  if (!reports) {
    return;
  }

  BKE_reports_clear(reports);

  MEM_delete(reports->lock);
  reports->lock = nullptr;
}

void BKE_reports_clear(ReportList *reports)
{
  Report *report, *report_next;

  if (!reports) {
    return;
  }

  std::scoped_lock lock(*reports->lock);

  report = static_cast<Report *>(reports->list.first);

  while (report) {
    report_next = report->next;
    MEM_freeN(report->message);
    MEM_freeN(report);
    report = report_next;
  }

  BLI_listbase_clear(&reports->list);
}

void BKE_reports_lock(ReportList *reports)
{
  reports->lock->lock();
}

void BKE_reports_unlock(ReportList *reports)
{
  reports->lock->unlock();
}

void BKE_reports_move_to_reports(ReportList *reports_dst, ReportList *reports_src)
{
  BLI_assert(reports_dst);
  if (!reports_src) {
    return;
  }

  std::scoped_lock lock(*reports_src->lock, *reports_dst->lock);

  BLI_movelisttolist(&reports_dst->list, &reports_src->list);
}

void BKE_report(ReportList *reports, eReportType type, const char *_message)
{
  Report *report;
  int len;
  const char *message = RPT_(_message);

  if (BKE_reports_print_test(reports, type)) {
    BKE_report_log(type, message, &LOG);
    fflush(stdout); /* this ensures the message is printed before a crash */
  }

  if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
    std::scoped_lock lock(*reports->lock);

    char *message_alloc;
    report = MEM_callocN<Report>("Report");
    report->type = type;
    report->typestr = BKE_report_type_str(type);

    len = strlen(message);
    message_alloc = MEM_malloc_arrayN<char>(size_t(len) + 1, "ReportMessage");
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
  const char *format = RPT_(_format);

  if (BKE_reports_print_test(reports, type)) {
    va_start(args, _format);
    const char *message = BLI_vsprintfN(format, args);
    va_end(args);
    BKE_report_log(type, message, &LOG);
    fflush(stdout); /* this ensures the message is printed before a crash */
    MEM_freeN(message);
  }

  if (reports && (reports->flag & RPT_STORE) && (type >= reports->storelevel)) {
    std::scoped_lock lock(*reports->lock);

    report = MEM_callocN<Report>("Report");

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

  std::scoped_lock lock(*reports->lock);

  const size_t prefix_len = strlen(prepend);
  LISTBASE_FOREACH (Report *, report, &reports->list) {
    char *message = BLI_string_joinN(prepend, report->message);
    MEM_freeN(report->message);
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
  reports_prepend_impl(reports, RPT_(prepend));
}

void BKE_reports_prependf(ReportList *reports, const char *prepend_format, ...)
{
  if (!reports || !reports->list.first) {
    return;
  }
  va_list args;
  va_start(args, prepend_format);
  char *prepend = BLI_vsprintfN(RPT_(prepend_format), args);
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

  std::scoped_lock lock(*reports->lock);

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

  std::scoped_lock lock(*reports->lock);

  reports->storelevel = level;
}

char *BKE_reports_string(ReportList *reports, eReportType level)
{
  DynStr *ds;
  char *cstring;

  if (!reports || !reports->list.first) {
    return nullptr;
  }

  std::scoped_lock lock(*reports->lock);

  ds = BLI_dynstr_new();
  LISTBASE_FOREACH (Report *, report, &reports->list) {
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

void BKE_reports_log(ReportList *reports, eReportType level, CLG_LogRef *log)
{
  if (reports == nullptr) {
    return;
  }

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    if (report->type >= level) {
      BKE_report_log(eReportType(report->type), report->message, log);
    }
  }
}

void BKE_reports_print(ReportList *reports, eReportType level)
{
  char *cstring = BKE_reports_string(reports, level);

  if (cstring == nullptr) {
    return;
  }

  /* A trailing newline is already part of `cstring`. */
  fputs(cstring, stdout);
  fflush(stdout);
  MEM_freeN(cstring);
}

Report *BKE_reports_last_displayable(ReportList *reports)
{
  std::scoped_lock lock(*reports->lock);

  LISTBASE_FOREACH_BACKWARD (Report *, report, &reports->list) {
    if (ELEM(report->type, RPT_ERROR, RPT_WARNING, RPT_INFO)) {
      return report;
    }
  }

  return nullptr;
}

bool BKE_reports_contain(ReportList *reports, eReportType level)
{
  if (reports != nullptr) {
    std::scoped_lock lock(*reports->lock);

    LISTBASE_FOREACH (Report *, report, &reports->list) {
      if (report->type >= level) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_report_write_file_fp(FILE *fp, ReportList *reports, const char *header)
{
  if (header) {
    fputs(header, fp);
  }

  std::scoped_lock lock(*reports->lock);

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    fprintf(fp, "%s  # %s\n", report->message, report->typestr);
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
