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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_init_exit.c
 *  \ingroup wm
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_report.h"

#include "BKE_packedFile.h"
#include "BKE_sequencer.h" /* free seq clipboard */
#include "BKE_material.h" /* clear_matcopybuf */
#include "BKE_tracking.h" /* free tracking clipboard */

#include "BLI_listbase.h"
// #include "BLI_scanfill.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "RE_engine.h"
#include "RE_pipeline.h"        /* RE_ free stuff */

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#ifdef WITH_GAMEENGINE
#include "BL_System.h"
#endif
#include "GHOST_Path-api.h"
#include "GHOST_C-api.h"

#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_cursors.h"
#include "wm_event_system.h"
#include "wm.h"
#include "wm_files.h"
#include "wm_window.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_node.h"
#include "ED_render.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "BLF_api.h"
#include "BLF_translation.h"

#include "GPU_buffers.h"
#include "GPU_extensions.h"
#include "GPU_draw.h"

#include "BKE_depsgraph.h"
#include "BKE_sound.h"

static void wm_init_reports(bContext *C)
{
	BKE_reports_init(CTX_wm_reports(C), RPT_STORE);
}
static void wm_free_reports(bContext *C)
{
	BKE_reports_clear(CTX_wm_reports(C));
}

int wm_start_with_console = 0; /* used in creator.c */

/* only called once, for startup */
void WM_init(bContext *C, int argc, const char **argv)
{
	if (!G.background) {
		wm_ghost_init(C);   /* note: it assigns C to ghost! */
		wm_init_cursor_data();
	}
	GHOST_CreateSystemPaths();
	wm_operatortype_init();
	WM_menutype_init();

	set_free_windowmanager_cb(wm_close_and_free);   /* library.c */
	set_blender_test_break_cb(wm_window_testbreak); /* blender.c */
	DAG_editors_update_cb(ED_render_id_flush_update, ED_render_scene_update); /* depsgraph.c */
	
	ED_spacetypes_init();   /* editors/space_api/spacetype.c */
	
	ED_file_init();         /* for fsmenu */
	ED_init_node_butfuncs();	
	
	BLF_init(11, U.dpi); /* Please update source/gamengine/GamePlayer/GPG_ghost.cpp if you change this */
	BLF_lang_init();
	/* get the default database, plus a wm */
	WM_read_homefile(C, NULL, G.factory_startup);

	BLF_lang_set(NULL);

	/* note: there is a bug where python needs initializing before loading the
	 * startup.blend because it may contain PyDrivers. It also needs to be after
	 * initializing space types and other internal data.
	 *
	 * However cant redo this at the moment. Solution is to load python
	 * before WM_read_homefile() or make py-drivers check if python is running.
	 * Will try fix when the crash can be repeated. - campbell. */

#ifdef WITH_PYTHON
	BPY_context_set(C); /* necessary evil */
	BPY_python_start(argc, argv);

	BPY_driver_reset();
	BPY_app_handlers_reset(FALSE); /* causes addon callbacks to be freed [#28068],
	                                * but this is actually what we want. */
	BPY_modules_load_user(C);
#else
	(void)argc; /* unused */
	(void)argv; /* unused */
#endif

	if (!G.background && !wm_start_with_console)
		GHOST_toggleConsole(3);

	wm_init_reports(C); /* reports cant be initialized before the wm */

	if (!G.background) {
		GPU_extensions_init();
		GPU_set_mipmap(!(U.gameflags & USER_DISABLE_MIPMAP));
		GPU_set_anisotropic(U.anisotropic_filter);
	
		UI_init();
	}
	
	clear_matcopybuf();
	ED_render_clear_mtex_copybuf();

	//	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		
	ED_preview_init_dbase();
	
	WM_read_history();

	/* allow a path of "", this is what happens when making a new file */
#if 0
	if (G.main->name[0] == 0)
		BLI_make_file_string("/", G.main->name, BLI_getDefaultDocumentFolder(), "untitled.blend");
#endif

	BLI_strncpy(G.lib, G.main->name, FILE_MAX);
}

