/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup creator
 */

#ifndef WITH_PYTHON_MODULE

#  include <cerrno>
#  include <cstdlib>
#  include <cstring>

#  include "MEM_guardedalloc.h"

#  include "CLG_log.h"

#  ifdef WIN32
#    include "BLI_winstuff.h"
#  endif

#  include "BLI_args.h"
#  include "BLI_dynstr.h"
#  include "BLI_fileops.h"
#  include "BLI_listbase.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string.h"
#  include "BLI_string_utf8.h"
#  include "BLI_system.h"
#  include "BLI_threads.h"
#  include "BLI_utildefines.h"
#  ifndef NDEBUG
#    include "BLI_mempool.h"
#  endif

#  include "BKE_appdir.hh"
#  include "BKE_blender.hh"
#  include "BKE_blender_cli_command.hh"
#  include "BKE_blender_version.h"
#  include "BKE_blendfile.hh"
#  include "BKE_context.hh"

#  include "BKE_global.hh"
#  include "BKE_image_format.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_scene.hh"
#  include "BKE_sound.hh"

#  include "GPU_context.hh"
#  ifdef WITH_OPENGL_BACKEND
#    include "GPU_capabilities.hh"
#    include "GPU_compilation_subprocess.hh"
#  endif

#  ifdef WITH_PYTHON
#    include "BPY_extern_python.hh"
#    include "BPY_extern_run.hh"
#  endif

#  include "RE_engine.h"
#  include "RE_pipeline.h"

#  include "WM_api.hh"

#  ifdef WITH_LIBMV
#    include "libmv-capi.h"
#  endif

#  include "DEG_depsgraph.hh"

#  include "WM_types.hh"

