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
 * Contributor(s): Blender Foundation 2007
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_files.c
 *  \ingroup wm
 *
 * User level access for blend file read/write, file-history and userprefs (including relevant operators).
 */


/* placed up here because of crappy
 * winsock stuff.
 */
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "zlib.h" /* wm_read_exotic() */

#ifdef WIN32
#  include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#  ifndef _WIN32_IE
#    define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#  endif
#  include <shlobj.h>  /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff
                        * because 'near' is disabled through BLI_windstuff */
#  include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"
#include "BLI_threads.h"
#include "BLI_callbacks.h"
#include "BLI_system.h"
#include BLI_SYSTEM_PID_H

#include "BLT_translation.h"

#include "BLF_api.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_appdir.h"
#include "BKE_autoexec.h"
#include "BKE_blender.h"
#include "BKE_blendfile.h"
#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_undo_system.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"
#include "BLO_undofile.h"  /* to save from an undo memfile */

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "ED_datafiles.h"
#include "ED_fileselect.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_undo.h"

#include "GHOST_C-api.h"
#include "GHOST_Path-api.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_draw.h"

/* only to report a missing engine */
#include "RE_engine.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_files.h"
#include "wm_window.h"
#include "wm_event_system.h"

static RecentFile *wm_file_history_find(const char *filepath);
static void wm_history_file_free(RecentFile *recent);
static void wm_history_file_update(void);
static void wm_history_file_write(void);


/* To be able to read files without windows closing, opening, moving
 * we try to prepare for worst case:
 * - active window gets active screen from file
 * - restoring the screens from non-active windows
 * Best case is all screens match, in that case they get assigned to proper window
 */
static void wm_window_match_init(bContext *C, ListBase *wmlist)
{
	wmWindowManager *wm;
	wmWindow *win, *active_win;

	*wmlist = G_MAIN->wm;
	BLI_listbase_clear(&G_MAIN->wm);

	active_win = CTX_wm_window(C);

	/* first wrap up running stuff */
	/* code copied from wm_init_exit.c */
	for (wm = wmlist->first; wm; wm = wm->id.next) {

		WM_jobs_kill_all(wm);

		for (win = wm->windows.first; win; win = win->next) {

			CTX_wm_window_set(C, win);  /* needed by operator close callbacks */
			WM_event_remove_handlers(C, &win->handlers);
			WM_event_remove_handlers(C, &win->modalhandlers);
			ED_screen_exit(C, win, win->screen);
		}
	}

	/* reset active window */
	CTX_wm_window_set(C, active_win);

	/* XXX Hack! We have to clear context menu here, because removing all modalhandlers above frees the active menu
	 *     (at least, in the 'startup splash' case), causing use-after-free error in later handling of the button
	 *     callbacks in UI code (see ui_apply_but_funcs_after()).
	 *     Tried solving this by always NULL-ing context's menu when setting wm/win/etc., but it broke popups refreshing
	 *     (see T47632), so for now just handling this specific case here. */
	CTX_wm_menu_set(C, NULL);

	ED_editors_exit(C);

	/* just had return; here from r12991, this code could just get removed?*/
#if 0
	if (wm == NULL) return;
	if (G.fileflags & G_FILE_NO_UI) return;

	/* we take apart the used screens from non-active window */
	for (win = wm->windows.first; win; win = win->next) {
		BLI_strncpy(win->screenname, win->screen->id.name, MAX_ID_NAME);
		if (win != wm->winactive) {
			BLI_remlink(&G_MAIN->screen, win->screen);
			//BLI_addtail(screenbase, win->screen);
		}
	}
#endif
}

static void wm_window_substitute_old(wmWindowManager *wm, wmWindow *oldwin, wmWindow *win)
{
	win->ghostwin = oldwin->ghostwin;
	win->multisamples = oldwin->multisamples;
	win->active = oldwin->active;
	if (win->active)
		wm->winactive = win;

	if (!G.background) /* file loading in background mode still calls this */
		GHOST_SetWindowUserData(win->ghostwin, win);    /* pointer back */

	oldwin->ghostwin = NULL;
	oldwin->multisamples = 0;

	win->eventstate = oldwin->eventstate;
	oldwin->eventstate = NULL;

	/* ensure proper screen rescaling */
	win->sizex = oldwin->sizex;
	win->sizey = oldwin->sizey;
	win->posx = oldwin->posx;
	win->posy = oldwin->posy;
}

/* match old WM with new, 4 cases:
 * 1- no current wm, no read wm: make new default
 * 2- no current wm, but read wm: that's OK, do nothing
 * 3- current wm, but not in file: try match screen names
 * 4- current wm, and wm in file: try match ghostwin
 */

static void wm_window_match_do(Main *bmain, bContext *C, ListBase *oldwmlist)
{
	wmWindowManager *oldwm, *wm;
	wmWindow *oldwin, *win;

	/* cases 1 and 2 */
	if (BLI_listbase_is_empty(oldwmlist)) {
		if (bmain->wm.first) {
			/* nothing todo */
		}
		else {
			wm_add_default(C);
		}
	}
	else {
		/* cases 3 and 4 */

		/* we've read file without wm..., keep current one entirely alive */
		if (BLI_listbase_is_empty(&bmain->wm)) {
			bScreen *screen = NULL;

			/* when loading without UI, no matching needed */
			if (!(G.fileflags & G_FILE_NO_UI) && (screen = CTX_wm_screen(C))) {

				/* match oldwm to new dbase, only old files */
				for (wm = oldwmlist->first; wm; wm = wm->id.next) {

					for (win = wm->windows.first; win; win = win->next) {
						/* all windows get active screen from file */
						if (screen->winid == 0)
							win->screen = screen;
						else
							win->screen = ED_screen_duplicate(bmain, win, screen);

						BLI_strncpy(win->screenname, win->screen->id.name + 2, sizeof(win->screenname));
						win->screen->winid = win->winid;
					}
				}
			}

			bmain->wm = *oldwmlist;

			/* screens were read from file! */
			ED_screens_initialize(bmain, bmain->wm.first);
		}
		else {
			bool has_match = false;

			/* what if old was 3, and loaded 1? */
			/* this code could move to setup_appdata */
			oldwm = oldwmlist->first;
			wm = bmain->wm.first;

			/* preserve key configurations in new wm, to preserve their keymaps */
			wm->keyconfigs = oldwm->keyconfigs;
			wm->addonconf = oldwm->addonconf;
			wm->defaultconf = oldwm->defaultconf;
			wm->userconf = oldwm->userconf;

			BLI_listbase_clear(&oldwm->keyconfigs);
			oldwm->addonconf = NULL;
			oldwm->defaultconf = NULL;
			oldwm->userconf = NULL;

			/* ensure making new keymaps and set space types */
			wm->initialized = 0;
			wm->winactive = NULL;

			/* only first wm in list has ghostwins */
			for (win = wm->windows.first; win; win = win->next) {
				for (oldwin = oldwm->windows.first; oldwin; oldwin = oldwin->next) {

					if (oldwin->winid == win->winid) {
						has_match = true;

						wm_window_substitute_old(wm, oldwin, win);
					}
				}
			}

			/* make sure at least one window is kept open so we don't lose the context, check T42303 */
			if (!has_match) {
				oldwin = oldwm->windows.first;
				win = wm->windows.first;

				wm_window_substitute_old(wm, oldwin, win);
			}

			wm_close_and_free_all(C, oldwmlist);
		}
	}
}

/* in case UserDef was read, we re-initialize all, and do versioning */
static void wm_init_userdef(Main *bmain, const bool read_userdef_from_memory)
{
	/* versioning is here */
	UI_init_userdef(bmain);

	MEM_CacheLimiter_set_maximum(((size_t)U.memcachelimit) * 1024 * 1024);
	BKE_sound_init(bmain);

	/* needed so loading a file from the command line respects user-pref [#26156] */
	SET_FLAG_FROM_TEST(G.fileflags, U.flag & USER_FILENOUI, G_FILE_NO_UI);

	/* set the python auto-execute setting from user prefs */
	/* enabled by default, unless explicitly enabled in the command line which overrides */
	if ((G.f & G_SCRIPT_OVERRIDE_PREF) == 0) {
		SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_SCRIPT_AUTOEXEC);
	}

	/* avoid re-saving for every small change to our prefs, allow overrides */
	if (read_userdef_from_memory) {
		BLO_update_defaults_userpref_blend();
	}

	/* update tempdir from user preferences */
	BKE_tempdir_init(U.tempdir);
}



