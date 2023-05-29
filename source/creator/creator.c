/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup creator
 */

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  include "utfconv.h"
#  include <windows.h>
#endif

#if defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG)
#  pragma comment(lib, "tbbmalloc_proxy.lib")
#  pragma comment(linker, "/include:__TBB_malloc_proxy")
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_genfile.h"

#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

/* Mostly initialization functions. */
#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_cpp_types.h"
#include "BKE_global.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_idtype.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_particle.h"
#include "BKE_shader_fx.h"
#include "BKE_sound.h"
#include "BKE_vfont.h"
#include "BKE_volume.h"

#ifndef WITH_PYTHON_MODULE
#  include "BLI_args.h"
#endif

#include "DEG_depsgraph.h"

#include "IMB_imbuf.h" /* For #IMB_init. */

#include "RE_engine.h"
#include "RE_texture.h"

#include "ED_datafiles.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "RNA_define.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

#include <signal.h>

#ifdef __FreeBSD__
#  include <floatingpoint.h>
#endif

#ifdef WITH_BINRELOC
#  include "binreloc.h"
#endif

#ifdef WITH_LIBMV
#  include "libmv-capi.h"
#endif

#ifdef WITH_CYCLES_LOGGING
#  include "CCL_api.h"
#endif

#ifdef WITH_SDL_DYNLOAD
#  include "sdlew.h"
#endif

#ifdef WITH_USD
#  include "usd.h"
#endif