#  include "creator_intern.h" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Build Defines
 * \{ */

/**
 * Support extracting arguments for all platforms (for documentation purposes).
 * These names match the upper case defines.
 *
 * \note these build-defines should only be used to exclude arguments
 * from `--help` when those arguments are not handled at all.
 * Where using them would be the same as passing in an unknown argument.
 * It's possible scripts are shared between platforms,
 * so it's preferable that known arguments are documented.
 */
struct BuildDefs {
  bool win32;
  bool with_cycles;
  bool with_ffmpeg;
  bool with_freestyle;
  bool with_libmv;
  bool with_opencolorio;
  bool with_opengl_backend;
  bool with_renderdoc;
  bool with_vulkan_backend;
  bool with_xr_openxr;
};

static void build_defs_init(BuildDefs *build_defs, bool force_all)
{
  if (force_all) {
    bool *var_end = (bool *)(build_defs + 1);
    for (bool *var = (bool *)build_defs; var < var_end; var++) {
      *var = true;
    }
    return;
  }

  memset(build_defs, 0x0, sizeof(*build_defs));

#  ifdef WIN32
  build_defs->win32 = true;
#  endif
#  ifdef WITH_CYCLES
  build_defs->with_cycles = true;
#  endif
#  ifdef WITH_FFMPEG
  build_defs->with_ffmpeg = true;
#  endif
#  ifdef WITH_FREESTYLE
  build_defs->with_freestyle = true;
#  endif
#  ifdef WITH_LIBMV
  build_defs->with_libmv = true;
#  endif
#  ifdef WITH_OPENGL_BACKEND
  build_defs->with_opengl_backend = true;
#  endif
#  ifdef WITH_OPENCOLORIO
  build_defs->with_opencolorio = true;
#  endif
#  ifdef WITH_RENDERDOC
  build_defs->with_renderdoc = true;
#  endif
#  ifdef WITH_VULKAN_BACKEND
  build_defs->with_vulkan_backend = true;
#  endif
#  ifdef WITH_XR_OPENXR
  build_defs->with_xr_openxr = true;
#  endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility String Parsing
 * \{ */

static bool parse_int_relative(const char *str,
                               const char *str_end_test,
                               int pos,
                               int neg,
                               int *r_value,
                               const char **r_err_msg)
{
  char *str_end = nullptr;
  long value;

  errno = 0;

  switch (*str) {
    case '+':
      value = pos + strtol(str + 1, &str_end, 10);
      break;
    case '-':
      value = (neg - strtol(str + 1, &str_end, 10)) + 1;
      break;
    default:
      value = strtol(str, &str_end, 10);
      break;
  }

  if (*str_end != '\0' && (str_end != str_end_test)) {
    static const char *msg = "not a number";
    *r_err_msg = msg;
    return false;
  }
  if ((errno == ERANGE) || ((value < INT_MIN) || (value > INT_MAX))) {
    static const char *msg = "exceeds range";
    *r_err_msg = msg;
    return false;
  }
  *r_value = int(value);
  return true;
}

static const char *parse_int_range_sep_search(const char *str, const char *str_end_test)
{
  const char *str_end_range = nullptr;
  if (str_end_test) {
    str_end_range = static_cast<const char *>(memchr(str, '.', (str_end_test - str) - 1));
    if (str_end_range && (str_end_range[1] != '.')) {
      str_end_range = nullptr;
    }
  }
  else {
    str_end_range = strstr(str, "..");
    if (str_end_range && (str_end_range[2] == '\0')) {
      str_end_range = nullptr;
    }
  }
  return str_end_range;
}

/**
 * Parse a number as a range, eg: `1..4`.
 *
 * The \a str_end_range argument is a result of #parse_int_range_sep_search.
 */
static bool parse_int_range_relative(const char *str,
                                     const char *str_end_range,
                                     const char *str_end_test,
                                     int pos,
                                     int neg,
                                     int r_value_range[2],
                                     const char **r_err_msg)
{
  if (parse_int_relative(str, str_end_range, pos, neg, &r_value_range[0], r_err_msg) &&
      parse_int_relative(str_end_range + 2, str_end_test, pos, neg, &r_value_range[1], r_err_msg))
  {
    return true;
  }
  return false;
}

static bool parse_int_relative_clamp(const char *str,
                                     const char *str_end_test,
                                     int pos,
                                     int neg,
                                     int min,
                                     int max,
                                     int *r_value,
                                     const char **r_err_msg)
{
  if (parse_int_relative(str, str_end_test, pos, neg, r_value, r_err_msg)) {
    CLAMP(*r_value, min, max);
    return true;
  }
  return false;
}

static bool parse_int_range_relative_clamp(const char *str,
                                           const char *str_end_range,
                                           const char *str_end_test,
                                           int pos,
                                           int neg,
                                           int min,
                                           int max,
                                           int r_value_range[2],
                                           const char **r_err_msg)
{
  if (parse_int_range_relative(
          str, str_end_range, str_end_test, pos, neg, r_value_range, r_err_msg))
  {
    CLAMP(r_value_range[0], min, max);
    CLAMP(r_value_range[1], min, max);
    return true;
  }
  return false;
}

/**
 * No clamping, fails with any number outside the range.
 */
static bool parse_int_strict_range(const char *str,
                                   const char *str_end_test,
                                   const int min,
                                   const int max,
                                   int *r_value,
                                   const char **r_err_msg)
{
  char *str_end = nullptr;
  long value;

  errno = 0;
  value = strtol(str, &str_end, 10);

  if (*str_end != '\0' && (str_end != str_end_test)) {
    static const char *msg = "not a number";
    *r_err_msg = msg;
    return false;
  }
  if ((errno == ERANGE) || ((value < min) || (value > max))) {
    static const char *msg = "exceeds range";
    *r_err_msg = msg;
    return false;
  }
  *r_value = int(value);
  return true;
}

static bool parse_int(const char *str,
                      const char *str_end_test,
                      int *r_value,
                      const char **r_err_msg)
{
  return parse_int_strict_range(str, str_end_test, INT_MIN, INT_MAX, r_value, r_err_msg);
}

static bool parse_int_clamp(const char *str,
                            const char *str_end_test,
                            int min,
                            int max,
                            int *r_value,
                            const char **r_err_msg)
{
  if (parse_int(str, str_end_test, r_value, r_err_msg)) {
    CLAMP(*r_value, min, max);
    return true;
  }
  return false;
}

#  if 0
/**
 * Version of #parse_int_relative_clamp
 * that parses a comma separated list of numbers.
 */
static int *parse_int_relative_clamp_n(
    const char *str, int pos, int neg, int min, int max, int *r_value_len, const char **r_err_msg)
{
  const char sep = ',';
  int len = 1;
  for (int i = 0; str[i]; i++) {
    if (str[i] == sep) {
      len++;
    }
  }

  int *values = MEM_malloc_arrayN<int>(size_t(len), __func__);
  int i = 0;
  while (true) {
    const char *str_end = strchr(str, sep);
    if (ELEM(*str, sep, '\0')) {
      static const char *msg = "incorrect comma use";
      *r_err_msg = msg;
      goto fail;
    }
    else if (parse_int_relative_clamp(str, str_end, pos, neg, min, max, &values[i], r_err_msg)) {
      i++;
    }
    else {
      goto fail; /* Error message already set. */
    }

    if (str_end) { /* Next. */
      str = str_end + 1;
    }
    else { /* Finished. */
      break;
    }
  }

  *r_value_len = i;
  return values;

fail:
  MEM_freeN(values);
  return nullptr;
}

#  endif

/**
 * Version of #parse_int_relative_clamp & #parse_int_range_relative_clamp
 * that parses a comma separated list of numbers.
 *
 * \note single values are evaluated as a range with matching start/end.
 */
static int (*parse_int_range_relative_clamp_n(const char *str,
                                              int pos,
                                              int neg,
                                              int min,
                                              int max,
                                              int *r_value_len,
                                              const char **r_err_msg))[2]
{
  const char sep = ',';
  int len = 1;
  for (int i = 0; str[i]; i++) {
    if (str[i] == sep) {
      len++;
    }
  }

  int (*values)[2] = MEM_malloc_arrayN<int[2]>(size_t(len), __func__);
  int i = 0;
  while (true) {
    const char *str_end_range;
    const char *str_end = strchr(str, sep);
    if (ELEM(*str, sep, '\0')) {
      static const char *msg = "incorrect comma use";
      *r_err_msg = msg;
      goto fail;
    }
    else if ((str_end_range = parse_int_range_sep_search(str, str_end)) ?
                 parse_int_range_relative_clamp(
                     str, str_end_range, str_end, pos, neg, min, max, values[i], r_err_msg) :
                 parse_int_relative_clamp(
                     str, str_end, pos, neg, min, max, &values[i][0], r_err_msg))
    {
      if (str_end_range == nullptr) {
        values[i][1] = values[i][0];
      }
      i++;
    }
    else {
      goto fail; /* Error message already set. */
    }

    if (str_end) { /* Next. */
      str = str_end + 1;
    }
    else { /* Finished. */
      break;
    }
  }

  *r_value_len = i;
  return values;

fail:
  MEM_freeN(values);
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Argument Handling
 *
 * Support executing an argument running instead of #WM_main which is deferred.
 * Needed for arguments which are handled early but require sub-systems
 * (Python in particular) * to be initialized.
 * \{ */

/* When the deferred argument is handled on Windows the `argv` will have been freed,
 * see `USE_WIN32_UNICODE_ARGS` in `creator.cc`. */

#  ifdef WIN32
static char **argv_duplicate(const char **argv, int argc)
{
  char **argv_copy = MEM_malloc_arrayN<char *>(size_t(argc), __func__);
  for (int i = 0; i < argc; i++) {
    argv_copy[i] = BLI_strdup(argv[i]);
  }
  return argv_copy;
}

static void argv_free(char **argv, int argc)
{
  for (int i = 0; i < argc; i++) {
    MEM_freeN(argv[i]);
  }
  MEM_freeN(argv);
}
#  endif /* !WIN32 */

struct BA_ArgCallback_Deferred {
  BA_ArgCallback func;
  int argc;
  const char **argv;
  void *data;
  /** Return-code. */
  int exit_code;
};

static bool main_arg_deferred_is_set()
{
  return app_state.main_arg_deferred != nullptr;
}

static void main_arg_deferred_setup(BA_ArgCallback func, int argc, const char **argv, void *data)
{
  BLI_assert(app_state.main_arg_deferred == nullptr);
  BA_ArgCallback_Deferred *d = MEM_callocN<BA_ArgCallback_Deferred>(__func__);
  d->func = func;
  d->argc = argc;
  d->argv = argv;
  d->data = data;
  d->exit_code = 0;
#  ifdef WIN32
  d->argv = const_cast<const char **>(argv_duplicate(d->argv, d->argc));
#  endif
  app_state.main_arg_deferred = d;
}

void main_arg_deferred_free()
{
  BA_ArgCallback_Deferred *d = app_state.main_arg_deferred;
  app_state.main_arg_deferred = nullptr;
#  ifdef WIN32
  argv_free(const_cast<char **>(d->argv), d->argc);
#  endif
  MEM_freeN(d);
}

static void main_arg_deferred_exit_code_set(int exit_code)
{
  BA_ArgCallback_Deferred *d = app_state.main_arg_deferred;
  BLI_assert(d != nullptr);
  d->exit_code = exit_code;
}

int main_arg_deferred_handle()
{
  BA_ArgCallback_Deferred *d = app_state.main_arg_deferred;
  d->func(d->argc, d->argv, d->data);
  return d->exit_code;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities Python Context Macro (#BPY_CTX_SETUP)
 * \{ */

#  ifdef WITH_PYTHON

struct BlendePyContextStore {
  wmWindowManager *wm;
  Scene *scene;
  wmWindow *win;
  bool has_win;
};

static void arg_py_context_backup(bContext *C, BlendePyContextStore *c_py)
{
  c_py->wm = CTX_wm_manager(C);
  c_py->scene = CTX_data_scene(C);
  c_py->has_win = c_py->wm && !BLI_listbase_is_empty(&c_py->wm->windows);
  if (c_py->has_win) {
    c_py->win = CTX_wm_window(C);
    CTX_wm_window_set(C, static_cast<wmWindow *>(c_py->wm->windows.first));
  }
  else {
    /* NOTE: this should never happen, although it may be possible when loading
     * `.blend` files without windowing data. Whatever the case, it shouldn't crash,
     * although typical scripts that accesses the context is not expected to work usefully. */
    c_py->win = nullptr;
    fprintf(stderr, "Python script running with missing context data.\n");
  }
}

static void arg_py_context_restore(bContext *C, BlendePyContextStore *c_py)
{
  /* Script may load a file, check old data is valid before using. */
  if (c_py->has_win) {
    if ((c_py->win == nullptr) || ((BLI_findindex(&G_MAIN->wm, c_py->wm) != -1) &&
                                   (BLI_findindex(&c_py->wm->windows, c_py->win) != -1)))
    {
      CTX_wm_window_set(C, c_py->win);
    }
  }

  if ((c_py->scene == nullptr) || BLI_findindex(&G_MAIN->scenes, c_py->scene) != -1) {
    CTX_data_scene_set(C, c_py->scene);
  }
}

/* Macro for context setup/reset. */
#    define BPY_CTX_SETUP(_cmd) \
      { \
        BlendePyContextStore py_c; \
        arg_py_context_backup(C, &py_c); \
        { \
          _cmd; \
        } \
        arg_py_context_restore(C, &py_c); \
      } \
      ((void)0)

#  endif /* WITH_PYTHON */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Handle Argument Callbacks
 *
 * \note Doc strings here are used in differently:
 *
 * - The `--help` message.
 * - The man page (for Unix systems),
 *   see: `doc/manpage/blender.1.py`
 * - Parsed and extracted for the manual,
 *   which converts our ad-hoc formatting to reStructuredText.
 *   see: https://docs.blender.org/manual/en/dev/advanced/command_line.html
 *
 * \{ */

static void print_version_full()
{
  printf("Blender %s\n", BKE_blender_version_string());
#  ifdef BUILD_DATE
  printf("\tbuild date: %s\n", build_date);
  printf("\tbuild time: %s\n", build_time);
  printf("\tbuild commit date: %s\n", build_commit_date);
  printf("\tbuild commit time: %s\n", build_commit_time);
  printf("\tbuild hash: %s\n", build_hash);
  printf("\tbuild branch: %s\n", build_branch);
  printf("\tbuild platform: %s\n", build_platform);
  printf("\tbuild type: %s\n", build_type);
  printf("\tbuild c flags: %s\n", build_cflags);
  printf("\tbuild c++ flags: %s\n", build_cxxflags);
  printf("\tbuild link flags: %s\n", build_linkflags);
  printf("\tbuild system: %s\n", build_system);
#  endif
}

static void print_version_short()
{
#  ifdef BUILD_DATE
  /* NOTE: We include built time since sometimes we need to tell broken from
   * working built of the same hash. */
  printf("Blender %s (hash %s built %s %s)\n",
         BKE_blender_version_string(),
         build_hash,
         build_date,
         build_time);
#  else
  printf("Blender %s\n", BKE_blender_version_string());
#  endif
}

static const char arg_handle_print_version_doc[] =
    "\n\t"
    "Print Blender version and exit.";
static int arg_handle_print_version(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  print_version_full();

  /* Handles cleanup before exit. */
  BKE_blender_atexit();

  exit(EXIT_SUCCESS);
  BLI_assert_unreachable();
  return 0;
}

static void print_help(bArgs *ba, bool all)
{
  BuildDefs defs;
  build_defs_init(&defs, all);

/* All printing must go via `PRINT` macro. */
#  define printf __ERROR__

#  define PRINT(...) BLI_args_printf(ba, __VA_ARGS__)

  PRINT("Blender %s\n", BKE_blender_version_string());
  PRINT("Usage: blender [args ...] [file] [args ...]\n");
  PRINT("\n");

  PRINT("Render Options:\n");
  BLI_args_print_arg_doc(ba, "--background");
  BLI_args_print_arg_doc(ba, "--render-anim");
  BLI_args_print_arg_doc(ba, "--scene");
  BLI_args_print_arg_doc(ba, "--render-frame");
  BLI_args_print_arg_doc(ba, "--frame-start");
  BLI_args_print_arg_doc(ba, "--frame-end");
  BLI_args_print_arg_doc(ba, "--frame-jump");
  BLI_args_print_arg_doc(ba, "--render-output");
  BLI_args_print_arg_doc(ba, "--engine");
  BLI_args_print_arg_doc(ba, "--threads");

  if (defs.with_cycles) {
    PRINT("Cycles Render Options:\n");
    PRINT("\tCycles add-on options must be specified following a double dash.\n");
    PRINT("\n");
    PRINT("--cycles-device <device>\n");
    PRINT("\tSet the device used for rendering.\n");
    PRINT("\tValid options are: 'CPU' 'CUDA' 'OPTIX' 'HIP' 'ONEAPI' 'METAL'.\n");
    PRINT("\n");
    PRINT("\tAppend +CPU to a GPU device to render on both CPU and GPU.\n");
    PRINT("\n");
    PRINT("\tExample:\n");
    PRINT("\t# blender -b file.blend -f 20 -- --cycles-device OPTIX\n");
    PRINT("--cycles-print-stats\n");
    PRINT("\tLog statistics about render memory and time usage.\n");
  }

  PRINT("\n");
  PRINT("Format Options:\n");
  BLI_args_print_arg_doc(ba, "--render-format");
  BLI_args_print_arg_doc(ba, "--use-extension");

  PRINT("\n");
  PRINT("Animation Playback Options:\n");
  BLI_args_print_arg_doc(ba, "-a");

  PRINT("\n");
  PRINT("Window Options:\n");
  BLI_args_print_arg_doc(ba, "--window-border");
  BLI_args_print_arg_doc(ba, "--window-maximized");
  BLI_args_print_arg_doc(ba, "--window-fullscreen");
  BLI_args_print_arg_doc(ba, "--window-geometry");
  BLI_args_print_arg_doc(ba, "--start-console");
  BLI_args_print_arg_doc(ba, "--no-native-pixels");
  BLI_args_print_arg_doc(ba, "--no-window-frame");
  BLI_args_print_arg_doc(ba, "--no-window-focus");

  PRINT("\n");
  PRINT("Python Options:\n");
  BLI_args_print_arg_doc(ba, "--enable-autoexec");
  BLI_args_print_arg_doc(ba, "--disable-autoexec");

  PRINT("\n");

  BLI_args_print_arg_doc(ba, "--python");
  BLI_args_print_arg_doc(ba, "--python-text");
  BLI_args_print_arg_doc(ba, "--python-expr");
  BLI_args_print_arg_doc(ba, "--python-console");
  BLI_args_print_arg_doc(ba, "--python-exit-code");
  BLI_args_print_arg_doc(ba, "--python-use-system-env");
  BLI_args_print_arg_doc(ba, "--addons");

  PRINT("\n");
  PRINT("Network Options:\n");
  BLI_args_print_arg_doc(ba, "--online-mode");
  BLI_args_print_arg_doc(ba, "--offline-mode");

  PRINT("\n");
  PRINT("Logging Options:\n");
  BLI_args_print_arg_doc(ba, "--log");
  BLI_args_print_arg_doc(ba, "--log-level");
  BLI_args_print_arg_doc(ba, "--log-show-memory");
  BLI_args_print_arg_doc(ba, "--log-show-source");
  BLI_args_print_arg_doc(ba, "--log-show-backtrace");
  BLI_args_print_arg_doc(ba, "--log-file");
  BLI_args_print_arg_doc(ba, "--log-list-categories");

  PRINT("\n");
  PRINT("Debug Options:\n");
  BLI_args_print_arg_doc(ba, "--debug");
  BLI_args_print_arg_doc(ba, "--debug-value");

  PRINT("\n");
  BLI_args_print_arg_doc(ba, "--debug-events");
  BLI_args_print_arg_doc(ba, "--debug-handlers");
  if (defs.with_libmv) {
    BLI_args_print_arg_doc(ba, "--debug-libmv");
  }
  BLI_args_print_arg_doc(ba, "--debug-memory");
  BLI_args_print_arg_doc(ba, "--debug-jobs");
  BLI_args_print_arg_doc(ba, "--debug-python");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-eval");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-build");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-tag");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-no-threads");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-time");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-pretty");
  BLI_args_print_arg_doc(ba, "--debug-depsgraph-uid");
  BLI_args_print_arg_doc(ba, "--debug-ghost");
  BLI_args_print_arg_doc(ba, "--debug-wintab");
  BLI_args_print_arg_doc(ba, "--debug-gpu");
  BLI_args_print_arg_doc(ba, "--debug-gpu-force-workarounds");
  BLI_args_print_arg_doc(ba, "--debug-gpu-compile-shaders");
  BLI_args_print_arg_doc(ba, "--debug-gpu-shader-debug-info");
  BLI_args_print_arg_doc(ba, "--debug-gpu-scope-capture");
  BLI_args_print_arg_doc(ba, "--debug-gpu-shader-source");
  if (defs.with_renderdoc) {
    BLI_args_print_arg_doc(ba, "--debug-gpu-renderdoc");
  }
  if (defs.with_vulkan_backend) {
    BLI_args_print_arg_doc(ba, "--debug-gpu-vulkan-local-read");
  }
  BLI_args_print_arg_doc(ba, "--debug-wm");
  if (defs.with_xr_openxr) {
    BLI_args_print_arg_doc(ba, "--debug-xr");
    BLI_args_print_arg_doc(ba, "--debug-xr-time");
  }
  BLI_args_print_arg_doc(ba, "--debug-all");
  BLI_args_print_arg_doc(ba, "--debug-io");

  PRINT("\n");
  BLI_args_print_arg_doc(ba, "--debug-fpe");
  BLI_args_print_arg_doc(ba, "--debug-exit-on-error");
  if (defs.with_freestyle) {
    BLI_args_print_arg_doc(ba, "--debug-freestyle");
  }
  BLI_args_print_arg_doc(ba, "--disable-crash-handler");
  BLI_args_print_arg_doc(ba, "--disable-abort-handler");

  BLI_args_print_arg_doc(ba, "--verbose");
  BLI_args_print_arg_doc(ba, "--quiet");

  PRINT("\n");
  PRINT("GPU Options:\n");
  BLI_args_print_arg_doc(ba, "--gpu-backend");
  BLI_args_print_arg_doc(ba, "--gpu-vsync");
  if (defs.with_opengl_backend) {
    BLI_args_print_arg_doc(ba, "--gpu-compilation-subprocesses");
  }
  BLI_args_print_arg_doc(ba, "--profile-gpu");

  PRINT("\n");
  PRINT("Misc Options:\n");
  BLI_args_print_arg_doc(ba, "--open-last");
  BLI_args_print_arg_doc(ba, "--app-template");
  BLI_args_print_arg_doc(ba, "--factory-startup");
  BLI_args_print_arg_doc(ba, "--enable-event-simulate");
  PRINT("\n");
  BLI_args_print_arg_doc(ba, "--env-system-datafiles");
  BLI_args_print_arg_doc(ba, "--env-system-scripts");
  BLI_args_print_arg_doc(ba, "--env-system-extensions");
  BLI_args_print_arg_doc(ba, "--env-system-python");
  PRINT("\n");
  BLI_args_print_arg_doc(ba, "-noaudio");
  BLI_args_print_arg_doc(ba, "-setaudio");
  PRINT("\n");
  BLI_args_print_arg_doc(ba, "--command");

  PRINT("\n");

  BLI_args_print_arg_doc(ba, "--help");
  BLI_args_print_arg_doc(ba, "/?");

  /* File type registration (Windows & Linux only). */
  BLI_args_print_arg_doc(ba, "--register");
  BLI_args_print_arg_doc(ba, "--register-allusers");
  BLI_args_print_arg_doc(ba, "--unregister");
  BLI_args_print_arg_doc(ba, "--unregister-allusers");
  /* Windows only. */
  BLI_args_print_arg_doc(ba, "--qos");

  BLI_args_print_arg_doc(ba, "--version");

  BLI_args_print_arg_doc(ba, "--");

  // PRINT("\n");
  // PRINT("Experimental Features:\n");

  /* Other options _must_ be last (anything not handled will show here).
   *
   * Note that it's good practice for this to remain empty,
   * nevertheless print if any exist. */
  if (BLI_args_has_other_doc(ba)) {
    PRINT("\n");
    PRINT("Other Options:\n");
    BLI_args_print_other_doc(ba);
  }

  PRINT("\n");
  PRINT("Argument Parsing:\n");
  PRINT("\tArguments must be separated by white space, eg:\n");
  PRINT("\t# blender -ba test.blend\n");
  PRINT("\t...will exit since '-ba' is an unknown argument.\n");
  PRINT("\n");

  PRINT("Argument Order:\n");
  PRINT("\tArguments are executed in the order they are given. eg:\n");
  PRINT("\t# blender --background test.blend --render-frame 1 --render-output \"/tmp\"\n");
  PRINT(
      "\t...will not render to '/tmp' because '--render-frame 1' renders before the output path "
      "is set.\n");
  PRINT("\t# blender --background --render-output /tmp test.blend --render-frame 1\n");
  PRINT(
      "\t...will not render to '/tmp' because loading the blend-file overwrites the render output "
      "that was set.\n");
  PRINT("\t# blender --background test.blend --render-output /tmp --render-frame 1\n");
  PRINT("\t...works as expected.\n");
  PRINT("\n");

  PRINT("Environment Variables:\n");
  PRINT("  $BLENDER_USER_RESOURCES  Replace default directory of all user files.\n");
  PRINT("                           Other 'BLENDER_USER_*' variables override when set.\n");
  PRINT("  $BLENDER_USER_CONFIG     Directory for user configuration files.\n");
  PRINT("  $BLENDER_USER_SCRIPTS    Directory for user scripts.\n");
  PRINT("  $BLENDER_USER_EXTENSIONS Directory for user extensions.\n");
  PRINT("  $BLENDER_USER_DATAFILES  Directory for user data files (icons, translations, ..).\n");
  PRINT("\n");
  PRINT("  $BLENDER_SYSTEM_RESOURCES  Replace default directory of all bundled resource files.\n");
  PRINT("  $BLENDER_SYSTEM_SCRIPTS    Directories to add extra scripts.\n");
  PRINT("  $BLENDER_SYSTEM_EXTENSIONS Directory for system extensions repository.\n");
  PRINT("  $BLENDER_SYSTEM_DATAFILES  Directory to replace bundled datafiles.\n");
  PRINT("  $BLENDER_SYSTEM_PYTHON     Directory to replace bundled Python libraries.\n");
  PRINT("  $BLENDER_CUSTOM_SPLASH     Full path to an image that replaces the splash screen.\n");
  PRINT(
      "  $BLENDER_CUSTOM_SPLASH_BANNER Full path to an image to overlay on the splash screen.\n");

  if (defs.with_opencolorio) {
    PRINT(
        "  $BLENDER_OCIO              Path to override the OpenColorIO configuration file.\n"
        "                             If not set, the 'OCIO' environment variable is used.\n");
  }
  if (defs.win32 || all) {
    PRINT("  $TEMP                      Store temporary files here (MS-Windows).\n");
  }
  if (!defs.win32 || all) {
    PRINT("  $TMPDIR                    Store temporary files here (UNIX Systems).\n");
  }
  PRINT(
      "                             The path must reference an existing directory "
      "or it will be ignored.\n");

#  undef printf
#  undef PRINT
}

ATTR_PRINTF_FORMAT(2, 0)
static void help_print_ds_fn(void *ds_v, const char *format, va_list args)
{
  DynStr *ds = static_cast<DynStr *>(ds_v);
  BLI_dynstr_vappendf(ds, format, args);
}

static char *main_args_help_as_string(bool all)
{
  DynStr *ds = BLI_dynstr_new();
  {
    bArgs *ba = BLI_args_create(0, nullptr);
    main_args_setup(nullptr, ba, all);
    BLI_args_print_fn_set(ba, help_print_ds_fn, ds);
    print_help(ba, all);
    BLI_args_destroy(ba);
  }
  char *buf = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);
  return buf;
}

static const char arg_handle_print_help_doc[] =
    "\n\t"
    "Print this help text and exit.";
static const char arg_handle_print_help_doc_win32[] =
    "\n\t"
    "Print this help text and exit (Windows only).";
static int arg_handle_print_help(int /*argc*/, const char ** /*argv*/, void *data)
{
  bArgs *ba = (bArgs *)data;

  print_help(ba, false);

  /* Handles cleanup before exit. */
  BKE_blender_atexit();

  exit(EXIT_SUCCESS);
  BLI_assert_unreachable();

  return 0;
}

static const char arg_handle_arguments_end_doc[] =
    "\n\t"
    "End option processing, following arguments passed unchanged. Access via Python's "
    "'sys.argv'.";
static int arg_handle_arguments_end(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  return -1;
}

/* Only to give help message. */
#  ifdef WITH_PYTHON_SECURITY /* Default. */
#    define PY_ENABLE_AUTO ""
#    define PY_DISABLE_AUTO ", (default)"
#  else
#    define PY_ENABLE_AUTO ", (default, non-standard compilation option)"
#    define PY_DISABLE_AUTO ""
#  endif

static const char arg_handle_python_set_doc_enable[] =
    "\n\t"
    "Enable automatic Python script execution" PY_ENABLE_AUTO ".";
static const char arg_handle_python_set_doc_disable[] =
    "\n\t"
    "Disable automatic Python script execution "
    "(Python-drivers & startup scripts)" PY_DISABLE_AUTO ".";
#  undef PY_ENABLE_AUTO
#  undef PY_DISABLE_AUTO

static int arg_handle_python_set(int /*argc*/, const char ** /*argv*/, void *data)
{
  if (bool(data)) {
    G.f |= G_FLAG_SCRIPT_AUTOEXEC;
  }
  else {
    G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
  }
  G.f |= G_FLAG_SCRIPT_OVERRIDE_PREF;
  return 0;
}

static const char arg_handle_internet_allow_set_doc_online[] =
    "\n\t"
    "Allow internet access, overriding the preference.";
static const char arg_handle_internet_allow_set_doc_offline[] =
    "\n\t"
    "Disallow internet access, overriding the preference.";

static int arg_handle_internet_allow_set(int /*argc*/, const char ** /*argv*/, void *data)
{
  G.f &= ~G_FLAG_INTERNET_OVERRIDE_PREF_ANY;
  if (bool(data)) {
    G.f |= G_FLAG_INTERNET_ALLOW;
    G.f |= G_FLAG_INTERNET_OVERRIDE_PREF_ONLINE;
  }
  else {
    G.f &= ~G_FLAG_INTERNET_ALLOW;
    G.f |= G_FLAG_INTERNET_OVERRIDE_PREF_OFFLINE;
  }
  return 0;
}

static const char arg_handle_crash_handler_disable_doc[] =
    "\n\t"
    "Disable the crash handler.";
static int arg_handle_crash_handler_disable(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  app_state.signal.use_crash_handler = false;
  return 0;
}

static const char arg_handle_abort_handler_disable_doc[] =
    "\n\t"
    "Disable the abort handler.";
static int arg_handle_abort_handler_disable(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  app_state.signal.use_abort_handler = false;
  return 0;
}

static void clog_abort_on_error_callback(void *fp)
{
  BLI_system_backtrace(static_cast<FILE *>(fp));
  fflush(static_cast<FILE *>(fp));
  abort();
}

static const char arg_handle_debug_exit_on_error_doc[] =
    "\n\t"
    "Immediately exit when internal errors are detected.";
static int arg_handle_debug_exit_on_error(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  MEM_enable_fail_on_memleak();
  CLG_error_fn_set(clog_abort_on_error_callback);
  return 0;
}

static const char arg_handle_quiet_set_doc[] =
    "\n\t"
    "Suppress status printing (warnings & errors are still printed).";
static int arg_handle_quiet_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  CLG_quiet_set(true);
  return 0;
}

/** Shared by `--background` & `--command`. */
static void background_mode_set()
{
  G.background = true;

  /* Background Mode Defaults:
   *
   * In general background mode should strive to match the behavior of running
   * Blender inside a graphical session, any exception to this should have a well
   * justified reason and be noted in the doc-string. */

  /* NOTE(@ideasman42): While there is no requirement for sound to be disabled in background-mode,
   * the use case for playing audio in background mode is enough of a special-case
   * that users who wish to do this can explicitly enable audio in background mode.
   * While the down sides for connecting to an audio device aren't terrible they include:
   * - Listing Blender as an active application which may output audio.
   * - Unnecessary overhead running an operation in background mode or ...
   * - Having to remember to include `-noaudio` with batch operations.
   * - A quiet but audible click when Blender starts & configures its audio device.
   */
  BKE_sound_force_device("None");
}

static const char arg_handle_background_mode_set_doc[] =
    "\n"
    "\tRun in background (often used for UI-less rendering).\n"
    "\n"
    "\tThe audio device is disabled in background-mode by default\n"
    "\tand can be re-enabled by passing in '-setaudio Default' afterwards.";
static int arg_handle_background_mode_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  if (!CLG_quiet_get()) {
    print_version_short();
  }
  background_mode_set();
  return 0;
}

static const char arg_handle_command_set_doc[] =
    "<command>\n"
    "\tRun a command which consumes all remaining arguments.\n"
    "\tUse '-c help' to list all other commands.\n"
    "\tPass '--help' after the command to see its help text.\n"
    "\n"
    "\tThis implies '--background' mode.";
static int arg_handle_command_set(int argc, const char **argv, void *data)
{
  if (!main_arg_deferred_is_set()) {
    if (argc < 2) {
      fprintf(stderr, "%s requires at least one argument\n", argv[0]);
      exit(EXIT_FAILURE);
      BLI_assert_unreachable();
    }
    /* Application "info" messages get in the way of command line output, suppress them. */
    CLG_quiet_set(true);

    background_mode_set();

    main_arg_deferred_setup(arg_handle_command_set, argc, argv, data);
  }
  else {
    bContext *C = static_cast<bContext *>(data);
    const char *id = argv[1];
    int exit_code;
    if (STREQ(id, "help")) {
      BKE_blender_cli_command_print_help();
      exit_code = EXIT_SUCCESS;
    }
    else {
      exit_code = BKE_blender_cli_command_exec(C, id, argc - 2, argv + 2);
    }
    main_arg_deferred_exit_code_set(exit_code);
  }

  /* Consume remaining arguments. */
  return argc - 1;
}

static const char arg_handle_disable_depsgraph_on_file_load_doc[] =
    "\n"
    "\tBackground mode: Do not systematically build and evaluate ViewLayers' dependency graphs\n"
    "\twhen loading a blend-file in background mode ('-b' or '-c' options).\n"
    "\n"
    "\tScripts requiring evaluated data then need to explicitly ensure that\n"
    "\tan evaluated depsgraph is available\n"
    "\t(e.g. by calling 'depsgraph = context.evaluated_depsgraph_get()').\n"
    "\n"
    "\tNOTE: this is a temporary option, in the future depsgraph will never be\n"
    "\tautomatically generated on file load in background mode.";
static int arg_handle_disable_depsgraph_on_file_load(int /*argc*/,
                                                     const char ** /*argv*/,
                                                     void * /*data*/)
{
  G.fileflags |= G_BACKGROUND_NO_DEPSGRAPH;
  return 0;
}

static const char arg_handle_disable_liboverride_auto_resync_doc[] =
    "\n"
    "\tDo not perform library override automatic resync when loading a new blend-file.\n"
    "\n"
    "\tNOTE: this is an alternative way to get the same effect as when setting the\n"
    "\t'No Override Auto Resync' User Preferences Debug option.";
static int arg_handle_disable_liboverride_auto_resync(int /*argc*/,
                                                      const char ** /*argv*/,
                                                      void * /*data*/)
{
  G.fileflags |= G_LIBOVERRIDE_NO_AUTO_RESYNC;
  return 0;
}

static const char arg_handle_log_level_set_doc[] =
    "<level>\n"
    "\tSet the logging verbosity level.\n"
    "\n"
    "\tfatal: Fatal errors only\n"
    "\terror: Errors only\n"
    "\twarning: Warnings\n"
    "\tinfo: Information about devices, files, configuration, operations\n"
    "\tdebug: Verbose messages for developers\n"
    "\ttrace: Very verbose code execution tracing";
static int arg_handle_log_level_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--log-level";
  if (argc > 1) {
    const char *err_msg = nullptr;

    if (STRCASEEQ(argv[1], "fatal")) {
      G.log.level = CLG_LEVEL_FATAL;
    }
    else if (STRCASEEQ(argv[1], "error")) {
      G.log.level = CLG_LEVEL_ERROR;
    }
    else if (STRCASEEQ(argv[1], "warning")) {
      G.log.level = CLG_LEVEL_WARN;
    }
    else if (STRCASEEQ(argv[1], "info")) {
      G.log.level = CLG_LEVEL_INFO;
    }
    else if (STRCASEEQ(argv[1], "debug")) {
      G.log.level = CLG_LEVEL_DEBUG;
    }
    else if (STRCASEEQ(argv[1], "trace")) {
      G.log.level = CLG_LEVEL_TRACE;
    }
    else if (parse_int_clamp(argv[1], nullptr, -1, INT_MAX, &G.log.level, &err_msg)) {
      /* Numeric level for backwards compatibility. */
      if (G.log.level < 0) {
        G.log.level = CLG_LEVEL_LEN - 1;
      }
      else {
        G.log.level = std::min(CLG_LEVEL_INFO + G.log.level, CLG_LEVEL_LEN - 1);
      }
    }
    else {
      fprintf(stderr, "\nError: Invalid log level '%s %s'.\n", arg_id, argv[1]);
      return 1;
    }

    CLG_level_set(CLG_Level(G.log.level));
    return 1;
  }
  fprintf(stderr, "\nError: '%s' no args given.\n", arg_id);
  return 0;
}

static const char arg_handle_log_show_source_set_doc[] =
    "\n\t"
    "Show source file and function name in output.";
static int arg_handle_log_show_source_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  CLG_output_use_source_set(true);
  return 0;
}

static const char arg_handle_log_show_backtrace_set_doc[] =
    "\n\t"
    "Show a back trace for each log message (debug builds only).";
static int arg_handle_log_show_backtrace_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  /* Ensure types don't become incompatible. */
  void (*fn)(FILE *fp) = BLI_system_backtrace;
  CLG_backtrace_fn_set((void (*)(void *))fn);
  return 0;
}