void WM_init_splash(bContext *C)
{
	if ((U.uiflag & USER_SPLASH_DISABLE) == 0) {
		wmWindowManager *wm = CTX_wm_manager(C);
		wmWindow *prevwin = CTX_wm_window(C);
	
		if (wm->windows.first) {
			CTX_wm_window_set(C, wm->windows.first);
			WM_operator_name_call(C, "WM_OT_splash", WM_OP_INVOKE_DEFAULT, NULL);
			CTX_wm_window_set(C, prevwin);
		}
	}
}

int WM_init_game(bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win;

	ScrArea *sa;
	ARegion *ar = NULL;

	Scene *scene = CTX_data_scene(C);

	if (!scene) {
		// XXX, this should not be needed.
		Main *bmain = CTX_data_main(C);
		scene = bmain->scene.first;
	}

	win = wm->windows.first;

	//first to get a valid window
	if (win)
		CTX_wm_window_set(C, win);

	sa = BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_VIEW3D, 0);
	ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);

	// if we have a valid 3D view
	if (sa && ar) {
		ARegion *arhide;

		CTX_wm_area_set(C, sa);
		CTX_wm_region_set(C, ar);

		/* disable quad view */
		if (ar->alignment == RGN_ALIGN_QSPLIT)
			WM_operator_name_call(C, "SCREEN_OT_region_quadview", WM_OP_EXEC_DEFAULT, NULL);

		/* toolbox, properties panel and header are hidden */
		for (arhide = sa->regionbase.first; arhide; arhide = arhide->next) {
			if (arhide->regiontype != RGN_TYPE_WINDOW) {
				if (!(arhide->flag & RGN_FLAG_HIDDEN)) {
					ED_region_toggle_hidden(C, arhide);
				}
			}
		}

		/* full screen the area */
		if (!sa->full) {
			ED_screen_full_toggle(C, win, sa);
		}

		/* Fullscreen */
		if ((scene->gm.playerflag & GAME_PLAYER_FULLSCREEN)) {
			WM_operator_name_call(C, "WM_OT_window_fullscreen_toggle", WM_OP_EXEC_DEFAULT, NULL);
			wm_get_screensize(&ar->winrct.xmax, &ar->winrct.ymax);
			ar->winx = ar->winrct.xmax + 1;
			ar->winy = ar->winrct.ymax + 1;
		}
		else {
			GHOST_RectangleHandle rect = GHOST_GetClientBounds(win->ghostwin);
			ar->winrct.ymax = GHOST_GetHeightRectangle(rect);
			ar->winrct.xmax = GHOST_GetWidthRectangle(rect);
			ar->winx = ar->winrct.xmax + 1;
			ar->winy = ar->winrct.ymax + 1;
			GHOST_DisposeRectangle(rect);
		}

		WM_operator_name_call(C, "VIEW3D_OT_game_start", WM_OP_EXEC_DEFAULT, NULL);

		sound_exit();

		return 1;
	}
	else {
		ReportTimerInfo *rti;

		BKE_report(&wm->reports, RPT_ERROR, "No valid 3D View found. Game auto start is not possible.");

		/* After adding the report to the global list, reset the report timer. */
		WM_event_remove_timer(wm, NULL, wm->reports.reporttimer);

		/* Records time since last report was added */
		wm->reports.reporttimer = WM_event_add_timer(wm, CTX_wm_window(C), TIMER, 0.02);

		rti = MEM_callocN(sizeof(ReportTimerInfo), "ReportTimerInfo");
		wm->reports.reporttimer->customdata = rti;
	}
	return 0;
}

/* free strings of open recent files */
static void free_openrecent(void)
{
	struct RecentFile *recent;
	
	for (recent = G.recent_files.first; recent; recent = recent->next)
		MEM_freeN(recent->filepath);
	
	BLI_freelistN(&(G.recent_files));
}


/* bad stuff*/

// XXX copy/paste buffer stuff...
extern void free_anim_copybuf(void); 
extern void free_anim_drivers_copybuf(void); 
extern void free_fmodifiers_copybuf(void); 
extern void free_posebuf(void); 