#include "creator_intern.h" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Local Defines
 * \{ */

/* When building as a Python module, don't use special argument handling
 * so the module loading logic can control the `argv` & `argc`. */
#if defined(WIN32) && !defined(WITH_PYTHON_MODULE)
#  define USE_WIN32_UNICODE_ARGS
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Application State
 * \{ */

/* written to by 'creator_args.c' */
struct ApplicationState app_state = {
    .signal =
        {
            .use_crash_handler = true,
            .use_abort_handler = true,
        },
    .exit_code_on_error =
        {
            .python = 0,
        },
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Application Level Callbacks
 *
 * Initialize callbacks for the modules that need them.
 * \{ */

static void callback_mem_error(const char *errorStr)
{
  fputs(errorStr, stderr);
  fflush(stderr);
}

static void main_callback_setup(void)
{
  /* Error output from the guarded allocation routines. */
  MEM_set_error_callback(callback_mem_error);
}

/* free data on early exit (if Python calls 'sys.exit()' while parsing args for eg). */
struct CreatorAtExitData {
#ifndef WITH_PYTHON_MODULE
  bArgs *ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  const char **argv;
  int argv_num;
#endif

#if defined(WITH_PYTHON_MODULE) && !defined(USE_WIN32_UNICODE_ARGS)
  void *_empty; /* Prevent empty struct error with MSVC. */
#endif
};

static void callback_main_atexit(void *user_data)
{
  struct CreatorAtExitData *app_init_data = user_data;

#ifndef WITH_PYTHON_MODULE
  if (app_init_data->ba) {
    BLI_args_destroy(app_init_data->ba);
    app_init_data->ba = NULL;
  }
#else
  UNUSED_VARS(app_init_data); /* May be unused. */
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  if (app_init_data->argv) {
    while (app_init_data->argv_num) {
      free((void *)app_init_data->argv[--app_init_data->argv_num]);
    }
    free((void *)app_init_data->argv);
    app_init_data->argv = NULL;
  }
#else
  UNUSED_VARS(app_init_data); /* May be unused. */
#endif
}

static void callback_clg_fatal(void *fp)
{
  BLI_system_backtrace(fp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blender as a Stand-Alone Python Module (bpy)
 *
 * While not officially supported, this can be useful for Python developers.
 * See: https://wiki.blender.org/wiki/Building_Blender/Other/BlenderAsPyModule
 * \{ */

#ifdef WITH_PYTHON_MODULE

/* Called in `bpy_interface.c` when building as a Python module. */
int main_python_enter(int argc, const char **argv);
void main_python_exit(void);

/* Rename the 'main' function, allowing Python initialization to call it. */
#  define main main_python_enter
static void *evil_C = NULL;

#  ifdef __APPLE__
/* Environment is not available in macOS shared libraries. */
#    include <crt_externs.h>
char **environ = NULL;
#  endif /* __APPLE__ */

#endif /* WITH_PYTHON_MODULE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name GMP Allocator Workaround
 * \{ */

#if (defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG) && defined(WITH_GMP)) || \
    defined(DOXYGEN)
#  include "gmp.h"
#  include "tbb/scalable_allocator.h"

void *gmp_alloc(size_t size)
{
  return scalable_malloc(size);
}
void *gmp_realloc(void *ptr, size_t UNUSED(old_size), size_t new_size)
{
  return scalable_realloc(ptr, new_size);
}

void gmp_free(void *ptr, size_t UNUSED(size))
{
  scalable_free(ptr);
}
/**
 * Use TBB's scalable_allocator on Windows.
 * `TBBmalloc` correctly captures all allocations already,
 * however, GMP is built with MINGW since it doesn't build with MSVC,
 * which TBB has issues hooking into automatically.
 */
void gmp_blender_init_allocator()
{
  mp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Function
 * \{ */

/**
 * Blender's main function responsibilities are:
 * - setup subsystems.
 * - handle arguments.
 * - run #WM_main() event loop,
 *   or exit immediately when running in background-mode.
 */
int main(int argc,
#ifdef USE_WIN32_UNICODE_ARGS
         const char **UNUSED(argv_c)
#else
         const char **argv
#endif
)
{
  bContext *C;

#ifndef WITH_PYTHON_MODULE
  bArgs *ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  char **argv;
  int argv_num;
#endif

  /* --- end declarations --- */

  /* Ensure we free data on early-exit. */
  struct CreatorAtExitData app_init_data = {NULL};
  BKE_blender_atexit_register(callback_main_atexit, &app_init_data);

  /* Un-buffered `stdout` makes `stdout` and `stderr` better synchronized, and helps
   * when stepping through code in a debugger (prints are immediately
   * visible). However disabling buffering causes lock contention on windows
   * see #76767 for details, since this is a debugging aid, we do not enable
   * the un-buffered behavior for release builds. */
#ifndef NDEBUG
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

#ifdef WIN32
  /* We delay loading of OPENMP so we can set the policy here. */
#  if defined(_MSC_VER)
  _putenv_s("OMP_WAIT_POLICY", "PASSIVE");
#  endif

#  ifdef USE_WIN32_UNICODE_ARGS
  /* Win32 Unicode Arguments. */
  {
    /* NOTE: Can't use `guardedalloc` allocation here, as it's not yet initialized
     * (it depends on the arguments passed in, which is what we're getting here!) */
    wchar_t **argv_16 = CommandLineToArgvW(GetCommandLineW(), &argc);
    argv = malloc(argc * sizeof(char *));
    for (argv_num = 0; argv_num < argc; argv_num++) {
      argv[argv_num] = alloc_utf_8_from_16(argv_16[argv_num], 0);
    }
    LocalFree(argv_16);

    /* free on early-exit */
    app_init_data.argv = argv;
    app_init_data.argv_num = argv_num;
  }
#  endif /* USE_WIN32_UNICODE_ARGS */
#endif   /* WIN32 */

  /* NOTE: Special exception for guarded allocator type switch:
   *       we need to perform switch from lock-free to fully
   *       guarded allocator before any allocation happened.
   */
  {
    int i;
    for (i = 0; i < argc; i++) {
      if (STR_ELEM(argv[i], "-d", "--debug", "--debug-memory", "--debug-all")) {
        printf("Switching to fully guarded memory allocator.\n");
        MEM_use_guarded_allocator();
        break;
      }
      if (STREQ(argv[i], "--")) {
        break;
      }
    }
    MEM_init_memleak_detection();
  }

#ifdef BUILD_DATE
  {
    time_t temp_time = build_commit_timestamp;
    struct tm *tm = gmtime(&temp_time);
    if (LIKELY(tm)) {
      strftime(build_commit_date, sizeof(build_commit_date), "%Y-%m-%d", tm);
      strftime(build_commit_time, sizeof(build_commit_time), "%H:%M", tm);
    }
    else {
      const char *unknown = "date-unknown";
      STRNCPY(build_commit_date, unknown);
      STRNCPY(build_commit_time, unknown);
    }
  }
#endif

#ifdef WITH_SDL_DYNLOAD
  sdlewInit();
#endif

  /* Initialize logging. */
  CLG_init();
  CLG_fatal_fn_set(callback_clg_fatal);

  C = CTX_create();

#ifdef WITH_PYTHON_MODULE
#  ifdef __APPLE__
  environ = *_NSGetEnviron();
#  endif

#  undef main
  evil_C = C;
#endif

#ifdef WITH_BINRELOC
  br_init(NULL);
#endif

#ifdef WITH_LIBMV
  libmv_initLogging(argv[0]);
#elif defined(WITH_CYCLES_LOGGING)
  CCL_init_logging(argv[0]);
#endif

#if defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG) && defined(WITH_GMP)
  gmp_blender_init_allocator();
#endif

  main_callback_setup();

#if defined(__APPLE__) && !defined(WITH_PYTHON_MODULE) && !defined(WITH_HEADLESS)
  /* Patch to ignore argument finder gives us (PID?) */
  if (argc == 2 && STRPREFIX(argv[1], "-psn_")) {
    extern int GHOST_HACK_getFirstFile(char buf[]);
    static char firstfilebuf[512];

    argc = 1;

    if (GHOST_HACK_getFirstFile(firstfilebuf)) {
      argc = 2;
      argv[1] = firstfilebuf;
    }
  }
#endif

#ifdef __FreeBSD__
  fpsetmask(0);
#endif

  /* Initialize path to executable. */
  BKE_appdir_program_path_init(argv[0]);

  BLI_threadapi_init();

  DNA_sdna_current_init();

  BKE_blender_globals_init(); /* blender.c */

  BKE_cpp_types_init();
  BKE_idtype_init();
  BKE_cachefiles_init();
  BKE_modifier_init();
  BKE_gpencil_modifier_init();
  BKE_shaderfx_init();
  BKE_volumes_init();
  DEG_register_node_types();

  BKE_brush_system_init();
  RE_texture_rng_init();

  BKE_callback_global_init();

  /* First test for background-mode (#Global.background) */
#ifndef WITH_PYTHON_MODULE
  ba = BLI_args_create(argc, (const char **)argv); /* skip binary path */

  /* Ensure we free on early exit. */
  app_init_data.ba = ba;

  main_args_setup(C, ba, false);

  /* Begin argument parsing, ignore leaks so arguments that call #exit
   * (such as '--version' & '--help') don't report leaks. */
  MEM_use_memleak_detection(false);

  /* Parse environment handling arguments. */
  BLI_args_parse(ba, ARG_PASS_ENVIRONMENT, NULL, NULL);

#else
  /* Using preferences or user startup makes no sense for #WITH_PYTHON_MODULE. */
  G.factory_startup = true;
#endif

  /* After parsing #ARG_PASS_ENVIRONMENT such as `--env-*`,
   * since they impact `BKE_appdir` behavior. */
  BKE_appdir_init();

  /* After parsing number of threads argument. */
  BLI_task_scheduler_init();

  /* Initialize sub-systems that use `BKE_appdir.h`. */
  IMB_init();

#ifdef WITH_USD
  USD_ensure_plugin_path_registered();
#endif

#ifndef WITH_PYTHON_MODULE
  /* First test for background-mode (#Global.background) */
  BLI_args_parse(ba, ARG_PASS_SETTINGS, NULL, NULL);

  main_signal_setup();
#endif

#ifdef WITH_FFMPEG
  /* Keep after #ARG_PASS_SETTINGS since debug flags are checked. */
  IMB_ffmpeg_init();
#endif

  /* After #ARG_PASS_SETTINGS arguments, this is so #WM_main_playanim skips #RNA_init. */
  RNA_init();

  RE_engines_init();
  BKE_node_system_init();
  BKE_particle_init_rng();
  /* End second initialization. */

#if defined(WITH_PYTHON_MODULE) || defined(WITH_HEADLESS)
  /* Python module mode ALWAYS runs in background-mode (for now). */
  G.background = true;
#else
  if (G.background) {
    main_signal_setup_background();
  }
#endif

  /* Background render uses this font too. */
  BKE_vfont_builtin_register(datatoc_bfont_pfb, datatoc_bfont_pfb_size);

  /* Initialize FFMPEG if built in, also needed for background-mode if videos are
   * rendered via FFMPEG. */
  BKE_sound_init_once();

  BKE_materials_init();

#ifndef WITH_PYTHON_MODULE
  if (G.background == 0) {
    BLI_args_parse(ba, ARG_PASS_SETTINGS_GUI, NULL, NULL);
  }
  BLI_args_parse(ba, ARG_PASS_SETTINGS_FORCE, NULL, NULL);
#endif

  WM_init(C, argc, (const char **)argv);

  /* Need to be after WM init so that userpref are loaded. */
  RE_engines_init_experimental();

#ifndef WITH_PYTHON
  printf(
      "\n* WARNING * - Blender compiled without Python!\n"
      "this is not intended for typical usage\n\n");
#endif

  CTX_py_init_set(C, true);
  WM_keyconfig_init(C);

#ifdef WITH_FREESTYLE
  /* Initialize Freestyle. */
  FRS_init();
  FRS_set_context(C);
#endif

  /* OK we are ready for it */
#ifndef WITH_PYTHON_MODULE
  /* Handles #ARG_PASS_FINAL. */
  BLI_args_parse(ba, ARG_PASS_FINAL, main_args_handle_load_file, C);
#endif

  /* Explicitly free data allocated for argument parsing:
   * - 'ba'
   * - 'argv' on WIN32.
   */
  callback_main_atexit(&app_init_data);
  BKE_blender_atexit_unregister(callback_main_atexit, &app_init_data);

  /* End argument parsing, allow memory leaks to be printed. */
  MEM_use_memleak_detection(true);

  /* Paranoid, avoid accidental re-use. */
#ifndef WITH_PYTHON_MODULE
  ba = NULL;
  (void)ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  argv = NULL;
  (void)argv;
#endif

#ifndef WITH_PYTHON_MODULE
  if (G.background) {
    /* Using window-manager API in background-mode is a bit odd, but works fine. */
    WM_exit(C);
  }
  else {
    /* Shows the splash as needed. */
    WM_init_splash_on_startup(C);

    WM_main(C);
  }
  /* Neither #WM_exit, #WM_main return, this quiets CLANG's `unreachable-code-return` warning. */
  BLI_assert_unreachable();

#endif /* !WITH_PYTHON_MODULE */

  return 0;

} /* End of `int main(...)` function. */

#ifdef WITH_PYTHON_MODULE
void main_python_exit(void)
{
  WM_exit_ex((bContext *)evil_C, true);
  evil_C = NULL;
}
#endif

/** \} */