static const char arg_handle_log_show_memory_set_doc[] =
    "\n\t"
    "Show memory usage for each log message.";
static int arg_handle_log_show_memory_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  CLG_output_use_memory_set(true);
  return 0;
}

static const char arg_handle_log_file_set_doc[] =
    "<filepath>\n"
    "\tSet a file to output the log to.";
static int arg_handle_log_file_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--log-file";
  if (argc > 1) {
    errno = 0;
    FILE *fp = BLI_fopen(argv[1], "w");
    if (fp == nullptr) {
      const char *err_msg = errno ? strerror(errno) : "unknown";
      fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
    }
    else {
      if (UNLIKELY(G.log.file != nullptr)) {
        fclose(static_cast<FILE *>(G.log.file));
      }
      G.log.file = fp;
      CLG_output_set(G.log.file);
    }
    return 1;
  }
  fprintf(stderr, "\nError: '%s' no args given.\n", arg_id);
  return 0;
}

static const char arg_handle_log_set_doc[] =
    "<match>\n"
    "\tEnable logging categories, taking a single comma separated argument.\n"
    "\n"
    "\t--log \"*\": log everything\n"
    "\t--log \"event\": logs every category starting with 'event'.\n"
    "\t--log \"render,cycles\": log both render and cycles messages.\n"
    "\t--log \"*mesh*\": log every category containing 'mesh' sub-string.\n"
    "\t--log \"*,^operator\": log everything except operators, with '^prefix' to exclude.";
