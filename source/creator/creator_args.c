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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file creator/creator_args.c
 *  \ingroup creator
 */

#ifndef WITH_PYTHON_MODULE

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_args.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_mempool.h"
#include "BLI_system.h"

#include "BLO_readfile.h"  /* only for BLO_has_bfile_extension */

#include "BKE_blender_version.h"
#include "BKE_context.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_image.h"

#include "DNA_screen_types.h"

#include "DEG_depsgraph.h"

#ifdef WITH_FFMPEG
#include "IMB_imbuf.h"
#endif

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "ED_datafiles.h"

#include "WM_api.h"

#include "GPU_basic_shader.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"

/* for passing information between creator and gameengine */
#ifdef WITH_GAMEENGINE
#  include "BL_System.h"
#else /* dummy */
#  define SYS_SystemHandle int
#endif

#ifdef WITH_LIBMV
#  include "libmv-capi.h"
#endif

#ifdef WITH_CYCLES_LOGGING
#  include "CCL_api.h"
#endif

#include "creator_intern.h"  /* own include */


/* -------------------------------------------------------------------- */

/** \name Utility String Parsing
 * \{ */

static bool parse_int_relative(
        const char *str, const char *str_end_test, int pos, int neg,
        int *r_value, const char **r_err_msg)
{
	char *str_end = NULL;
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
	else if ((errno == ERANGE) || ((value < INT_MIN || value > INT_MAX))) {
		static const char *msg = "exceeds range";
		*r_err_msg = msg;
		return false;
	}
	else {
		*r_value = (int)value;
		return true;
	}
}

static const char *parse_int_range_sep_search(const char *str, const char *str_end_test)
{
	const char *str_end_range = NULL;
	if (str_end_test) {
		str_end_range = memchr(str, '.', (str_end_test - str) - 1);
		if (str_end_range && (str_end_range[1] != '.')) {
			str_end_range = NULL;
		}
	}
	else {
		str_end_range = strstr(str, "..");
		if (str_end_range && (str_end_range[2] == '\0')) {
			str_end_range = NULL;
		}
	}
	return str_end_range;
}

/**
 * Parse a number as a range, eg: `1..4`.
 *
 * The \a str_end_range argument is a result of #parse_int_range_sep_search.
 */
static bool parse_int_range_relative(
        const char *str, const char *str_end_range, const char *str_end_test, int pos, int neg,
        int r_value_range[2], const char **r_err_msg)
{
	if (parse_int_relative(str,               str_end_range, pos, neg, &r_value_range[0], r_err_msg) &&
	    parse_int_relative(str_end_range + 2, str_end_test,  pos, neg, &r_value_range[1], r_err_msg))
	{
		return true;
	}
	else {
		return false;
	}
}

static bool parse_int_relative_clamp(
        const char *str, const char *str_end_test, int pos, int neg, int min, int max,
        int *r_value, const char **r_err_msg)
{
	if (parse_int_relative(str, str_end_test, pos, neg, r_value, r_err_msg)) {
		CLAMP(*r_value, min, max);
		return true;
	}
	else {
		return false;
	}
}

static bool parse_int_range_relative_clamp(
        const char *str, const char *str_end_range, const char *str_end_test, int pos, int neg, int min, int max,
        int r_value_range[2], const char **r_err_msg)
{
	if (parse_int_range_relative(str, str_end_range, str_end_test, pos, neg, r_value_range, r_err_msg)) {
		CLAMP(r_value_range[0], min, max);
		CLAMP(r_value_range[1], min, max);
		return true;
	}
	else {
		return false;
	}
}

/**
 * No clamping, fails with any number outside the range.
 */
static bool parse_int_strict_range(
        const char *str, const char *str_end_test, const int min, const int max,
        int *r_value, const char **r_err_msg)
{
	char *str_end = NULL;
	long value;

	errno = 0;
	value = strtol(str, &str_end, 10);

	if (*str_end != '\0' && (str_end != str_end_test)) {
		static const char *msg = "not a number";
		*r_err_msg = msg;
		return false;
	}
	else if ((errno == ERANGE) || ((value < min || value > max))) {
		static const char *msg = "exceeds range";
		*r_err_msg = msg;
		return false;
	}
	else {
		*r_value = (int)value;
		return true;
	}
}

static bool parse_int(
        const char *str, const char *str_end_test,
        int *r_value, const char **r_err_msg)
{
	return parse_int_strict_range(str, str_end_test, INT_MIN, INT_MAX, r_value, r_err_msg);
}

static bool parse_int_clamp(
        const char *str, const char *str_end_test, int min, int max,
        int *r_value, const char **r_err_msg)
{
	if (parse_int(str, str_end_test, r_value, r_err_msg)) {
		CLAMP(*r_value, min, max);
		return true;
	}
	else {
		return false;
	}
}

#if 0
/**
 * Version of #parse_int_relative_clamp
 * that parses a comma separated list of numbers.
 */
static int *parse_int_relative_clamp_n(
        const char *str, int pos, int neg, int min, int max,
        int *r_value_len, const char **r_err_msg)
{
	const char sep = ',';
	int len = 1;
	for (int i = 0; str[i]; i++) {
		if (str[i] == sep) {
			len++;
		}
	}

	int *values = MEM_mallocN(sizeof(*values) * len, __func__);
	int i = 0;
	while (true) {
		const char *str_end = strchr(str, sep);
		if ((*str == sep) || (*str == '\0')) {
			static const char *msg = "incorrect comma use";
			*r_err_msg = msg;
			goto fail;

		}
		else if (parse_int_relative_clamp(str, str_end, pos, neg, min, max, &values[i], r_err_msg)) {
			i++;
		}
		else {
			goto fail;  /* error message already set */
		}

		if (str_end) {  /* next */
			str = str_end + 1;
		}
		else {  /* finished */
			break;
		}
	}

	*r_value_len = i;
	return values;

fail:
	MEM_freeN(values);
	return NULL;
}

#endif

/**
 * Version of #parse_int_relative_clamp & #parse_int_range_relative_clamp
 * that parses a comma separated list of numbers.
 *
 * \note single values are evaluated as a range with matching start/end.
 */
static int (*parse_int_range_relative_clamp_n(
        const char *str, int pos, int neg, int min, int max,
        int *r_value_len, const char **r_err_msg))[2]
{
	const char sep = ',';
	int len = 1;
	for (int i = 0; str[i]; i++) {
		if (str[i] == sep) {
			len++;
		}
	}

	int (*values)[2] = MEM_mallocN(sizeof(*values) * len, __func__);
	int i = 0;
	while (true) {
		const char *str_end_range;
		const char *str_end = strchr(str, sep);
		if ((*str == sep) || (*str == '\0')) {
			static const char *msg = "incorrect comma use";
			*r_err_msg = msg;
			goto fail;
		}
		else if ((str_end_range = parse_int_range_sep_search(str, str_end)) ?
		         parse_int_range_relative_clamp(str, str_end_range, str_end, pos, neg, min, max, values[i], r_err_msg) :
		         parse_int_relative_clamp(str, str_end, pos, neg, min, max, &values[i][0], r_err_msg))
		{
			if (str_end_range == NULL) {
				values[i][1] = values[i][0];
			}
			i++;
		}
		else {
			goto fail;  /* error message already set */
		}

		if (str_end) {  /* next */
			str = str_end + 1;
		}
		else {  /* finished */
			break;
		}
	}

	*r_value_len = i;
	return values;

fail:
	MEM_freeN(values);
	return NULL;
}

/** \} */


/* -------------------------------------------------------------------- */

#ifdef WITH_PYTHON

/** \name Utilities Python Context Macro (#BPY_CTX_SETUP)
 * \{ */
struct BlendePyContextStore {
	wmWindowManager *wm;
	Scene *scene;
	wmWindow *win;
	bool has_win;
};

static void arg_py_context_backup(
        bContext *C, struct BlendePyContextStore *c_py,
        const char *script_id)
{
	c_py->wm = CTX_wm_manager(C);
	c_py->scene = CTX_data_scene(C);
	c_py->has_win = !BLI_listbase_is_empty(&c_py->wm->windows);
	if (c_py->has_win) {
		c_py->win = CTX_wm_window(C);
		CTX_wm_window_set(C, c_py->wm->windows.first);
	}
	else {
		c_py->win = NULL;
		fprintf(stderr, "Python script \"%s\" "
		        "running with missing context data.\n", script_id);
	}
}