/* called in creator.c even... tsk, split this! */
/* note, doesnt run exit() call WM_exit() for that */
void WM_exit_ext(bContext *C, const short do_python)
{
	wmWindow *win;

	sound_exit();


	/* first wrap up running stuff, we assume only the active WM is running */
	/* modal handlers are on window level freed, others too? */
	/* note; same code copied in wm_files.c */
	if (C && CTX_wm_manager(C)) {
		
		WM_jobs_stop_all(CTX_wm_manager(C));
		
		for (win = CTX_wm_manager(C)->windows.first; win; win = win->next) {
			
			CTX_wm_window_set(C, win);  /* needed by operator close callbacks */
			WM_event_remove_handlers(C, &win->handlers);
			WM_event_remove_handlers(C, &win->modalhandlers);
			ED_screen_exit(C, win, win->screen);
		}
	}
	wm_operatortype_free();
	wm_dropbox_free();
	WM_menutype_free();
	
	/* all non-screen and non-space stuff editors did, like editmode */
	if (C)
		ED_editors_exit(C);

//	XXX	
//	BIF_GlobalReebFree();
//	BIF_freeRetarget();
	BIF_freeTemplates(C);
	
	BKE_vfont_free_global_ttf(); /* bke_font.h */

	free_openrecent();
	
	BKE_mball_cubeTable_free();
	
	ED_preview_free_dbase();  /* frees a Main dbase, before free_blender! */

	if (C && CTX_wm_manager(C))
		wm_free_reports(C);  /* before free_blender! - since the ListBases get freed there */

	seq_free_clipboard(); /* sequencer.c */
	BKE_tracking_free_clipboard();
		
	free_blender();  /* blender.c, does entire library and spacetypes */
//	free_matcopybuf();
	free_anim_copybuf();
	free_anim_drivers_copybuf();
	free_fmodifiers_copybuf();
	free_posebuf();

	BLF_exit();

#ifdef WITH_INTERNATIONAL
	BLF_free_unifont();
#endif
	
	ANIM_keyingset_infos_exit();
	
	RE_FreeAllRender();
	RE_engines_exit();
	
//	free_txt_data();
	

#ifdef WITH_PYTHON
	/* option not to close python so we can use 'atexit' */
	if (do_python) {
		/* XXX - old note */
		/* before free_blender so py's gc happens while library still exists */
		/* needed at least for a rare sigsegv that can happen in pydrivers */

		/* Update for blender 2.5, move after free_blender because blender now holds references to PyObject's
		 * so decref'ing them after python ends causes bad problems every time
		 * the pyDriver bug can be fixed if it happens again we can deal with it then */
		BPY_python_end();
	}
#else
	(void)do_python;
#endif

	GPU_global_buffer_pool_free();
	GPU_free_unused_buffers();
	GPU_extensions_exit();

	if (!G.background) {
		BKE_undo_save_quit();  /* saves quit.blend if global undo is on */
	}
	BKE_reset_undo(); 
	
	ED_file_exit(); /* for fsmenu */

	UI_exit();
	BKE_userdef_free();

	RNA_exit(); /* should be after BPY_python_end so struct python slots are cleared */
	
	wm_ghost_exit();

	CTX_free(C);
#ifdef WITH_GAMEENGINE
	SYS_DeleteSystem(SYS_GetSystem());
#endif
	
	GHOST_DisposeSystemPaths();

	if (MEM_get_memory_blocks_in_use() != 0) {
		printf("Error: Not freed memory blocks: %d\n", MEM_get_memory_blocks_in_use());
		MEM_printmemlist();
	}
	wm_autosave_delete();
	
	printf("\nBlender quit\n");
	
#ifdef WIN32   
	/* ask user to press enter when in debug mode */
	if (G.debug & G_DEBUG) {
		printf("press enter key to exit...\n\n");
		getchar();
	}
#endif 
}

void WM_exit(bContext *C)
{
	WM_exit_ext(C, 1);
	exit(G.afbreek == 1);
}