static int arg_handle_log_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--log";
  if (argc > 1) {
    const char *str_step = argv[1];
    while (*str_step) {
      const char *str_step_end = strchr(str_step, ',');
      int str_step_len = str_step_end ? (str_step_end - str_step) : strlen(str_step);

      if (str_step[0] == '^') {
        CLG_type_filter_exclude(str_step + 1, str_step_len - 1);
      }
      else {
        CLG_type_filter_include(str_step, str_step_len);
      }

      if (str_step_end) {
        /* Typically only be one, but don't fail on multiple. */
        while (*str_step_end == ',') {
          str_step_end++;
        }
        str_step = str_step_end;
      }
      else {
        break;
      }
    }
    return 1;
  }
  fprintf(stderr, "\nError: '%s' no args given.\n", arg_id);
  return 0;
}

static const char arg_handle_list_clog_cats_doc[] =
    "\n"
    "\tList all available logging categories for '--log', and exit.\n";

static int arg_handle_list_clog_cats(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  auto print_identifier = [](const char *identifier, void *) { printf("%s\n", identifier); };
  CLG_logref_list_all(print_identifier, nullptr);
  BKE_blender_atexit();
  exit(EXIT_SUCCESS);
  return 0;
}

static const char arg_handle_debug_mode_set_doc[] =
    "\n"
    "\tTurn debugging on.\n"
    "\n"
    "\t* Enables memory error detection\n"
    "\t* Disables mouse grab (to interact with a debugger in some cases)\n"
    "\t* Keeps Python's 'sys.stdin' rather than setting it to None";
static int arg_handle_debug_mode_set(int /*argc*/, const char ** /*argv*/, void *data)
{
  G.debug |= G_DEBUG;
  printf("Blender %s\n", BKE_blender_version_string());
  MEM_set_memory_debug();
#  ifndef NDEBUG
  BLI_mempool_set_memory_debug();
#  endif

#  ifdef WITH_BUILDINFO
  printf("Build: %s %s %s %s\n", build_date, build_time, build_platform, build_type);
#  endif

  BLI_args_print(static_cast<bArgs *>(data));
  return 0;
}

static const char arg_handle_debug_mode_generic_set_doc_freestyle[] =
    "\n\t"
    "Enable debug messages for Freestyle.";
static const char arg_handle_debug_mode_generic_set_doc_python[] =
    "\n\t"
    "Enable debug messages for Python.";
static const char arg_handle_debug_mode_generic_set_doc_events[] =
    "\n\t"
    "Enable debug messages for the event system.";
static const char arg_handle_debug_mode_generic_set_doc_handlers[] =
    "\n\t"
    "Enable debug messages for event handling.";
static const char arg_handle_debug_mode_generic_set_doc_wm[] =
    "\n\t"
    "Enable debug messages for the window manager, shows all operators in search, shows "
    "keymap errors.";
static const char arg_handle_debug_mode_generic_set_doc_ghost[] =
    "\n\t"
    "Enable debug messages for Ghost (Linux only).";
static const char arg_handle_debug_mode_generic_set_doc_wintab[] =
    "\n\t"
    "Enable debug messages for Wintab.";
static const char arg_handle_debug_mode_generic_set_doc_xr[] =
    "\n\t"
    "Enable debug messages for virtual reality contexts.\n"
    "\tEnables the OpenXR API validation layer, (OpenXR) debug messages and general information "
    "prints.";
static const char arg_handle_debug_mode_generic_set_doc_xr_time[] =
    "\n\t"
    "Enable debug messages for virtual reality frame rendering times.";
static const char arg_handle_debug_mode_generic_set_doc_jobs[] =
    "\n\t"
    "Enable time profiling for background jobs.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph[] =
    "\n\t"
    "Enable all debug messages from dependency graph.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_build[] =
    "\n\t"
    "Enable debug messages from dependency graph related on graph construction.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_tag[] =
    "\n\t"
    "Enable debug messages from dependency graph related on tagging.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_time[] =
    "\n\t"
    "Enable debug messages from dependency graph related on timing.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_eval[] =
    "\n\t"
    "Enable debug messages from dependency graph related on evaluation.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_no_threads[] =
    "\n\t"
    "Switch dependency graph to a single threaded evaluation.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_pretty[] =
    "\n\t"
    "Enable colors for dependency graph debug messages.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_uid[] =
    "\n\t"
    "Verify validness of session-wide identifiers assigned to ID data-blocks.";
static const char arg_handle_debug_mode_generic_set_doc_gpu_force_workarounds[] =
    "\n\t"
    "Enable workarounds for typical GPU issues and disable all GPU extensions.";
static const char arg_handle_debug_mode_generic_set_doc_gpu_force_vulkan_local_read[] =
    "\n\t"
    "Force Vulkan dynamic rendering local read when supported by device.";

static int arg_handle_debug_mode_generic_set(int /*argc*/, const char ** /*argv*/, void *data)
{
  G.debug |= POINTER_AS_INT(data);
  return 0;
}

static const char arg_handle_debug_mode_io_doc[] =
    "\n\t"
    "Enable debug messages for I/O.";
static int arg_handle_debug_mode_io(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  G.debug |= G_DEBUG_IO;
  return 0;
}

static const char arg_handle_debug_mode_all_doc[] =
    "\n\t"
    "Enable all debug messages.";
static int arg_handle_debug_mode_all(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  G.debug |= G_DEBUG_ALL;
#  ifdef WITH_LIBMV
  libmv_startDebugLogging();
#  endif
  return 0;
}

static const char arg_handle_debug_mode_libmv_doc[] =
    "\n\t"
    "Enable debug messages from libmv library.";
static int arg_handle_debug_mode_libmv(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
#  ifdef WITH_LIBMV
  libmv_startDebugLogging();
#  endif
  return 0;
}

static const char arg_handle_debug_mode_cycles_doc[] =
    "\n\t"
    "Enable debug messages from Cycles.";
static int arg_handle_debug_mode_cycles(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  const char *cycles_filter = "cycles.*";
  CLG_type_filter_include(cycles_filter, strlen(cycles_filter));
  return 0;
}

static const char arg_handle_debug_mode_ffmpeg_doc[] =
    "\n\t"
    "Enable debug messages from FFmpeg video input and output.";
static int arg_handle_debug_mode_ffmpeg(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  const char *video_filter = "video.*";
  CLG_type_filter_include(video_filter, strlen(video_filter));
  return 0;
}

static const char arg_handle_debug_mode_memory_set_doc[] =
    "\n\t"
    "Enable fully guarded memory allocation and debugging.";
static int arg_handle_debug_mode_memory_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  MEM_set_memory_debug();
  return 0;
}

static const char arg_handle_debug_value_set_doc[] =
    "<value>\n"
    "\tSet debug value of <value> on startup.";
static int arg_handle_debug_value_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--debug-value";
  if (argc > 1) {
    const char *err_msg = nullptr;
    int value;
    if (!parse_int(argv[1], nullptr, &value, &err_msg)) {
      fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
      return 1;
    }

    G.debug_value = value;

    return 1;
  }
  fprintf(stderr, "\nError: you must specify debug value to set.\n");
  return 0;
}

static const char arg_handle_debug_gpu_set_doc[] =
    "\n"
    "\tEnable GPU debug context and information for OpenGL 4.3+.";
static int arg_handle_debug_gpu_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  /* Also enable logging because that how gl errors are reported. */
  const char *gpu_filter = "gpu.*";
  CLG_type_filter_include(gpu_filter, strlen(gpu_filter));
  G.debug |= G_DEBUG_GPU;
  return 0;
}

static const char arg_handle_debug_gpu_compile_shaders_set_doc[] =
    "\n"
    "\tCompile all statically defined shaders to test platform compatibility.";
static int arg_handle_debug_gpu_compile_shaders_set(int /*argc*/,
                                                    const char ** /*argv*/,
                                                    void * /*data*/)
{
  G.debug |= G_DEBUG_GPU_COMPILE_SHADERS;
  return 0;
}

static const char arg_handle_debug_gpu_scope_capture_set_doc[] =
    "\n"
    "\tCapture the GPU commands issued inside the give scope name.";
static int arg_handle_debug_gpu_scope_capture_set(int argc, const char **argv, void * /*data*/)
{
  if (argc > 1) {
    STRNCPY(G.gpu_debug_scope_name, argv[1]);
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a scope name to capture.\n");
  return 0;
}

static const char arg_handle_debug_gpu_shader_source_doc[] =
    "\n"
    "\tCapture the GPU commands issued inside the give scope name.\n"
    "\tFiles are saved in the current working directory inside a directory named \"Shaders\".";
static int arg_handle_debug_gpu_shader_source(int argc, const char **argv, void * /*data*/)
{
  if (argc > 1) {
    STRNCPY(G.gpu_debug_shader_source_name, argv[1]);
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a shader name to capture.\n");
  return 0;
}

static const char arg_handle_debug_gpu_renderdoc_set_doc[] =
    "\n"
    "\tEnable RenderDoc integration for GPU frame grabbing and debugging.";
static int arg_handle_debug_gpu_renderdoc_set(int /*argc*/,
                                              const char ** /*argv*/,
                                              void * /*data*/)
{
#  ifdef WITH_RENDERDOC
  G.debug |= G_DEBUG_GPU_RENDERDOC | G_DEBUG_GPU | G_DEBUG_GPU_SHADER_DEBUG_INFO;
#  else
  BLI_assert_unreachable();
#  endif
  return 0;
}

static const char arg_handle_debug_gpu_shader_debug_info_set_doc[] =
    "\n"
    "\tEnable shader debug info generation (Vulkan only).";
static int arg_handle_debug_gpu_shader_debug_info_set(int /*argc*/,
                                                      const char ** /*argv*/,
                                                      void * /*data*/)
{
  G.debug |= G_DEBUG_GPU_SHADER_DEBUG_INFO;
  return 0;
}

static const char arg_handle_gpu_backend_set_doc_all[] =
    "\n"
    "\tForce to use a specific GPU backend. Valid options: "
    "'vulkan',  "
    "'metal',  "
    "'opengl'.";
static const char arg_handle_gpu_backend_set_doc[] =
    "\n"
    "\tForce to use a specific GPU backend. Valid options: "
#  ifdef WITH_OPENGL_BACKEND
    "'opengl'"
#  endif
#  ifdef WITH_METAL_BACKEND
    "'metal'"
#  endif
#  ifdef WITH_VULKAN_BACKEND
#    if defined(WITH_OPENGL_BACKEND) || defined(WITH_METAL_BACKEND)
    " or "
#    endif
    "'vulkan'"
#  endif
    ".";
static int arg_handle_gpu_backend_set(int argc, const char **argv, void * /*data*/)
{
  if (argc < 2) {
    fprintf(stderr, "\nError: GPU backend must follow '--gpu-backend'.\n");
    return 0;
  }
  const char *backends_supported[3] = {nullptr};
  int backends_supported_num = 0;

  GPUBackendType gpu_backend = GPU_BACKEND_NONE;

  /* NOLINTBEGIN: bugprone-assignment-in-if-condition */
  if (false) {
    /* Use a dummy block to make the following `ifdef` blocks work. */
  }
#  ifdef WITH_OPENGL_BACKEND
  else if (STREQ(argv[1], (backends_supported[backends_supported_num++] = "opengl"))) {
    gpu_backend = GPU_BACKEND_OPENGL;
  }
#  endif
#  ifdef WITH_VULKAN_BACKEND
  else if (STREQ(argv[1], (backends_supported[backends_supported_num++] = "vulkan"))) {
    gpu_backend = GPU_BACKEND_VULKAN;
  }
#  endif
#  ifdef WITH_METAL_BACKEND
  else if (STREQ(argv[1], (backends_supported[backends_supported_num++] = "metal"))) {
    gpu_backend = GPU_BACKEND_METAL;
  }
#  endif
  else {
    fprintf(stderr, "\nError: Unrecognized GPU backend for '--gpu-backend', expected one of [");
    for (int i = 0; i < backends_supported_num; i++) {
      fprintf(stderr, (i + 1 != backends_supported_num) ? "%s, " : "%s", backends_supported[i]);
    }
    fprintf(stderr, "].\n");
    return 1;
  }
  /* NOLINTEND: bugprone-assignment-in-if-condition */

  GPU_backend_type_selection_set_override(gpu_backend);

  return 1;
}

static const char arg_handle_gpu_vsync_set_doc[] =
    "\n"
    "\tSet the VSync.\n"
    "\tValid options are: 'on', 'off' & 'auto' for adaptive sync.\n"
    "\n"
    "\t* The default settings depend on the GPU driver.\n"
    "\t* Disabling VSync can be useful for testing performance.\n"
    "\t* 'auto' is only supported by the OpenGL backend.";
static int arg_handle_gpu_vsync_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--gpu-vsync";

  if (argc < 2) {
    fprintf(stderr, "\nError: VSync value must follow '%s'.\n", arg_id);
    return 0;
  }

  /* Must be compatible with #GHOST_TVSyncModes. */
  int vsync;
  if (STREQ(argv[1], "on")) {
    vsync = 1;
  }
  else if (STREQ(argv[1], "off")) {
    vsync = 0;
  }
  else if (STREQ(argv[1], "auto")) {
    vsync = -1;
  }
  else {
    fprintf(stderr, "\nError: expected a value in [on, off, auto] '%s %s'.\n", arg_id, argv[1]);
    return 1;
  }

  GPU_backend_vsync_set_override(vsync);

  return 1;
}

static const char arg_handle_gpu_compilation_subprocesses_set_doc[] =
    "\n"
    "\tOverride the Max Compilation Subprocesses setting (OpenGL only).";
static int arg_handle_gpu_compilation_subprocesses_set(int argc,
                                                       const char **argv,
                                                       void * /*data*/)
{
  const char *arg_id = "--gpu-compilation-subprocesses";
  const int min = 0, max = BLI_system_thread_count();
  if (argc > 1) {
    const char *err_msg = nullptr;
    int subprocesses;
    if (!parse_int_strict_range(argv[1], nullptr, min, max, &subprocesses, &err_msg)) {
      fprintf(stderr,
              "\nError: %s '%s %s', expected number in [%d..%d].\n",
              err_msg,
              arg_id,
              argv[1],
              min,
              max);
      return 1;
    }

#  ifdef WITH_OPENGL_BACKEND
    GPU_compilation_subprocess_override_set(subprocesses);
#  else
    UNUSED_VARS(subprocesses);
    BLI_assert_unreachable();
#  endif
    return 1;
  }
  fprintf(stderr,
          "\nError: you must specify a number of subprocesses in [%d..%d] '%s'.\n",
          min,
          max,
          arg_id);
  return 0;
}

static const char arg_handle_debug_fpe_set_doc[] =
    "\n\t"
    "Enable floating-point exceptions.";
static int arg_handle_debug_fpe_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  main_signal_setup_fpe();
  return 0;
}