/* return codes */
#define BKE_READ_EXOTIC_FAIL_PATH       -3 /* file format is not supported */
#define BKE_READ_EXOTIC_FAIL_FORMAT     -2 /* file format is not supported */
#define BKE_READ_EXOTIC_FAIL_OPEN       -1 /* Can't open the file */
#define BKE_READ_EXOTIC_OK_BLEND         0 /* .blend file */
#if 0
#define BKE_READ_EXOTIC_OK_OTHER         1 /* other supported formats */
#endif


/* intended to check for non-blender formats but for now it only reads blends */
static int wm_read_exotic(const char *name)
{
	int len;
	gzFile gzfile;
	char header[7];
	int retval;

	/* make sure we're not trying to read a directory.... */

	len = strlen(name);
	if (len > 0 && ELEM(name[len - 1], '/', '\\')) {
		retval = BKE_READ_EXOTIC_FAIL_PATH;
	}
	else {
		gzfile = BLI_gzopen(name, "rb");
		if (gzfile == NULL) {
			retval = BKE_READ_EXOTIC_FAIL_OPEN;
		}
		else {
			len = gzread(gzfile, header, sizeof(header));
			gzclose(gzfile);
			if (len == sizeof(header) && STREQLEN(header, "BLENDER", 7)) {
				retval = BKE_READ_EXOTIC_OK_BLEND;
			}
			else {
#if 0           /* historic stuff - no longer used */
				WM_cursor_wait(true);

				if (is_foo_format(name)) {
					read_foo(name);
					retval = BKE_READ_EXOTIC_OK_OTHER;
				}
				else
#endif
				{
					retval = BKE_READ_EXOTIC_FAIL_FORMAT;
				}
#if 0
				WM_cursor_wait(false);
#endif
			}
		}
	}

	return retval;
}

void WM_file_autoexec_init(const char *filepath)
{
	if (G.f & G_SCRIPT_OVERRIDE_PREF) {
		return;
	}

	if (G.f & G_SCRIPT_AUTOEXEC) {
		char path[FILE_MAX];
		BLI_split_dir_part(filepath, path, sizeof(path));
		if (BKE_autoexec_match(path)) {
			G.f &= ~G_SCRIPT_AUTOEXEC;
		}
	}
}

void wm_file_read_report(bContext *C, Main *bmain)
{
	ReportList *reports = NULL;
	Scene *sce;

	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->r.engine[0] &&
		    BLI_findstring(&R_engines, sce->r.engine, offsetof(RenderEngineType, idname)) == NULL)
		{
			if (reports == NULL) {
				reports = CTX_wm_reports(C);
			}

			BKE_reportf(reports, RPT_ERROR,
			            "Engine '%s' not available for scene '%s' (an add-on may need to be installed or enabled)",
			            sce->r.engine, sce->id.name + 2);
		}
	}

	if (reports) {
		if (!G.background) {
			WM_report_banner_show();
		}
	}
}

/**
 * Logic shared between #WM_file_read & #wm_homefile_read,
 * updates to make after reading a file.
 */
static void wm_file_read_post(bContext *C, const bool is_startup_file, const bool use_userdef)
{
	Main *bmain = CTX_data_main(C);
	bool addons_loaded = false;
	wmWindowManager *wm = CTX_wm_manager(C);

	if (!G.background) {
		/* remove windows which failed to be added via WM_check */
		wm_window_ghostwindows_remove_invalid(C, wm);
	}

	CTX_wm_window_set(C, wm->windows.first);

	ED_editors_init(C);
	DAG_on_visible_update(bmain, true);

#ifdef WITH_PYTHON
	if (is_startup_file) {
		/* possible python hasn't been initialized */
		if (CTX_py_init_get(C)) {
			if (use_userdef) {
				/* Only run when we have a template path found. */
				if (BKE_appdir_app_template_any()) {
					BPY_execute_string(C, "__import__('bl_app_template_utils').reset()");
				}
				/* sync addons, these may have changed from the defaults */
				BPY_execute_string(C, "__import__('addon_utils').reset_all()");
			}
			BPY_python_reset(C);
			addons_loaded = true;
		}
	}
	else {
		/* run any texts that were loaded in and flagged as modules */
		BPY_python_reset(C);
		addons_loaded = true;
	}
#else
	UNUSED_VARS(use_userdef);
#endif  /* WITH_PYTHON */

	WM_operatortype_last_properties_clear_all();

	/* important to do before NULL'ing the context */
	BLI_callback_exec(bmain, NULL, BLI_CB_EVT_VERSION_UPDATE);
	BLI_callback_exec(bmain, NULL, BLI_CB_EVT_LOAD_POST);

	/* Would otherwise be handled by event loop.
	 *
	 * Disabled for startup file, since it causes problems when PyDrivers are used in the startup file.
	 * While its possible state of startup file may be wrong,
	 * in this case users nearly always load a file to replace the startup file. */
	if (G.background && (is_startup_file == false)) {
		BKE_scene_update_tagged(bmain->eval_ctx, bmain, CTX_data_scene(C));
	}

	WM_event_add_notifier(C, NC_WM | ND_FILEREAD, NULL);

	/* report any errors.
	 * currently disabled if addons aren't yet loaded */
	if (addons_loaded) {
		wm_file_read_report(C, bmain);
	}

	if (!G.background) {
		if (wm->undo_stack == NULL) {
			wm->undo_stack = BKE_undosys_stack_create();
		}
		else {
			BKE_undosys_stack_clear(wm->undo_stack);
		}
		BKE_undosys_stack_init_from_main(wm->undo_stack, bmain);
		BKE_undosys_stack_init_from_context(wm->undo_stack, C);
	}

	if (!G.background) {
		/* in background mode this makes it hard to load
		 * a blend file and do anything since the screen
		 * won't be set to a valid value again */
		CTX_wm_window_set(C, NULL); /* exits queues */
	}
}

bool WM_file_read(bContext *C, const char *filepath, ReportList *reports)
{
	/* assume automated tasks with background, don't write recent file list */
	const bool do_history = (G.background == false) && (CTX_wm_manager(C)->op_undo_depth == 0);
	bool success = false;
	int retval;

	/* so we can get the error message */
	errno = 0;

	WM_cursor_wait(1);

	BLI_callback_exec(CTX_data_main(C), NULL, BLI_CB_EVT_LOAD_PRE);

	UI_view2d_zoom_cache_reset();

	/* first try to append data from exotic file formats... */
	/* it throws error box when file doesn't exist and returns -1 */
	/* note; it should set some error message somewhere... (ton) */
	retval = wm_read_exotic(filepath);

	/* we didn't succeed, now try to read Blender file */
	if (retval == BKE_READ_EXOTIC_OK_BLEND) {
		int G_f = G.f;
		ListBase wmbase;

		/* put aside screens to match with persistent windows later */
		/* also exit screens and editors */
		wm_window_match_init(C, &wmbase);

		/* confusing this global... */
		G.relbase_valid = 1;
		retval = BKE_blendfile_read(C, filepath, reports, 0);

		/* BKE_file_read sets new Main into context. */
		Main *bmain = CTX_data_main(C);

		/* when loading startup.blend's, we can be left with a blank path */
		if (BKE_main_blendfile_path(bmain)) {
			G.save_over = 1;
		}
		else {
			G.save_over = 0;
			G.relbase_valid = 0;
		}

		/* this flag is initialized by the operator but overwritten on read.
		 * need to re-enable it here else drivers + registered scripts wont work. */
		if (G.f != G_f) {
			const int flags_keep = (G_SCRIPT_AUTOEXEC | G_SCRIPT_OVERRIDE_PREF);
			G.f = (G.f & ~flags_keep) | (G_f & flags_keep);
		}

		/* match the read WM with current WM */
		wm_window_match_do(bmain, C, &wmbase);
		WM_check(C); /* opens window(s), checks keymaps */

		if (retval == BKE_BLENDFILE_READ_OK_USERPREFS) {
			/* in case a userdef is read from regular .blend */
			wm_init_userdef(bmain, false);
		}

		if (retval != BKE_BLENDFILE_READ_FAIL) {
			if (do_history) {
				wm_history_file_update();
			}
		}

		wm_file_read_post(C, false, false);

		success = true;
	}
	else if (retval == BKE_READ_EXOTIC_FAIL_OPEN) {
		BKE_reportf(reports, RPT_ERROR, "Cannot read file '%s': %s", filepath,
		            errno ? strerror(errno) : TIP_("unable to open the file"));
	}
	else if (retval == BKE_READ_EXOTIC_FAIL_FORMAT) {
		BKE_reportf(reports, RPT_ERROR, "File format is not supported in file '%s'", filepath);
	}
	else if (retval == BKE_READ_EXOTIC_FAIL_PATH) {
		BKE_reportf(reports, RPT_ERROR, "File path '%s' invalid", filepath);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Unknown error loading '%s'", filepath);
		BLI_assert(!"invalid 'retval'");
	}


	if (success == false) {
		/* remove from recent files list */
		if (do_history) {
			RecentFile *recent = wm_file_history_find(filepath);
			if (recent) {
				wm_history_file_free(recent);
				wm_history_file_write();
			}
		}
	}

	WM_cursor_wait(0);

	return success;

}


