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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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

#include "BLI_args.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

/* mostly init functions */
#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_particle.h"
#include "BKE_shader_fx.h"
#include "BKE_sound.h"
#include "BKE_volume.h"

#include "DEG_depsgraph.h"

#include "IMB_imbuf.h" /* for IMB_init */

#include "RE_engine.h"
#include "RE_render_ext.h"

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

#include "creator_intern.h" /* own include */

/* Local Function prototypes. */
#ifdef WITH_PYTHON_MODULE
int main_python_enter(int argc, const char **argv);
void main_python_exit(void);
#endif

#ifdef WITH_USD
/**
 * Workaround to make it possible to pass a path at runtime to USD.
 *
 * USD requires some JSON files, and it uses a static constructor to determine the possible
 * file-system paths to find those files. This made it impossible for Blender to pass a path to the
 * USD library at runtime, as the constructor would run before Blender's main() function. We have
 * patched USD (see usd.diff) to avoid that particular static constructor, and have an
 * initialization function instead.
 *
 * This function is implemented in the USD source code, pxr/base/lib/plug/initConfig.cpp.
 */
void usd_initialise_plugin_path(const char *datafiles_usd_path);
#endif

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

/* -------------------------------------------------------------------- */
/** \name Application Level Callbacks
 *
 * Initialize callbacks for the modules that need them.
 *
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
  bArgs *ba;
#ifdef WIN32
  const char **argv;
  int argv_num;
#endif
};

static void callback_main_atexit(void *user_data)
{
  struct CreatorAtExitData *app_init_data = user_data;

  if (app_init_data->ba) {
    BLI_argsFree(app_init_data->ba);
    app_init_data->ba = NULL;
  }

#ifdef WIN32
  if (app_init_data->argv) {
    while (app_init_data->argv_num) {
      free((void *)app_init_data->argv[--app_init_data->argv_num]);
    }
    free((void *)app_init_data->argv);
    app_init_data->argv = NULL;
  }
#endif
}

static void callback_clg_fatal(void *fp)
{
  BLI_system_backtrace(fp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Function
 * \{ */

#ifdef WITH_PYTHON_MODULE
/* allow python module to call main */
#  define main main_python_enter
static void *evil_C = NULL;

#  ifdef __APPLE__
/* Environment is not available in macOS shared libraries. */
#    include <crt_externs.h>
char **environ = NULL;
#  endif
#endif

/**
 * Blender's main function responsibilities are:
 * - setup subsystems.
 * - handle arguments.
 * - run #WM_main() event loop,
 *   or exit immediately when running in background-mode.
 */
int main(int argc,
#ifdef WIN32
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

#ifdef WIN32
  char **argv;
  int argv_num;
#endif

  /* --- end declarations --- */

  /* ensure we free data on early-exit */
  struct CreatorAtExitData app_init_data = {NULL};
  BKE_blender_atexit_register(callback_main_atexit, &app_init_data);

  /* Un-buffered `stdout` makes `stdout` and `stderr` better synchronized, and helps
   * when stepping through code in a debugger (prints are immediately
   * visible). However disabling buffering causes lock contention on windows
   * see T76767 for details, since this is a debugging aid, we do not enable
   * the un-buffered behavior for release builds. */
#ifndef NDEBUG
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

#ifdef WIN32
  /* We delay loading of OPENMP so we can set the policy here. */
#  if defined(_MSC_VER)
  _putenv_s("OMP_WAIT_POLICY", "PASSIVE");
#  endif

  /* Win32 Unicode Arguments. */
  /* NOTE: cannot use guardedalloc malloc here, as it's not yet initialized
   *       (it depends on the arguments passed in, which is what we're getting here!)
   */
  {
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
#endif /* WIN32 */

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
      else if (STREQ(argv[i], "--")) {
        break;
      }
    }
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
      BLI_strncpy(build_commit_date, unknown, sizeof(build_commit_date));
      BLI_strncpy(build_commit_time, unknown, sizeof(build_commit_time));
    }
  }
#endif

#ifdef WITH_SDL_DYNLOAD
  sdlewInit();
#endif

  /* Initialize logging */
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

  main_callback_setup();