static const char arg_handle_app_template_doc[] =
    "<template>\n"
    "\tSet the application template (matching the directory name), use 'default' for none.";
static int arg_handle_app_template(int argc, const char **argv, void * /*data*/)
{
  if (argc > 1) {
    const char *app_template = STREQ(argv[1], "default") ? "" : argv[1];
    WM_init_state_app_template_set(app_template);
    return 1;
  }
  fprintf(stderr, "\nError: App template must follow '--app-template'.\n");
  return 0;
}

static const char arg_handle_factory_startup_set_doc[] =
    "\n\t"
    "Skip reading the '" BLENDER_STARTUP_FILE "' in the users home directory.";
static int arg_handle_factory_startup_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  G.factory_startup = true;
  G.f |= G_FLAG_USERPREF_NO_SAVE_ON_EXIT;
  return 0;
}

static const char arg_handle_enable_event_simulate_doc[] =
    "\n\t"
    "Enable event simulation testing feature 'bpy.types.Window.event_simulate'.";
static int arg_handle_enable_event_simulate(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  G.f |= G_FLAG_EVENT_SIMULATE;
  return 0;
}

static const char arg_handle_env_system_set_doc_datafiles[] =
    "\n\t"
    "Set the " STRINGIFY_ARG(BLENDER_SYSTEM_DATAFILES) " environment variable.";
static const char arg_handle_env_system_set_doc_scripts[] =
    "\n\t"
    "Set the " STRINGIFY_ARG(BLENDER_SYSTEM_SCRIPTS) " environment variable.";
static const char arg_handle_env_system_set_doc_python[] =
    "\n\t"
    "Set the " STRINGIFY_ARG(BLENDER_SYSTEM_PYTHON) " environment variable.";
static const char arg_handle_env_system_set_doc_extensions[] =
    "\n\t"
    "Set the " STRINGIFY_ARG(BLENDER_SYSTEM_EXTENSIONS) " environment variable.";

static int arg_handle_env_system_set(int argc, const char **argv, void * /*data*/)
{
  /* `--env-system-scripts` -> `BLENDER_SYSTEM_SCRIPTS` */

  char env[64] = "BLENDER";
  char *ch_dst = env + 7;           /* Skip `BLENDER`. */
  const char *ch_src = argv[0] + 5; /* Skip `--env`. */

  if (argc < 2) {
    fprintf(stderr, "%s requires one argument\n", argv[0]);
    exit(EXIT_FAILURE);
    BLI_assert_unreachable();
  }

  for (; *ch_src; ch_src++, ch_dst++) {
    *ch_dst = (*ch_src == '-') ? '_' : (*ch_src) - 32; /* Inline #toupper(). */
  }

  *ch_dst = '\0';
  BLI_setenv(env, argv[1]);
  return 1;
}

static const char arg_handle_playback_mode_doc[] =
    "<options> <file(s)>\n"
    "\tInstead of showing Blender's user interface, this runs Blender as an animation player,\n"
    "\tto view movies and image sequences rendered in Blender (ignored if '-b' is set).\n"
    "\n"
    "\tPlayback Arguments:\n"
    "\n"
    "\t-p <sx> <sy>\n"
    "\t\tOpen with lower left corner at <sx>, <sy>.\n"
    "\t-m\n"
    "\t\tRead from disk (Do not buffer).\n"
    "\t-f <fps> <fps_base>\n"
    "\t\tSpecify FPS to start with.\n"
    "\t-j <frame>\n"
    "\t\tSet frame step to <frame>.\n"
    "\t-s <frame>\n"
    "\t\tPlay from <frame>.\n"
    "\t-e <frame>\n"
    "\t\tPlay until <frame>.\n"
    "\t-c <cache_memory>\n"
    "\t\tAmount of memory in megabytes to allow for caching images during playback.\n"
    "\t\tZero disables (clamping to a fixed number of frames instead).";
static int arg_handle_playback_mode(int argc, const char **argv, void * /*data*/)
{
  /* Ignore the animation player if `-b` was given first. */
  if (G.background == 0) {
    /* Skip this argument (`-a`). */
    const int exit_code = WM_main_playanim(argc - 1, argv + 1);

    exit(exit_code);
  }

  return -2;
}

static const char arg_handle_window_geometry_doc[] =
    "<sx> <sy> <w> <h>\n"
    "\tOpen with lower left corner at <sx>, <sy> and width and height as <w>, <h>.";
static int arg_handle_window_geometry(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "-p / --window-geometry";
  int params[4], i;

  if (argc < 5) {
    fprintf(stderr, "Error: requires four arguments '%s'\n", arg_id);
    exit(1);
  }

  for (i = 0; i < 4; i++) {
    const char *err_msg = nullptr;
    if (!parse_int(argv[i + 1], nullptr, &params[i], &err_msg)) {
      fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
      exit(1);
    }
  }

  WM_init_state_size_set(UNPACK4(params));

  return 4;
}

static const char arg_handle_native_pixels_set_doc[] =
    "\n\t"
    "Do not use native pixel size, for high resolution displays (MacBook 'Retina').";
static int arg_handle_native_pixels_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_native_pixels(false);
  return 0;
}

static const char arg_handle_window_border_doc[] =
    "\n\t"
    "Force opening with borders, in a normal (non maximized) state.";
static int arg_handle_window_border(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_state_normal_set();
  return 0;
}

static const char arg_handle_window_fullscreen_doc[] =
    "\n\t"
    "Force opening full-screen.";
static int arg_handle_window_fullscreen(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_state_fullscreen_set();
  return 0;
}

static const char arg_handle_window_maximized_doc[] =
    "\n\t"
    "Force opening maximized.";
static int arg_handle_window_maximized(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_state_maximized_set();
  return 0;
}

static const char arg_handle_no_window_frame_doc[] =
    "\n\t"
    "Disable all window decorations (Linux only).";
static int arg_handle_no_window_frame(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_window_frame_set(false);
  return 0;
}

static const char arg_handle_no_window_focus_doc[] =
    "\n\t"
    "Open behind other windows and without taking focus.";
static int arg_handle_no_window_focus(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_window_focus_set(false);
  return 0;
}

static const char arg_handle_start_with_console_doc[] =
    "\n\t"
    "Start with the console window open (ignored if '-b' is set), (Windows only).";
static int arg_handle_start_with_console(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  WM_init_state_start_with_console_set(true);
  return 0;
}

static bool arg_handle_extension_registration(const bool do_register, const bool all_users)
{
  /* Logic runs in #main_args_handle_registration. */
#  ifdef WIN32
  /* This process has been launched with the permissions needed
   * to register or unregister, so just do it now and then exit. */
  if (do_register) {
    BLI_windows_register_blend_extension(all_users);
  }
  else {
    BLI_windows_unregister_blend_extension(all_users);
  }
  TerminateProcess(GetCurrentProcess(), 0);
  return true;
#  else
  char *error_msg = nullptr;
  bool result = WM_platform_associate_set(do_register, all_users, &error_msg);
  if (error_msg) {
    fprintf(stderr, "Error: %s\n", error_msg);
    MEM_freeN(error_msg);
  }
  return result;
#  endif
}

static const char arg_handle_register_extension_doc[] =
    "\n\t"
    "Register blend-file extension for current user, then exit (Windows & Linux only).";
static int arg_handle_register_extension(int argc, const char **argv, void *data)
{
  CLG_quiet_set(true);
  background_mode_set();

#  if !(defined(WIN32) || defined(__APPLE__))
  if (!main_arg_deferred_is_set()) {
    main_arg_deferred_setup(arg_handle_register_extension, argc, argv, data);
    return argc - 1;
  }
#  else
  UNUSED_VARS(argv, data);
#  endif
  arg_handle_extension_registration(true, false);
  return argc - 1;
}

static const char arg_handle_register_extension_all_doc[] =
    "\n\t"
    "Register blend-file extension for all users, then exit (Windows & Linux only).";
static int arg_handle_register_extension_all(int argc, const char **argv, void *data)
{
  CLG_quiet_set(true);
  background_mode_set();

#  if !(defined(WIN32) || defined(__APPLE__))
  if (!main_arg_deferred_is_set()) {
    main_arg_deferred_setup(arg_handle_register_extension_all, argc, argv, data);
    return argc - 1;
  }
#  else
  UNUSED_VARS(argv, data);
#  endif
  arg_handle_extension_registration(true, true);
  return argc - 1;
}

static const char arg_handle_unregister_extension_doc[] =
    "\n\t"
    "Unregister blend-file extension for current user, then exit (Windows & Linux only).";
static int arg_handle_unregister_extension(int argc, const char **argv, void *data)
{
  CLG_quiet_set(true);
  background_mode_set();

#  if !(defined(WIN32) || defined(__APPLE__))
  if (!main_arg_deferred_is_set()) {
    main_arg_deferred_setup(arg_handle_unregister_extension, argc, argv, data);
    return argc - 1;
  }
#  else
  UNUSED_VARS(argc, argv, data);
#  endif
  arg_handle_extension_registration(false, false);
  return 0;
}

static const char arg_handle_unregister_extension_all_doc[] =
    "\n\t"
    "Unregister blend-file extension for all users, then exit (Windows & Linux only).";
static int arg_handle_unregister_extension_all(int argc, const char **argv, void *data)
{
  CLG_quiet_set(true);
  background_mode_set();

#  if !(defined(WIN32) || defined(__APPLE__))
  if (!main_arg_deferred_is_set()) {
    main_arg_deferred_setup(arg_handle_unregister_extension_all, argc, argv, data);
    return argc - 1;
  }
#  else
  UNUSED_VARS(argc, argv, data);
#  endif
  arg_handle_extension_registration(false, true);
  return 0;
}

static const char arg_handle_qos_set_doc[] =
    "<level>\n"
    "\tSet the Quality of Service (QoS) mode for hybrid CPU architectures (Windows only).\n"
    "\n"
    "\tdefault: Uses the default behavior of the OS.\n"
    "\thigh: Always makes use of performance cores.\n"
    "\teco: Schedules Blender threads exclusively to efficiency cores.";
static int arg_handle_qos_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--qos";
  if (argc > 1) {
#  ifdef _WIN32
    QoSMode qos_mode;
    if (STRCASEEQ(argv[1], "default")) {
      qos_mode = QoSMode::DEFAULT;
    }
    else if (STRCASEEQ(argv[1], "high")) {
      qos_mode = QoSMode::HIGH;
    }
    else if (STRCASEEQ(argv[1], "eco")) {
      qos_mode = QoSMode::ECO;
    }
    else {
      fprintf(stderr, "\nError: Invalid QoS level '%s %s'.\n", arg_id, argv[1]);
      return 1;
    }
    BLI_windows_process_set_qos(qos_mode, QoSPrecedence::CMDLINE_ARG);
#  else
    UNUSED_VARS(argv);
    fprintf(stderr, "\nError: '%s' is Windows only.\n", arg_id);
#  endif
    return 1;
  }
  fprintf(stderr, "\nError: '%s' no args given.\n", arg_id);
  return 0;
}

static const char arg_handle_audio_disable_doc[] =
    "\n\t"
    "Force sound system to None.";
static int arg_handle_audio_disable(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  BKE_sound_force_device("None");
  return 0;
}

static const char arg_handle_audio_set_doc[] =
    "\n\t"
    "Force sound system to a specific device.\n"
    "\t'None' 'Default' 'SDL' 'OpenAL' 'CoreAudio' 'JACK' 'PulseAudio' 'WASAPI'.";