/**
 * Called on startup, (context entirely filled with NULLs)
 * or called for 'New File' both startup.blend and userpref.blend are checked.
 *
 * \param use_factory_settings: Ignore on-disk startup file, use bundled ``datatoc_startup_blend`` instead.
 * Used for "Restore Factory Settings".
 * \param use_userdef: Load factory settings as well as startup file.
 * Disabled for "File New" we don't want to reload preferences.
 * \param filepath_startup_override: Optional path pointing to an alternative blend file (may be NULL).
 * \param app_template_override: Template to use instead of the template defined in user-preferences.
 * When not-null, this is written into the user preferences.
 */
int wm_homefile_read(
        bContext *C, ReportList *reports,
        bool use_factory_settings, bool use_empty_data, bool use_userdef,
        const char *filepath_startup_override, const char *app_template_override)
{
	Main *bmain = G_MAIN;  /* Context does not always have valid main pointer here... */
	ListBase wmbase;
	bool success = false;

	char filepath_startup[FILE_MAX];
	char filepath_userdef[FILE_MAX];

	/* When 'app_template' is set: '{BLENDER_USER_CONFIG}/{app_template}' */
	char app_template_system[FILE_MAX];
	/* When 'app_template' is set: '{BLENDER_SYSTEM_SCRIPTS}/startup/bl_app_templates_system/{app_template}' */
	char app_template_config[FILE_MAX];

	/* Indicates whether user preferences were really load from memory.
	 *
	 * This is used for versioning code, and for this we can not rely on use_factory_settings
	 * passed via argument. This is because there might be configuration folder
	 * exists but it might not have userpref.blend and in this case we fallback to
	 * reading home file from memory.
	 *
	 * And in this case versioning code is to be run.
	 */
	bool read_userdef_from_memory = false;
	eBLOReadSkip skip_flags = use_userdef ? 0 : BLO_READ_SKIP_USERDEF;

	/* options exclude eachother */
	BLI_assert((use_factory_settings && filepath_startup_override) == 0);

	if ((G.f & G_SCRIPT_OVERRIDE_PREF) == 0) {
		SET_FLAG_FROM_TEST(G.f, (U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0, G_SCRIPT_AUTOEXEC);
	}

	BLI_callback_exec(CTX_data_main(C), NULL, BLI_CB_EVT_LOAD_PRE);

	UI_view2d_zoom_cache_reset();

	G.relbase_valid = 0;

	/* put aside screens to match with persistent windows later */
	wm_window_match_init(C, &wmbase);

	filepath_startup[0] = '\0';
	filepath_userdef[0] = '\0';
	app_template_system[0] = '\0';
	app_template_config[0] = '\0';

	const char * const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);
	if (!use_factory_settings) {
		if (cfgdir) {
			BLI_path_join(filepath_startup, sizeof(filepath_startup), cfgdir, BLENDER_STARTUP_FILE, NULL);
			if (use_userdef) {
				BLI_path_join(filepath_userdef, sizeof(filepath_startup), cfgdir, BLENDER_USERPREF_FILE, NULL);
			}
		}
		else {
			use_factory_settings = true;
		}

		if (filepath_startup_override) {
			BLI_strncpy(filepath_startup, filepath_startup_override, FILE_MAX);
		}
	}

	/* load preferences before startup.blend */
	if (use_userdef) {
		if (!use_factory_settings && BLI_exists(filepath_userdef)) {
			UserDef *userdef = BKE_blendfile_userdef_read(filepath_userdef, NULL);
			if (userdef != NULL) {
				BKE_blender_userdef_data_set_and_free(userdef);
				userdef = NULL;

				skip_flags |= BLO_READ_SKIP_USERDEF;
				printf("Read prefs: %s\n", filepath_userdef);
			}
		}
	}

	const char *app_template = NULL;

	if (filepath_startup_override != NULL) {
		/* pass */
	}
	else if (app_template_override) {
		/* This may be clearing the current template by setting to an empty string. */
		app_template = app_template_override;
	}
	else if (!use_factory_settings && U.app_template[0]) {
		app_template = U.app_template;
	}

	if ((app_template != NULL) && (app_template[0] != '\0')) {
		BKE_appdir_app_template_id_search(app_template, app_template_system, sizeof(app_template_system));

		/* Insert template name into startup file. */

		/* note that the path is being set even when 'use_factory_settings == true'
		 * this is done so we can load a templates factory-settings */
		if (!use_factory_settings) {
			BLI_path_join(app_template_config, sizeof(app_template_config), cfgdir, app_template, NULL);
			BLI_path_join(filepath_startup, sizeof(filepath_startup), app_template_config, BLENDER_STARTUP_FILE, NULL);
			if (BLI_access(filepath_startup, R_OK) != 0) {
				filepath_startup[0] = '\0';
			}
		}
		else {
			filepath_startup[0] = '\0';
		}

		if (filepath_startup[0] == '\0') {
			BLI_path_join(filepath_startup, sizeof(filepath_startup), app_template_system, BLENDER_STARTUP_FILE, NULL);
		}
	}

	if (!use_factory_settings || (filepath_startup[0] != '\0')) {
		if (BLI_access(filepath_startup, R_OK) == 0) {
			success = (BKE_blendfile_read(C, filepath_startup, NULL, skip_flags) != BKE_BLENDFILE_READ_FAIL);
		}
		if (BLI_listbase_is_empty(&U.themes)) {
			if (G.debug & G_DEBUG)
				printf("\nNote: No (valid) '%s' found, fall back to built-in default.\n\n", filepath_startup);
			success = false;
		}
	}

	if (success == false && filepath_startup_override && reports) {
		/* We can not return from here because wm is already reset */
		BKE_reportf(reports, RPT_ERROR, "Could not read '%s'", filepath_startup_override);
	}

	if (success == false) {
		success = BKE_blendfile_read_from_memory(
		        C, datatoc_startup_blend, datatoc_startup_blend_size,
		        NULL, skip_flags, true);
		if (success) {
			if (use_userdef) {
				if ((skip_flags & BLO_READ_SKIP_USERDEF) == 0) {
					read_userdef_from_memory = true;
				}
			}
		}
		if (BLI_listbase_is_empty(&wmbase)) {
			wm_clear_default_size(C);
		}
	}

	if (use_empty_data) {
		BKE_blendfile_read_make_empty(C);
	}

	/* Load template preferences,
	 * unlike regular preferences we only use some of the settings,
	 * see: BKE_blender_userdef_set_app_template */
	if (app_template_system[0] != '\0') {
		char temp_path[FILE_MAX];
		temp_path[0] = '\0';
		if (!use_factory_settings) {
			BLI_path_join(temp_path, sizeof(temp_path), app_template_config, BLENDER_USERPREF_FILE, NULL);
			if (BLI_access(temp_path, R_OK) != 0) {
				temp_path[0] = '\0';
			}
		}

		if (temp_path[0] == '\0') {
			BLI_path_join(temp_path, sizeof(temp_path), app_template_system, BLENDER_USERPREF_FILE, NULL);
		}

		if (use_userdef) {
			UserDef *userdef_template = NULL;
			/* just avoids missing file warning */
			if (BLI_exists(temp_path)) {
				userdef_template = BKE_blendfile_userdef_read(temp_path, NULL);
			}
			if (userdef_template == NULL) {
				/* we need to have preferences load to overwrite preferences from previous template */
				userdef_template = BKE_blendfile_userdef_read_from_memory(
				        datatoc_startup_blend, datatoc_startup_blend_size, NULL);
				read_userdef_from_memory = true;
			}
			if (userdef_template) {
				BKE_blender_userdef_app_template_data_set_and_free(userdef_template);
				userdef_template = NULL;
			}
		}
	}

	if (app_template_override) {
		BLI_strncpy(U.app_template, app_template_override, sizeof(U.app_template));
	}

	/* prevent buggy files that had G_FILE_RELATIVE_REMAP written out by mistake. Screws up autosaves otherwise
	 * can remove this eventually, only in a 2.53 and older, now its not written */
	G.fileflags &= ~G_FILE_RELATIVE_REMAP;

	bmain = CTX_data_main(C);

	if (use_userdef) {
		/* check userdef before open window, keymaps etc */
		wm_init_userdef(bmain, read_userdef_from_memory);
	}

	/* match the read WM with current WM */
	wm_window_match_do(bmain, C, &wmbase);
	WM_check(C); /* opens window(s), checks keymaps */

	bmain->name[0] = '\0';

	if (use_userdef) {
		/* When loading factory settings, the reset solid OpenGL lights need to be applied. */
		if (!G.background) {
			GPU_default_lights();
		}
	}

	/* start with save preference untitled.blend */
	G.save_over = 0;
	/* disable auto-play in startup.blend... */
	G.fileflags &= ~G_FILE_AUTOPLAY;

	wm_file_read_post(C, true, use_userdef);

	return true;
}