static void arg_py_context_restore(
        bContext *C, struct BlendePyContextStore *c_py)
{
	/* script may load a file, check old data is valid before using */
	if (c_py->has_win) {
		if ((c_py->win == NULL) ||
		    ((BLI_findindex(&G_MAIN->wm, c_py->wm) != -1) &&
		     (BLI_findindex(&c_py->wm->windows, c_py->win) != -1)))
		{
			CTX_wm_window_set(C, c_py->win);
		}
	}

	if ((c_py->scene == NULL) ||
	    BLI_findindex(&G_MAIN->scene, c_py->scene) != -1)
	{
		CTX_data_scene_set(C, c_py->scene);
	}
}

/* macro for context setup/reset */
#define BPY_CTX_SETUP(_cmd) \
	{ \
		struct BlendePyContextStore py_c; \
		arg_py_context_backup(C, &py_c, argv[1]); \
		{ _cmd; } \
		arg_py_context_restore(C, &py_c); \
	} ((void)0)

#endif /* WITH_PYTHON */

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

static const char arg_handle_print_version_doc[] =
"\n\tPrint Blender version and exit."
;
static int arg_handle_print_version(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	printf(BLEND_VERSION_STRING_FMT);
#ifdef BUILD_DATE
	printf("\tbuild date: %s\n", build_date);
	printf("\tbuild time: %s\n", build_time);
	printf("\tbuild commit date: %s\n", build_commit_date);
	printf("\tbuild commit time: %s\n", build_commit_time);
	printf("\tbuild hash: %s\n", build_hash);
	printf("\tbuild platform: %s\n", build_platform);
	printf("\tbuild type: %s\n", build_type);
	printf("\tbuild c flags: %s\n", build_cflags);
	printf("\tbuild c++ flags: %s\n", build_cxxflags);
	printf("\tbuild link flags: %s\n", build_linkflags);
	printf("\tbuild system: %s\n", build_system);
#endif
	exit(0);

	return 0;
}

static const char arg_handle_print_help_doc[] =
"\n\tPrint this help text and exit."
;
static const char arg_handle_print_help_doc_win32[] =
"\n\tPrint this help text and exit (windows only)."
;
static int arg_handle_print_help(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
	bArgs *ba = (bArgs *)data;

	printf(BLEND_VERSION_STRING_FMT);
	printf("Usage: blender [args ...] [file] [args ...]\n\n");

	printf("Render Options:\n");
	BLI_argsPrintArgDoc(ba, "--background");
	BLI_argsPrintArgDoc(ba, "--render-anim");
	BLI_argsPrintArgDoc(ba, "--scene");
	BLI_argsPrintArgDoc(ba, "--render-frame");
	BLI_argsPrintArgDoc(ba, "--frame-start");
	BLI_argsPrintArgDoc(ba, "--frame-end");
	BLI_argsPrintArgDoc(ba, "--frame-jump");
	BLI_argsPrintArgDoc(ba, "--render-output");
	BLI_argsPrintArgDoc(ba, "--engine");
	BLI_argsPrintArgDoc(ba, "--threads");

	printf("\n");
	printf("Format Options:\n");
	BLI_argsPrintArgDoc(ba, "--render-format");
	BLI_argsPrintArgDoc(ba, "--use-extension");

	printf("\n");
	printf("Animation Playback Options:\n");
	BLI_argsPrintArgDoc(ba, "-a");

	printf("\n");
	printf("Window Options:\n");
	BLI_argsPrintArgDoc(ba, "--window-border");
	BLI_argsPrintArgDoc(ba, "--window-fullscreen");
	BLI_argsPrintArgDoc(ba, "--window-geometry");
	BLI_argsPrintArgDoc(ba, "--start-console");
	BLI_argsPrintArgDoc(ba, "--no-native-pixels");


	printf("\n");
	printf("Game Engine Specific Options:\n");
	BLI_argsPrintArgDoc(ba, "-g");

	printf("\n");
	printf("Python Options:\n");
	BLI_argsPrintArgDoc(ba, "--enable-autoexec");
	BLI_argsPrintArgDoc(ba, "--disable-autoexec");

	printf("\n");

	BLI_argsPrintArgDoc(ba, "--python");
	BLI_argsPrintArgDoc(ba, "--python-text");
	BLI_argsPrintArgDoc(ba, "--python-expr");
	BLI_argsPrintArgDoc(ba, "--python-console");
	BLI_argsPrintArgDoc(ba, "--python-exit-code");
	BLI_argsPrintArgDoc(ba, "--addons");

	printf("\n");
	printf("Logging Options:\n");
	BLI_argsPrintArgDoc(ba, "--log");
	BLI_argsPrintArgDoc(ba, "--log-level");
	BLI_argsPrintArgDoc(ba, "--log-show-basename");
	BLI_argsPrintArgDoc(ba, "--log-show-backtrace");
	BLI_argsPrintArgDoc(ba, "--log-file");

	printf("\n");
	printf("Debug Options:\n");
	BLI_argsPrintArgDoc(ba, "--debug");
	BLI_argsPrintArgDoc(ba, "--debug-value");

	printf("\n");
	BLI_argsPrintArgDoc(ba, "--debug-events");
#ifdef WITH_FFMPEG
	BLI_argsPrintArgDoc(ba, "--debug-ffmpeg");
#endif
	BLI_argsPrintArgDoc(ba, "--debug-handlers");
#ifdef WITH_LIBMV
	BLI_argsPrintArgDoc(ba, "--debug-libmv");
#endif
#ifdef WITH_CYCLES_LOGGING
	BLI_argsPrintArgDoc(ba, "--debug-cycles");
#endif
	BLI_argsPrintArgDoc(ba, "--debug-memory");
	BLI_argsPrintArgDoc(ba, "--debug-jobs");
	BLI_argsPrintArgDoc(ba, "--debug-python");
	BLI_argsPrintArgDoc(ba, "--debug-depsgraph");
	BLI_argsPrintArgDoc(ba, "--debug-depsgraph-eval");
	BLI_argsPrintArgDoc(ba, "--debug-depsgraph-build");
	BLI_argsPrintArgDoc(ba, "--debug-depsgraph-tag");
	BLI_argsPrintArgDoc(ba, "--debug-depsgraph-no-threads");

	BLI_argsPrintArgDoc(ba, "--debug-gpumem");
	BLI_argsPrintArgDoc(ba, "--debug-gpu-shaders");
	BLI_argsPrintArgDoc(ba, "--debug-wm");
	BLI_argsPrintArgDoc(ba, "--debug-all");
	BLI_argsPrintArgDoc(ba, "--debug-io");

	printf("\n");
	BLI_argsPrintArgDoc(ba, "--debug-fpe");
	BLI_argsPrintArgDoc(ba, "--disable-crash-handler");

	printf("\n");
	printf("Misc Options:\n");
	BLI_argsPrintArgDoc(ba, "--app-template");
	BLI_argsPrintArgDoc(ba, "--factory-startup");
	printf("\n");
	BLI_argsPrintArgDoc(ba, "--env-system-datafiles");
	BLI_argsPrintArgDoc(ba, "--env-system-scripts");
	BLI_argsPrintArgDoc(ba, "--env-system-python");
	printf("\n");
	BLI_argsPrintArgDoc(ba, "-nojoystick");
	BLI_argsPrintArgDoc(ba, "-noglsl");
	BLI_argsPrintArgDoc(ba, "-noaudio");
	BLI_argsPrintArgDoc(ba, "-setaudio");

	printf("\n");

	BLI_argsPrintArgDoc(ba, "--help");

#ifdef WIN32
	BLI_argsPrintArgDoc(ba, "-R");
	BLI_argsPrintArgDoc(ba, "-r");
#endif
	BLI_argsPrintArgDoc(ba, "--version");

	BLI_argsPrintArgDoc(ba, "--");

	printf("\n");
	printf("Experimental Features:\n");
	BLI_argsPrintArgDoc(ba, "--enable-new-depsgraph");
	BLI_argsPrintArgDoc(ba, "--enable-new-basic-shader-glsl");

	/* Other options _must_ be last (anything not handled will show here) */
	printf("\n");
	printf("Other Options:\n");
	BLI_argsPrintOtherDoc(ba);

	printf("\n");
	printf("Argument Parsing:\n");
	printf("\tArguments must be separated by white space, eg:\n");
	printf("\t# blender -ba test.blend\n");
	printf("\t...will ignore the 'a'.\n");
	printf("\t# blender -b test.blend -f8\n");
	printf("\t...will ignore '8' because there is no space between the '-f' and the frame value.\n\n");

	printf("Argument Order:\n");
	printf("\tArguments are executed in the order they are given. eg:\n");
	printf("\t# blender --background test.blend --render-frame 1 --render-output '/tmp'\n");
	printf("\t...will not render to '/tmp' because '--render-frame 1' renders before the output path is set.\n");
	printf("\t# blender --background --render-output /tmp test.blend --render-frame 1\n");
	printf("\t...will not render to '/tmp' because loading the blend-file overwrites the render output that was set.\n");
	printf("\t# blender --background test.blend --render-output /tmp --render-frame 1\n");
	printf("\t...works as expected.\n\n");

	printf("Environment Variables:\n");
	printf("  $BLENDER_USER_CONFIG      Directory for user configuration files.\n");
	printf("  $BLENDER_USER_SCRIPTS     Directory for user scripts.\n");
	printf("  $BLENDER_SYSTEM_SCRIPTS   Directory for system wide scripts.\n");
	printf("  $BLENDER_USER_DATAFILES   Directory for user data files (icons, translations, ..).\n");
	printf("  $BLENDER_SYSTEM_DATAFILES Directory for system wide data files.\n");
	printf("  $BLENDER_SYSTEM_PYTHON    Directory for system Python libraries.\n");
#ifdef WIN32
	printf("  $TEMP                     Store temporary files here.\n");
#else
	printf("  $TMP or $TMPDIR           Store temporary files here.\n");
#endif
#ifdef WITH_SDL
	printf("  $SDL_AUDIODRIVER          LibSDL audio driver - alsa, esd, dma.\n");
#endif
	printf("  $PYTHONHOME               Path to the Python directory, eg. /usr/lib/python.\n\n");

	exit(0);

	return 0;
}