static int arg_handle_audio_set(int argc, const char **argv, void * /*data*/)
{
  if (argc < 1) {
    fprintf(stderr, "-setaudio requires one argument\n");
    exit(1);
  }

  const char *device = argv[1];
  if (STREQ(device, "Default")) {
    /* Unset any forced device. */
    device = nullptr;
  }

  BKE_sound_force_device(device);
  return 1;
}

static const char arg_handle_output_set_doc[] =
    "<path>\n"
    "\tSet the render path and file name.\n"
    "\tUse '//' at the start of the path to render relative to the blend-file.\n"
    "\n"
    "\tYou can use path templating features such as '{blend_name}' in the path.\n"
    "\tSee Blender's documentation on path templates for more details.\n"
    "\n"
    "\tThe '#' characters are replaced by the frame number, and used to define zero padding.\n"
    "\n"
    "\t* 'animation_##_test.png' becomes 'animation_01_test.png'\n"
    "\t* 'test-######.png' becomes 'test-000001.png'\n"
    "\n"
    "\tWhen the filename does not contain '#', the suffix '####' is added to the filename.\n"
    "\n"
    "\tThe frame number will be added at the end of the filename, eg:\n"
    "\t# blender -b animation.blend -o //render_ -F PNG -x 1 -a\n"
    "\t'//render_' becomes '//render_####', writing frames as '//render_0001.png'";
static int arg_handle_output_set(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  if (argc > 1) {
    Scene *scene = CTX_data_scene(C);
    if (scene) {
      STRNCPY(scene->r.pic, argv[1]);
      DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
    }
    else {
      fprintf(stderr, "\nError: no blend loaded. cannot use '-o / --render-output'.\n");
    }
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a path after '-o  / --render-output'.\n");
  return 0;
}

static const char arg_handle_engine_set_doc[] =
    "<engine>\n"
    "\tSpecify the render engine.\n"
    "\tUse '-E help' to list available engines.";
static int arg_handle_engine_set(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  if (argc >= 2) {
    const char *engine_name = argv[1];

    if (STREQ(engine_name, "help")) {
      printf("Blender Engine Listing:\n");
      LISTBASE_FOREACH (RenderEngineType *, type, &R_engines) {
        printf("\t%s\n", type->idname);
      }
      WM_exit_ex(C, false, false);
      exit(0);
    }
    else {
      Scene *scene = CTX_data_scene(C);
      if (scene) {
        /* Backwards compatibility. */
        if (STREQ(engine_name, "BLENDER_EEVEE_NEXT")) {
          engine_name = "BLENDER_EEVEE";
        }

        if (BLI_findstring(&R_engines, engine_name, offsetof(RenderEngineType, idname))) {
          STRNCPY_UTF8(scene->r.engine, engine_name);
          DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
        }
        else {
          fprintf(stderr, "\nError: engine not found '%s'\n", engine_name);
          WM_exit_ex(C, false, false);
          exit(1);
        }
      }
      else {
        fprintf(stderr,
                "\nError: no blend loaded. "
                "order the arguments so '-E / --engine' is after a blend is loaded.\n");
      }
    }

    return 1;
  }
  fprintf(stderr, "\nEngine not specified, give 'help' for a list of available engines.\n");
  return 0;
}

static const char arg_handle_image_type_set_doc[] =
    "<format>\n"
    "\tSet the render format.\n"
    "\tValid options are:\n"
    "\t'TGA' 'RAWTGA' 'JPEG' 'IRIS' 'PNG' 'BMP' 'HDR' 'TIFF'.\n"
    "\n"
    "\tFormats that can be compiled into Blender, not available on all systems:\n"
    "\t'OPEN_EXR' 'OPEN_EXR_MULTILAYER' 'FFMPEG' 'CINEON' 'DPX' 'JP2' 'WEBP'.";
static int arg_handle_image_type_set(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  if (argc > 1) {
    const char *imtype = argv[1];
    Scene *scene = CTX_data_scene(C);
    if (scene) {
      const char imtype_new = BKE_imtype_from_arg(imtype);

      if (imtype_new == R_IMF_IMTYPE_INVALID) {
        fprintf(stderr,
                "\nError: Format from '-F / --render-format' not known or not compiled in this "
                "release.\n");
      }
      else {
        BKE_image_format_set(&scene->r.im_format, &scene->id, imtype_new);
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
    }
    else {
      fprintf(stderr,
              "\nError: no blend loaded. "
              "order the arguments so '-F  / --render-format' is after the blend is loaded.\n");
    }
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a format after '-F  / --render-format'.\n");
  return 0;
}

static const char arg_handle_threads_set_doc[] =
    "<threads>\n"
    "\tUse amount of <threads> for rendering and other operations\n"
    "\t[1-" STRINGIFY(BLENDER_MAX_THREADS) "], 0 to use the systems processor count.";
static int arg_handle_threads_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "-t / --threads";
  const int min = 0, max = BLENDER_MAX_THREADS;
  if (argc > 1) {
    const char *err_msg = nullptr;
    int threads;
    if (!parse_int_strict_range(argv[1], nullptr, min, max, &threads, &err_msg)) {
      fprintf(stderr,
              "\nError: %s '%s %s', expected number in [%d..%d].\n",
              err_msg,
              arg_id,
              argv[1],
              min,
              max);
      return 1;
    }

    BLI_system_num_threads_override_set(threads);
    return 1;
  }
  fprintf(stderr,
          "\nError: you must specify a number of threads in [%d..%d] '%s'.\n",
          min,
          max,
          arg_id);
  return 0;
}

static const char arg_handle_verbosity_set_doc[] =
    "<verbose>\n"
    "\tSet the logging verbosity level for debug messages that support it.";
static int arg_handle_verbosity_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--verbose";
  if (argc > 1) {
    const char *err_msg = nullptr;
    int level;
    if (!parse_int(argv[1], nullptr, &level, &err_msg)) {
      fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
    }

#  ifdef WITH_LIBMV
    libmv_setLoggingVerbosity(level);
#  endif

    return 1;
  }
  fprintf(stderr, "\nError: you must specify a verbosity level.\n");
  return 0;
}

static const char arg_handle_extension_set_doc[] =
    "<bool>\n"
    "\tSet option to add the file extension to the end of the file.";
static int arg_handle_extension_set(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  if (argc > 1) {
    Scene *scene = CTX_data_scene(C);
    if (scene) {
      if (argv[1][0] == '0') {
        scene->r.scemode &= ~R_EXTENSION;
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
      else if (argv[1][0] == '1') {
        scene->r.scemode |= R_EXTENSION;
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
      else {
        fprintf(stderr,
                "\nError: Use '-x 1 / -x 0' To set the extension option or '--use-extension'\n");
      }
    }
    else {
      fprintf(stderr,
              "\nError: no blend loaded. "
              "order the arguments so '-o ' is after '-x '.\n");
    }
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a path after '- '.\n");
  return 0;
}

static void add_log_render_filter()
{
  const char *render_filter = "render.*";
  CLG_type_filter_include(render_filter, strlen(render_filter));
}

static const char arg_handle_render_frame_doc[] =
    "<frame>\n"
    "\tRender frame <frame> and save it.\n"
    "\n"
    "\t* +<frame> start frame relative, -<frame> end frame relative.\n"
    "\t* A comma separated list of frames can also be used (no spaces).\n"
    "\t* A range of frames can be expressed using '..' separator between the first and last "
    "frames (inclusive).\n";
static int arg_handle_render_frame(int argc, const char **argv, void *data)
{
  const char *arg_id = "-f / --render-frame";
  bContext *C = static_cast<bContext *>(data);
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    add_log_render_filter();

    Main *bmain = CTX_data_main(C);

    if (argc > 1) {
      const char *err_msg = nullptr;
      Render *re;
      ReportList reports;

      int (*frame_range_arr)[2], frames_range_len;
      if ((frame_range_arr = parse_int_range_relative_clamp_n(argv[1],
                                                              scene->r.sfra,
                                                              scene->r.efra,
                                                              MINAFRAME,
                                                              MAXFRAME,
                                                              &frames_range_len,
                                                              &err_msg)) == nullptr)
      {
        fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
        return 1;
      }

      re = RE_NewSceneRender(scene);
      BKE_reports_init(&reports, RPT_STORE);
      RE_SetReports(re, &reports);
      for (int i = 0; i < frames_range_len; i++) {
        /* We could pass in frame ranges,
         * but prefer having exact behavior as passing in multiple frames. */
        if ((frame_range_arr[i][0] <= frame_range_arr[i][1]) == 0) {
          fprintf(stderr, "\nWarning: negative range ignored '%s %s'.\n", arg_id, argv[1]);
        }

        for (int frame = frame_range_arr[i][0]; frame <= frame_range_arr[i][1]; frame++) {
          RE_RenderAnim(re, bmain, scene, nullptr, nullptr, frame, frame, scene->r.frame_step);
        }
      }
      RE_SetReports(re, nullptr);
      BKE_reports_free(&reports);
      MEM_freeN(frame_range_arr);
      return 1;
    }
    fprintf(stderr, "\nError: frame number must follow '%s'.\n", arg_id);
    return 0;
  }
  fprintf(stderr, "\nError: no blend loaded. cannot use '%s'.\n", arg_id);
  return 0;
}

static const char arg_handle_render_animation_doc[] =
    "\n\t"
    "Render frames from start to end (inclusive).";
static int arg_handle_render_animation(int /*argc*/, const char ** /*argv*/, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    add_log_render_filter();

    Main *bmain = CTX_data_main(C);
    Render *re = RE_NewSceneRender(scene);
    ReportList reports;
    BKE_reports_init(&reports, RPT_STORE);
    RE_SetReports(re, &reports);
    RE_RenderAnim(
        re, bmain, scene, nullptr, nullptr, scene->r.sfra, scene->r.efra, scene->r.frame_step);
    RE_SetReports(re, nullptr);
    BKE_reports_free(&reports);
  }
  else {
    fprintf(stderr, "\nError: no blend loaded. cannot use '-a'.\n");
  }
  return 0;
}

static const char arg_handle_scene_set_doc[] =
    "<name>\n"
    "\tSet the active scene <name> for rendering.";
static int arg_handle_scene_set(int argc, const char **argv, void *data)
{
  if (argc > 1) {
    bContext *C = static_cast<bContext *>(data);
    Scene *scene = BKE_scene_set_name(CTX_data_main(C), argv[1]);
    if (scene) {
      CTX_data_scene_set(C, scene);

      /* Set the scene of the first window, see: #55991,
       * otherwise scripts that run later won't get this scene back from the context. */
      wmWindow *win = CTX_wm_window(C);
      if (win == nullptr) {
        win = static_cast<wmWindow *>(CTX_wm_manager(C)->windows.first);
      }
      if (win != nullptr) {
        WM_window_set_active_scene(CTX_data_main(C), C, win, scene);
      }
    }
    return 1;
  }
  fprintf(stderr, "\nError: Scene name must follow '-S / --scene'.\n");
  return 0;
}

static const char arg_handle_frame_start_set_doc[] =
    "<frame>\n"
    "\tSet start to frame <frame>, supports +/- for relative frames too.";
static int arg_handle_frame_start_set(int argc, const char **argv, void *data)
{
  const char *arg_id = "-s / --frame-start";
  bContext *C = static_cast<bContext *>(data);
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    if (argc > 1) {
      const char *err_msg = nullptr;
      if (!parse_int_relative_clamp(argv[1],
                                    nullptr,
                                    scene->r.sfra,
                                    scene->r.sfra - 1,
                                    MINAFRAME,
                                    MAXFRAME,
                                    &scene->r.sfra,
                                    &err_msg))
      {
        fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
      }
      else {
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
      return 1;
    }
    fprintf(stderr, "\nError: frame number must follow '%s'.\n", arg_id);
    return 0;
  }
  fprintf(stderr, "\nError: no blend loaded. cannot use '%s'.\n", arg_id);
  return 0;
}

static const char arg_handle_frame_end_set_doc[] =
    "<frame>\n"
    "\tSet end to frame <frame>, supports +/- for relative frames too.";
static int arg_handle_frame_end_set(int argc, const char **argv, void *data)
{
  const char *arg_id = "-e / --frame-end";
  bContext *C = static_cast<bContext *>(data);
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    if (argc > 1) {
      const char *err_msg = nullptr;
      if (!parse_int_relative_clamp(argv[1],
                                    nullptr,
                                    scene->r.efra,
                                    scene->r.efra - 1,
                                    MINAFRAME,
                                    MAXFRAME,
                                    &scene->r.efra,
                                    &err_msg))
      {
        fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
      }
      else {
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
      return 1;
    }
    fprintf(stderr, "\nError: frame number must follow '%s'.\n", arg_id);
    return 0;
  }
  fprintf(stderr, "\nError: no blend loaded. cannot use '%s'.\n", arg_id);
  return 0;
}

static const char arg_handle_frame_skip_set_doc[] =
    "<frames>\n"
    "\tSet number of frames to step forward after each rendered frame.";
static int arg_handle_frame_skip_set(int argc, const char **argv, void *data)
{
  const char *arg_id = "-j / --frame-jump";
  bContext *C = static_cast<bContext *>(data);
  Scene *scene = CTX_data_scene(C);
  if (scene) {
    if (argc > 1) {
      const char *err_msg = nullptr;
      if (!parse_int_clamp(argv[1], nullptr, 1, MAXFRAME, &scene->r.frame_step, &err_msg)) {
        fprintf(stderr, "\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
      }
      else {
        DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      }
      return 1;
    }
    fprintf(stderr, "\nError: number of frames to step must follow '%s'.\n", arg_id);
    return 0;
  }
  fprintf(stderr, "\nError: no blend loaded. cannot use '%s'.\n", arg_id);
  return 0;
}

static const char arg_handle_python_file_run_doc[] =
    "<filepath>\n"
    "\tRun the given Python script file.";
static int arg_handle_python_file_run(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);

  /* Workaround for scripts not getting a `bpy.context.scene`, causes internal errors elsewhere. */
  if (argc > 1) {
#  ifdef WITH_PYTHON
    /* Make the path absolute because its needed for relative linked blends to be found. */
    char filepath[FILE_MAX];
    STRNCPY(filepath, argv[1]);
    BLI_path_canonicalize_native(filepath, sizeof(filepath));

    bool ok;
    BPY_CTX_SETUP(ok = BPY_run_filepath(C, filepath, nullptr));
    if (!ok && app_state.exit_code_on_error.python) {
      fprintf(stderr, "\nError: script failed, file: '%s', exiting.\n", argv[1]);
      WM_exit(C, app_state.exit_code_on_error.python);
    }
#  else
    UNUSED_VARS(C);
    fprintf(stderr, "This Blender was built without Python support\n");
#  endif /* WITH_PYTHON */

    return 1;
  }
  fprintf(stderr, "\nError: you must specify a filepath after '%s'.\n", argv[0]);
  return 0;
}

static const char arg_handle_python_text_run_doc[] =
    "<name>\n"
    "\tRun the given Python script text block.";
static int arg_handle_python_text_run(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);

  /* Workaround for scripts not getting a `bpy.context.scene`, causes internal errors elsewhere. */
  if (argc > 1) {
#  ifdef WITH_PYTHON
    Main *bmain = CTX_data_main(C);
    /* Make the path absolute because its needed for relative linked blends to be found. */
    Text *text = (Text *)BKE_libblock_find_name(bmain, ID_TXT, argv[1]);
    bool ok;

    if (text) {
      BPY_CTX_SETUP(ok = BPY_run_text(C, text, nullptr, false));
    }
    else {
      fprintf(stderr, "\nError: text block not found %s.\n", argv[1]);
      ok = false;
    }

    if (!ok && app_state.exit_code_on_error.python) {
      fprintf(stderr, "\nError: script failed, text: '%s', exiting.\n", argv[1]);
      WM_exit(C, app_state.exit_code_on_error.python);
    }
#  else
    UNUSED_VARS(C);
    fprintf(stderr, "This Blender was built without Python support\n");
#  endif /* WITH_PYTHON */

    return 1;
  }

  fprintf(stderr, "\nError: you must specify a text block after '%s'.\n", argv[0]);
  return 0;
}

static const char arg_handle_python_expr_run_doc[] =
    "<expression>\n"
    "\tRun the given expression as a Python script.";
static int arg_handle_python_expr_run(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);

  /* Workaround for scripts not getting a `bpy.context.scene`, causes internal errors elsewhere. */
  if (argc > 1) {
#  ifdef WITH_PYTHON
    bool ok;
    BPY_CTX_SETUP(ok = BPY_run_string_exec(C, nullptr, argv[1]));
    if (!ok && app_state.exit_code_on_error.python) {
      fprintf(stderr, "\nError: script failed, expr: '%s', exiting.\n", argv[1]);
      WM_exit(C, app_state.exit_code_on_error.python);
    }
#  else
    UNUSED_VARS(C);
    fprintf(stderr, "This Blender was built without Python support\n");
#  endif /* WITH_PYTHON */

    return 1;
  }
  fprintf(stderr, "\nError: you must specify a Python expression after '%s'.\n", argv[0]);
  return 0;
}

