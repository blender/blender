/*
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
 */

#ifndef __CLG_LOG_H__
#define __CLG_LOG_H__

/** \file
 * \ingroup clog
 *
 * C Logging Library (clog)
 * ========================
 *
 * Usage
 * -----
 *
 * - `CLG_LOGREF_DECLARE_GLOBAL` macro to declare #CLG_LogRef pointers.
 * - `CLOG_` prefixed macros for logging.
 *
 * Identifiers
 * -----------
 *
 * #CLG_LogRef holds an identifier which defines the category of the logger.
 *
 * You can define and use identifiers as needed, logging will lazily initialize them.
 *
 * By convention lower case dot separated identifiers are used, eg:
 * `module.sub_module`, this allows filtering by `module.*`,
 * see #CLG_type_filter_include, #CLG_type_filter_exclude
 *
 * There is currently no functionality to remove a category once it's created.
 *
 * Severity
 * --------
 *
 * - `INFO`: Simply log events, uses verbosity levels to control how much information to show.
 * - `WARN`: General warnings (which aren't necessary to show to users).
 * - `ERROR`: An error we can recover from, should not happen.
 * - `FATAL`: Similar to assert. This logs the message, then a stack trace and abort.
 * Verbosity Level
 * ---------------
 *
 * Usage:
 *
 * - 0: Always show (used for warnings, errors).
 *   Should never get in the way or become annoying.
 *
 * - 1: Top level module actions (eg: load a file, create a new window .. etc).
 *
 * - 2: Actions within a module (steps which compose an action, but don't flood output).
 *   Running a tool, full data recalculation.
 *
 * - 3: Detailed actions which may be of interest when debugging internal logic of a module
 *   These *may* flood the log with details.
 *
 * - 4+: May be used for more details than 3, should be avoided but not prevented.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __GNUC__
#  define _CLOG_ATTR_NONNULL(args...) __attribute__((nonnull(args)))
#else
#  define _CLOG_ATTR_NONNULL(...)
#endif

#ifdef __GNUC__
#  define _CLOG_ATTR_PRINTF_FORMAT(format_param, dots_param) \
    __attribute__((format(printf, format_param, dots_param)))
#else
#  define _CLOG_ATTR_PRINTF_FORMAT(format_param, dots_param)
#endif

#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)

struct CLogContext;

/* Don't typedef enums. */
enum CLG_LogFlag {
  CLG_FLAG_USE = (1 << 0),
};

enum CLG_Severity {
  CLG_SEVERITY_INFO = 0,
  CLG_SEVERITY_WARN,
  CLG_SEVERITY_ERROR,
  CLG_SEVERITY_FATAL,
};
#define CLG_SEVERITY_LEN (CLG_SEVERITY_FATAL + 1)

/* Each logger ID has one of these. */
typedef struct CLG_LogType {
  struct CLG_LogType *next;
  char identifier[64];
  /** FILE output. */
  struct CLogContext *ctx;
  /** Control behavior. */
  int level;
  enum CLG_LogFlag flag;
} CLG_LogType;

typedef struct CLG_LogRef {
  const char *identifier;
  CLG_LogType *type;
} CLG_LogRef;

void CLG_log_str(CLG_LogType *lg,
                 enum CLG_Severity severity,
                 const char *file_line,
                 const char *fn,
                 const char *message) _CLOG_ATTR_NONNULL(1, 3, 4, 5);
void CLG_logf(CLG_LogType *lg,
              enum CLG_Severity severity,
              const char *file_line,
              const char *fn,
              const char *format,
              ...) _CLOG_ATTR_NONNULL(1, 3, 4, 5) _CLOG_ATTR_PRINTF_FORMAT(5, 6);

/* Main initializer and distructor (per session, not logger). */
void CLG_init(void);
void CLG_exit(void);

void CLG_output_set(void *file_handle);
void CLG_output_use_basename_set(int value);
void CLG_output_use_timestamp_set(int value);
void CLG_fatal_fn_set(void (*fatal_fn)(void *file_handle));
void CLG_backtrace_fn_set(void (*fatal_fn)(void *file_handle));

void CLG_type_filter_include(const char *type_filter, int type_filter_len);
void CLG_type_filter_exclude(const char *type_filter, int type_filter_len);

void CLG_level_set(int level);

void CLG_logref_init(CLG_LogRef *clg_ref);