/** \name WM History File API
 * \{ */

void wm_history_file_read(void)
{
	char name[FILE_MAX];
	LinkNode *l, *lines;
	struct RecentFile *recent;
	const char *line;
	int num;
	const char * const cfgdir = BKE_appdir_folder_id(BLENDER_USER_CONFIG, NULL);

	if (!cfgdir) return;

	BLI_make_file_string("/", name, cfgdir, BLENDER_HISTORY_FILE);

	lines = BLI_file_read_as_lines(name);

	BLI_listbase_clear(&G.recent_files);

	/* read list of recent opened files from recent-files.txt to memory */
	for (l = lines, num = 0; l && (num < U.recent_files); l = l->next) {
		line = l->link;
		/* don't check if files exist, causes slow startup for remote/external drives */
		if (line[0]) {
			recent = (RecentFile *)MEM_mallocN(sizeof(RecentFile), "RecentFile");
			BLI_addtail(&(G.recent_files), recent);
			recent->filepath = BLI_strdup(line);
			num++;
		}
	}

	BLI_file_free_lines(lines);
}

static RecentFile *wm_history_file_new(const char *filepath)
{
	RecentFile *recent = MEM_mallocN(sizeof(RecentFile), "RecentFile");
	recent->filepath = BLI_strdup(filepath);
	return recent;
}

static void wm_history_file_free(RecentFile *recent)
{
	BLI_assert(BLI_findindex(&G.recent_files, recent) != -1);
	MEM_freeN(recent->filepath);
	BLI_freelinkN(&G.recent_files, recent);
}

static RecentFile *wm_file_history_find(const char *filepath)
{
	return BLI_findstring_ptr(&G.recent_files, filepath, offsetof(RecentFile, filepath));
}

/**
 * Write #BLENDER_HISTORY_FILE as-is, without checking the environment
 * (thats handled by #wm_history_file_update).
 */
static void wm_history_file_write(void)
{
	const char *user_config_dir;
	char name[FILE_MAX];
	FILE *fp;

	/* will be NULL in background mode */
	user_config_dir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL);
	if (!user_config_dir)
		return;

	BLI_make_file_string("/", name, user_config_dir, BLENDER_HISTORY_FILE);

	fp = BLI_fopen(name, "w");
	if (fp) {
		struct RecentFile *recent;
		for (recent = G.recent_files.first; recent; recent = recent->next) {
			fprintf(fp, "%s\n", recent->filepath);
		}
		fclose(fp);
	}
}

/**
 * Run after saving a file to refresh the #BLENDER_HISTORY_FILE list.
 */
static void wm_history_file_update(void)
{
	RecentFile *recent;
	const char *blendfile_name = BKE_main_blendfile_path_from_global();

	/* no write history for recovered startup files */
	if (blendfile_name[0] == '\0') {
		return;
	}

	recent = G.recent_files.first;
	/* refresh recent-files.txt of recent opened files, when current file was changed */
	if (!(recent) || (BLI_path_cmp(recent->filepath, blendfile_name) != 0)) {

		recent = wm_file_history_find(blendfile_name);
		if (recent) {
			BLI_remlink(&G.recent_files, recent);
		}
		else {
			RecentFile *recent_next;
			for (recent = BLI_findlink(&G.recent_files, U.recent_files - 1); recent; recent = recent_next) {
				recent_next = recent->next;
				wm_history_file_free(recent);
			}
			recent = wm_history_file_new(blendfile_name);
		}

		/* add current file to the beginning of list */
		BLI_addhead(&(G.recent_files), recent);

		/* write current file to recent-files.txt */
		wm_history_file_write();

		/* also update most recent files on System */
		GHOST_addToSystemRecentFiles(blendfile_name);
	}
}

/** \} */


/* screen can be NULL */
static ImBuf *blend_file_thumb(Main *bmain, Scene *scene, bScreen *screen, BlendThumbnail **thumb_pt)
{
	/* will be scaled down, but gives some nice oversampling */
	ImBuf *ibuf;
	BlendThumbnail *thumb;
	char err_out[256] = "unknown";

	/* screen if no camera found */
	ScrArea *sa = NULL;
	ARegion *ar = NULL;
	View3D *v3d = NULL;

	/* In case we are given a valid thumbnail data, just generate image from it. */
	if (*thumb_pt) {
		thumb = *thumb_pt;
		return BKE_main_thumbnail_to_imbuf(NULL, thumb);
	}

	/* scene can be NULL if running a script at startup and calling the save operator */
	if (G.background || scene == NULL)
		return NULL;

	if ((scene->camera == NULL) && (screen != NULL)) {
		sa = BKE_screen_find_big_area(screen, SPACE_VIEW3D, 0);
		ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
		if (ar) {
			v3d = sa->spacedata.first;
		}
	}

	if (scene->camera == NULL && v3d == NULL) {
		return NULL;
	}

	/* gets scaled to BLEN_THUMB_SIZE */
	if (scene->camera) {
		ibuf = ED_view3d_draw_offscreen_imbuf_simple(
		        bmain, scene, scene->camera,
		        BLEN_THUMB_SIZE * 2, BLEN_THUMB_SIZE * 2,
		        IB_rect, V3D_OFSDRAW_NONE, OB_SOLID, R_ALPHAPREMUL, 0, NULL,
		        NULL, NULL, err_out);
	}
	else {
		ibuf = ED_view3d_draw_offscreen_imbuf(
		        bmain, scene, v3d, ar,
		        BLEN_THUMB_SIZE * 2, BLEN_THUMB_SIZE * 2,
		        IB_rect, V3D_OFSDRAW_NONE, R_ALPHAPREMUL, 0, NULL,
		        NULL, NULL, err_out);
	}

	if (ibuf) {
		float aspect = (scene->r.xsch * scene->r.xasp) / (scene->r.ysch * scene->r.yasp);

		/* dirty oversampling */
		IMB_scaleImBuf(ibuf, BLEN_THUMB_SIZE, BLEN_THUMB_SIZE);

		/* add pretty overlay */
		IMB_thumb_overlay_blend(ibuf->rect, ibuf->x, ibuf->y, aspect);

		thumb = BKE_main_thumbnail_from_imbuf(NULL, ibuf);
	}
	else {
		/* '*thumb_pt' needs to stay NULL to prevent a bad thumbnail from being handled */
		fprintf(stderr, "blend_file_thumb failed to create thumbnail: %s\n", err_out);
		thumb = NULL;
	}

	/* must be freed by caller */
	*thumb_pt = thumb;

	return ibuf;
}