static const char arg_handle_arguments_end_doc[] =
"\n\tEnd option processing, following arguments passed unchanged. Access via Python's 'sys.argv'."
;
static int arg_handle_arguments_end(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	return -1;
}

/* only to give help message */
#ifndef WITH_PYTHON_SECURITY /* default */
#  define   PY_ENABLE_AUTO ", (default)"
#  define   PY_DISABLE_AUTO ""
#else
#  define   PY_ENABLE_AUTO ""
#  define   PY_DISABLE_AUTO ", (compiled as non-standard default)"
#endif

static const char arg_handle_python_set_doc_enable[] =
"\n\tEnable automatic Python script execution" PY_ENABLE_AUTO "."
;
static const char arg_handle_python_set_doc_disable[] =
"\n\tDisable automatic Python script execution (pydrivers & startup scripts)" PY_DISABLE_AUTO "."
;
#undef PY_ENABLE_AUTO
#undef PY_DISABLE_AUTO

static int arg_handle_python_set(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
	if ((bool)data) {
		G.f |= G_SCRIPT_AUTOEXEC;
	}
	else {
		G.f &= ~G_SCRIPT_AUTOEXEC;
	}
	G.f |= G_SCRIPT_OVERRIDE_PREF;
	return 0;
}

static const char arg_handle_crash_handler_disable_doc[] =
"\n\tDisable the crash handler."
;
static int arg_handle_crash_handler_disable(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	app_state.signal.use_crash_handler = false;
	return 0;
}

static const char arg_handle_abort_handler_disable_doc[] =
"\n\tDisable the abort handler."
;
static int arg_handle_abort_handler_disable(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	app_state.signal.use_abort_handler = false;
	return 0;
}

static const char arg_handle_background_mode_set_doc[] =
"\n\tRun in background (often used for UI-less rendering)."
;
static int arg_handle_background_mode_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	G.background = 1;
	return 0;
}

static const char arg_handle_log_level_set_doc[] =
"<level>\n"
"\n"
"\tSet the logging verbosity level (higher for more details) defaults to 1, use -1 to log all levels."
;
static int arg_handle_log_level_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "--log-level";
	if (argc > 1) {
		const char *err_msg = NULL;
		if (!parse_int_clamp(argv[1], NULL, -1, INT_MAX, &G.log.level, &err_msg)) {
			printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
		}
		else {
			if (G.log.level == -1) {
				G.log.level = INT_MAX;
			}
			CLG_level_set(G.log.level);
		}
		return 1;
	}
	else {
		printf("\nError: '%s' no args given.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_log_show_basename_set_doc[] =
"\n\tOnly show file name in output (not the leading path)."
;
static int arg_handle_log_show_basename_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	CLG_output_use_basename_set(true);
	return 0;
}

static const char arg_handle_log_show_backtrace_set_doc[] =
"\n\tShow a back trace for each log message (debug builds only)."
;
static int arg_handle_log_show_backtrace_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	/* Ensure types don't become incompatible. */
	void (*fn)(FILE *fp) = BLI_system_backtrace;
	CLG_backtrace_fn_set((void (*)(void *))fn);
	return 0;
}

static const char arg_handle_log_file_set_doc[] =
"<filename>\n"
"\n"
"\tSet a file to output the log to."
;
static int arg_handle_log_file_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "--log-file";
	if (argc > 1) {
		errno = 0;
		FILE *fp = BLI_fopen(argv[1], "w");
		if (fp == NULL) {
			const char *err_msg = errno ? strerror(errno) : "unknown";
			printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
		}
		else {
			if (UNLIKELY(G.log.file != NULL)) {
				fclose(G.log.file);
			}
			G.log.file = fp;
			CLG_output_set(G.log.file);
		}
		return 1;
	}
	else {
		printf("\nError: '%s' no args given.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_log_set_doc[] =
"<match>\n"
"\tEnable logging categories, taking a single comma separated argument.\n"
"\tMultiple categories can be matched using a '.*' suffix,\n"
"\tso '--log \"wm.*\"' logs every kind of window-manager message.\n"
"\tUse \"^\" prefix to ignore, so '--log \"*,^wm.operator.*\"' logs all except for 'wm.operators.*'\n"
"\tUse \"*\" to log everything."
;
static int arg_handle_log_set(int argc, const char **argv, void *UNUSED(data))
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
				/* typically only be one, but don't fail on multiple.*/
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
	else {
		printf("\nError: '%s' no args given.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_debug_mode_set_doc[] =
"\n"
"\tTurn debugging on.\n"
"\n"
"\t* Enables memory error detection\n"
"\t* Disables mouse grab (to interact with a debugger in some cases)\n"
"\t* Keeps Python's 'sys.stdin' rather than setting it to None"
;
static int arg_handle_debug_mode_set(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
	G.debug |= G_DEBUG;  /* std output printf's */
	printf(BLEND_VERSION_STRING_FMT);
	MEM_set_memory_debug();
#ifndef NDEBUG
	BLI_mempool_set_memory_debug();
#endif

#ifdef WITH_BUILDINFO
	printf("Build: %s %s %s %s\n", build_date, build_time, build_platform, build_type);
#endif

	BLI_argsPrint(data);
	return 0;
}

#ifdef WITH_FFMPEG
static const char arg_handle_debug_mode_generic_set_doc_ffmpeg[] =
"\n\tEnable debug messages from FFmpeg library.";
#endif
#ifdef WITH_FREESTYLE
static const char arg_handle_debug_mode_generic_set_doc_freestyle[] =
"\n\tEnable debug messages for FreeStyle.";
#endif
static const char arg_handle_debug_mode_generic_set_doc_python[] =
"\n\tEnable debug messages for Python.";
static const char arg_handle_debug_mode_generic_set_doc_events[] =
"\n\tEnable debug messages for the event system.";
static const char arg_handle_debug_mode_generic_set_doc_handlers[] =
"\n\tEnable debug messages for event handling.";
static const char arg_handle_debug_mode_generic_set_doc_wm[] =
"\n\tEnable debug messages for the window manager, also prints every operator call.";
static const char arg_handle_debug_mode_generic_set_doc_jobs[] =
"\n\tEnable time profiling for background jobs.";
static const char arg_handle_debug_mode_generic_set_doc_gpu[] =
"\n\tEnable gpu debug context and information for OpenGL 4.3+.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph[] =
"\n\tEnable all debug messages from dependency graph.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_build[] =
"\n\tEnable debug messages from dependency graph related on graph construction.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_tag[] =
"\n\tEnable debug messages from dependency graph related on tagging.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_time[] =
"\n\tEnable debug messages from dependency graph related on timing.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_eval[] =
"\n\tEnable debug messages from dependency graph related on evaluation.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_no_threads[] =
"\n\tSwitch dependency graph to a single threaded evaluation.";
static const char arg_handle_debug_mode_generic_set_doc_depsgraph_pretty[] =
"\n\tEnable colors for dependency graph debug messages.";
static const char arg_handle_debug_mode_generic_set_doc_gpumem[] =
"\n\tEnable GPU memory stats in status bar.";

static int arg_handle_debug_mode_generic_set(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
	G.debug |= GET_INT_FROM_POINTER(data);
	return 0;
}

static const char arg_handle_debug_mode_io_doc[] =
"\n\tEnable debug messages for I/O (collada, ...).";
static int arg_handle_debug_mode_io(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	G.debug |= G_DEBUG_IO;
	return 0;
}

static const char arg_handle_debug_mode_all_doc[] =
"\n\tEnable all debug messages.";
static int arg_handle_debug_mode_all(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	G.debug |= G_DEBUG_ALL;
#ifdef WITH_LIBMV
	libmv_startDebugLogging();
#endif
#ifdef WITH_CYCLES_LOGGING
	CCL_start_debug_logging();
#endif
	return 0;
}

#ifdef WITH_LIBMV
static const char arg_handle_debug_mode_libmv_doc[] =
"\n\tEnable debug messages from libmv library."
;
static int arg_handle_debug_mode_libmv(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	libmv_startDebugLogging();

	return 0;
}
#endif

#ifdef WITH_CYCLES_LOGGING
static const char arg_handle_debug_mode_cycles_doc[] =
"\n\tEnable debug messages from Cycles."
;
static int arg_handle_debug_mode_cycles(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	CCL_start_debug_logging();
	return 0;
}
#endif

static const char arg_handle_debug_mode_memory_set_doc[] =
"\n\tEnable fully guarded memory allocation and debugging."
;
static int arg_handle_debug_mode_memory_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	MEM_set_memory_debug();
	return 0;
}

static const char arg_handle_debug_value_set_doc[] =
"<value>\n"
"\tSet debug value of <value> on startup."
;
static int arg_handle_debug_value_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "--debug-value";
	if (argc > 1) {
		const char *err_msg = NULL;
		int value;
		if (!parse_int(argv[1], NULL, &value, &err_msg)) {
			printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
			return 1;
		}

		G.debug_value = value;

		return 1;
	}
	else {
		printf("\nError: you must specify debug value to set.\n");
		return 0;
	}
}

static const char arg_handle_debug_fpe_set_doc[] =
"\n\tEnable floating point exceptions."
;
static int arg_handle_debug_fpe_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	main_signal_setup_fpe();
	return 0;
}

static const char arg_handle_app_template_doc[] =
"\n\tSet the application template, use 'default' for none."
;
static int arg_handle_app_template(int argc, const char **argv, void *UNUSED(data))
{
	if (argc > 1) {
		const char *app_template = STREQ(argv[1], "default") ? "" : argv[1];
		WM_init_state_app_template_set(app_template);
		return 1;
	}
	else {
		printf("\nError: App template must follow '--app-template'.\n");
		return 0;
	}
}

static const char arg_handle_factory_startup_set_doc[] =
"\n\tSkip reading the " STRINGIFY(BLENDER_STARTUP_FILE) " in the users home directory."
;
static int arg_handle_factory_startup_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	G.factory_startup = 1;
	return 0;
}