#if defined(__APPLE__) && !defined(WITH_PYTHON_MODULE) && !defined(WITH_HEADLESS)
  /* Patch to ignore argument finder gives us (PID?) */
  if (argc == 2 && STREQLEN(argv[1], "-psn_", 5)) {
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

  /* initialize path to executable */
  BKE_appdir_program_path_init(argv[0]);

  BLI_threadapi_init();
  BLI_thread_put_process_on_fast_node();

  DNA_sdna_current_init();

  BKE_blender_globals_init(); /* blender.c */

  BKE_idtype_init();
  IMB_init();
  BKE_cachefiles_init();
  BKE_images_init();
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
  ba = BLI_argsInit(argc, (const char **)argv); /* skip binary path */

  /* Ensure we free on early exit. */
  app_init_data.ba = ba;

  main_args_setup(C, ba);

  BLI_argsParse(ba, 1, NULL, NULL);

  main_signal_setup();

#else
  /* Using preferences or user startup makes no sense for #WITH_PYTHON_MODULE. */
  G.factory_startup = true;
#endif

  /* After parsing number of threads argument. */
  BLI_task_scheduler_init();

#ifdef WITH_FFMPEG
  IMB_ffmpeg_init();
#endif

  /* After level 1 arguments, this is so #WM_main_playanim skips #RNA_init. */
  RNA_init();

  RE_engines_init();
  init_nodesystem();
  psys_init_rng();
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

  /* Initialize ffmpeg if built in, also needed for background-mode if videos are
   * rendered via ffmpeg. */
  BKE_sound_init_once();

  BKE_materials_init();

#ifdef WITH_USD
  /* Tell USD which directory to search for its JSON files. If 'datafiles/usd'
   * does not exist, the USD library will not be able to read or write any files. */
  usd_initialise_plugin_path(BKE_appdir_folder_id(BLENDER_DATAFILES, "usd"));
#endif

  if (G.background == 0) {
#ifndef WITH_PYTHON_MODULE
    BLI_argsParse(ba, 2, NULL, NULL);
    BLI_argsParse(ba, 3, NULL, NULL);
#endif
    WM_init(C, argc, (const char **)argv);

    /* This is properly initialized with user-preferences,
     * but this is default.
     * Call after loading the #BLENDER_STARTUP_FILE so we can read #U.tempdir */
    BKE_tempdir_init(U.tempdir);
  }
  else {
#ifndef WITH_PYTHON_MODULE
    BLI_argsParse(ba, 3, NULL, NULL);
#endif

    WM_init(C, argc, (const char **)argv);

    /* Don't use user preferences #U.tempdir */
    BKE_tempdir_init(NULL);
  }
#ifdef WITH_PYTHON
  /**
   * \note the #U.pythondir string is NULL until #WM_init() is executed,
   * so we provide the BPY_ function below to append the user defined
   * python-dir to Python's `sys.path` at this point.  Simply putting
   * #WM_init() before #BPY_python_start() crashes Blender at startup.
   */

  /* TODO: #U.pythondir */
#else
  printf(
      "\n* WARNING * - Blender compiled without Python!\n"
      "this is not intended for typical usage\n\n");
#endif

  CTX_py_init_set(C, true);
  WM_keyconfig_init(C);

#ifdef WITH_FREESTYLE
  /* Initialize Freestyle. */
  FRS_initialize();
  FRS_set_context(C);
#endif

  /* OK we are ready for it */
#ifndef WITH_PYTHON_MODULE
  main_args_setup_post(C, ba);
#endif

  /* Explicitly free data allocated for argument parsing:
   * - 'ba'
   * - 'argv' on WIN32.
   */
  callback_main_atexit(&app_init_data);
  BKE_blender_atexit_unregister(callback_main_atexit, &app_init_data);

  /* Paranoid, avoid accidental re-use. */
#ifndef WITH_PYTHON_MODULE
  ba = NULL;
  (void)ba;
#endif

#ifdef WIN32
  argv = NULL;
  (void)argv;
#endif

#ifdef WITH_PYTHON_MODULE
  /* Keep blender in background-mode running. */
  return 0;
#endif

  if (G.background) {
    /* Using window-manager API in background-mode is a bit odd, but works fine. */
    WM_exit(C);
  }
  else {
    if (!G.file_loaded) {
      WM_init_splash(C);
    }
  }

  WM_main(C);

  return 0;
} /* End of int main(...) function. */

#ifdef WITH_PYTHON_MODULE
void main_python_exit(void)
{
  WM_exit_ex((bContext *)evil_C, true);
  evil_C = NULL;
}
#endif

/** \} */