static const char arg_handle_python_console_run_doc[] =
    "\n\t"
    "Run Blender with an interactive console.";
static int arg_handle_python_console_run(int /*argc*/, const char ** /*argv*/, void *data)
{
  bContext *C = static_cast<bContext *>(data);
#  ifdef WITH_PYTHON
  const char *imports[] = {"code", nullptr};
  BPY_CTX_SETUP(BPY_run_string_eval(C, imports, "code.interact()"));
#  else
  UNUSED_VARS(C);
  fprintf(stderr, "This Blender was built without python support\n");
#  endif /* WITH_PYTHON */

  return 0;
}

static const char arg_handle_python_exit_code_set_doc[] =
    "<code>\n"
    "\tSet the exit-code in [0..255] to exit if a Python exception is raised\n"
    "\t(only for scripts executed from the command line), zero disables.";
static int arg_handle_python_exit_code_set(int argc, const char **argv, void * /*data*/)
{
  const char *arg_id = "--python-exit-code";
  if (argc > 1) {
    const char *err_msg = nullptr;
    const int min = 0, max = 255;
    int exit_code;
    if (!parse_int_strict_range(argv[1], nullptr, min, max, &exit_code, &err_msg)) {
      fprintf(stderr,
              "\nError: %s '%s %s', expected number in [%d..%d].\n",
              err_msg,
              arg_id,
              argv[1],
              min,
              max);
      return 1;
    }

    app_state.exit_code_on_error.python = uchar(exit_code);
    return 1;
  }
  fprintf(stderr, "\nError: you must specify an exit code number '%s'.\n", arg_id);
  return 0;
}

static const char arg_handle_python_use_system_env_set_doc[] =
    "\n\t"
    "Allow Python to use system environment variables such as 'PYTHONPATH' and the user "
    "site-packages directory.";
static int arg_handle_python_use_system_env_set(int /*argc*/,
                                                const char ** /*argv*/,
                                                void * /*data*/)
{
#  ifdef WITH_PYTHON
  BPY_python_use_system_env();
#  endif
  return 0;
}

static const char arg_handle_addons_set_doc[] =
    "<addon(s)>\n"
    "\tComma separated list (no spaces) of add-ons to enable in addition to any default add-ons.";
static int arg_handle_addons_set(int argc, const char **argv, void *data)
{
  /* Workaround for scripts not getting a `bpy.context.scene`, causes internal errors elsewhere. */
  if (argc > 1) {
#  ifdef WITH_PYTHON
    const char script_str[] =
        "from _bpy_internal.addons.cli import set_from_cli\n"
        "set_from_cli('%s')";
    const int slen = strlen(argv[1]) + (sizeof(script_str) - 2);
    char *str = static_cast<char *>(malloc(slen));
    bContext *C = static_cast<bContext *>(data);
    BLI_snprintf(str, slen, script_str, argv[1]);

    BLI_assert(strlen(str) + 1 == slen);
    BPY_CTX_SETUP(BPY_run_string_exec(C, nullptr, str));
    free(str);
#  else
    UNUSED_VARS(argv, data);
#  endif /* WITH_PYTHON */
    return 1;
  }
  fprintf(stderr, "\nError: you must specify a comma separated list after '--addons'.\n");
  return 0;
}

static const char arg_handle_profile_gpu_set_doc[] =
    "\n"
    "\tEnable CPU & GPU performance profiling for GPU debug groups\n"
    "\t(Outputs a profile.json file in the Trace Event Format to the current directory)";
static int arg_handle_profile_gpu_set(int /*argc*/, const char ** /*argv*/, void * /*data*/)
{
  G.profile_gpu = true;
  return 0;
}

/**
 * Implementation for #arg_handle_load_last_file, also used by `--open-last`.
 * \return true on success.
 */
static bool handle_load_file(bContext *C, const char *filepath_arg, const bool load_empty_file)
{
  /* Make the path absolute because its needed for relative linked blends to be found. */
  char filepath[FILE_MAX];
  STRNCPY(filepath, filepath_arg);
  BLI_path_canonicalize_native(filepath, sizeof(filepath));

  /* Load the file. */
  ReportList reports;
  BKE_reports_init(&reports, RPT_PRINT | RPT_STORE);
  BKE_report_print_level_set(&reports, RPT_WARNING);
  /* When activating from the command line there isn't an exact equivalent to operator properties.
   * Instead, enabling auto-execution via `--enable-autoexec` causes the auto-execution
   * check to be skipped (if it's set), so it's fine to always enable the check here. */
  const bool use_scripts_autoexec_check = true;
  const bool success = WM_file_read(C, filepath, use_scripts_autoexec_check, &reports);

  wmWindowManager *wm = CTX_wm_manager(C);
  WM_reports_from_reports_move(wm, &reports);
  BKE_reports_free(&reports);

  if (success) {
    if (G.background) {
      /* Ensure we use 'C->data.scene' for background render. */
      CTX_wm_window_set(C, nullptr);
    }
  }
  else {
    /* Failed to load file, stop processing arguments if running in background mode. */
    if (G.background) {
      /* Set `is_break` if running in the background mode so
       * blender will return non-zero exit code which then
       * could be used in automated script to control how
       * good or bad things are. */
      G.is_break = true;
      return false;
    }

    const char *error_msg_generic = "file could not be loaded";
    const char *error_msg = nullptr;

    if (load_empty_file == false) {
      error_msg = error_msg_generic;
    }
    else if (!BKE_blendfile_extension_check(filepath)) {
      /* Non-blend. Continue loading and give warning. */
      G_MAIN->is_read_invalid = true;
      return true;
    }
    else if (BLI_exists(filepath)) {
      /* When a file is found but can't be loaded, handling it as a new file
       * could cause it to be unintentionally overwritten (data loss).
       * Further this is almost certainly not that a user would expect or want.
       * If they do, they can delete the file beforehand. */
      error_msg = error_msg_generic;
    }

    if (error_msg) {
      fprintf(stderr, "Error: %s, exiting! %s\n", error_msg, filepath);
      WM_exit(C, EXIT_FAILURE);
      /* Unreachable, return for clarity. */
      return false;
    }

    /* Behave as if a file was loaded, calling "Save" will write to the `filepath` from the CLI.
     *
     * WARNING: The path referenced may be incorrect, no attempt is made to validate the path
     * here or check that writing to it will work. If the users enters the path of a directory
     * that doesn't exist (for example) saving will fail.
     * Attempting to create the file at this point is possible but likely to cause more
     * trouble than it's worth (what with network drives), removable devices ... etc. */

    STRNCPY(G_MAIN->filepath, filepath);
    printf("... opened default scene instead; saving will write to: %s\n", filepath);
  }

  return true;
}

int main_args_handle_load_file(int /*argc*/, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  const char *filepath = argv[0];

  /* NOTE: we could skip these, but so far we always tried to load these files. */
  if (argv[0][0] == '-') {
    fprintf(stderr, "unknown argument, loading as file: %s\n", filepath);
  }

  if (!handle_load_file(C, filepath, true)) {
    return -1;
  }
  return 0;
}

static const char arg_handle_load_last_file_doc[] =
    "\n\t"
    "Open the most recently opened blend file, instead of the default startup file.";
static int arg_handle_load_last_file(int /*argc*/, const char ** /*argv*/, void *data)
{
  if (BLI_listbase_is_empty(&G.recent_files)) {
    fprintf(stderr, "Warning: no recent files known, opening default startup file instead.\n");
    return -1;
  }

  bContext *C = static_cast<bContext *>(data);
  const RecentFile *recent_file = static_cast<const RecentFile *>(G.recent_files.first);
  if (!handle_load_file(C, recent_file->filepath, false)) {
    return -1;
  }
  return 0;
}