static const char arg_handle_env_system_set_doc_datafiles[] =
"\n\tSet the "STRINGIFY_ARG (BLENDER_SYSTEM_DATAFILES)" environment variable.";
static const char arg_handle_env_system_set_doc_scripts[] =
"\n\tSet the "STRINGIFY_ARG (BLENDER_SYSTEM_SCRIPTS)" environment variable.";
static const char arg_handle_env_system_set_doc_python[] =
"\n\tSet the "STRINGIFY_ARG (BLENDER_SYSTEM_PYTHON)" environment variable.";

static int arg_handle_env_system_set(int argc, const char **argv, void *UNUSED(data))
{
	/* "--env-system-scripts" --> "BLENDER_SYSTEM_SCRIPTS" */

	char env[64] = "BLENDER";
	char *ch_dst = env + 7; /* skip BLENDER */
	const char *ch_src = argv[0] + 5; /* skip --env */

	if (argc < 2) {
		printf("%s requires one argument\n", argv[0]);
		exit(1);
	}

	for (; *ch_src; ch_src++, ch_dst++) {
		*ch_dst = (*ch_src == '-') ? '_' : (*ch_src) - 32; /* toupper() */
	}

	*ch_dst = '\0';
	BLI_setenv(env, argv[1]);
	return 1;
}

static const char arg_handle_playback_mode_doc[] =
"<options> <file(s)>\n"
"\tPlayback <file(s)>, only operates this way when not running in background.\n\n"
"\t-p <sx> <sy>\n"
"\t\tOpen with lower left corner at <sx>, <sy>.\n"
"\t-m\n"
"\t\tRead from disk (Do not buffer).\n"
"\t-f <fps> <fps-base>\n"
"\t\tSpecify FPS to start with.\n"
"\t-j <frame>\n"
"\t\tSet frame step to <frame>.\n"
"\t-s <frame>\n"
"\t\tPlay from <frame>.\n"
"\t-e <frame>\n"
"\t\tPlay until <frame>."
;
static int arg_handle_playback_mode(int argc, const char **argv, void *UNUSED(data))
{
	/* not if -b was given first */
	if (G.background == 0) {
#ifdef WITH_FFMPEG
		/* Setup FFmpeg with current debug flags. */
		IMB_ffmpeg_init();
#endif

		WM_main_playanim(argc, argv); /* not the same argc and argv as before */
		exit(0); /* 2.4x didn't do this */
	}

	return -2;
}

static const char arg_handle_window_geometry_doc[] =
"<sx> <sy> <w> <h>\n"
"\tOpen with lower left corner at <sx>, <sy> and width and height as <w>, <h>."
;
static int arg_handle_window_geometry(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "-p / --window-geometry";
	int params[4], i;

	if (argc < 5) {
		fprintf(stderr, "Error: requires four arguments '%s'\n", arg_id);
		exit(1);
	}

	for (i = 0; i < 4; i++) {
		const char *err_msg = NULL;
		if (!parse_int(argv[i + 1], NULL, &params[i], &err_msg)) {
			printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
			exit(1);
		}
	}

	WM_init_state_size_set(UNPACK4(params));

	return 4;
}

static const char arg_handle_native_pixels_set_doc[] =
"\n\tDo not use native pixel size, for high resolution displays (MacBook 'Retina')."
;
static int arg_handle_native_pixels_set(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	WM_init_native_pixels(false);
	return 0;
}

static const char arg_handle_with_borders_doc[] =
"\n\tForce opening with borders."
;
static int arg_handle_with_borders(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	WM_init_state_normal_set();
	return 0;
}

static const char arg_handle_without_borders_doc[] =
"\n\tForce opening in fullscreen mode."
;
static int arg_handle_without_borders(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	WM_init_state_fullscreen_set();
	return 0;
}

extern bool wm_start_with_console; /* wm_init_exit.c */

static const char arg_handle_start_with_console_doc[] =
"\n\tStart with the console window open (ignored if -b is set), (Windows only)."
;
static int arg_handle_start_with_console(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	wm_start_with_console = true;
	return 0;
}

static const char arg_handle_register_extension_doc[] =
"\n\tRegister blend-file extension, then exit (Windows only)."
;
static const char arg_handle_register_extension_doc_silent[] =
"\n\tSilently register blend-file extension, then exit (Windows only)."
;
static int arg_handle_register_extension(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
#ifdef WIN32
	if (data)
		G.background = 1;
	RegisterBlendExtension();
#else
	(void)data; /* unused */
#endif
	return 0;
}

static const char arg_handle_joystick_disable_doc[] =
"\n\tDisable joystick support."
;
static int arg_handle_joystick_disable(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
#ifndef WITH_GAMEENGINE
	(void)data;
#else
	SYS_SystemHandle *syshandle = data;

	/**
	 * don't initialize joysticks if user doesn't want to use joysticks
	 * failed joystick initialization delays over 5 seconds, before game engine start
	 */
	SYS_WriteCommandLineInt(*syshandle, "nojoystick", 1);
	if (G.debug & G_DEBUG) printf("disabling nojoystick\n");
#endif

	return 0;
}

static const char arg_handle_glsl_disable_doc[] =
"\n\tDisable GLSL shading."
;
static int arg_handle_glsl_disable(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	GPU_extensions_disable();
	return 0;
}

static const char arg_handle_audio_disable_doc[] =
"\n\tForce sound system to None."
;
static int arg_handle_audio_disable(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	BKE_sound_force_device("Null");
	return 0;
}

static const char arg_handle_audio_set_doc[] =
"\n\tForce sound system to a specific device.\n\t'NULL' 'SDL' 'OPENAL' 'JACK'."
;
static int arg_handle_audio_set(int argc, const char **argv, void *UNUSED(data))
{
	if (argc < 1) {
		fprintf(stderr, "-setaudio require one argument\n");
		exit(1);
	}

	BKE_sound_force_device(argv[1]);
	return 1;
}