/* easy access from gdb */
bool write_crash_blend(void)
{
	char path[FILE_MAX];
	int fileflags = G.fileflags & ~(G_FILE_HISTORY); /* don't do file history on crash file */

	BLI_strncpy(path, BKE_main_blendfile_path_from_global(), sizeof(path));
	BLI_path_extension_replace(path, sizeof(path), "_crash.blend");
	if (BLO_write_file(G_MAIN, path, fileflags, NULL, NULL)) {
		printf("written: %s\n", path);
		return 1;
	}
	else {
		printf("failed: %s\n", path);
		return 0;
	}
}

/**
 * \see #wm_homefile_write_exec wraps #BLO_write_file in a similar way.
 */
static int wm_file_write(bContext *C, const char *filepath, int fileflags, ReportList *reports)
{
	Main *bmain = CTX_data_main(C);
	Library *li;
	int len;
	int ret = -1;
	BlendThumbnail *thumb, *main_thumb;
	ImBuf *ibuf_thumb = NULL;

	len = strlen(filepath);

	if (len == 0) {
		BKE_report(reports, RPT_ERROR, "Path is empty, cannot save");
		return ret;
	}

	if (len >= FILE_MAX) {
		BKE_report(reports, RPT_ERROR, "Path too long, cannot save");
		return ret;
	}

	/* Check if file write permission is ok */
	if (BLI_exists(filepath) && !BLI_file_is_writable(filepath)) {
		BKE_reportf(reports, RPT_ERROR, "Cannot save blend file, path '%s' is not writable", filepath);
		return ret;
	}

	/* note: used to replace the file extension (to ensure '.blend'),
	 * no need to now because the operator ensures,
	 * its handy for scripts to save to a predefined name without blender editing it */

	/* send the OnSave event */
	for (li = bmain->library.first; li; li = li->id.next) {
		if (BLI_path_cmp(li->filepath, filepath) == 0) {
			BKE_reportf(reports, RPT_ERROR, "Cannot overwrite used library '%.240s'", filepath);
			return ret;
		}
	}

	/* Call pre-save callbacks befores writing preview, that way you can generate custom file thumbnail... */
	BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_PRE);

	/* blend file thumbnail */
	/* save before exit_editmode, otherwise derivedmeshes for shared data corrupt #27765) */
	/* Main now can store a .blend thumbnail, usefull for background mode or thumbnail customization. */
	main_thumb = thumb = bmain->blen_thumb;
	if ((U.flag & USER_SAVE_PREVIEWS) && BLI_thread_is_main()) {
		ibuf_thumb = blend_file_thumb(bmain, CTX_data_scene(C), CTX_wm_screen(C), &thumb);
	}

	/* operator now handles overwrite checks */

	if (G.fileflags & G_AUTOPACK) {
		packAll(bmain, reports, false);
	}

	/* don't forget not to return without! */
	WM_cursor_wait(1);

	ED_editors_flush_edits(C, false);

	fileflags |= G_FILE_HISTORY; /* write file history */

	/* first time saving */
	/* XXX temp solution to solve bug, real fix coming (ton) */
	if ((BKE_main_blendfile_path(bmain)[0] == '\0') && !(fileflags & G_FILE_SAVE_COPY)) {
		BLI_strncpy(bmain->name, filepath, sizeof(bmain->name));
	}

	/* XXX temp solution to solve bug, real fix coming (ton) */
	bmain->recovered = 0;

	if (BLO_write_file(CTX_data_main(C), filepath, fileflags, reports, thumb)) {
		const bool do_history = (G.background == false) && (CTX_wm_manager(C)->op_undo_depth == 0);

		if (!(fileflags & G_FILE_SAVE_COPY)) {
			G.relbase_valid = 1;
			BLI_strncpy(bmain->name, filepath, sizeof(bmain->name));  /* is guaranteed current file */

			G.save_over = 1; /* disable untitled.blend convention */
		}

		SET_FLAG_FROM_TEST(G.fileflags, fileflags & G_FILE_COMPRESS, G_FILE_COMPRESS);
		SET_FLAG_FROM_TEST(G.fileflags, fileflags & G_FILE_AUTOPLAY, G_FILE_AUTOPLAY);

		/* prevent background mode scripts from clobbering history */
		if (do_history) {
			wm_history_file_update();
		}

		BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_POST);

		/* run this function after because the file cant be written before the blend is */
		if (ibuf_thumb) {
			IMB_thumb_delete(filepath, THB_FAIL); /* without this a failed thumb overrides */
			ibuf_thumb = IMB_thumb_create(filepath, THB_LARGE, THB_SOURCE_BLEND, ibuf_thumb);
		}

		ret = 0;  /* Success. */
	}

	if (ibuf_thumb) {
		IMB_freeImBuf(ibuf_thumb);
	}
	if (thumb && thumb != main_thumb) {
		MEM_freeN(thumb);
	}

	WM_cursor_wait(0);

	return ret;
}

/************************ autosave ****************************/

void wm_autosave_location(char *filepath)
{
	const int pid = abs(getpid());
	char path[1024];
#ifdef WIN32
	const char *savedir;
#endif

	if (G_MAIN && G.relbase_valid) {
		const char *basename = BLI_path_basename(BKE_main_blendfile_path_from_global());
		int len = strlen(basename) - 6;
		BLI_snprintf(path, sizeof(path), "%.*s.blend", len, basename);
	}
	else {
		BLI_snprintf(path, sizeof(path), "%d.blend", pid);
	}

#ifdef WIN32
	/* XXX Need to investigate how to handle default location of '/tmp/'
	 * This is a relative directory on Windows, and it may be
	 * found. Example:
	 * Blender installed on D:\ drive, D:\ drive has D:\tmp\
	 * Now, BLI_exists() will find '/tmp/' exists, but
	 * BLI_make_file_string will create string that has it most likely on C:\
	 * through get_default_root().
	 * If there is no C:\tmp autosave fails. */
	if (!BLI_exists(BKE_tempdir_base())) {
		savedir = BKE_appdir_folder_id_create(BLENDER_USER_AUTOSAVE, NULL);
		BLI_make_file_string("/", filepath, savedir, path);
		return;
	}
#endif

	BLI_make_file_string("/", filepath, BKE_tempdir_base(), path);
}

void WM_autosave_init(wmWindowManager *wm)
{
	wm_autosave_timer_ended(wm);

	if (U.flag & USER_AUTOSAVE)
		wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime * 60.0);
}

void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *UNUSED(wt))
{
	wmWindow *win;
	wmEventHandler *handler;
	char filepath[FILE_MAX];

	WM_event_remove_timer(wm, NULL, wm->autosavetimer);

	/* if a modal operator is running, don't autosave, but try again in 10 seconds */
	for (win = wm->windows.first; win; win = win->next) {
		for (handler = win->modalhandlers.first; handler; handler = handler->next) {
			if (handler->op) {
				wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, 10.0);
				if (G.debug) {
					printf("Skipping auto-save, modal operator running, retrying in ten seconds...\n");
				}
				return;
			}
		}
	}

	wm_autosave_location(filepath);

	if (U.uiflag & USER_GLOBALUNDO) {
		/* fast save of last undobuffer, now with UI */
		struct MemFile *memfile = ED_undosys_stack_memfile_get_active(wm->undo_stack);
		if (memfile) {
			BLO_memfile_write_file(memfile, filepath);
		}
	}
	else {
		/*  save as regular blend file */
		int fileflags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_AUTOPLAY | G_FILE_HISTORY);

		ED_editors_flush_edits(C, false);

		/* Error reporting into console */
		BLO_write_file(CTX_data_main(C), filepath, fileflags, NULL, NULL);
	}
	/* do timer after file write, just in case file write takes a long time */
	wm->autosavetimer = WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime * 60.0);
}

void wm_autosave_timer_ended(wmWindowManager *wm)
{
	if (wm->autosavetimer) {
		WM_event_remove_timer(wm, NULL, wm->autosavetimer);
		wm->autosavetimer = NULL;
	}
}

void wm_autosave_delete(void)
{
	char filename[FILE_MAX];

	wm_autosave_location(filename);

	if (BLI_exists(filename)) {
		char str[FILE_MAX];
		BLI_make_file_string("/", str, BKE_tempdir_base(), BLENDER_QUIT_FILE);

		/* if global undo; remove tempsave, otherwise rename */
		if (U.uiflag & USER_GLOBALUNDO) BLI_delete(filename, false, false);
		else BLI_rename(filename, str);
	}
}

void wm_autosave_read(bContext *C, ReportList *reports)
{
	char filename[FILE_MAX];

	wm_autosave_location(filename);
	WM_file_read(C, filename, reports);
}


