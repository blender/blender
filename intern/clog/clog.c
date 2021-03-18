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

/** \file
 * \ingroup clog
 */

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Disable for small single threaded programs
 * to avoid having to link with pthreads. */
#ifdef WITH_CLOG_PTHREADS
#  include "atomic_ops.h"
#  include <pthread.h>
#endif

/* For 'isatty' to check for color. */
#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
#  include <sys/time.h>
#  include <unistd.h>
#endif

#if defined(_MSC_VER)
#  include <Windows.h>

#  include <VersionHelpers.h> /* This needs to be included after Windows.h. */
#  include <io.h>
#  if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#endif

/* For printing timestamp. */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

/* Only other dependency (could use regular malloc too). */
#include "MEM_guardedalloc.h"

/* own include. */
#include "CLG_log.h"

/* Local utility defines */
#define STREQ(a, b) (strcmp(a, b) == 0)
#define STREQLEN(a, b, n) (strncmp(a, b, n) == 0)

#ifdef _WIN32
#  define PATHSEP_CHAR '\\'
#else
#  define PATHSEP_CHAR '/'
#endif

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

typedef struct CLG_IDFilter {
  struct CLG_IDFilter *next;
  /** Over alloc. */
  char match[0];
} CLG_IDFilter;

typedef struct CLogContext {
  /** Single linked list of types.  */
  CLG_LogType *types;
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_t types_lock;
#endif

  /* exclude, include filters.  */
  CLG_IDFilter *filters[2];
  bool use_color;
  bool use_basename;
  bool use_timestamp;

  /** Borrowed, not owned. */
  int output;
  FILE *output_file;

  /** For timer (use_timestamp). */
  uint64_t timestamp_tick_start;

  /** For new types. */
  struct {
    int level;
  } default_type;

  struct {
    void (*error_fn)(void *file_handle);
    void (*fatal_fn)(void *file_handle);
    void (*backtrace_fn)(void *file_handle);
  } callbacks;
} CLogContext;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mini Buffer Functionality
 *
 * Use so we can do a single call to write.
 * \{ */

#define CLOG_BUF_LEN_INIT 512

typedef struct CLogStringBuf {
  char *data;
  uint len;
  uint len_alloc;
  bool is_alloc;
} CLogStringBuf;

static void clg_str_init(CLogStringBuf *cstr, char *buf_stack, uint buf_stack_len)
{
  cstr->data = buf_stack;
  cstr->len_alloc = buf_stack_len;
  cstr->len = 0;
  cstr->is_alloc = false;
}

static void clg_str_free(CLogStringBuf *cstr)
{
  if (cstr->is_alloc) {
    MEM_freeN(cstr->data);
  }
}

static void clg_str_reserve(CLogStringBuf *cstr, const uint len)
{
  if (len > cstr->len_alloc) {
    cstr->len_alloc *= 2;
    if (len > cstr->len_alloc) {
      cstr->len_alloc = len;
    }

    if (cstr->is_alloc) {
      cstr->data = MEM_reallocN(cstr->data, cstr->len_alloc);
    }
    else {
      /* Copy the static buffer. */
      char *data = MEM_mallocN(cstr->len_alloc, __func__);
      memcpy(data, cstr->data, cstr->len);
      cstr->data = data;
      cstr->is_alloc = true;
    }
  }
}

static void clg_str_append_with_len(CLogStringBuf *cstr, const char *str, const uint len)
{
  uint len_next = cstr->len + len;
  clg_str_reserve(cstr, len_next);
  char *str_dst = cstr->data + cstr->len;
  memcpy(str_dst, str, len);
#if 0 /* no need. */
  str_dst[len] = '\0';
#endif
  cstr->len = len_next;
}

static void clg_str_append(CLogStringBuf *cstr, const char *str)
{
  clg_str_append_with_len(cstr, str, strlen(str));
}

ATTR_PRINTF_FORMAT(2, 0)
static void clg_str_vappendf(CLogStringBuf *cstr, const char *fmt, va_list args)
{
  /* Use limit because windows may use '-1' for a formatting error. */
  const uint len_max = 65535;
  while (true) {
    uint len_avail = cstr->len_alloc - cstr->len;

    va_list args_cpy;
    va_copy(args_cpy, args);
    int retval = vsnprintf(cstr->data + cstr->len, len_avail, fmt, args_cpy);
    va_end(args_cpy);

    if (retval < 0) {
      /* Some encoding error happened, not much we can do here, besides skipping/canceling this
       * message. */
      break;
    }
    else if ((uint)retval <= len_avail) {
      /* Copy was successful. */
      cstr->len += (uint)retval;
      break;
    }
    else {
      /* vsnprintf was not successful, due to lack of allocated space, retval contains expected
       * length of the formatted string, use it to allocate required amount of memory. */
      uint len_alloc = cstr->len + (uint)retval;
      if (len_alloc >= len_max) {
        /* Safe upper-limit, just in case... */
        break;
      }
      clg_str_reserve(cstr, len_alloc);
      len_avail = cstr->len_alloc - cstr->len;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

enum eCLogColor {
  COLOR_DEFAULT,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_YELLOW,

  COLOR_RESET,
};
#define COLOR_LEN (COLOR_RESET + 1)

static const char *clg_color_table[COLOR_LEN] = {NULL};
#ifdef _WIN32
static DWORD clg_previous_console_mode = 0;
#endif

static void clg_color_table_init(bool use_color)
{
  for (int i = 0; i < COLOR_LEN; i++) {
    clg_color_table[i] = "";
  }
  if (use_color) {
    clg_color_table[COLOR_DEFAULT] = "\033[1;37m";
    clg_color_table[COLOR_RED] = "\033[1;31m";
    clg_color_table[COLOR_GREEN] = "\033[1;32m";
    clg_color_table[COLOR_YELLOW] = "\033[1;33m";
    clg_color_table[COLOR_RESET] = "\033[0m";
  }
}

static const char *clg_severity_str[CLG_SEVERITY_LEN] = {
    [CLG_SEVERITY_INFO] = "INFO",
    [CLG_SEVERITY_WARN] = "WARN",
    [CLG_SEVERITY_ERROR] = "ERROR",
    [CLG_SEVERITY_FATAL] = "FATAL",
};

static const char *clg_severity_as_text(enum CLG_Severity severity)
{
  bool ok = (unsigned int)severity < CLG_SEVERITY_LEN;
  assert(ok);
  if (ok) {
    return clg_severity_str[severity];
  }
  else {
    return "INVALID_SEVERITY";
  }
}

static enum eCLogColor clg_severity_to_color(enum CLG_Severity severity)
{
  assert((unsigned int)severity < CLG_SEVERITY_LEN);
  enum eCLogColor color = COLOR_DEFAULT;
  switch (severity) {
    case CLG_SEVERITY_INFO:
      color = COLOR_DEFAULT;
      break;
    case CLG_SEVERITY_WARN:
      color = COLOR_YELLOW;
      break;
    case CLG_SEVERITY_ERROR:
    case CLG_SEVERITY_FATAL:
      color = COLOR_RED;
      break;
    default:
      /* should never get here. */
      assert(false);
  }
  return color;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Type Access
 * \{ */

/**
 * Filter the identifier based on very basic globbing.
 * - `foo` exact match of `foo`.
 * - `foo.bar` exact match for `foo.bar`
 * - `foo.*` match for `foo` & `foo.bar` & `foo.bar.baz`
 * - `*bar*` match for `foo.bar` & `baz.bar` & `foo.barbaz`
 * - `*` matches everything.
 */
static bool clg_ctx_filter_check(CLogContext *ctx, const char *identifier)
{
  const size_t identifier_len = strlen(identifier);
  for (uint i = 0; i < 2; i++) {
    const CLG_IDFilter *flt = ctx->filters[i];
    while (flt != NULL) {
      const size_t len = strlen(flt->match);
      if (STREQ(flt->match, "*") || ((len == identifier_len) && (STREQ(identifier, flt->match)))) {
        return (bool)i;
      }
      if (flt->match[0] == '*' && flt->match[len - 1] == '*') {
        char *match = MEM_callocN(sizeof(char) * len - 1, __func__);
        memcpy(match, flt->match + 1, len - 2);
        if (strstr(identifier, match) != NULL) {
          return (bool)i;
        }
      }
      else if ((len >= 2) && (STREQLEN(".*", &flt->match[len - 2], 2))) {
        if (((identifier_len == len - 2) && STREQLEN(identifier, flt->match, len - 2)) ||
            ((identifier_len >= len - 1) && STREQLEN(identifier, flt->match, len - 1))) {
          return (bool)i;
        }
      }
      flt = flt->next;
    }
  }
  return false;
}

/**
 * \note This should never be called per logging call.
 * Searching is only to get an initial handle.
 */
static CLG_LogType *clg_ctx_type_find_by_name(CLogContext *ctx, const char *identifier)
{
  for (CLG_LogType *ty = ctx->types; ty; ty = ty->next) {
    if (STREQ(identifier, ty->identifier)) {
      return ty;
    }
  }
  return NULL;
}

static CLG_LogType *clg_ctx_type_register(CLogContext *ctx, const char *identifier)
{
  assert(clg_ctx_type_find_by_name(ctx, identifier) == NULL);
  CLG_LogType *ty = MEM_callocN(sizeof(*ty), __func__);
  ty->next = ctx->types;
  ctx->types = ty;
  strncpy(ty->identifier, identifier, sizeof(ty->identifier) - 1);
  ty->ctx = ctx;
  ty->level = ctx->default_type.level;

  if (clg_ctx_filter_check(ctx, ty->identifier)) {
    ty->flag |= CLG_FLAG_USE;
  }
  return ty;
}

static void clg_ctx_error_action(CLogContext *ctx)
{
  if (ctx->callbacks.error_fn != NULL) {
    ctx->callbacks.error_fn(ctx->output_file);
  }
}

static void clg_ctx_fatal_action(CLogContext *ctx)
{
  if (ctx->callbacks.fatal_fn != NULL) {
    ctx->callbacks.fatal_fn(ctx->output_file);
  }
  fflush(ctx->output_file);
  abort();
}

static void clg_ctx_backtrace(CLogContext *ctx)
{
  /* Note: we avoid writing to 'FILE', for back-trace we make an exception,
   * if necessary we could have a version of the callback that writes to file
   * descriptor all at once. */
  ctx->callbacks.backtrace_fn(ctx->output_file);
  fflush(ctx->output_file);
}

static uint64_t clg_timestamp_ticks_get(void)
{
  uint64_t tick;
#if defined(_MSC_VER)
  tick = GetTickCount64();
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  tick = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
  return tick;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logging API
 * \{ */

static void write_timestamp(CLogStringBuf *cstr, const uint64_t timestamp_tick_start)
{
  char timestamp_str[64];
  const uint64_t timestamp = clg_timestamp_ticks_get() - timestamp_tick_start;
  const uint timestamp_len = snprintf(timestamp_str,
                                      sizeof(timestamp_str),
                                      "%" PRIu64 ".%03u ",
                                      timestamp / 1000,
                                      (uint)(timestamp % 1000));
  clg_str_append_with_len(cstr, timestamp_str, timestamp_len);
}

static void write_severity(CLogStringBuf *cstr, enum CLG_Severity severity, bool use_color)
{
  assert((unsigned int)severity < CLG_SEVERITY_LEN);
  if (use_color) {
    enum eCLogColor color = clg_severity_to_color(severity);
    clg_str_append(cstr, clg_color_table[color]);
    clg_str_append(cstr, clg_severity_as_text(severity));
    clg_str_append(cstr, clg_color_table[COLOR_RESET]);
  }
  else {
    clg_str_append(cstr, clg_severity_as_text(severity));
  }
}

static void write_type(CLogStringBuf *cstr, CLG_LogType *lg)
{
  clg_str_append(cstr, " (");
  clg_str_append(cstr, lg->identifier);
  clg_str_append(cstr, "): ");
}

static void write_file_line_fn(CLogStringBuf *cstr,
                               const char *file_line,
                               const char *fn,
                               const bool use_basename)
{
  uint file_line_len = strlen(file_line);
  if (use_basename) {
    uint file_line_offset = file_line_len;
    while (file_line_offset-- > 0) {
      if (file_line[file_line_offset] == PATHSEP_CHAR) {
        file_line_offset++;
        break;
      }
    }
    file_line += file_line_offset;
    file_line_len -= file_line_offset;
  }
  clg_str_append_with_len(cstr, file_line, file_line_len);

  clg_str_append(cstr, " ");
  clg_str_append(cstr, fn);
  clg_str_append(cstr, ": ");
}

void CLG_log_str(CLG_LogType *lg,
                 enum CLG_Severity severity,
                 const char *file_line,
                 const char *fn,
                 const char *message)
{
  CLogStringBuf cstr;
  char cstr_stack_buf[CLOG_BUF_LEN_INIT];
  clg_str_init(&cstr, cstr_stack_buf, sizeof(cstr_stack_buf));

  if (lg->ctx->use_timestamp) {
    write_timestamp(&cstr, lg->ctx->timestamp_tick_start);
  }

  write_severity(&cstr, severity, lg->ctx->use_color);
  write_type(&cstr, lg);

  {
    write_file_line_fn(&cstr, file_line, fn, lg->ctx->use_basename);
    clg_str_append(&cstr, message);
  }
  clg_str_append(&cstr, "\n");

  /* could be optional */
  int bytes_written = write(lg->ctx->output, cstr.data, cstr.len);
  (void)bytes_written;

  clg_str_free(&cstr);

  if (lg->ctx->callbacks.backtrace_fn) {
    clg_ctx_backtrace(lg->ctx);
  }

  if (severity == CLG_SEVERITY_FATAL) {
    clg_ctx_fatal_action(lg->ctx);
  }
}

void CLG_logf(CLG_LogType *lg,
              enum CLG_Severity severity,
              const char *file_line,
              const char *fn,
              const char *fmt,
              ...)
{
  CLogStringBuf cstr;
  char cstr_stack_buf[CLOG_BUF_LEN_INIT];
  clg_str_init(&cstr, cstr_stack_buf, sizeof(cstr_stack_buf));

  if (lg->ctx->use_timestamp) {
    write_timestamp(&cstr, lg->ctx->timestamp_tick_start);
  }

  write_severity(&cstr, severity, lg->ctx->use_color);
  write_type(&cstr, lg);

  {
    write_file_line_fn(&cstr, file_line, fn, lg->ctx->use_basename);

    va_list ap;
    va_start(ap, fmt);
    clg_str_vappendf(&cstr, fmt, ap);
    va_end(ap);
  }
  clg_str_append(&cstr, "\n");

  /* could be optional */
  int bytes_written = write(lg->ctx->output, cstr.data, cstr.len);
  (void)bytes_written;

  clg_str_free(&cstr);

  if (lg->ctx->callbacks.backtrace_fn) {
    clg_ctx_backtrace(lg->ctx);
  }

  if (severity == CLG_SEVERITY_ERROR) {
    clg_ctx_error_action(lg->ctx);
  }

  if (severity == CLG_SEVERITY_FATAL) {
    clg_ctx_fatal_action(lg->ctx);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logging Context API
 * \{ */

static void CLG_ctx_output_set(CLogContext *ctx, void *file_handle)
{
  ctx->output_file = file_handle;
  ctx->output = fileno(ctx->output_file);
#if defined(__unix__) || defined(__APPLE__)
  ctx->use_color = isatty(ctx->output);
#elif defined(WIN32)
  /* As of Windows 10 build 18298 all the standard consoles supports color
   * like the Linux Terminal do, but it needs to be turned on.
   * To turn on colors we need to enable virtual terminal processing by passing the flag
   * ENABLE_VIRTUAL_TERMINAL_PROCESSING into SetConsoleMode.
   * If the system doesn't support virtual terminal processing it will fail silently and the flag
   * will not be set. */

  GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &clg_previous_console_mode);

  ctx->use_color = 0;
  if (IsWindows10OrGreater() && isatty(ctx->output)) {
    DWORD mode = clg_previous_console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode)) {
      ctx->use_color = 1;
    }
  }
#endif
}

static void CLG_ctx_output_use_basename_set(CLogContext *ctx, int value)
{
  ctx->use_basename = (bool)value;
}

static void CLG_ctx_output_use_timestamp_set(CLogContext *ctx, int value)
{
  ctx->use_timestamp = (bool)value;
  if (ctx->use_timestamp) {
    ctx->timestamp_tick_start = clg_timestamp_ticks_get();
  }
}

/** Action on error severity. */
static void CLT_ctx_error_fn_set(CLogContext *ctx, void (*error_fn)(void *file_handle))
{
  ctx->callbacks.error_fn = error_fn;
}

/** Action on fatal severity. */
static void CLG_ctx_fatal_fn_set(CLogContext *ctx, void (*fatal_fn)(void *file_handle))
{
  ctx->callbacks.fatal_fn = fatal_fn;
}

static void CLG_ctx_backtrace_fn_set(CLogContext *ctx, void (*backtrace_fn)(void *file_handle))
{
  ctx->callbacks.backtrace_fn = backtrace_fn;
}

static void clg_ctx_type_filter_append(CLG_IDFilter **flt_list,
                                       const char *type_match,
                                       int type_match_len)
{
  if (type_match_len == 0) {
    return;
  }
  CLG_IDFilter *flt = MEM_callocN(sizeof(*flt) + (type_match_len + 1), __func__);
  flt->next = *flt_list;
  *flt_list = flt;
  memcpy(flt->match, type_match, type_match_len);
  /* no need to null terminate since we calloc'd */
}

static void CLG_ctx_type_filter_exclude(CLogContext *ctx,
                                        const char *type_match,
                                        int type_match_len)
{
  clg_ctx_type_filter_append(&ctx->filters[0], type_match, type_match_len);
}

static void CLG_ctx_type_filter_include(CLogContext *ctx,
                                        const char *type_match,
                                        int type_match_len)
{
  clg_ctx_type_filter_append(&ctx->filters[1], type_match, type_match_len);
}

static void CLG_ctx_level_set(CLogContext *ctx, int level)
{
  ctx->default_type.level = level;
  for (CLG_LogType *ty = ctx->types; ty; ty = ty->next) {
    ty->level = level;
  }
}

static CLogContext *CLG_ctx_init(void)
{
  CLogContext *ctx = MEM_callocN(sizeof(*ctx), __func__);
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_init(&ctx->types_lock, NULL);
#endif
  ctx->default_type.level = 1;
  CLG_ctx_output_set(ctx, stdout);

  return ctx;
}

static void CLG_ctx_free(CLogContext *ctx)
{
#if defined(WIN32)
  SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), clg_previous_console_mode);
#endif
  while (ctx->types != NULL) {
    CLG_LogType *item = ctx->types;
    ctx->types = item->next;
    MEM_freeN(item);
  }

  for (uint i = 0; i < 2; i++) {
    while (ctx->filters[i] != NULL) {
      CLG_IDFilter *item = ctx->filters[i];
      ctx->filters[i] = item->next;
      MEM_freeN(item);
    }
  }
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_destroy(&ctx->types_lock);
#endif
  MEM_freeN(ctx);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Logging API
 *
 * Currently uses global context.
 * \{ */

/* We could support multiple at once, for now this seems not needed. */
static struct CLogContext *g_ctx = NULL;

void CLG_init(void)
{
  g_ctx = CLG_ctx_init();

  clg_color_table_init(g_ctx->use_color);
}

void CLG_exit(void)
{
  CLG_ctx_free(g_ctx);
}

void CLG_output_set(void *file_handle)
{
  CLG_ctx_output_set(g_ctx, file_handle);
}

void CLG_output_use_basename_set(int value)
{
  CLG_ctx_output_use_basename_set(g_ctx, value);
}

void CLG_output_use_timestamp_set(int value)
{
  CLG_ctx_output_use_timestamp_set(g_ctx, value);
}

void CLG_error_fn_set(void (*error_fn)(void *file_handle))
{
  CLT_ctx_error_fn_set(g_ctx, error_fn);
}

void CLG_fatal_fn_set(void (*fatal_fn)(void *file_handle))
{
  CLG_ctx_fatal_fn_set(g_ctx, fatal_fn);
}

void CLG_backtrace_fn_set(void (*fatal_fn)(void *file_handle))
{
  CLG_ctx_backtrace_fn_set(g_ctx, fatal_fn);
}

void CLG_type_filter_exclude(const char *type_match, int type_match_len)
{
  CLG_ctx_type_filter_exclude(g_ctx, type_match, type_match_len);
}

void CLG_type_filter_include(const char *type_match, int type_match_len)
{
  CLG_ctx_type_filter_include(g_ctx, type_match, type_match_len);
}

void CLG_level_set(int level)
{
  CLG_ctx_level_set(g_ctx, level);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logging Reference API
 *
 * Use to avoid look-ups each time.
 * \{ */

void CLG_logref_init(CLG_LogRef *clg_ref)
{
#ifdef WITH_CLOG_PTHREADS
  /* Only runs once when initializing a static type in most cases. */
  pthread_mutex_lock(&g_ctx->types_lock);
#endif
  if (clg_ref->type == NULL) {
    CLG_LogType *clg_ty = clg_ctx_type_find_by_name(g_ctx, clg_ref->identifier);
    if (clg_ty == NULL) {
      clg_ty = clg_ctx_type_register(g_ctx, clg_ref->identifier);
    }
#ifdef WITH_CLOG_PTHREADS
    atomic_cas_ptr((void **)&clg_ref->type, clg_ref->type, clg_ty);
#else
    clg_ref->type = clg_ty;
#endif
  }
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_unlock(&g_ctx->types_lock);
#endif
}

int CLG_color_support_get(CLG_LogRef *clg_ref)
{
  if (clg_ref->type == NULL) {
    CLG_logref_init(clg_ref);
  }
  return clg_ref->type->ctx->use_color;
}

/** \} */
