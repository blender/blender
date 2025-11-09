/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_clog
 */

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mutex>
#include <set>

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
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>

#  include <VersionHelpers.h> /* This needs to be included after Windows.h. */
#  include <io.h>
#  if !defined(ENABLE_VIRTUAL_TERMINAL_PROCESSING)
#    define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#  endif
#endif

/* For printing timestamp. */
#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <algorithm>

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

static std::mutex LOG_MUTEX;

struct CLG_IDFilter {
  struct CLG_IDFilter *next;
  /** Over alloc. */
  char match[0];
};

struct CLogContext {
  /** Single linked list of types. */
  CLG_LogType *types;
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_t types_lock;
#endif

  /* exclude, include filters. */
  CLG_IDFilter *filters[2];
  bool use_color;
  bool use_source;
  bool use_basename;
  bool use_timestamp;
  bool use_memory;

  /** Borrowed, not owned. */
  int output;
  FILE *output_file;

  /** For timer (use_timestamp). */
  uint64_t timestamp_tick_start;

  /** For new types. */
  struct {
    CLG_Level level;
  } default_type;

  struct {
    void (*error_fn)(void *file_handle);
    void (*fatal_fn)(void *file_handle);
    void (*backtrace_fn)(void *file_handle);
  } callbacks;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Global LogRef Single Linked List
 * \{ */

static CLG_LogRef **clg_all_refs_p()
{
  /* Inside a function for correct static initialization order, otherwise
   * all_refs might get null initialized only after logrefs are registered.*/
  static CLG_LogRef *all_refs = nullptr;
  return &all_refs;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mini Buffer Functionality
 *
 * Use so we can do a single call to write.
 * \{ */

#define CLOG_BUF_LEN_INIT 512

struct CLogStringBuf {
  char *data;
  uint len;
  uint len_alloc;
  bool is_alloc;
};

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
    cstr->len_alloc = std::max(len, cstr->len_alloc);

    if (cstr->is_alloc) {
      cstr->data = static_cast<char *>(MEM_reallocN(cstr->data, cstr->len_alloc));
    }
    else {
      /* Copy the static buffer. */
      char *data = MEM_malloc_arrayN<char>(cstr->len_alloc, __func__);
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

static void clg_str_append_char(CLogStringBuf *cstr, const char c, const uint len)
{
  uint len_next = cstr->len + len;
  clg_str_reserve(cstr, len_next);
  char *str_dst = cstr->data + cstr->len;
  memset(str_dst, c, len);
#if 0 /* no need. */
  str_dst[len] = '\0';
#endif
  cstr->len = len_next;
}

ATTR_PRINTF_FORMAT(2, 0)
static void clg_str_vappendf(CLogStringBuf *cstr, const char *format, va_list args)
{
  /* Use limit because windows may use '-1' for a formatting error. */
  const uint len_max = 65535;
  uint len_avail = cstr->len_alloc - cstr->len;
  while (true) {
    va_list args_cpy;
    va_copy(args_cpy, args);
    int retval = vsnprintf(cstr->data + cstr->len, len_avail, format, args_cpy);
    va_end(args_cpy);

    if (retval < 0) {
      /* Some encoding error happened, not much we can do here, besides skipping/canceling this
       * message. */
      break;
    }

    if ((uint)retval <= len_avail) {
      /* Copy was successful. */
      cstr->len += (uint)retval;
      break;
    }

    /* `vsnprintf` was not successful, due to lack of allocated space, `retval` contains expected
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

static void clg_str_indent_multiline(CLogStringBuf *cstr, const uint indent_len)
{
  /* If there are multiple lines, indent them the same as the first line for readability. */
  if (indent_len < 2) {
    return;
  }

  uint num_newlines = 0;
  for (uint i = 0; i < cstr->len; i++) {
    if (cstr->data[i] == '\n') {
      num_newlines++;
    }
  }
  if (num_newlines == 0) {
    return;
  }

  const char *old_data = cstr->data;
  const uint old_len = cstr->len;
  const bool old_is_alloc = cstr->is_alloc;

  cstr->len_alloc = cstr->len + (num_newlines * indent_len);
  cstr->len = 0;
  cstr->data = MEM_malloc_arrayN<char>(cstr->len_alloc, __func__);
  cstr->is_alloc = true;

  for (uint i = 0; i < old_len; i++) {
    cstr->data[cstr->len++] = old_data[i];
    if (old_data[i] == '\n') {
      memset(cstr->data + cstr->len, ' ', indent_len);
      cstr->data[cstr->len + indent_len - 2] = '|';
      cstr->len += indent_len;
    }
  }

  if (old_is_alloc) {
    MEM_freeN(old_data);
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
  COLOR_DIM,

  COLOR_RESET,
};
#define COLOR_LEN (COLOR_RESET + 1)

static const char *clg_color_table[COLOR_LEN] = {nullptr};
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
    clg_color_table[COLOR_DIM] = "\033[2;37m";
    clg_color_table[COLOR_RESET] = "\033[0m";
  }
}

static const char *clg_level_as_text(enum CLG_Level level)
{
  switch (level) {
    case CLG_LEVEL_FATAL:
      return "FATAL";
    case CLG_LEVEL_ERROR:
      return "ERROR";
    case CLG_LEVEL_WARN:
      return "WARNING";
    case CLG_LEVEL_INFO:
      return "INFO";
    case CLG_LEVEL_DEBUG:
      return "DEBUG";
    case CLG_LEVEL_TRACE:
      return "TRACE";
  }

  return "INVALID_LEVEL";
}

static enum eCLogColor clg_level_to_color(enum CLG_Level level)
{
  switch (level) {
    case CLG_LEVEL_FATAL:
    case CLG_LEVEL_ERROR:
      return COLOR_RED;
    case CLG_LEVEL_WARN:
      return COLOR_YELLOW;
    case CLG_LEVEL_INFO:
    case CLG_LEVEL_DEBUG:
    case CLG_LEVEL_TRACE:
      return COLOR_DEFAULT;
  }
  /* should never get here. */
  assert(false);
  return COLOR_DEFAULT;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Type Access
 * \{ */

/**
 * Filter the identifier based on very basic globbing.
 * - `foo` matches everything starting with `foo`.
 * - `*bar*` match for `foo.bar` & `baz.bar` & `foo.barbaz`
 * - `*` matches everything.
 */
static bool clg_ctx_filter_check(CLogContext *ctx, const char *identifier)
{
  if (ctx->filters[0] == nullptr && ctx->filters[1] == nullptr &&
      ctx->default_type.level >= CLG_LEVEL_INFO)
  {
    /* No filters but level specified? Match everything. */
    return true;
  }

  const size_t identifier_len = strlen(identifier);
  for (uint i = 0; i < 2; i++) {
    const CLG_IDFilter *flt = ctx->filters[i];
    while (flt != nullptr) {
      const size_t len = strlen(flt->match);
      if (STREQ(flt->match, "*") ||
          ((len <= identifier_len) && (STREQLEN(identifier, flt->match, len))))
      {
        return (bool)i;
      }
      if (flt->match[0] == '*' && flt->match[len - 1] == '*') {
        char *match = MEM_calloc_arrayN<char>(len - 1, __func__);
        memcpy(match, flt->match + 1, len - 2);
        const bool success = (strstr(identifier, match) != nullptr);
        MEM_freeN(match);
        if (success) {
          return (bool)i;
        }
      }
      else if ((len >= 2) && (STREQLEN(".*", &flt->match[len - 2], 2))) {
        /* `foo.*` and `foo.bar.*` support kept for backwards compatibility.
         * `foo` and `foo.bar` now do the same thing. */
        if (((identifier_len == len - 2) && STREQLEN(identifier, flt->match, len - 2)) ||
            ((identifier_len >= len - 1) && STREQLEN(identifier, flt->match, len - 1)))
        {
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
  return nullptr;
}

static CLG_LogType *clg_ctx_type_register(CLogContext *ctx, const char *identifier)
{
  assert(clg_ctx_type_find_by_name(ctx, identifier) == nullptr);
  CLG_LogType *ty = MEM_callocN<CLG_LogType>(__func__);
  ty->next = ctx->types;
  ctx->types = ty;
  strncpy(ty->identifier, identifier, sizeof(ty->identifier) - 1);
  ty->ctx = ctx;

  if (clg_ctx_filter_check(ctx, ty->identifier)) {
    ty->level = ctx->default_type.level;
  }
  else {
    ty->level = std::min(ctx->default_type.level, CLG_LEVEL_WARN);
  }

  return ty;
}

static void clg_ctx_error_action(CLogContext *ctx)
{
  if (ctx->callbacks.error_fn != nullptr) {
    ctx->callbacks.error_fn(ctx->output_file);
  }
}

static void clg_ctx_fatal_action(CLogContext *ctx)
{
  if (ctx->callbacks.fatal_fn != nullptr) {
    ctx->callbacks.fatal_fn(ctx->output_file);
  }
  fflush(ctx->output_file);
  abort();
}

static void clg_ctx_backtrace(CLogContext *ctx)
{
  /* NOTE: we avoid writing to 'FILE', for back-trace we make an exception,
   * if necessary we could have a version of the callback that writes to file
   * descriptor all at once. */
  ctx->callbacks.backtrace_fn(ctx->output_file);
  fflush(ctx->output_file);
}

static uint64_t clg_timestamp_ticks_get()
{
  uint64_t tick;
#if defined(_MSC_VER)
  tick = GetTickCount64();
#else
  struct timeval tv;
  gettimeofday(&tv, nullptr);
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
  char timestamp_str[128];
  const uint64_t timestamp = clg_timestamp_ticks_get() - timestamp_tick_start;
  const int h = int(timestamp / (1000 * 60 * 60));
  const int m = int((timestamp / (1000 * 60)) % 60);
  const int s = int((timestamp / 1000) % 60);
  const int r = int(timestamp % 1000);

  const uint timestamp_len =
      (h > 0) ?
          snprintf(timestamp_str, sizeof(timestamp_str), "%.2d:%.2d:%.2d.%.3d  ", h, m, s, r) :
          snprintf(timestamp_str, sizeof(timestamp_str), "%.2d:%.2d.%.3d  ", m, s, r);

  clg_str_append_with_len(cstr, timestamp_str, timestamp_len);
}

static void write_memory(CLogStringBuf *cstr)
{
  const uint64_t mem_in_use = MEM_get_memory_in_use() / (1024 * 1024);
  char memory_str[128];
  const uint len = snprintf(memory_str, sizeof(memory_str), "%dM", (int)mem_in_use);

  clg_str_append_with_len(cstr, memory_str, len);

  const uint memory_align_width = 5;
  const uint num_spaces = (len < memory_align_width) ? memory_align_width - len : 0;
  clg_str_append_char(cstr, ' ', num_spaces + 2);
}

static void write_level(CLogStringBuf *cstr, enum CLG_Level level, bool use_color)
{
  if (level >= CLG_LEVEL_INFO) {
    return;
  }

  if (use_color) {
    enum eCLogColor color = clg_level_to_color(level);
    clg_str_append(cstr, clg_color_table[color]);
    clg_str_append(cstr, clg_level_as_text(level));
    clg_str_append(cstr, clg_color_table[COLOR_RESET]);
  }
  else {
    clg_str_append(cstr, clg_level_as_text(level));
  }

  clg_str_append(cstr, " ");
}

static void write_type(CLogStringBuf *cstr, const CLG_LogType *lg)
{
  const uint len = strlen(lg->identifier);
  clg_str_append_with_len(cstr, lg->identifier, len);

  const uint type_align_width = 16;
  const uint num_spaces = (len < type_align_width) ? type_align_width - len : 0;
  clg_str_append_char(cstr, ' ', num_spaces + 1);
}

static void write_file_line_fn(CLogStringBuf *cstr,
                               const char *file_line,
                               const char *fn,
                               const bool use_basename,
                               const bool use_color)
{
  if (use_color) {
    clg_str_append(cstr, clg_color_table[COLOR_DIM]);
  }

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

  if (use_color) {
    clg_str_append(cstr, clg_color_table[COLOR_RESET]);
  }
}

void CLG_log_str(const CLG_LogType *lg,
                 enum CLG_Level level,
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
  if (lg->ctx->use_memory) {
    write_memory(&cstr);
  }
  write_type(&cstr, lg);

  clg_str_append(&cstr, "| ");

  const uint64_t multiline_indent_len = cstr.len;

  write_level(&cstr, level, lg->ctx->use_color);

  clg_str_append(&cstr, message);

  if (lg->ctx->use_source) {
    clg_str_append(&cstr, "\n");
    write_file_line_fn(&cstr, file_line, fn, lg->ctx->use_basename, lg->ctx->use_color);
  }

  clg_str_indent_multiline(&cstr, multiline_indent_len);

  clg_str_append(&cstr, "\n");

  /* Output could be optional. */
  {
    /* Mutex to avoid garbled output with threads and multi line output. */
    std::scoped_lock lock(LOG_MUTEX);
    int bytes_written = write(lg->ctx->output, cstr.data, cstr.len);
    (void)bytes_written;
  }

  clg_str_free(&cstr);

  if (lg->ctx->callbacks.backtrace_fn) {
    clg_ctx_backtrace(lg->ctx);
  }

  if (level == CLG_LEVEL_FATAL) {
    clg_ctx_fatal_action(lg->ctx);
  }
}

void CLG_logf(const CLG_LogType *lg,
              enum CLG_Level level,
              const char *file_line,
              const char *fn,
              const char *format,
              ...)
{
  CLogStringBuf cstr;
  char cstr_stack_buf[CLOG_BUF_LEN_INIT];
  clg_str_init(&cstr, cstr_stack_buf, sizeof(cstr_stack_buf));

  if (lg->ctx->use_timestamp) {
    write_timestamp(&cstr, lg->ctx->timestamp_tick_start);
  }
  if (lg->ctx->use_memory) {
    write_memory(&cstr);
  }
  write_type(&cstr, lg);

  clg_str_append(&cstr, "| ");

  const uint64_t multiline_indent_len = cstr.len;

  write_level(&cstr, level, lg->ctx->use_color);

  {
    va_list ap;
    va_start(ap, format);
    clg_str_vappendf(&cstr, format, ap);
    va_end(ap);
  }

  if (lg->ctx->use_source) {
    clg_str_append(&cstr, "\n");
    write_file_line_fn(&cstr, file_line, fn, lg->ctx->use_basename, lg->ctx->use_color);
  }

  clg_str_indent_multiline(&cstr, multiline_indent_len);

  clg_str_append(&cstr, "\n");

  /* Output could be optional. */
  {
    /* Mutex to avoid garbled output with threads and multi line output. */
    std::scoped_lock lock(LOG_MUTEX);
    int bytes_written = write(lg->ctx->output, cstr.data, cstr.len);
    (void)bytes_written;
  }

  clg_str_free(&cstr);

  if (lg->ctx->callbacks.backtrace_fn) {
    clg_ctx_backtrace(lg->ctx);
  }

  if (level == CLG_LEVEL_ERROR) {
    clg_ctx_error_action(lg->ctx);
  }

  if (level == CLG_LEVEL_FATAL) {
    clg_ctx_fatal_action(lg->ctx);
  }
}

void CLG_log_raw(const CLG_LogType *lg, const char *message)
{
  /* Write raw text without any formatting. */
  int bytes_written = write(lg->ctx->output, message, strlen(message));
  (void)bytes_written;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logging Context API
 * \{ */

static void CLG_ctx_output_set(CLogContext *ctx, void *file_handle)
{
  ctx->output_file = static_cast<FILE *>(file_handle);
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

static void CLG_ctx_output_use_source_set(CLogContext *ctx, int value)
{
  ctx->use_source = (bool)value;
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

static void CLG_ctx_output_use_memory_set(CLogContext *ctx, int value)
{
  ctx->use_memory = (bool)value;
}

/** Action on error level. */
static void CLT_ctx_error_fn_set(CLogContext *ctx, void (*error_fn)(void *file_handle))
{
  ctx->callbacks.error_fn = error_fn;
}

/** Action on fatal level. */
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
  CLG_IDFilter *flt = static_cast<CLG_IDFilter *>(
      MEM_callocN(sizeof(*flt) + type_match_len + 1, __func__));
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
  if (ctx->default_type.level <= CLG_LEVEL_WARN) {
    ctx->default_type.level = CLG_LEVEL_INFO;
  }
}

static void CLG_ctx_level_set(CLogContext *ctx, CLG_Level level)
{
  ctx->default_type.level = level;
  for (CLG_LogType *ty = ctx->types; ty; ty = ty->next) {
    ty->level = level;
  }
}

static CLogContext *CLG_ctx_init()
{
  CLogContext *ctx = MEM_callocN<CLogContext>(__func__);
#ifdef WITH_CLOG_PTHREADS
  pthread_mutex_init(&ctx->types_lock, nullptr);
#endif
  ctx->default_type.level = CLG_LEVEL_WARN;
  ctx->use_source = true;
  CLG_ctx_output_set(ctx, stdout);

  return ctx;
}

static void CLG_ctx_free(CLogContext *ctx)
{
#if defined(WIN32)
  SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), clg_previous_console_mode);
#endif
  while (ctx->types != nullptr) {
    CLG_LogType *item = ctx->types;
    ctx->types = item->next;
    MEM_freeN(item);
  }

  for (CLG_LogRef *ref = *clg_all_refs_p(); ref; ref = ref->next) {
    ref->type = nullptr;
  }

  for (uint i = 0; i < 2; i++) {
    while (ctx->filters[i] != nullptr) {
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
static struct CLogContext *g_ctx = nullptr;
/* Separate to preserve this after freeing context. */
static bool g_quiet = false;

void CLG_init()
{
  g_ctx = CLG_ctx_init();

  clg_color_table_init(g_ctx->use_color);
}

void CLG_exit()
{
  CLG_ctx_free(g_ctx);
}

void CLG_output_set(void *file_handle)
{
  CLG_ctx_output_set(g_ctx, file_handle);
}

void CLG_output_use_source_set(int value)
{
  CLG_ctx_output_use_source_set(g_ctx, value);
}

void CLG_output_use_basename_set(int value)
{
  CLG_ctx_output_use_basename_set(g_ctx, value);
}

void CLG_output_use_timestamp_set(int value)
{
  CLG_ctx_output_use_timestamp_set(g_ctx, value);
}

void CLG_output_use_memory_set(int value)
{
  CLG_ctx_output_use_memory_set(g_ctx, value);
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

void CLG_level_set(CLG_Level level)
{
  CLG_ctx_level_set(g_ctx, level);
}

void CLG_quiet_set(bool quiet)
{
  g_quiet = quiet;
}

bool CLG_quiet_get()
{
  return g_quiet;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Logging Reference API
 *
 * Use to avoid look-ups each time.
 * \{ */

void CLG_logref_register(CLG_LogRef *clg_ref)
{
  /* Add to global list of refs, both for setting the type to null on CLG_exit()
   * and so CLG_logref_list_all can be used to print all categories. */
  static std::mutex mutex;
  std::scoped_lock lock(mutex);
  CLG_LogRef **all_refs = clg_all_refs_p();
  clg_ref->next = *all_refs;
  *all_refs = clg_ref;
}

void CLG_logref_list_all(void (*callback)(const char *identifier, void *user_data),
                         void *user_data)
{
  /* Generate sorted list of unique identifiers. */
  auto cmp = [](const char *a, const char *b) { return std::strcmp(a, b) < 0; };
  std::set<const char *, decltype(cmp)> identifiers(cmp);
  for (CLG_LogRef *ref = *clg_all_refs_p(); ref; ref = ref->next) {
    identifiers.insert(ref->identifier);
  }

  for (const char *identifier : identifiers) {
    callback(identifier, user_data);
  }
}

void CLG_logref_init(CLG_LogRef *clg_ref)
{
  if (g_ctx == nullptr) {
    fprintf(stderr, "CLG logging used without initialization, aborting.\n");
    abort();
  }

#ifdef WITH_CLOG_PTHREADS
  /* Only runs once when initializing a static type in most cases. */
  pthread_mutex_lock(&g_ctx->types_lock);
#endif
  if (clg_ref->type == nullptr) {
    CLG_LogType *clg_ty = clg_ctx_type_find_by_name(g_ctx, clg_ref->identifier);
    if (clg_ty == nullptr) {
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
  if (clg_ref->type == nullptr) {
    CLG_logref_init(clg_ref);
  }
  return clg_ref->type->ctx->use_color;
}

/** \} */