static const char arg_handle_output_set_doc[] =
"<path>\n"
"\tSet the render path and file name.\n"
"\tUse '//' at the start of the path to render relative to the blend-file.\n"
"\n"
"\tThe '#' characters are replaced by the frame number, and used to define zero padding.\n"
"\n"
"\t* 'ani_##_test.png' becomes 'ani_01_test.png'\n"
"\t* 'test-######.png' becomes 'test-000001.png'\n"
"\n"
"\tWhen the filename does not contain '#', The suffix '####' is added to the filename.\n"
"\n"
"\tThe frame number will be added at the end of the filename, eg:\n"
"\t# blender -b foobar.blend -o //render_ -F PNG -x 1 -a\n"
"\t'//render_' becomes '//render_####', writing frames as '//render_0001.png'"
;
static int arg_handle_output_set(int argc, const char **argv, void *data)
{
	bContext *C = data;
	if (argc > 1) {
		Scene *scene = CTX_data_scene(C);
		if (scene) {
			BLI_strncpy(scene->r.pic, argv[1], sizeof(scene->r.pic));
		}
		else {
			printf("\nError: no blend loaded. cannot use '-o / --render-output'.\n");
		}
		return 1;
	}
	else {
		printf("\nError: you must specify a path after '-o  / --render-output'.\n");
		return 0;
	}
}

static const char arg_handle_engine_set_doc[] =
"<engine>\n"
"\tSpecify the render engine.\n\tUse -E help to list available engines."
;
static int arg_handle_engine_set(int argc, const char **argv, void *data)
{
	bContext *C = data;
	if (argc >= 2) {
		if (STREQ(argv[1], "help")) {
			RenderEngineType *type = NULL;
			printf("Blender Engine Listing:\n");
			for (type = R_engines.first; type; type = type->next) {
				printf("\t%s\n", type->idname);
			}
			exit(0);
		}
		else {
			Scene *scene = CTX_data_scene(C);
			if (scene) {
				RenderData *rd = &scene->r;

				if (BLI_findstring(&R_engines, argv[1], offsetof(RenderEngineType, idname))) {
					BLI_strncpy_utf8(rd->engine, argv[1], sizeof(rd->engine));
				}
				else {
					printf("\nError: engine not found '%s'\n", argv[1]);
					exit(1);
				}
			}
			else {
				printf("\nError: no blend loaded. "
				       "order the arguments so '-E  / --engine ' is after a blend is loaded.\n");
			}
		}

		return 1;
	}
	else {
		printf("\nEngine not specified, give 'help' for a list of available engines.\n");
		return 0;
	}
}

static const char arg_handle_image_type_set_doc[] =
"<format>\n"
"\tSet the render format.\n"
"\tValid options are 'TGA' 'RAWTGA' 'JPEG' 'IRIS' 'IRIZ' 'AVIRAW' 'AVIJPEG' 'PNG' 'BMP'\n"
"\n"
"\tFormats that can be compiled into Blender, not available on all systems: 'HDR' 'TIFF' 'EXR' 'MULTILAYER'\n"
"\t'MPEG' 'FRAMESERVER' 'CINEON' 'DPX' 'DDS' 'JP2'"
;
static int arg_handle_image_type_set(int argc, const char **argv, void *data)
{
	bContext *C = data;
	if (argc > 1) {
		const char *imtype = argv[1];
		Scene *scene = CTX_data_scene(C);
		if (scene) {
			const char imtype_new = BKE_imtype_from_arg(imtype);

			if (imtype_new == R_IMF_IMTYPE_INVALID) {
				printf("\nError: Format from '-F / --render-format' not known or not compiled in this release.\n");
			}
			else {
				scene->r.im_format.imtype = imtype_new;
			}
		}
		else {
			printf("\nError: no blend loaded. "
			       "order the arguments so '-F  / --render-format' is after the blend is loaded.\n");
		}
		return 1;
	}
	else {
		printf("\nError: you must specify a format after '-F  / --render-foramt'.\n");
		return 0;
	}
}

static const char arg_handle_threads_set_doc[] =
"<threads>\n"
"\tUse amount of <threads> for rendering and other operations\n"
"\t[1-" STRINGIFY(BLENDER_MAX_THREADS) "], 0 for systems processor count."
;
static int arg_handle_threads_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "-t / --threads";
	const int min = 0, max = BLENDER_MAX_THREADS;
	if (argc > 1) {
		const char *err_msg = NULL;
		int threads;
		if (!parse_int_strict_range(argv[1], NULL, min, max, &threads, &err_msg)) {
			printf("\nError: %s '%s %s', expected number in [%d..%d].\n", err_msg, arg_id, argv[1], min, max);
			return 1;
		}

		BLI_system_num_threads_override_set(threads);
		return 1;
	}
	else {
		printf("\nError: you must specify a number of threads in [%d..%d] '%s'.\n", min, max, arg_id);
		return 0;
	}
}

static const char arg_handle_depsgraph_use_new_doc[] =
"\n\tUse new dependency graph."
;
static int arg_handle_depsgraph_use_new(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	printf("Using new dependency graph.\n");
	DEG_depsgraph_switch_to_new();
	return 0;
}

static const char arg_handle_basic_shader_glsl_use_new_doc[] =
"\n\tUse new GLSL basic shader."
;
static int arg_handle_basic_shader_glsl_use_new(int UNUSED(argc), const char **UNUSED(argv), void *UNUSED(data))
{
	printf("Using new GLSL basic shader.\n");
	GPU_basic_shader_use_glsl_set(true);
	return 0;
}

static const char arg_handle_verbosity_set_doc[] =
"<verbose>\n"
"\tSet logging verbosity level."
;
static int arg_handle_verbosity_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "--verbose";
	if (argc > 1) {
		const char *err_msg = NULL;
		int level;
		if (!parse_int(argv[1], NULL, &level, &err_msg)) {
			printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
		}

#ifdef WITH_LIBMV
		libmv_setLoggingVerbosity(level);
#elif defined(WITH_CYCLES_LOGGING)
		CCL_logging_verbosity_set(level);
#else
		(void)level;
#endif

		return 1;
	}
	else {
		printf("\nError: you must specify a verbosity level.\n");
		return 0;
	}
}

static const char arg_handle_extension_set_doc[] =
"<bool>\n"
"\tSet option to add the file extension to the end of the file."
;
static int arg_handle_extension_set(int argc, const char **argv, void *data)
{
	bContext *C = data;
	if (argc > 1) {
		Scene *scene = CTX_data_scene(C);
		if (scene) {
			if (argv[1][0] == '0') {
				scene->r.scemode &= ~R_EXTENSION;
			}
			else if (argv[1][0] == '1') {
				scene->r.scemode |= R_EXTENSION;
			}
			else {
				printf("\nError: Use '-x 1 / -x 0' To set the extension option or '--use-extension'\n");
			}
		}
		else {
			printf("\nError: no blend loaded. "
			       "order the arguments so '-o ' is after '-x '.\n");
		}
		return 1;
	}
	else {
		printf("\nError: you must specify a path after '- '.\n");
		return 0;
	}
}

static const char arg_handle_ge_parameters_set_doc[] =
"Game Engine specific options\n"
"\n"
"\t'fixedtime'\n"
"\t\tRun on 50 hertz without dropping frames.\n"
"\t'vertexarrays'\n"
"\t\tUse Vertex Arrays for rendering (usually faster).\n"
"\t'nomipmap'\n"
"\t\tNo Texture Mipmapping.\n"
"\t'linearmipmap'\n"
"\t\tLinear Texture Mipmapping instead of Nearest (default)."
;
static int arg_handle_ge_parameters_set(int argc, const char **argv, void *data)
{
	int a = 0;
#ifdef WITH_GAMEENGINE
	SYS_SystemHandle syshandle = *(SYS_SystemHandle *)data;
#else
	(void)data;
#endif

	/**
	 * gameengine parameters are automatically put into system
	 * -g [paramname = value]
	 * -g [boolparamname]
	 * example:
	 * -g novertexarrays
	 * -g maxvertexarraysize = 512
	 */

	if (argc >= 1) {
		const char *paramname = argv[a];
		/* check for single value versus assignment */
		if (a + 1 < argc && (*(argv[a + 1]) == '=')) {
			a++;
			if (a + 1 < argc) {
				a++;
				/* assignment */
#ifdef WITH_GAMEENGINE
				SYS_WriteCommandLineString(syshandle, paramname, argv[a]);
#endif
			}
			else {
				printf("Error: argument assignment (%s) without value.\n", paramname);
				return 0;
			}
			/* name arg eaten */

		}
		else {
#ifdef WITH_GAMEENGINE
			SYS_WriteCommandLineInt(syshandle, argv[a], 1);
#endif
			/* doMipMap */
			if (STREQ(argv[a], "nomipmap")) {
				GPU_set_mipmap(G_MAIN, 0); //doMipMap = 0;
			}
			/* linearMipMap */
			if (STREQ(argv[a], "linearmipmap")) {
				GPU_set_mipmap(G_MAIN, 1);
				GPU_set_linear_mipmap(1); //linearMipMap = 1;
			}


		} /* if (*(argv[a + 1]) == '=') */
	}

	return a;
}

