/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file creator/creator.c
 *  \ingroup creator
 */

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  if defined(_MSC_VER) && defined(_M_X64)
#    include <math.h> /* needed for _set_FMA3_enable */
#  endif
#  include <windows.h>
#  include "utfconv.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_genfile.h"

#include "BLI_args.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"
#include "BLI_string.h"

/* mostly init functions */
#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h" /* for DAG_init */
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_sound.h"
#include "BKE_image.h"
#include "BKE_particle.h"


#include "IMB_imbuf.h"  /* for IMB_init */

#include "RE_engine.h"
#include "RE_render_ext.h"

#include "ED_datafiles.h"

#include "WM_api.h"

#include "RNA_define.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

/* for passing information between creator and gameengine */
#ifdef WITH_GAMEENGINE
#  include "BL_System.h"
#else /* dummy */
#  define SYS_SystemHandle int
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

#include "creator_intern.h"  /* own include */


/*	Local Function prototypes */
#ifdef WITH_PYTHON_MODULE
int  main_python_enter(int argc, const char **argv);
void main_python_exit(void);
#endif

/* written to by 'creator_args.c' */
struct ApplicationState app_state = {
	.signal = {
		.use_crash_handler = true,
		.use_abort_handler = true,
	},
	.exit_code_on_error = {
		.python = 0,
	}
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
	/* Error output from the alloc routines: */
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

/** \} */



/* -------------------------------------------------------------------- */

/** \name Main Function
 * \{ */

#ifdef WITH_PYTHON_MODULE
/* allow python module to call main */
#  define main main_python_enter
static void *evil_C = NULL;

#  ifdef __APPLE__
     /* environ is not available in mac shared libraries */
#    include <crt_externs.h>
char **environ = NULL;
#  endif
#endif

/**
 * Blender's main function responsibilities are:
 * - setup subsystems.
 * - handle arguments.
 * - run #WM_main() event loop,
 *   or exit immediately when running in background mode.
 */
int main(
        int argc,
#ifdef WIN32
        const char **UNUSED(argv_c)
#else
        const char **argv
#endif
        )
{
	bContext *C;
	SYS_SystemHandle syshandle;

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

#ifdef WIN32
	/* We delay loading of openmp so we can set the policy here. */
# if defined(_MSC_VER)
	_putenv_s("OMP_WAIT_POLICY", "PASSIVE");
# endif

	/* FMA3 support in the 2013 CRT is broken on Vista and Windows 7 RTM (fixed in SP1). Just disable it. */
#  if defined(_MSC_VER) && defined(_M_X64)
	_set_FMA3_enable(0);
#  endif

	/* Win32 Unicode Args */
	/* NOTE: cannot use guardedalloc malloc here, as it's not yet initialized
	 *       (it depends on the args passed in, which is what we're getting here!)
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
#endif  /* WIN32 */

	/* NOTE: Special exception for guarded allocator type switch:
	 *       we need to perform switch from lock-free to fully
	 *       guarded allocator before any allocation happened.
	 */
	{
		int i;
		for (i = 0; i < argc; i++) {
			if (STREQ(argv[i], "--debug") || STREQ(argv[i], "-d") ||
			    STREQ(argv[i], "--debug-memory") || STREQ(argv[i], "--debug-all"))
			{
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

	C = CTX_create();

#ifdef WITH_PYTHON_MODULE
#ifdef __APPLE__
	environ = *_NSGetEnviron();
#endif

#undef main
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
	
#if defined(__APPLE__) && !defined(WITH_PYTHON_MODULE)
	/* patch to ignore argument finder gives us (pid?) */
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

	DNA_sdna_current_init();

	BKE_blender_globals_init();  /* blender.c */

	IMB_init();
	BKE_cachefiles_init();
	BKE_images_init();
	BKE_modifier_init();
	DAG_init();

	BKE_brush_system_init();
	RE_texture_rng_init();
	

	BLI_callback_global_init();

#ifdef WITH_GAMEENGINE
	syshandle = SYS_GetSystem();
#else
	syshandle = 0;
#endif

	/* first test for background */
#ifndef WITH_PYTHON_MODULE
	ba = BLI_argsInit(argc, (const char **)argv); /* skip binary path */

	/* ensure we free on early exit */
	app_init_data.ba = ba;

	main_args_setup(C, ba, &syshandle);

	BLI_argsParse(ba, 1, NULL, NULL);

	main_signal_setup();

#else
	G.factory_startup = true;  /* using preferences or user startup makes no sense for py-as-module */
	(void)syshandle;
#endif

#ifdef WITH_FFMPEG
	IMB_ffmpeg_init();
#endif

	/* after level 1 args, this is so playanim skips RNA init */
	RNA_init();

	RE_engines_init();
	init_nodesystem();
	psys_init_rng();
	/* end second init */


#if defined(WITH_PYTHON_MODULE) || defined(WITH_HEADLESS)
	G.background = true; /* python module mode ALWAYS runs in background mode (for now) */
#else
	if (G.background) {
		main_signal_setup_background();
	}
#endif

	/* background render uses this font too */
	BKE_vfont_builtin_register(datatoc_bfont_pfb, datatoc_bfont_pfb_size);

	/* Initialize ffmpeg if built in, also needed for bg mode if videos are
	 * rendered via ffmpeg */
	BKE_sound_init_once();
	
	init_def_material();

	if (G.background == 0) {
#ifndef WITH_PYTHON_MODULE
		BLI_argsParse(ba, 2, NULL, NULL);
		BLI_argsParse(ba, 3, NULL, NULL);
#endif
		WM_init(C, argc, (const char **)argv);

		/* this is properly initialized with user defs, but this is default */
		/* call after loading the startup.blend so we can read U.tempdir */
		BKE_tempdir_init(U.tempdir);
	}
	else {
#ifndef WITH_PYTHON_MODULE
		BLI_argsParse(ba, 3, NULL, NULL);
#endif

		WM_init(C, argc, (const char **)argv);

		/* don't use user preferences temp dir */
		BKE_tempdir_init(NULL);
	}
#ifdef WITH_PYTHON
	/**
	 * NOTE: the U.pythondir string is NULL until WM_init() is executed,
	 * so we provide the BPY_ function below to append the user defined
	 * python-dir to Python's sys.path at this point.  Simply putting
	 * WM_init() before #BPY_python_start() crashes Blender at startup.
	 */

	/* TODO - U.pythondir */
#else
	printf("\n* WARNING * - Blender compiled without Python!\nthis is not intended for typical usage\n\n");
#endif
	
	CTX_py_init_set(C, 1);
	WM_keymap_init(C);

#ifdef WITH_FREESTYLE
	/* initialize Freestyle */
	FRS_initialize();
	FRS_set_context(C);
#endif

	/* OK we are ready for it */
#ifndef WITH_PYTHON_MODULE
	main_args_setup_post(C, ba);
	
	if (G.background == 0) {
		if (!G.file_loaded)
			if (U.uiflag2 & USER_KEEP_SESSION)
				WM_recover_last_session(C, NULL);
	}

#endif

	/* Explicitly free data allocated for argument parsing:
	 * - 'ba'
	 * - 'argv' on WIN32.
	 */
	callback_main_atexit(&app_init_data);
	BKE_blender_atexit_unregister(callback_main_atexit, &app_init_data);

	/* paranoid, avoid accidental re-use */
#ifndef WITH_PYTHON_MODULE
	ba = NULL;
	(void)ba;
#endif

#ifdef WIN32
	argv = NULL;
	(void)argv;
#endif

#ifdef WITH_PYTHON_MODULE
	return 0; /* keep blender in background mode running */
#endif

	if (G.background) {
		/* Using window-manager API in background mode is a bit odd, but works fine. */
		WM_exit(C);
	}
	else {
		if (G.fileflags & G_FILE_AUTOPLAY) {
			if (G.f & G_SCRIPT_AUTOEXEC) {
				if (WM_init_game(C)) {
					return 0;
				}
			}
			else {
				if (!(G.f & G_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
					G.f |= G_SCRIPT_AUTOEXEC_FAIL;
					BLI_snprintf(G.autoexec_fail, sizeof(G.autoexec_fail), "Game AutoStart");
				}
			}
		}

		if (!G.file_loaded) {
			WM_init_splash(C);
		}
	}

	WM_main(C);

	return 0;
} /* end of int main(argc, argv)	*/

#ifdef WITH_PYTHON_MODULE
void main_python_exit(void)
{
	WM_exit_ext((bContext *)evil_C, true);
	evil_C = NULL;
}
#endif

/** \} */
