/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_clog
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
 */

#pragma once

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

enum CLG_Level {
  /* Similar to assert. This logs the message, then a stack trace and abort. */
  CLG_LEVEL_FATAL = 0,
  /* An error we can recover from, should not happen. */
  CLG_LEVEL_ERROR = 1,
  /* General warnings (which aren't necessary to show to users). */
  CLG_LEVEL_WARN = 2,
  /* Information about devices, files, configuration, user operations. */
  CLG_LEVEL_INFO = 3,
  /* Debugging information for developers. */
  CLG_LEVEL_DEBUG = 4,
  /* Very verbose code execution tracing. */
  CLG_LEVEL_TRACE = 5,
};
#define CLG_LEVEL_LEN (CLG_LEVEL_TRACE + 1)

/* Each logger ID has one of these. */
struct CLG_LogType {
  struct CLG_LogType *next;
  char identifier[64];
  /** FILE output. */
  struct CLogContext *ctx;
  /** Control behavior. */
  CLG_Level level;
};

struct CLG_LogRef {
  CLG_LogRef(const char *identifier);

  const char *identifier;
  CLG_LogType *type;
  struct CLG_LogRef *next;
};

void CLG_log_str(const CLG_LogType *lg,
                 enum CLG_Level level,
                 const char *file_line,
                 const char *fn,
                 const char *message) _CLOG_ATTR_NONNULL(1, 3, 4, 5);
void CLG_logf(const CLG_LogType *lg,
              enum CLG_Level level,
              const char *file_line,
              const char *fn,
              const char *format,
              ...) _CLOG_ATTR_NONNULL(1, 3, 4, 5) _CLOG_ATTR_PRINTF_FORMAT(5, 6);
void CLG_log_raw(const CLG_LogType *lg, const char *message);

/* Main initializer and destructor (per session, not logger). */
void CLG_init();
void CLG_exit();

void CLG_output_set(void *file_handle);
void CLG_output_use_source_set(int value);
void CLG_output_use_basename_set(int value);
void CLG_output_use_timestamp_set(int value);
void CLG_output_use_memory_set(int value);
void CLG_error_fn_set(void (*error_fn)(void *file_handle));
void CLG_fatal_fn_set(void (*fatal_fn)(void *file_handle));
void CLG_backtrace_fn_set(void (*fatal_fn)(void *file_handle));

void CLG_type_filter_include(const char *type_match, int type_match_len);
void CLG_type_filter_exclude(const char *type_match, int type_match_len);

void CLG_level_set(CLG_Level level);

void CLG_logref_init(CLG_LogRef *clg_ref);

void CLG_logref_register(CLG_LogRef *clg_ref);
void CLG_logref_list_all(void (*callback)(const char *identifier, void *user_data),
                         void *user_data);

int CLG_color_support_get(CLG_LogRef *clg_ref);

/* When true, quiet any NOCHECK logs that would otherwise be printed regardless of log filters
 * and levels. This is used so command line tools can control output without unnecessary noise.
 *
 * Note this does not silence log filters and levels that have been explicitly enabled. */
void CLG_quiet_set(bool quiet);
bool CLG_quiet_get();

inline CLG_LogRef::CLG_LogRef(const char *identifier)
    : identifier(identifier), type(nullptr), next(nullptr)
{
  CLG_logref_register(this);
}

/** Declare outside function, declare as extern in header. */
#define CLG_LOGREF_DECLARE_GLOBAL(var, id) \
  static CLG_LogRef _static_##var = {id}; \
  CLG_LogRef *var = &_static_##var

/** Initialize struct once. */
#define CLOG_ENSURE(clg_ref) \
  ((clg_ref)->type ? (clg_ref)->type : (CLG_logref_init(clg_ref), (clg_ref)->type))

#define CLOG_CHECK(clg_ref, verbose_level, ...) \
  ((void)CLOG_ENSURE(clg_ref), ((clg_ref)->type->level >= verbose_level))

#define CLOG_AT_LEVEL(clg_ref, verbose_level, ...) \
  { \
    const CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (_lg_ty->level >= verbose_level) { \
      CLG_logf(_lg_ty, verbose_level, __FILE__ ":" STRINGIFY(__LINE__), __func__, __VA_ARGS__); \
    } \
  } \
  ((void)0)

#define CLOG_AT_LEVEL_NOCHECK(clg_ref, verbose_level, ...) \
  { \
    const CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (!CLG_quiet_get() || _lg_ty->level >= verbose_level) { \
      CLG_logf(_lg_ty, verbose_level, __FILE__ ":" STRINGIFY(__LINE__), __func__, __VA_ARGS__); \
    } \
  } \
  ((void)0)

#define CLOG_STR_AT_LEVEL(clg_ref, verbose_level, str) \
  { \
    const CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (_lg_ty->level >= verbose_level) { \
      CLG_log_str(_lg_ty, verbose_level, __FILE__ ":" STRINGIFY(__LINE__), __func__, str); \
    } \
  } \
  ((void)0)

#define CLOG_STR_AT_LEVEL_NOCHECK(clg_ref, verbose_level, str) \
  { \
    const CLG_LogType *_lg_ty = CLOG_ENSURE(clg_ref); \
    if (!CLG_quiet_get() || _lg_ty->level >= verbose_level) { \
      CLG_log_str(_lg_ty, verbose_level, __FILE__ ":" STRINGIFY(__LINE__), __func__, str); \
    } \
  } \
  ((void)0)

/* Log with format string. */
#define CLOG_FATAL(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_FATAL, __VA_ARGS__)
#define CLOG_ERROR(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_ERROR, __VA_ARGS__)
#define CLOG_WARN(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_WARN, __VA_ARGS__)
#define CLOG_INFO(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_INFO, __VA_ARGS__)
#define CLOG_DEBUG(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_DEBUG, __VA_ARGS__)
#define CLOG_TRACE(clg_ref, ...) CLOG_AT_LEVEL(clg_ref, CLG_LEVEL_TRACE, __VA_ARGS__)

/* Log single string. */
#define CLOG_STR_FATAL(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_FATAL, str)
#define CLOG_STR_ERROR(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_ERROR, str)
#define CLOG_STR_WARN(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_WARN, str)
#define CLOG_STR_INFO(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_INFO, str)
#define CLOG_STR_DEBUG(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_DEBUG, str)
#define CLOG_STR_TRACE(clg_ref, str) CLOG_STR_AT_LEVEL(clg_ref, CLG_LEVEL_TRACE, str)

/* Log regardless of filters and levels, for a few important messages like blend save and load.
 * Only #CLG_quiet_set will silence these. */
#define CLOG_INFO_NOCHECK(clg_ref, format, ...) \
  CLOG_AT_LEVEL_NOCHECK(clg_ref, CLG_LEVEL_INFO, format, __VA_ARGS__)
#define CLOG_STR_INFO_NOCHECK(clg_ref, str) CLOG_STR_AT_LEVEL_NOCHECK(clg_ref, CLG_LEVEL_INFO, str)