void main_args_setup(bContext *C, bArgs *ba, bool all)
{
/** Expand the doc-string from the function. */
#  define CB(a) a##_doc, a
/** A version of `CB` that expands an additional suffix. */
#  define CB_EX(a, b) a##_doc_##b, a
/** A version of `CB` that uses `all`, needed when the doc-string depends on build options. */
#  define CB_ALL(a) (all ? a##_doc_all : a##_doc), a

  BuildDefs defs;
  build_defs_init(&defs, all);

  /* end argument processing after -- */
  BLI_args_pass_set(ba, -1);
  BLI_args_add(ba, "--", nullptr, CB(arg_handle_arguments_end), nullptr);

  /* Pass: Environment Setup
   *
   * It's important these run before any initialization is done, since they set up
   * the environment used to access data-files, which are be used when initializing
   * sub-systems such as color management. */
  BLI_args_pass_set(ba, ARG_PASS_ENVIRONMENT);
  BLI_args_add(
      ba, nullptr, "--python-use-system-env", CB(arg_handle_python_use_system_env_set), nullptr);

  /* Note that we could add used environment variables too. */
  BLI_args_add(
      ba, nullptr, "--env-system-datafiles", CB_EX(arg_handle_env_system_set, datafiles), nullptr);
  BLI_args_add(
      ba, nullptr, "--env-system-scripts", CB_EX(arg_handle_env_system_set, scripts), nullptr);
  BLI_args_add(
      ba, nullptr, "--env-system-python", CB_EX(arg_handle_env_system_set, python), nullptr);
  BLI_args_add(ba,
               nullptr,
               "--env-system-extensions",
               CB_EX(arg_handle_env_system_set, extensions),
               nullptr);

  BLI_args_add(ba, "-t", "--threads", CB(arg_handle_threads_set), nullptr);

  /* Include in the environment pass so it's possible display errors initializing subsystems,
   * especially `bpy.appdir` since it's useful to show errors finding paths on startup. */
  BLI_args_add(ba, nullptr, "--log", CB(arg_handle_log_set), ba);
  BLI_args_add(ba, nullptr, "--log-level", CB(arg_handle_log_level_set), ba);
  BLI_args_add(ba, nullptr, "--log-show-source", CB(arg_handle_log_show_source_set), ba);
  BLI_args_add(ba, nullptr, "--log-show-backtrace", CB(arg_handle_log_show_backtrace_set), ba);
  BLI_args_add(ba, nullptr, "--log-show-memory", CB(arg_handle_log_show_memory_set), ba);
  BLI_args_add(ba, nullptr, "--log-file", CB(arg_handle_log_file_set), ba);

  /* GPU backend selection should be part of #ARG_PASS_ENVIRONMENT for correct GPU context
   * selection for animation player. */
  BLI_args_add(ba, nullptr, "--gpu-backend", CB_ALL(arg_handle_gpu_backend_set), nullptr);
  BLI_args_add(ba, nullptr, "--gpu-vsync", CB(arg_handle_gpu_vsync_set), nullptr);
  if (defs.with_opengl_backend) {
    BLI_args_add(ba,
                 nullptr,
                 "--gpu-compilation-subprocesses",
                 CB(arg_handle_gpu_compilation_subprocesses_set),
                 nullptr);
  }
  BLI_args_add(ba, nullptr, "--profile-gpu", CB(arg_handle_profile_gpu_set), nullptr);

  /* Pass: Background Mode & Settings
   *
   * Also and commands that exit after usage. */
  BLI_args_pass_set(ba, ARG_PASS_SETTINGS);
  BLI_args_add(ba, "-h", "--help", CB(arg_handle_print_help), ba);

  /* MS-Windows only. */
  BLI_args_add(ba, "/?", nullptr, CB_EX(arg_handle_print_help, win32), ba);

  BLI_args_add(ba, "-v", "--version", CB(arg_handle_print_version), nullptr);
  BLI_args_add(ba, nullptr, "--log-list-categories", CB(arg_handle_list_clog_cats), nullptr);

  BLI_args_add(ba, "-y", "--enable-autoexec", CB_EX(arg_handle_python_set, enable), (void *)true);
  BLI_args_add(
      ba, "-Y", "--disable-autoexec", CB_EX(arg_handle_python_set, disable), (void *)false);

  BLI_args_add(
      ba, nullptr, "--offline-mode", CB_EX(arg_handle_internet_allow_set, offline), (void *)false);
  BLI_args_add(
      ba, nullptr, "--online-mode", CB_EX(arg_handle_internet_allow_set, online), (void *)true);

  BLI_args_add(
      ba, nullptr, "--disable-crash-handler", CB(arg_handle_crash_handler_disable), nullptr);
  BLI_args_add(
      ba, nullptr, "--disable-abort-handler", CB(arg_handle_abort_handler_disable), nullptr);

  BLI_args_add(ba, "-q", "--quiet", CB(arg_handle_quiet_set), nullptr);
  BLI_args_add(ba, "-b", "--background", CB(arg_handle_background_mode_set), nullptr);
  /* Command implies background mode (defers execution). */
  BLI_args_add(ba, "-c", "--command", CB(arg_handle_command_set), C);

  BLI_args_add(ba, nullptr, "--qos", CB(arg_handle_qos_set), nullptr);

  BLI_args_add(ba,
               nullptr,
               "--disable-depsgraph-on-file-load",
               CB(arg_handle_disable_depsgraph_on_file_load),
               nullptr);

  BLI_args_add(ba,
               nullptr,
               "--disable-liboverride-auto-resync",
               CB(arg_handle_disable_liboverride_auto_resync),
               nullptr);

  BLI_args_add(ba, "-a", nullptr, CB(arg_handle_playback_mode), nullptr);

  BLI_args_add(ba, "-d", "--debug", CB(arg_handle_debug_mode_set), ba);

  if (defs.with_ffmpeg) {
    BLI_args_add(ba, nullptr, "--debug-ffmpeg", CB(arg_handle_debug_mode_ffmpeg), nullptr);
  }

  if (defs.with_freestyle) {
    BLI_args_add(ba,
                 nullptr,
                 "--debug-freestyle",
                 CB_EX(arg_handle_debug_mode_generic_set, freestyle),
                 (void *)G_DEBUG_FREESTYLE);
  }
  BLI_args_add(ba,
               nullptr,
               "--debug-python",
               CB_EX(arg_handle_debug_mode_generic_set, python),
               (void *)G_DEBUG_PYTHON);
  BLI_args_add(ba,
               nullptr,
               "--debug-events",
               CB_EX(arg_handle_debug_mode_generic_set, events),
               (void *)G_DEBUG_EVENTS);
  BLI_args_add(ba,
               nullptr,
               "--debug-handlers",
               CB_EX(arg_handle_debug_mode_generic_set, handlers),
               (void *)G_DEBUG_HANDLERS);
  BLI_args_add(
      ba, nullptr, "--debug-wm", CB_EX(arg_handle_debug_mode_generic_set, wm), (void *)G_DEBUG_WM);
  if (defs.with_xr_openxr) {
    BLI_args_add(ba,
                 nullptr,
                 "--debug-xr",
                 CB_EX(arg_handle_debug_mode_generic_set, xr),
                 (void *)G_DEBUG_XR);
    BLI_args_add(ba,
                 nullptr,
                 "--debug-xr-time",
                 CB_EX(arg_handle_debug_mode_generic_set, xr_time),
                 (void *)G_DEBUG_XR_TIME);
  }
  BLI_args_add(ba,
               nullptr,
               "--debug-ghost",
               CB_EX(arg_handle_debug_mode_generic_set, ghost),
               (void *)G_DEBUG_GHOST);
  BLI_args_add(ba,
               nullptr,
               "--debug-wintab",
               CB_EX(arg_handle_debug_mode_generic_set, wintab),
               (void *)G_DEBUG_WINTAB);
  BLI_args_add(ba, nullptr, "--debug-all", CB(arg_handle_debug_mode_all), nullptr);

  BLI_args_add(ba, nullptr, "--debug-io", CB(arg_handle_debug_mode_io), nullptr);

  BLI_args_add(ba, nullptr, "--debug-fpe", CB(arg_handle_debug_fpe_set), nullptr);

  if (defs.with_libmv) {
    BLI_args_add(ba, nullptr, "--debug-libmv", CB(arg_handle_debug_mode_libmv), nullptr);
  }
  if (defs.with_cycles) {
    BLI_args_add(ba, nullptr, "--debug-cycles", CB(arg_handle_debug_mode_cycles), nullptr);
  }
  BLI_args_add(ba, nullptr, "--debug-memory", CB(arg_handle_debug_mode_memory_set), nullptr);

  BLI_args_add(ba, nullptr, "--debug-value", CB(arg_handle_debug_value_set), nullptr);
  BLI_args_add(ba,
               nullptr,
               "--debug-jobs",
               CB_EX(arg_handle_debug_mode_generic_set, jobs),
               (void *)G_DEBUG_JOBS);
  BLI_args_add(ba, nullptr, "--debug-gpu", CB(arg_handle_debug_gpu_set), nullptr);
  BLI_args_add(ba,
               nullptr,
               "--debug-gpu-compile-shaders",
               CB(arg_handle_debug_gpu_compile_shaders_set),
               nullptr);
  BLI_args_add(ba,
               nullptr,
               "--debug-gpu-scope-capture",
               CB(arg_handle_debug_gpu_scope_capture_set),
               nullptr);
  BLI_args_add(
      ba, nullptr, "--debug-gpu-shader-source", CB(arg_handle_debug_gpu_shader_source), nullptr);
  if (defs.with_renderdoc) {
    BLI_args_add(
        ba, nullptr, "--debug-gpu-renderdoc", CB(arg_handle_debug_gpu_renderdoc_set), nullptr);
  }
  BLI_args_add(ba,
               nullptr,
               "--debug-gpu-shader-debug-info",
               CB(arg_handle_debug_gpu_shader_debug_info_set),
               nullptr);

  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph),
               (void *)G_DEBUG_DEPSGRAPH);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-build",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_build),
               (void *)G_DEBUG_DEPSGRAPH_BUILD);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-eval",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_eval),
               (void *)G_DEBUG_DEPSGRAPH_EVAL);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-tag",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_tag),
               (void *)G_DEBUG_DEPSGRAPH_TAG);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-time",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_time),
               (void *)G_DEBUG_DEPSGRAPH_TIME);
  BLI_args_add(ba,

               nullptr,
               "--debug-depsgraph-no-threads",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_no_threads),
               (void *)G_DEBUG_DEPSGRAPH_NO_THREADS);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-pretty",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_pretty),
               (void *)G_DEBUG_DEPSGRAPH_PRETTY);
  BLI_args_add(ba,
               nullptr,
               "--debug-depsgraph-uid",
               CB_EX(arg_handle_debug_mode_generic_set, depsgraph_uid),
               (void *)G_DEBUG_DEPSGRAPH_UID);
  BLI_args_add(ba,
               nullptr,
               "--debug-gpu-force-workarounds",
               CB_EX(arg_handle_debug_mode_generic_set, gpu_force_workarounds),
               (void *)G_DEBUG_GPU_FORCE_WORKAROUNDS);
  if (defs.with_vulkan_backend) {
    BLI_args_add(ba,
                 nullptr,
                 "--debug-gpu-vulkan-local-read",
                 CB_EX(arg_handle_debug_mode_generic_set, gpu_force_vulkan_local_read),
                 (void *)G_DEBUG_GPU_FORCE_VULKAN_LOCAL_READ);
  }
  BLI_args_add(ba, nullptr, "--debug-exit-on-error", CB(arg_handle_debug_exit_on_error), nullptr);

  BLI_args_add(ba, nullptr, "--verbose", CB(arg_handle_verbosity_set), nullptr);

  BLI_args_add(ba, nullptr, "--app-template", CB(arg_handle_app_template), nullptr);
  BLI_args_add(ba, nullptr, "--factory-startup", CB(arg_handle_factory_startup_set), nullptr);
  BLI_args_add(
      ba, nullptr, "--enable-event-simulate", CB(arg_handle_enable_event_simulate), nullptr);

  /* Pass: Custom Window Stuff. */
  BLI_args_pass_set(ba, ARG_PASS_SETTINGS_GUI);
  BLI_args_add(ba, "-p", "--window-geometry", CB(arg_handle_window_geometry), nullptr);
  BLI_args_add(ba, "-w", "--window-border", CB(arg_handle_window_border), nullptr);
  BLI_args_add(ba, "-W", "--window-fullscreen", CB(arg_handle_window_fullscreen), nullptr);
  BLI_args_add(ba, "-M", "--window-maximized", CB(arg_handle_window_maximized), nullptr);
  BLI_args_add(ba, nullptr, "--no-window-frame", CB(arg_handle_no_window_frame), nullptr);
  BLI_args_add(ba, nullptr, "--no-window-focus", CB(arg_handle_no_window_focus), nullptr);
  BLI_args_add(ba, "-con", "--start-console", CB(arg_handle_start_with_console), nullptr);
  BLI_args_add(ba, "-r", "--register", CB(arg_handle_register_extension), nullptr);
  BLI_args_add(ba, nullptr, "--register-allusers", CB(arg_handle_register_extension_all), nullptr);
  BLI_args_add(ba, nullptr, "--unregister", CB(arg_handle_unregister_extension), nullptr);
  BLI_args_add(
      ba, nullptr, "--unregister-allusers", CB(arg_handle_unregister_extension_all), nullptr);
  BLI_args_add(ba, nullptr, "--no-native-pixels", CB(arg_handle_native_pixels_set), ba);

  /* Pass: Disabling Things & Forcing Settings. */
  BLI_args_pass_set(ba, ARG_PASS_SETTINGS_FORCE);
  BLI_args_add_case(ba, "-noaudio", 1, nullptr, 0, CB(arg_handle_audio_disable), nullptr);
  BLI_args_add_case(ba, "-setaudio", 1, nullptr, 0, CB(arg_handle_audio_set), nullptr);

  /* Pass: Processing Arguments. */
  /* NOTE: Use #WM_exit for these callbacks, not `exit()`
   * so temporary files are properly cleaned up. */
  BLI_args_pass_set(ba, ARG_PASS_FINAL);
  BLI_args_add(ba, "-f", "--render-frame", CB(arg_handle_render_frame), C);
  BLI_args_add(ba, "-a", "--render-anim", CB(arg_handle_render_animation), C);
  BLI_args_add(ba, "-S", "--scene", CB(arg_handle_scene_set), C);
  BLI_args_add(ba, "-s", "--frame-start", CB(arg_handle_frame_start_set), C);
  BLI_args_add(ba, "-e", "--frame-end", CB(arg_handle_frame_end_set), C);
  BLI_args_add(ba, "-j", "--frame-jump", CB(arg_handle_frame_skip_set), C);
  BLI_args_add(ba, "-P", "--python", CB(arg_handle_python_file_run), C);
  BLI_args_add(ba, nullptr, "--python-text", CB(arg_handle_python_text_run), C);
  BLI_args_add(ba, nullptr, "--python-expr", CB(arg_handle_python_expr_run), C);
  BLI_args_add(ba, nullptr, "--python-console", CB(arg_handle_python_console_run), C);
  BLI_args_add(ba, nullptr, "--python-exit-code", CB(arg_handle_python_exit_code_set), nullptr);
  BLI_args_add(ba, nullptr, "--addons", CB(arg_handle_addons_set), C);

  BLI_args_add(ba, "-o", "--render-output", CB(arg_handle_output_set), C);
  BLI_args_add(ba, "-E", "--engine", CB(arg_handle_engine_set), C);

  BLI_args_add(ba, "-F", "--render-format", CB(arg_handle_image_type_set), C);
  BLI_args_add(ba, "-x", "--use-extension", CB(arg_handle_extension_set), C);

  BLI_args_add(ba, nullptr, "--open-last", CB(arg_handle_load_last_file), C);

#  undef CB
#  undef CB_EX
#  undef CB_ALL

#  ifdef WITH_PYTHON
  /* Use for Python to extract help text (Python can't call directly - bad-level call). */
  BPY_python_app_help_text_fn = main_args_help_as_string;
#  else
  /* Quiet unused function warning. */
  (void)main_args_help_as_string;
#  endif
}

/** \} */

#endif /* !WITH_PYTHON_MODULE */