/** Declare outside function, declare as extern in header. */
#define CLG_LOGREF_DECLARE_GLOBAL(var, id) \
  static CLG_LogRef _static_##var = {id}; \
  CLG_LogRef *var = &_static_##var

/** Initialize struct once. */
#define CLOG_ENSURE(clg_ref) \
  ((clg_ref)->type ? (clg_ref)->type : (CLG_logref_init(clg_ref), (clg_ref)->type))

#define CLOG_CHECK(clg_ref, verbose_level, ...) \
  ((void)CLOG_ENSURE(clg_ref), \
   ((clg_ref)->type->flag & CLG_FLAG_USE) && ((clg_ref)->type->level >= verbose_level))

#define CLOG_AT_SEVERITY(clg_ref, severity, verbose_level, ...) \
  { \
    CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (((_lg_ty->flag & CLG_FLAG_USE) && (_lg_ty->level >= verbose_level)) || \
        (severity >= CLG_SEVERITY_WARN)) { \
      CLG_logf(_lg_ty, severity, __FILE__ ":" STRINGIFY(__LINE__), __func__, __VA_ARGS__); \
    } \
  } \
  ((void)0)

#define CLOG_STR_AT_SEVERITY(clg_ref, severity, verbose_level, str) \
  { \
    CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (((_lg_ty->flag & CLG_FLAG_USE) && (_lg_ty->level >= verbose_level)) || \
        (severity >= CLG_SEVERITY_WARN)) { \
      CLG_log_str(_lg_ty, severity, __FILE__ ":" STRINGIFY(__LINE__), __func__, str); \
    } \
  } \
  ((void)0)

#define CLOG_STR_AT_SEVERITY_N(clg_ref, severity, verbose_level, str) \
  { \
    CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (((_lg_ty->flag & CLG_FLAG_USE) && (_lg_ty->level >= verbose_level)) || \
        (severity >= CLG_SEVERITY_WARN)) { \
      const char *_str = str; \
      CLG_log_str(_lg_ty, severity, __FILE__ ":" STRINGIFY(__LINE__), __func__, _str); \
      MEM_freeN((void *)_str); \
    } \
  } \
  ((void)0)

#define CLOG_INFO(clg_ref, level, ...) \
  CLOG_AT_SEVERITY(clg_ref, CLG_SEVERITY_INFO, level, __VA_ARGS__)
#define CLOG_WARN(clg_ref, ...) CLOG_AT_SEVERITY(clg_ref, CLG_SEVERITY_WARN, 0, __VA_ARGS__)
#define CLOG_ERROR(clg_ref, ...) CLOG_AT_SEVERITY(clg_ref, CLG_SEVERITY_ERROR, 0, __VA_ARGS__)
#define CLOG_FATAL(clg_ref, ...) CLOG_AT_SEVERITY(clg_ref, CLG_SEVERITY_FATAL, 0, __VA_ARGS__)

#define CLOG_STR_INFO(clg_ref, level, str) \
  CLOG_STR_AT_SEVERITY(clg_ref, CLG_SEVERITY_INFO, level, str)
#define CLOG_STR_WARN(clg_ref, str) CLOG_STR_AT_SEVERITY(clg_ref, CLG_SEVERITY_WARN, 0, str)
#define CLOG_STR_ERROR(clg_ref, str) CLOG_STR_AT_SEVERITY(clg_ref, CLG_SEVERITY_ERROR, 0, str)
#define CLOG_STR_FATAL(clg_ref, str) CLOG_STR_AT_SEVERITY(clg_ref, CLG_SEVERITY_FATAL, 0, str)

/* Allocated string which is immediately freed. */
#define CLOG_STR_INFO_N(clg_ref, level, str) \
  CLOG_STR_AT_SEVERITY_N(clg_ref, CLG_SEVERITY_INFO, level, str)
#define CLOG_STR_WARN_N(clg_ref, str) CLOG_STR_AT_SEVERITY_N(clg_ref, CLG_SEVERITY_WARN, 0, str)
#define CLOG_STR_ERROR_N(clg_ref, str) CLOG_STR_AT_SEVERITY_N(clg_ref, CLG_SEVERITY_ERROR, 0, str)
#define CLOG_STR_FATAL_N(clg_ref, str) CLOG_STR_AT_SEVERITY_N(clg_ref, CLG_SEVERITY_FATAL, 0, str)

#ifdef __cplusplus
}
#endif

#endif /* __CLG_LOG_H__ */