/** \name Initialize WM_OT_open_xxx properties
 *
 * Check if load_ui was set by the caller.
 * Fall back to user preference when file flags not specified.
 *
 * \{ */

void wm_open_init_load_ui(wmOperator *op, bool use_prefs)
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "load_ui");
	if (!RNA_property_is_set(op->ptr, prop)) {
		bool value = use_prefs ?
		             ((U.flag & USER_FILENOUI) == 0) :
		             ((G.fileflags & G_FILE_NO_UI) == 0);

		RNA_property_boolean_set(op->ptr, prop, value);
	}
}

void wm_open_init_use_scripts(wmOperator *op, bool use_prefs)
{
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
	if (!RNA_property_is_set(op->ptr, prop)) {
		/* use G_SCRIPT_AUTOEXEC rather than the userpref because this means if
		 * the flag has been disabled from the command line, then opening
		 * from the menu wont enable this setting. */
		bool value = use_prefs ?
		             ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) :
		             ((G.f & G_SCRIPT_AUTOEXEC) != 0);

		RNA_property_boolean_set(op->ptr, prop, value);
	}
}

/** \} */

void WM_file_tag_modified(void)
{
	wmWindowManager *wm = G_MAIN->wm.first;
	if (wm->file_saved) {
		wm->file_saved = 0;
		/* notifier that data changed, for save-over warning or header */
		WM_main_add_notifier(NC_WM | ND_DATACHANGED, NULL);
	}
}

/** \name Preferences/startup save & load.
 *
 * \{ */

/**
 * \see #wm_file_write wraps #BLO_write_file in a similar way.
 */
static int wm_homefile_write_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	char filepath[FILE_MAX];
	int fileflags;

	const char *app_template = U.app_template[0] ? U.app_template : NULL;
	const char * const cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, app_template);
	if (cfgdir == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Unable to create user config path");
		return OPERATOR_CANCELLED;
	}

	BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_PRE);

	/* check current window and close it if temp */
	if (win && win->screen->temp)
		wm_window_close(C, wm, win);

	/* update keymaps in user preferences */
	WM_keyconfig_update(wm);

	BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_STARTUP_FILE, NULL);

	printf("trying to save homefile at %s ", filepath);

	ED_editors_flush_edits(C, false);

	/*  force save as regular blend file */
	fileflags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_AUTOPLAY | G_FILE_HISTORY);

	if (BLO_write_file(bmain, filepath, fileflags | G_FILE_USERPREFS, op->reports, NULL) == 0) {
		printf("fail\n");
		return OPERATOR_CANCELLED;
	}

	printf("ok\n");

	G.save_over = 0;

	BLI_callback_exec(bmain, NULL, BLI_CB_EVT_SAVE_POST);

	return OPERATOR_FINISHED;
}

void WM_OT_save_homefile(wmOperatorType *ot)
{
	ot->name = "Save Startup File";
	ot->idname = "WM_OT_save_homefile";
	ot->description = "Make the current file the default .blend file, includes preferences";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_homefile_write_exec;
}

static int wm_userpref_autoexec_add_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	bPathCompare *path_cmp = MEM_callocN(sizeof(bPathCompare), "bPathCompare");
	BLI_addtail(&U.autoexec_paths, path_cmp);
	return OPERATOR_FINISHED;
}

void WM_OT_userpref_autoexec_path_add(wmOperatorType *ot)
{
	ot->name = "Add Autoexec Path";
	ot->idname = "WM_OT_userpref_autoexec_path_add";
	ot->description = "Add path to exclude from autoexecution";

	ot->exec = wm_userpref_autoexec_add_exec;

	ot->flag = OPTYPE_INTERNAL;
}

static int wm_userpref_autoexec_remove_exec(bContext *UNUSED(C), wmOperator *op)
{
	const int index = RNA_int_get(op->ptr, "index");
	bPathCompare *path_cmp = BLI_findlink(&U.autoexec_paths, index);
	if (path_cmp) {
		BLI_freelinkN(&U.autoexec_paths, path_cmp);
	}
	return OPERATOR_FINISHED;
}

void WM_OT_userpref_autoexec_path_remove(wmOperatorType *ot)
{
	ot->name = "Remove Autoexec Path";
	ot->idname = "WM_OT_userpref_autoexec_path_remove";
	ot->description = "Remove path to exclude from autoexecution";

	ot->exec = wm_userpref_autoexec_remove_exec;

	ot->flag = OPTYPE_INTERNAL;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, 1000);
}

/* Only save the prefs block. operator entry */
static int wm_userpref_write_exec(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	char filepath[FILE_MAX];
	const char *cfgdir;
	bool ok = true;

	/* update keymaps in user preferences */
	WM_keyconfig_update(wm);

	if ((cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL))) {
		bool ok_write;
		BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_USERPREF_FILE, NULL);
		printf("trying to save userpref at %s ", filepath);

		if (U.app_template[0]) {
			ok_write = BKE_blendfile_userdef_write_app_template(filepath, op->reports);
		}
		else {
			ok_write = BKE_blendfile_userdef_write(filepath, op->reports);
		}

		if (ok_write) {
			printf("ok\n");
		}
		else {
			printf("fail\n");
			ok = false;
		}
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "Unable to create userpref path");
	}

	if (U.app_template[0]) {
		if ((cfgdir = BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, U.app_template))) {
			/* Also save app-template prefs */
			BLI_path_join(filepath, sizeof(filepath), cfgdir, BLENDER_USERPREF_FILE, NULL);
			printf("trying to save app-template userpref at %s ", filepath);
			if (BKE_blendfile_userdef_write(filepath, op->reports) != 0) {
				printf("ok\n");
			}
			else {
				printf("fail\n");
				ok = false;
			}
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Unable to create app-template userpref path");
			ok = false;
		}
	}

	return ok ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_OT_save_userpref(wmOperatorType *ot)
{
	ot->name = "Save User Settings";
	ot->idname = "WM_OT_save_userpref";
	ot->description = "Save user preferences separately, overrides startup file preferences";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_userpref_write_exec;
}

static int wm_history_file_read_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
	ED_file_read_bookmarks();
	wm_history_file_read();
	return OPERATOR_FINISHED;
}

void WM_OT_read_history(wmOperatorType *ot)
{
	ot->name = "Reload History File";
	ot->idname = "WM_OT_read_history";
	ot->description = "Reloads history and bookmarks";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_history_file_read_exec;

	/* this operator is only used for loading settings from a previous blender install */
	ot->flag = OPTYPE_INTERNAL;
}