static const char arg_handle_render_frame_doc[] =
"<frame>\n"
"\tRender frame <frame> and save it.\n"
"\n"
"\t* +<frame> start frame relative, -<frame> end frame relative.\n"
"\t* A comma separated list of frames can also be used (no spaces).\n"
"\t* A range of frames can be expressed using '..' separator between the first and last frames (inclusive).\n"
;
static int arg_handle_render_frame(int argc, const char **argv, void *data)
{
	const char *arg_id = "-f / --render-frame";
	bContext *C = data;
	Scene *scene = CTX_data_scene(C);
	if (scene) {
		Main *bmain = CTX_data_main(C);

		if (argc > 1) {
			const char *err_msg = NULL;
			Render *re;
			ReportList reports;

			int (*frame_range_arr)[2], frames_range_len;
			if ((frame_range_arr = parse_int_range_relative_clamp_n(
			         argv[1], scene->r.sfra, scene->r.efra, MINAFRAME, MAXFRAME,
			         &frames_range_len, &err_msg)) == NULL)
			{
				printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
				return 1;
			}

			re = RE_NewSceneRender(scene);
			BLI_threaded_malloc_begin();
			BKE_reports_init(&reports, RPT_STORE);

			RE_SetReports(re, &reports);
			for (int i = 0; i < frames_range_len; i++) {
				/* We could pass in frame ranges,
				 * but prefer having exact behavior as passing in multiple frames */
				if ((frame_range_arr[i][0] <= frame_range_arr[i][1]) == 0) {
					printf("\nWarning: negative range ignored '%s %s'.\n", arg_id, argv[1]);
				}

				for (int frame = frame_range_arr[i][0]; frame <= frame_range_arr[i][1]; frame++) {
					RE_BlenderAnim(re, bmain, scene, NULL, scene->lay, frame, frame, scene->r.frame_step);
				}
			}
			RE_SetReports(re, NULL);
			BKE_reports_clear(&reports);
			BLI_threaded_malloc_end();
			MEM_freeN(frame_range_arr);
			return 1;
		}
		else {
			printf("\nError: frame number must follow '%s'.\n", arg_id);
			return 0;
		}
	}
	else {
		printf("\nError: no blend loaded. cannot use '%s'.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_render_animation_doc[] =
"\n\tRender frames from start to end (inclusive)."
;
static int arg_handle_render_animation(int UNUSED(argc), const char **UNUSED(argv), void *data)
{
	bContext *C = data;
	Scene *scene = CTX_data_scene(C);
	if (scene) {
		Main *bmain = CTX_data_main(C);
		Render *re = RE_NewSceneRender(scene);
		ReportList reports;
		BLI_threaded_malloc_begin();
		BKE_reports_init(&reports, RPT_STORE);
		RE_SetReports(re, &reports);
		RE_BlenderAnim(re, bmain, scene, NULL, scene->lay, scene->r.sfra, scene->r.efra, scene->r.frame_step);
		RE_SetReports(re, NULL);
		BKE_reports_clear(&reports);
		BLI_threaded_malloc_end();
	}
	else {
		printf("\nError: no blend loaded. cannot use '-a'.\n");
	}
	return 0;
}

static const char arg_handle_scene_set_doc[] =
"<name>\n"
"\tSet the active scene <name> for rendering."
;
static int arg_handle_scene_set(int argc, const char **argv, void *data)
{
	if (argc > 1) {
		bContext *C = data;
		Scene *scene = BKE_scene_set_name(CTX_data_main(C), argv[1]);
		if (scene) {
			CTX_data_scene_set(C, scene);

			/* Set the scene of the first window, see: T55991,
			 * otherwise scrips that run later won't get this scene back from the context. */
			wmWindow *win = CTX_wm_window(C);
			if (win == NULL) {
				win = CTX_wm_manager(C)->windows.first;
			}
			if (win != NULL) {
				win->screen->scene = scene;
			}
		}
		return 1;
	}
	else {
		printf("\nError: Scene name must follow '-S / --scene'.\n");
		return 0;
	}
}

static const char arg_handle_frame_start_set_doc[] =
"<frame>\n"
"\tSet start to frame <frame>, supports +/- for relative frames too."
;
static int arg_handle_frame_start_set(int argc, const char **argv, void *data)
{
	const char *arg_id = "-s / --frame-start";
	bContext *C = data;
	Scene *scene = CTX_data_scene(C);
	if (scene) {
		if (argc > 1) {
			const char *err_msg = NULL;
			if (!parse_int_relative_clamp(
			        argv[1], NULL, scene->r.sfra, scene->r.sfra - 1, MINAFRAME, MAXFRAME,
			        &scene->r.sfra, &err_msg))
			{
				printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
			}
			return 1;
		}
		else {
			printf("\nError: frame number must follow '%s'.\n", arg_id);
			return 0;
		}
	}
	else {
		printf("\nError: no blend loaded. cannot use '%s'.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_frame_end_set_doc[] =
"<frame>\n"
"\tSet end to frame <frame>, supports +/- for relative frames too."
;
static int arg_handle_frame_end_set(int argc, const char **argv, void *data)
{
	const char *arg_id = "-e / --frame-end";
	bContext *C = data;
	Scene *scene = CTX_data_scene(C);
	if (scene) {
		if (argc > 1) {
			const char *err_msg = NULL;
			if (!parse_int_relative_clamp(
			        argv[1], NULL, scene->r.efra, scene->r.efra - 1, MINAFRAME, MAXFRAME,
			        &scene->r.efra, &err_msg))
			{
				printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
			}
			return 1;
		}
		else {
			printf("\nError: frame number must follow '%s'.\n", arg_id);
			return 0;
		}
	}
	else {
		printf("\nError: no blend loaded. cannot use '%s'.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_frame_skip_set_doc[] =
"<frames>\n"
"\tSet number of frames to step forward after each rendered frame."
;
static int arg_handle_frame_skip_set(int argc, const char **argv, void *data)
{
	const char *arg_id = "-j / --frame-jump";
	bContext *C = data;
	Scene *scene = CTX_data_scene(C);
	if (scene) {
		if (argc > 1) {
			const char *err_msg = NULL;
			if (!parse_int_clamp(argv[1], NULL, 1, MAXFRAME, &scene->r.frame_step, &err_msg)) {
				printf("\nError: %s '%s %s'.\n", err_msg, arg_id, argv[1]);
			}
			return 1;
		}
		else {
			printf("\nError: number of frames to step must follow '%s'.\n", arg_id);
			return 0;
		}
	}
	else {
		printf("\nError: no blend loaded. cannot use '%s'.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_python_file_run_doc[] =
"<filename>\n"
"\tRun the given Python script file."
;
static int arg_handle_python_file_run(int argc, const char **argv, void *data)
{
#ifdef WITH_PYTHON
	bContext *C = data;

	/* workaround for scripts not getting a bpy.context.scene, causes internal errors elsewhere */
	if (argc > 1) {
		/* Make the path absolute because its needed for relative linked blends to be found */
		char filename[FILE_MAX];
		BLI_strncpy(filename, argv[1], sizeof(filename));
		BLI_path_cwd(filename, sizeof(filename));

		bool ok;
		BPY_CTX_SETUP(ok = BPY_execute_filepath(C, filename, NULL));
		if (!ok && app_state.exit_code_on_error.python) {
			printf("\nError: script failed, file: '%s', exiting.\n", argv[1]);
			exit(app_state.exit_code_on_error.python);
		}
		return 1;
	}
	else {
		printf("\nError: you must specify a filepath after '%s'.\n", argv[0]);
		return 0;
	}
#else
	UNUSED_VARS(argc, argv, data);
	printf("This Blender was built without Python support\n");
	return 0;
#endif /* WITH_PYTHON */
}

static const char arg_handle_python_text_run_doc[] =
"<name>\n"
"\tRun the given Python script text block."
;
static int arg_handle_python_text_run(int argc, const char **argv, void *data)
{
#ifdef WITH_PYTHON
	bContext *C = data;

	/* workaround for scripts not getting a bpy.context.scene, causes internal errors elsewhere */
	if (argc > 1) {
		Main *bmain = CTX_data_main(C);
		/* Make the path absolute because its needed for relative linked blends to be found */
		struct Text *text = (struct Text *)BKE_libblock_find_name(bmain, ID_TXT, argv[1]);
		bool ok;

		if (text) {
			BPY_CTX_SETUP(ok = BPY_execute_text(C, text, NULL, false));
		}
		else {
			printf("\nError: text block not found %s.\n", argv[1]);
			ok = false;
		}

		if (!ok && app_state.exit_code_on_error.python) {
			printf("\nError: script failed, text: '%s', exiting.\n", argv[1]);
			exit(app_state.exit_code_on_error.python);
		}

		return 1;
	}
	else {
		printf("\nError: you must specify a text block after '%s'.\n", argv[0]);
		return 0;
	}
#else
	UNUSED_VARS(argc, argv, data);
	printf("This Blender was built without Python support\n");
	return 0;
#endif /* WITH_PYTHON */
}

static const char arg_handle_python_expr_run_doc[] =
"<expression>\n"
"\tRun the given expression as a Python script."
;
static int arg_handle_python_expr_run(int argc, const char **argv, void *data)
{
#ifdef WITH_PYTHON
	bContext *C = data;

	/* workaround for scripts not getting a bpy.context.scene, causes internal errors elsewhere */
	if (argc > 1) {
		bool ok;
		BPY_CTX_SETUP(ok = BPY_execute_string_ex(C, argv[1], false));
		if (!ok && app_state.exit_code_on_error.python) {
			printf("\nError: script failed, expr: '%s', exiting.\n", argv[1]);
			exit(app_state.exit_code_on_error.python);
		}
		return 1;
	}
	else {
		printf("\nError: you must specify a Python expression after '%s'.\n", argv[0]);
		return 0;
	}
#else
	UNUSED_VARS(argc, argv, data);
	printf("This Blender was built without Python support\n");
	return 0;
#endif /* WITH_PYTHON */
}

static const char arg_handle_python_console_run_doc[] =
"\n\tRun Blender with an interactive console."
;
static int arg_handle_python_console_run(int UNUSED(argc), const char **argv, void *data)
{
#ifdef WITH_PYTHON
	bContext *C = data;

	BPY_CTX_SETUP(BPY_execute_string(C, "__import__('code').interact()"));

	return 0;
#else
	UNUSED_VARS(argv, data);
	printf("This Blender was built without python support\n");
	return 0;
#endif /* WITH_PYTHON */
}

static const char arg_handle_python_exit_code_set_doc[] =
"<code>\n"
"\tSet the exit-code in [0..255] to exit if a Python exception is raised\n"
"\t(only for scripts executed from the command line), zero disables."
;
static int arg_handle_python_exit_code_set(int argc, const char **argv, void *UNUSED(data))
{
	const char *arg_id = "--python-exit-code";
	if (argc > 1) {
		const char *err_msg = NULL;
		const int min = 0, max = 255;
		int exit_code;
		if (!parse_int_strict_range(argv[1], NULL, min, max, &exit_code, &err_msg)) {
			printf("\nError: %s '%s %s', expected number in [%d..%d].\n", err_msg, arg_id, argv[1], min, max);
			return 1;
		}

		app_state.exit_code_on_error.python = (unsigned char)exit_code;
		return 1;
	}
	else {
		printf("\nError: you must specify an exit code number '%s'.\n", arg_id);
		return 0;
	}
}

static const char arg_handle_addons_set_doc[] =
"<addon(s)>\n"
"\tComma separated list of add-ons (no spaces)."
;
static int arg_handle_addons_set(int argc, const char **argv, void *data)
{
	/* workaround for scripts not getting a bpy.context.scene, causes internal errors elsewhere */
	if (argc > 1) {
#ifdef WITH_PYTHON
		const char script_str[] =
		        "from addon_utils import check, enable\n"
		        "for m in '%s'.split(','):\n"
		        "    if check(m)[1] is False:\n"
		        "        enable(m, persistent=True)";
		const int slen = strlen(argv[1]) + (sizeof(script_str) - 2);
		char *str = malloc(slen);
		bContext *C = data;
		BLI_snprintf(str, slen, script_str, argv[1]);

		BLI_assert(strlen(str) + 1 == slen);
		BPY_CTX_SETUP(BPY_execute_string_ex(C, str, false));
		free(str);
#else
		UNUSED_VARS(argv, data);
#endif /* WITH_PYTHON */
		return 1;
	}
	else {
		printf("\nError: you must specify a comma separated list after '--addons'.\n");
		return 0;
	}
}

static int arg_handle_load_file(int UNUSED(argc), const char **argv, void *data)
{
	bContext *C = data;
	ReportList reports;
	bool success;

	/* Make the path absolute because its needed for relative linked blends to be found */
	char filename[FILE_MAX];

	/* note, we could skip these, but so far we always tried to load these files */
	if (argv[0][0] == '-') {
		fprintf(stderr, "unknown argument, loading as file: %s\n", argv[0]);
	}

	BLI_strncpy(filename, argv[0], sizeof(filename));
	BLI_path_cwd(filename, sizeof(filename));

	/* load the file */
	BKE_reports_init(&reports, RPT_PRINT);
	WM_file_autoexec_init(filename);
	success = WM_file_read(C, filename, &reports);
	BKE_reports_clear(&reports);

	if (success) {
		if (G.background) {
			/* ensuer we use 'C->data.scene' for background render */
			CTX_wm_window_set(C, NULL);
		}
	}
	else {
		/* failed to load file, stop processing arguments if running in background mode */
		if (G.background) {
			/* Set is_break if running in the background mode so
			 * blender will return non-zero exit code which then
			 * could be used in automated script to control how
			 * good or bad things are.
			 */
			G.is_break = true;
			return -1;
		}

		if (BLO_has_bfile_extension(filename)) {
			/* Just pretend a file was loaded, so the user can press Save and it'll save at the filename from the CLI. */
			BLI_strncpy(G_MAIN->name, filename, FILE_MAX);
			G.relbase_valid = true;
			G.save_over = true;
			printf("... opened default scene instead; saving will write to: %s\n", filename);
		}
		else {
			printf("Error: argument has no '.blend' file extension, not using as new file, exiting! %s\n", filename);
			G.is_break = true;
			WM_exit(C);
		}
	}

	G.file_loaded = 1;

	return 0;
}


void main_args_setup(bContext *C, bArgs *ba, SYS_SystemHandle *syshandle)
{

#define CB(a) a##_doc, a
#define CB_EX(a, b) a##_doc_##b, a

	//BLI_argsAdd(ba, pass, short_arg, long_arg, doc, cb, C);

	/* end argument processing after -- */
	BLI_argsAdd(ba, -1, "--", NULL, CB(arg_handle_arguments_end), NULL);

	/* first pass: background mode, disable python and commands that exit after usage */
	BLI_argsAdd(ba, 1, "-h", "--help", CB(arg_handle_print_help), ba);
	/* Windows only */
	BLI_argsAdd(ba, 1, "/?", NULL, CB_EX(arg_handle_print_help, win32), ba);

	BLI_argsAdd(ba, 1, "-v", "--version", CB(arg_handle_print_version), NULL);

	BLI_argsAdd(ba, 1, "-y", "--enable-autoexec", CB_EX(arg_handle_python_set, enable), (void *)true);
	BLI_argsAdd(ba, 1, "-Y", "--disable-autoexec", CB_EX(arg_handle_python_set, disable), (void *)false);

	BLI_argsAdd(ba, 1, NULL, "--disable-crash-handler", CB(arg_handle_crash_handler_disable), NULL);
	BLI_argsAdd(ba, 1, NULL, "--disable-abort-handler", CB(arg_handle_abort_handler_disable), NULL);

	BLI_argsAdd(ba, 1, "-b", "--background", CB(arg_handle_background_mode_set), NULL);

	BLI_argsAdd(ba, 1, "-a", NULL, CB(arg_handle_playback_mode), NULL);

	BLI_argsAdd(ba, 1, NULL, "--log", CB(arg_handle_log_set), ba);
	BLI_argsAdd(ba, 1, NULL, "--log-level", CB(arg_handle_log_level_set), ba);
	BLI_argsAdd(ba, 1, NULL, "--log-show-basename", CB(arg_handle_log_show_basename_set), ba);
	BLI_argsAdd(ba, 1, NULL, "--log-show-backtrace", CB(arg_handle_log_show_backtrace_set), ba);
	BLI_argsAdd(ba, 1, NULL, "--log-file", CB(arg_handle_log_file_set), ba);

	BLI_argsAdd(ba, 1, "-d", "--debug", CB(arg_handle_debug_mode_set), ba);

#ifdef WITH_FFMPEG
	BLI_argsAdd(ba, 1, NULL, "--debug-ffmpeg",
	            CB_EX(arg_handle_debug_mode_generic_set, ffmpeg), (void *)G_DEBUG_FFMPEG);
#endif

#ifdef WITH_FREESTYLE
	BLI_argsAdd(ba, 1, NULL, "--debug-freestyle",
	            CB_EX(arg_handle_debug_mode_generic_set, freestyle), (void *)G_DEBUG_FREESTYLE);
#endif

	BLI_argsAdd(ba, 1, NULL, "--debug-python",
	            CB_EX(arg_handle_debug_mode_generic_set, python), (void *)G_DEBUG_PYTHON);
	BLI_argsAdd(ba, 1, NULL, "--debug-events",
	            CB_EX(arg_handle_debug_mode_generic_set, events), (void *)G_DEBUG_EVENTS);
	BLI_argsAdd(ba, 1, NULL, "--debug-handlers",
	            CB_EX(arg_handle_debug_mode_generic_set, handlers), (void *)G_DEBUG_HANDLERS);
	BLI_argsAdd(ba, 1, NULL, "--debug-wm",
	            CB_EX(arg_handle_debug_mode_generic_set, wm), (void *)G_DEBUG_WM);
	BLI_argsAdd(ba, 1, NULL, "--debug-all", CB(arg_handle_debug_mode_all), NULL);

	BLI_argsAdd(ba, 1, NULL, "--debug-io", CB(arg_handle_debug_mode_io), NULL);

	BLI_argsAdd(ba, 1, NULL, "--debug-fpe",
	            CB(arg_handle_debug_fpe_set), NULL);

#ifdef WITH_LIBMV
	BLI_argsAdd(ba, 1, NULL, "--debug-libmv", CB(arg_handle_debug_mode_libmv), NULL);
#endif
#ifdef WITH_CYCLES_LOGGING
	BLI_argsAdd(ba, 1, NULL, "--debug-cycles", CB(arg_handle_debug_mode_cycles), NULL);
#endif
	BLI_argsAdd(ba, 1, NULL, "--debug-memory", CB(arg_handle_debug_mode_memory_set), NULL);

	BLI_argsAdd(ba, 1, NULL, "--debug-value",
	            CB(arg_handle_debug_value_set), NULL);
	BLI_argsAdd(ba, 1, NULL, "--debug-jobs",
	            CB_EX(arg_handle_debug_mode_generic_set, jobs), (void *)G_DEBUG_JOBS);
	BLI_argsAdd(ba, 1, NULL, "--debug-gpu",
	            CB_EX(arg_handle_debug_mode_generic_set, gpu), (void *)G_DEBUG_GPU);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph), (void *)G_DEBUG_DEPSGRAPH);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-build",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_build), (void *)G_DEBUG_DEPSGRAPH_BUILD);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-eval",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_eval), (void *)G_DEBUG_DEPSGRAPH_EVAL);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-tag",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_tag), (void *)G_DEBUG_DEPSGRAPH_TAG);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-time",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_time), (void *)G_DEBUG_DEPSGRAPH_TIME);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-no-threads",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_no_threads), (void *)G_DEBUG_DEPSGRAPH_NO_THREADS);
	BLI_argsAdd(ba, 1, NULL, "--debug-depsgraph-pretty",
	            CB_EX(arg_handle_debug_mode_generic_set, depsgraph_pretty), (void *)G_DEBUG_DEPSGRAPH_PRETTY);
	BLI_argsAdd(ba, 1, NULL, "--debug-gpumem",
	            CB_EX(arg_handle_debug_mode_generic_set, gpumem), (void *)G_DEBUG_GPU_MEM);
	BLI_argsAdd(ba, 1, NULL, "--debug-gpu-shaders",
	            CB_EX(arg_handle_debug_mode_generic_set, gpumem), (void *)G_DEBUG_GPU_SHADERS);

	BLI_argsAdd(ba, 1, NULL, "--enable-new-depsgraph", CB(arg_handle_depsgraph_use_new), NULL);
	BLI_argsAdd(ba, 1, NULL, "--enable-new-basic-shader-glsl", CB(arg_handle_basic_shader_glsl_use_new), NULL);

	BLI_argsAdd(ba, 1, NULL, "--verbose", CB(arg_handle_verbosity_set), NULL);

	BLI_argsAdd(ba, 1, NULL, "--app-template", CB(arg_handle_app_template), NULL);
	BLI_argsAdd(ba, 1, NULL, "--factory-startup", CB(arg_handle_factory_startup_set), NULL);

	/* TODO, add user env vars? */
	BLI_argsAdd(ba, 1, NULL, "--env-system-datafiles", CB_EX(arg_handle_env_system_set, datafiles), NULL);
	BLI_argsAdd(ba, 1, NULL, "--env-system-scripts", CB_EX(arg_handle_env_system_set, scripts), NULL);
	BLI_argsAdd(ba, 1, NULL, "--env-system-python", CB_EX(arg_handle_env_system_set, python), NULL);

	/* second pass: custom window stuff */
	BLI_argsAdd(ba, 2, "-p", "--window-geometry", CB(arg_handle_window_geometry), NULL);
	BLI_argsAdd(ba, 2, "-w", "--window-border", CB(arg_handle_with_borders), NULL);
	BLI_argsAdd(ba, 2, "-W", "--window-fullscreen", CB(arg_handle_without_borders), NULL);
	BLI_argsAdd(ba, 2, "-con", "--start-console", CB(arg_handle_start_with_console), NULL);
	BLI_argsAdd(ba, 2, "-R", NULL, CB(arg_handle_register_extension), NULL);
	BLI_argsAdd(ba, 2, "-r", NULL, CB_EX(arg_handle_register_extension, silent), ba);
	BLI_argsAdd(ba, 2, NULL, "--no-native-pixels", CB(arg_handle_native_pixels_set), ba);

	/* third pass: disabling things and forcing settings */
	BLI_argsAddCase(ba, 3, "-nojoystick", 1, NULL, 0, CB(arg_handle_joystick_disable), syshandle);
	BLI_argsAddCase(ba, 3, "-noglsl", 1, NULL, 0, CB(arg_handle_glsl_disable), NULL);
	BLI_argsAddCase(ba, 3, "-noaudio", 1, NULL, 0, CB(arg_handle_audio_disable), NULL);
	BLI_argsAddCase(ba, 3, "-setaudio", 1, NULL, 0, CB(arg_handle_audio_set), NULL);

	/* fourth pass: processing arguments */
	BLI_argsAdd(ba, 4, "-g", NULL, CB(arg_handle_ge_parameters_set), syshandle);
	BLI_argsAdd(ba, 4, "-f", "--render-frame", CB(arg_handle_render_frame), C);
	BLI_argsAdd(ba, 4, "-a", "--render-anim", CB(arg_handle_render_animation), C);
	BLI_argsAdd(ba, 4, "-S", "--scene", CB(arg_handle_scene_set), C);
	BLI_argsAdd(ba, 4, "-s", "--frame-start", CB(arg_handle_frame_start_set), C);
	BLI_argsAdd(ba, 4, "-e", "--frame-end", CB(arg_handle_frame_end_set), C);
	BLI_argsAdd(ba, 4, "-j", "--frame-jump", CB(arg_handle_frame_skip_set), C);
	BLI_argsAdd(ba, 4, "-P", "--python", CB(arg_handle_python_file_run), C);
	BLI_argsAdd(ba, 4, NULL, "--python-text", CB(arg_handle_python_text_run), C);
	BLI_argsAdd(ba, 4, NULL, "--python-expr", CB(arg_handle_python_expr_run), C);
	BLI_argsAdd(ba, 4, NULL, "--python-console", CB(arg_handle_python_console_run), C);
	BLI_argsAdd(ba, 4, NULL, "--python-exit-code", CB(arg_handle_python_exit_code_set), NULL);
	BLI_argsAdd(ba, 4, NULL, "--addons", CB(arg_handle_addons_set), C);

	BLI_argsAdd(ba, 4, "-o", "--render-output", CB(arg_handle_output_set), C);
	BLI_argsAdd(ba, 4, "-E", "--engine", CB(arg_handle_engine_set), C);

	BLI_argsAdd(ba, 4, "-F", "--render-format", CB(arg_handle_image_type_set), C);
	BLI_argsAdd(ba, 1, "-t", "--threads", CB(arg_handle_threads_set), NULL);
	BLI_argsAdd(ba, 4, "-x", "--use-extension", CB(arg_handle_extension_set), C);

#undef CB
#undef CB_EX

}

/**
 * Needs to be added separately.
 */
void main_args_setup_post(bContext *C, bArgs *ba)
{
	BLI_argsParse(ba, 4, arg_handle_load_file, C);
}

/** \} */

#endif /* WITH_PYTHON_MODULE */