static int wm_homefile_read_exec(bContext *C, wmOperator *op)
{
	const bool use_factory_settings = (STREQ(op->type->idname, "WM_OT_read_factory_settings"));
	bool use_userdef = false;
	char filepath_buf[FILE_MAX];
	const char *filepath = NULL;

	if (!use_factory_settings) {
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");

		/* This can be used when loading of a start-up file should only change
		 * the scene content but keep the blender UI as it is. */
		wm_open_init_load_ui(op, true);
		SET_FLAG_FROM_TEST(G.fileflags, !RNA_boolean_get(op->ptr, "load_ui"), G_FILE_NO_UI);

		if (RNA_property_is_set(op->ptr, prop)) {
			RNA_property_string_get(op->ptr, prop, filepath_buf);
			filepath = filepath_buf;
			if (BLI_access(filepath, R_OK)) {
				BKE_reportf(op->reports, RPT_ERROR, "Can't read alternative start-up file: '%s'", filepath);
				return OPERATOR_CANCELLED;
			}
		}
	}
	else {
		/* always load UI for factory settings (prefs will re-init) */
		G.fileflags &= ~G_FILE_NO_UI;
		/* Always load preferences with factory settings. */
		use_userdef = true;
	}

	char app_template_buf[sizeof(U.app_template)];
	const char *app_template;
	PropertyRNA *prop_app_template = RNA_struct_find_property(op->ptr, "app_template");
	const bool use_splash = !use_factory_settings && RNA_boolean_get(op->ptr, "use_splash");
	const bool use_empty_data = RNA_boolean_get(op->ptr, "use_empty");

	if (prop_app_template && RNA_property_is_set(op->ptr, prop_app_template)) {
		RNA_property_string_get(op->ptr, prop_app_template, app_template_buf);
		app_template = app_template_buf;

		/* Always load preferences when switching templates. */
		use_userdef = true;
	}
	else {
		app_template = NULL;
	}

	if (wm_homefile_read(C, op->reports, use_factory_settings, use_empty_data, use_userdef, filepath, app_template)) {
		if (use_splash) {
			WM_init_splash(C);
		}
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void WM_OT_read_homefile(wmOperatorType *ot)
{
	PropertyRNA *prop;
	ot->name = "Reload Start-Up File";
	ot->idname = "WM_OT_read_homefile";
	ot->description = "Open the default file (doesn't save the current file)";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_homefile_read_exec;

	prop = RNA_def_string_file_path(ot->srna, "filepath", NULL,
	                                FILE_MAX, "File Path",
	                                "Path to an alternative start-up file");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	/* So scripts can use an alternative start-up file without the UI */
	prop = RNA_def_boolean(ot->srna, "load_ui", true, "Load UI",
	                       "Load user interface setup from the .blend file");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	prop = RNA_def_boolean(ot->srna, "use_empty", false, "Empty", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	/* So the splash can be kept open after loading a file (for templates). */
	prop = RNA_def_boolean(ot->srna, "use_splash", false, "Splash", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	prop = RNA_def_string(ot->srna, "app_template", "Template", sizeof(U.app_template), "", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	/* omit poll to run in background mode */
}

void WM_OT_read_factory_settings(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Load Factory Settings";
	ot->idname = "WM_OT_read_factory_settings";
	ot->description = "Load default file and user preferences";

	ot->invoke = WM_operator_confirm;
	ot->exec = wm_homefile_read_exec;

	prop = RNA_def_string(ot->srna, "app_template", "Template", sizeof(U.app_template), "", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	prop = RNA_def_boolean(ot->srna, "use_empty", false, "Empty", "");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

	/* omit poll to run in background mode */
}

/** \} */

/** \name Open main .blend file.
 *
 * \{ */

/**
 * Wrap #WM_file_read, shared by file reading operators.
 */
static bool wm_file_read_opwrap(bContext *C, const char *filepath, ReportList *reports,
                                const bool autoexec_init)
{
	bool success;

	/* XXX wm in context is not set correctly after WM_file_read -> crash */
	/* do it before for now, but is this correct with multiple windows? */
	WM_event_add_notifier(C, NC_WINDOW, NULL);

	if (autoexec_init) {
		WM_file_autoexec_init(filepath);
	}

	success = WM_file_read(C, filepath, reports);

	return success;
}

/* currently fits in a pointer */
struct FileRuntime {
	bool is_untrusted;
};

static int wm_open_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Main *bmain = CTX_data_main(C);
	const char *openname = BKE_main_blendfile_path(bmain);

	if (CTX_wm_window(C) == NULL) {
		/* in rare cases this could happen, when trying to invoke in background
		 * mode on load for example. Don't use poll for this because exec()
		 * can still run without a window */
		BKE_report(op->reports, RPT_ERROR, "Context window not set");
		return OPERATOR_CANCELLED;
	}

	/* if possible, get the name of the most recently used .blend file */
	if (G.recent_files.first) {
		struct RecentFile *recent = G.recent_files.first;
		openname = recent->filepath;
	}

	RNA_string_set(op->ptr, "filepath", openname);
	wm_open_init_load_ui(op, true);
	wm_open_init_use_scripts(op, true);
	op->customdata = NULL;

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int wm_open_mainfile_exec(bContext *C, wmOperator *op)
{
	char filepath[FILE_MAX];
	bool success;

	RNA_string_get(op->ptr, "filepath", filepath);

	/* re-use last loaded setting so we can reload a file without changing */
	wm_open_init_load_ui(op, false);
	wm_open_init_use_scripts(op, false);

	if (RNA_boolean_get(op->ptr, "load_ui"))
		G.fileflags &= ~G_FILE_NO_UI;
	else
		G.fileflags |= G_FILE_NO_UI;

	if (RNA_boolean_get(op->ptr, "use_scripts"))
		G.f |= G_SCRIPT_AUTOEXEC;
	else
		G.f &= ~G_SCRIPT_AUTOEXEC;

	success = wm_file_read_opwrap(C, filepath, op->reports, !(G.f & G_SCRIPT_AUTOEXEC));

	/* for file open also popup for warnings, not only errors */
	BKE_report_print_level_set(op->reports, RPT_WARNING);

	if (success) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static bool wm_open_mainfile_check(bContext *UNUSED(C), wmOperator *op)
{
	struct FileRuntime *file_info = (struct FileRuntime *)&op->customdata;
	PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_scripts");
	bool is_untrusted = false;
	char path[FILE_MAX];
	char *lslash;

	RNA_string_get(op->ptr, "filepath", path);

	/* get the dir */
	lslash = (char *)BLI_last_slash(path);
	if (lslash) *(lslash + 1) = '\0';

	if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) {
		if (BKE_autoexec_match(path) == true) {
			RNA_property_boolean_set(op->ptr, prop, false);
			is_untrusted = true;
		}
	}

	if (file_info) {
		file_info->is_untrusted = is_untrusted;
	}

	return is_untrusted;
}

static void wm_open_mainfile_ui(bContext *UNUSED(C), wmOperator *op)
{
	struct FileRuntime *file_info = (struct FileRuntime *)&op->customdata;
	uiLayout *layout = op->layout;
	uiLayout *col = op->layout;
	const char *autoexec_text;

	uiItemR(layout, op->ptr, "load_ui", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, false);
	if (file_info->is_untrusted) {
		autoexec_text = IFACE_("Trusted Source [Untrusted Path]");
		uiLayoutSetActive(col, false);
		uiLayoutSetEnabled(col, false);
	}
	else {
		autoexec_text = IFACE_("Trusted Source");
	}

	uiItemR(col, op->ptr, "use_scripts", 0, autoexec_text, ICON_NONE);
}

void WM_OT_open_mainfile(wmOperatorType *ot)
{
	ot->name = "Open Blender File";
	ot->idname = "WM_OT_open_mainfile";
	ot->description = "Open a Blender file";

	ot->invoke = wm_open_mainfile_invoke;
	ot->exec = wm_open_mainfile_exec;
	ot->check = wm_open_mainfile_check;
	ot->ui = wm_open_mainfile_ui;
	/* omit window poll so this can work in background mode */

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER, FILE_BLENDER, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_boolean(ot->srna, "load_ui", true, "Load UI", "Load user interface setup in the .blend file");
	RNA_def_boolean(ot->srna, "use_scripts", true, "Trusted Source",
	                "Allow .blend file to execute scripts automatically, default available from system preferences");
}

/** \} */

/** \name Reload (revert) main .blend file.
 *
 * \{ */

static int wm_revert_mainfile_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	bool success;
	char filepath[FILE_MAX];

	wm_open_init_use_scripts(op, false);

	if (RNA_boolean_get(op->ptr, "use_scripts"))
		G.f |= G_SCRIPT_AUTOEXEC;
	else
		G.f &= ~G_SCRIPT_AUTOEXEC;

	BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
	success = wm_file_read_opwrap(C, filepath, op->reports, !(G.f & G_SCRIPT_AUTOEXEC));

	if (success) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static bool wm_revert_mainfile_poll(bContext *UNUSED(C))
{
	return G.relbase_valid;
}

void WM_OT_revert_mainfile(wmOperatorType *ot)
{
	ot->name = "Revert";
	ot->idname = "WM_OT_revert_mainfile";
	ot->description = "Reload the saved file";
	ot->invoke = WM_operator_confirm;

	RNA_def_boolean(ot->srna, "use_scripts", true, "Trusted Source",
	                "Allow .blend file to execute scripts automatically, default available from system preferences");

	ot->exec = wm_revert_mainfile_exec;
	ot->poll = wm_revert_mainfile_poll;
}

/** \} */

/** \name Recover last session & auto-save.
 *
 * \{ */

void WM_recover_last_session(bContext *C, ReportList *reports)
{
	Main *bmain = CTX_data_main(C);
	char filepath[FILE_MAX];

	BLI_make_file_string("/", filepath, BKE_tempdir_base(), BLENDER_QUIT_FILE);
	/* if reports==NULL, it's called directly without operator, we add a quick check here */
	if (reports || BLI_exists(filepath)) {
		G.fileflags |= G_FILE_RECOVER;

		wm_file_read_opwrap(C, filepath, reports, true);

		G.fileflags &= ~G_FILE_RECOVER;

		/* XXX bad global... fixme */
		if (BKE_main_blendfile_path(bmain)[0] != '\0') {
			G.file_loaded = 1;	/* prevents splash to show */
		}
		else {
			G.relbase_valid = 0;
			G.save_over = 0;    /* start with save preference untitled.blend */
		}

	}
}

static int wm_recover_last_session_exec(bContext *C, wmOperator *op)
{
	WM_recover_last_session(C, op->reports);
	return OPERATOR_FINISHED;
}

void WM_OT_recover_last_session(wmOperatorType *ot)
{
	ot->name = "Recover Last Session";
	ot->idname = "WM_OT_recover_last_session";
	ot->description = "Open the last closed file (\"" BLENDER_QUIT_FILE "\")";
	ot->invoke = WM_operator_confirm;

	ot->exec = wm_recover_last_session_exec;
}

static int wm_recover_auto_save_exec(bContext *C, wmOperator *op)
{
	char filepath[FILE_MAX];
	bool success;

	RNA_string_get(op->ptr, "filepath", filepath);

	G.fileflags |= G_FILE_RECOVER;

	success = wm_file_read_opwrap(C, filepath, op->reports, true);

	G.fileflags &= ~G_FILE_RECOVER;

	if (success) {
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int wm_recover_auto_save_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	char filename[FILE_MAX];

	wm_autosave_location(filename);
	RNA_string_set(op->ptr, "filepath", filename);
	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void WM_OT_recover_auto_save(wmOperatorType *ot)
{
	ot->name = "Recover Auto Save";
	ot->idname = "WM_OT_recover_auto_save";
	ot->description = "Open an automatically saved file to recover it";

	ot->exec = wm_recover_auto_save_exec;
	ot->invoke = wm_recover_auto_save_invoke;

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_BLENDER, FILE_BLENDER, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH, FILE_LONGDISPLAY, FILE_SORT_TIME);
}

/** \} */

/** \name Save main .blend file.
 *
 * \{ */

static void wm_filepath_default(char *filepath)
{
	if (G.save_over == false) {
		BLI_ensure_filename(filepath, FILE_MAX, "untitled.blend");
	}
}

static void save_set_compress(wmOperator *op)
{
	PropertyRNA *prop;

	prop = RNA_struct_find_property(op->ptr, "compress");
	if (!RNA_property_is_set(op->ptr, prop)) {
		if (G.save_over) {  /* keep flag for existing file */
			RNA_property_boolean_set(op->ptr, prop, (G.fileflags & G_FILE_COMPRESS) != 0);
		}
		else {  /* use userdef for new file */
			RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_FILECOMPRESS) != 0);
		}
	}
}

static void save_set_filepath(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	PropertyRNA *prop;
	char name[FILE_MAX];

	prop = RNA_struct_find_property(op->ptr, "filepath");
	if (!RNA_property_is_set(op->ptr, prop)) {
		/* if not saved before, get the name of the most recently used .blend file */
		if (BKE_main_blendfile_path(bmain)[0] == '\0' && G.recent_files.first) {
			struct RecentFile *recent = G.recent_files.first;
			BLI_strncpy(name, recent->filepath, FILE_MAX);
		}
		else {
			BLI_strncpy(name, bmain->name, FILE_MAX);
		}

		wm_filepath_default(name);
		RNA_property_string_set(op->ptr, prop, name);
	}
}

static int wm_save_as_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{

	save_set_compress(op);
	save_set_filepath(C, op);

	WM_event_add_fileselect(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* function used for WM_OT_save_mainfile too */
static int wm_save_as_mainfile_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	char path[FILE_MAX];
	int fileflags;

	save_set_compress(op);

	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		RNA_string_get(op->ptr, "filepath", path);
	}
	else {
		BLI_strncpy(path, BKE_main_blendfile_path(bmain), FILE_MAX);
		wm_filepath_default(path);
	}

	fileflags = G.fileflags & ~G_FILE_USERPREFS;

	/* set compression flag */
	SET_FLAG_FROM_TEST(
	        fileflags, RNA_boolean_get(op->ptr, "compress"),
	        G_FILE_COMPRESS);
	SET_FLAG_FROM_TEST(
	        fileflags, RNA_boolean_get(op->ptr, "relative_remap"),
	        G_FILE_RELATIVE_REMAP);
	SET_FLAG_FROM_TEST(
	        fileflags,
	        (RNA_struct_property_is_set(op->ptr, "copy") &&
	         RNA_boolean_get(op->ptr, "copy")),
	        G_FILE_SAVE_COPY);

	if (wm_file_write(C, path, fileflags, op->reports) != 0)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_WM | ND_FILESAVE, NULL);

	if (RNA_boolean_get(op->ptr, "exit")) {
		wm_exit_schedule_delayed(C);
	}

	return OPERATOR_FINISHED;
}

/* function used for WM_OT_save_mainfile too */
static bool blend_save_check(bContext *UNUSED(C), wmOperator *op)
{
	char filepath[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filepath);
	if (!BLO_has_bfile_extension(filepath)) {
		/* some users would prefer BLI_path_extension_replace(),
		 * we keep getting nitpicking bug reports about this - campbell */
		BLI_path_extension_ensure(filepath, FILE_MAX, ".blend");
		RNA_string_set(op->ptr, "filepath", filepath);
		return true;
	}
	return false;
}

void WM_OT_save_as_mainfile(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Save As Blender File";
	ot->idname = "WM_OT_save_as_mainfile";
	ot->description = "Save the current file in the desired location";

	ot->invoke = wm_save_as_mainfile_invoke;
	ot->exec = wm_save_as_mainfile_exec;
	ot->check = blend_save_check;
	/* omit window poll so this can work in background mode */

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER, FILE_BLENDER, FILE_SAVE,
	        WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
	RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
	RNA_def_boolean(ot->srna, "relative_remap", true, "Remap Relative",
	                "Remap relative paths when saving in a different directory");
	prop = RNA_def_boolean(ot->srna, "copy", false, "Save Copy",
	                "Save a copy of the actual working state but does not make saved file active");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int wm_save_mainfile_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	int ret;

	/* cancel if no active window */
	if (CTX_wm_window(C) == NULL)
		return OPERATOR_CANCELLED;

	save_set_compress(op);
	save_set_filepath(C, op);

	/* if we're saving for the first time and prefer relative paths - any existing paths will be absolute,
	 * enable the option to remap paths to avoid confusion [#37240] */
	if ((G.relbase_valid == false) && (U.flag & USER_RELPATHS)) {
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, "relative_remap");
		if (!RNA_property_is_set(op->ptr, prop)) {
			RNA_property_boolean_set(op->ptr, prop, true);
		}
	}

	if (G.save_over) {
		char path[FILE_MAX];

		RNA_string_get(op->ptr, "filepath", path);
		if (RNA_boolean_get(op->ptr, "check_existing") && BLI_exists(path)) {
			ret = WM_operator_confirm_message_ex(C, op, IFACE_("Save Over?"), ICON_QUESTION, path);
		}
		else {
			ret = wm_save_as_mainfile_exec(C, op);
			/* Without this there is no feedback the file was saved. */
			BKE_reportf(op->reports, RPT_INFO, "Saved \"%s\"", BLI_path_basename(path));
		}
	}
	else {
		WM_event_add_fileselect(C, op);
		ret = OPERATOR_RUNNING_MODAL;
	}

	return ret;
}

void WM_OT_save_mainfile(wmOperatorType *ot)
{
	ot->name = "Save Blender File";
	ot->idname = "WM_OT_save_mainfile";
	ot->description = "Save the current Blender file";

	ot->invoke = wm_save_mainfile_invoke;
	ot->exec = wm_save_as_mainfile_exec;
	ot->check = blend_save_check;
	/* omit window poll so this can work in background mode */

	PropertyRNA *prop;
	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_BLENDER, FILE_BLENDER, FILE_SAVE,
	        WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
	RNA_def_boolean(ot->srna, "compress", false, "Compress", "Write compressed .blend file");
	RNA_def_boolean(ot->srna, "relative_remap", false, "Remap Relative",
	                "Remap relative paths when saving in a different directory");

	prop = RNA_def_boolean(ot->srna, "exit", false, "Exit", "Exit Blender after saving");
	RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */
