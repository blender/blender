/*
 * $Id$
 *
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
 */


	/* placed up here because of crappy
	 * winsock stuff.
	 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef WIN32
#include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#endif
#include <shlobj.h> /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff because 'near' is disabled through BLI_windstuff */
#include <process.h> /* getpid */
#include "BLI_winstuff.h"
#else
#include <unistd.h> /* getpid */
#endif

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_ipo_types.h" // XXX old animation system
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_exotic.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_sound.h"
#include "BKE_texture.h"


#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "RNA_access.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "ED_datafiles.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "ED_util.h"

#include "GHOST_C-api.h"
#include "GHOST_Path-api.h"

#include "UI_interface.h"

#include "GPU_draw.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_files.h"
#include "wm_window.h"
#include "wm_event_system.h"

static void write_history(void);

/* To be able to read files without windows closing, opening, moving 
   we try to prepare for worst case:
   - active window gets active screen from file 
   - restoring the screens from non-active windows 
   Best case is all screens match, in that case they get assigned to proper window  
*/
static void wm_window_match_init(bContext *C, ListBase *wmlist)
{
	wmWindowManager *wm;
	wmWindow *win, *active_win;
	
	*wmlist= G.main->wm;
	G.main->wm.first= G.main->wm.last= NULL;
	
	active_win = CTX_wm_window(C);

	/* first wrap up running stuff */
	/* code copied from wm_init_exit.c */
	for(wm= wmlist->first; wm; wm= wm->id.next) {
		
		WM_jobs_stop_all(wm);
		
		for(win= wm->windows.first; win; win= win->next) {
		
			CTX_wm_window_set(C, win);	/* needed by operator close callbacks */
			WM_event_remove_handlers(C, &win->handlers);
			WM_event_remove_handlers(C, &win->modalhandlers);
			ED_screen_exit(C, win, win->screen);
		}
	}
	
	/* reset active window */
	CTX_wm_window_set(C, active_win);

	ED_editors_exit(C);
	
return;	
	if(wm==NULL) return;
	if(G.fileflags & G_FILE_NO_UI) return;
	
	/* we take apart the used screens from non-active window */
	for(win= wm->windows.first; win; win= win->next) {
		BLI_strncpy(win->screenname, win->screen->id.name, MAX_ID_NAME);
		if(win!=wm->winactive) {
			BLI_remlink(&G.main->screen, win->screen);
			//BLI_addtail(screenbase, win->screen);
		}
	}
}

/* match old WM with new, 4 cases:
  1- no current wm, no read wm: make new default
  2- no current wm, but read wm: that's OK, do nothing
  3- current wm, but not in file: try match screen names
  4- current wm, and wm in file: try match ghostwin 
*/

static void wm_window_match_do(bContext *C, ListBase *oldwmlist)
{
	wmWindowManager *oldwm, *wm;
	wmWindow *oldwin, *win;
	
	/* cases 1 and 2 */
	if(oldwmlist->first==NULL) {
		if(G.main->wm.first); /* nothing todo */
		else
			wm_add_default(C);
	}
	else {
		/* cases 3 and 4 */
		
		/* we've read file without wm..., keep current one entirely alive */
		if(G.main->wm.first==NULL) {
			/* when loading without UI, no matching needed */
			if(!(G.fileflags & G_FILE_NO_UI)) {
				bScreen *screen= CTX_wm_screen(C);

				/* match oldwm to new dbase, only old files */
				for(wm= oldwmlist->first; wm; wm= wm->id.next) {
					
					for(win= wm->windows.first; win; win= win->next) {
						/* all windows get active screen from file */
						if(screen->winid==0)
							win->screen= screen;
						else 
							win->screen= ED_screen_duplicate(win, screen);
						
						BLI_strncpy(win->screenname, win->screen->id.name+2, sizeof(win->screenname));
						win->screen->winid= win->winid;
					}
				}
			}
			
			G.main->wm= *oldwmlist;
			
			/* screens were read from file! */
			ED_screens_initialize(G.main->wm.first);
		}
		else {
			/* what if old was 3, and loaded 1? */
			/* this code could move to setup_appdata */
			oldwm= oldwmlist->first;
			wm= G.main->wm.first;

			/* ensure making new keymaps and set space types */
			wm->initialized= 0;
			wm->winactive= NULL;
			
			/* only first wm in list has ghostwins */
			for(win= wm->windows.first; win; win= win->next) {
				for(oldwin= oldwm->windows.first; oldwin; oldwin= oldwin->next) {
					
					if(oldwin->winid == win->winid ) {
						win->ghostwin= oldwin->ghostwin;
						win->active= oldwin->active;
						if(win->active)
							wm->winactive= win;

						if(!G.background) /* file loading in background mode still calls this */
							GHOST_SetWindowUserData(win->ghostwin, win);	/* pointer back */

						oldwin->ghostwin= NULL;
						
						win->eventstate= oldwin->eventstate;
						oldwin->eventstate= NULL;
						
						/* ensure proper screen rescaling */
						win->sizex= oldwin->sizex;
						win->sizey= oldwin->sizey;
						win->posx= oldwin->posx;
						win->posy= oldwin->posy;
					}
				}
			}
			wm_close_and_free_all(C, oldwmlist);
		}
	}
}

/* in case UserDef was read, we re-initialize all, and do versioning */
static void wm_init_userdef(bContext *C)
{
	UI_init_userdef();
	MEM_CacheLimiter_set_maximum(U.memcachelimit * 1024 * 1024);
	sound_init(CTX_data_main(C));

	/* needed so loading a file from the command line respects user-pref [#26156] */
	if(U.flag & USER_FILENOUI)	G.fileflags |= G_FILE_NO_UI;
	else						G.fileflags &= ~G_FILE_NO_UI;

	/* set the python auto-execute setting from user prefs */
	/* disabled by default, unless explicitly enabled in the command line */
	if ((U.flag & USER_SCRIPT_AUTOEXEC_DISABLE) == 0) G.f |=  G_SCRIPT_AUTOEXEC;
	if(U.tempdir[0]) BLI_where_is_temp(btempdir, FILE_MAX, 1);
}

void WM_read_file(bContext *C, const char *name, ReportList *reports)
{
	int retval;

	/* so we can get the error message */
	errno = 0;

	WM_cursor_wait(1);

	/* first try to append data from exotic file formats... */
	/* it throws error box when file doesnt exist and returns -1 */
	/* note; it should set some error message somewhere... (ton) */
	retval= BKE_read_exotic(CTX_data_scene(C), name);
	
	/* we didn't succeed, now try to read Blender file */
	if (retval == BKE_READ_EXOTIC_OK_BLEND) {
		int G_f= G.f;
		ListBase wmbase;

		/* put aside screens to match with persistant windows later */
		/* also exit screens and editors */
		wm_window_match_init(C, &wmbase); 
		
		retval= BKE_read_file(C, name, reports);
		G.save_over = 1;

		/* this flag is initialized by the operator but overwritten on read.
		 * need to re-enable it here else drivers + registered scripts wont work. */
		if(G_f & G_SCRIPT_AUTOEXEC) G.f |= G_SCRIPT_AUTOEXEC;
		else						G.f &= ~G_SCRIPT_AUTOEXEC;

		/* match the read WM with current WM */
		wm_window_match_do(C, &wmbase);
		WM_check(C); /* opens window(s), checks keymaps */
		
// XXX		mainwindow_set_filename_to_title(G.main->name);

		if(retval == BKE_READ_FILE_OK_USERPREFS) wm_init_userdef(C);	// in case a userdef is read from regular .blend
		
		if (retval != BKE_READ_FILE_FAIL) {
			G.relbase_valid = 1;
			if(!G.background) /* assume automated tasks with background, dont write recent file list */
				write_history();
		}


		WM_event_add_notifier(C, NC_WM|ND_FILEREAD, NULL);
//		refresh_interface_font();

		CTX_wm_window_set(C, CTX_wm_manager(C)->windows.first);

		ED_editors_init(C);
		DAG_on_load_update(CTX_data_main(C), TRUE);

#ifdef WITH_PYTHON
		/* run any texts that were loaded in and flagged as modules */
		BPY_driver_reset();
		BPY_modules_load_user(C);
#endif
		CTX_wm_window_set(C, NULL); /* exits queues */

		// XXX		undo_editmode_clear();
		BKE_reset_undo();
		BKE_write_undo(C, "original");	/* save current state */
		
	}
	else if(retval == BKE_READ_EXOTIC_OK_OTHER)
		BKE_write_undo(C, "Import file");
	else if(retval == BKE_READ_EXOTIC_FAIL_OPEN) {
		BKE_reportf(reports, RPT_ERROR, "Can't read file: \"%s\", %s.", name, errno ? strerror(errno) : "Unable to open the file");
	}
	else if(retval == BKE_READ_EXOTIC_FAIL_FORMAT) {
		BKE_reportf(reports, RPT_ERROR, "File format is not supported in file: \"%s\".", name);
	}
	else if(retval == BKE_READ_EXOTIC_FAIL_PATH) {
		BKE_reportf(reports, RPT_ERROR, "File path invalid: \"%s\".", name);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "Unknown error loading: \"%s\".", name);
		BLI_assert(!"invalid 'retval'");
	}

	WM_cursor_wait(0);

}


/* called on startup,  (context entirely filled with NULLs) */
/* or called for 'New File' */
/* op can be NULL */
int WM_read_homefile(bContext *C, ReportList *reports, short from_memory)
{
	ListBase wmbase;
	char tstr[FILE_MAXDIR+FILE_MAXFILE];
	int success= 0;
	
	free_ttfont(); /* still weird... what does it here? */
		
	G.relbase_valid = 0;
	if (!from_memory) {
		char *cfgdir = BLI_get_folder(BLENDER_USER_CONFIG, NULL);
		if (cfgdir) {
			BLI_make_file_string(G.main->name, tstr, cfgdir, BLENDER_STARTUP_FILE);
		} else {
			tstr[0] = '\0';
			from_memory = 1;
			BKE_report(reports, RPT_INFO, "Config directory with "STRINGIFY(BLENDER_STARTUP_FILE)" file not found.");
		}
	}
	
	/* prevent loading no UI */
	G.fileflags &= ~G_FILE_NO_UI;
	
	/* put aside screens to match with persistant windows later */
	wm_window_match_init(C, &wmbase); 
	
	if (!from_memory && BLI_exists(tstr)) {
		success = (BKE_read_file(C, tstr, NULL) != BKE_READ_FILE_FAIL);
		
		if(U.themes.first==NULL) {
			printf("\nError: No valid "STRINGIFY(BLENDER_STARTUP_FILE)", fall back to built-in default.\n\n");
			success = 0;
		}
	}
	if(success==0) {
		success = BKE_read_file_from_memory(C, datatoc_startup_blend, datatoc_startup_blend_size, NULL);
		if (wmbase.first == NULL) wm_clear_default_size(C);
	}
	
	/* prevent buggy files that had G_FILE_RELATIVE_REMAP written out by mistake. Screws up autosaves otherwise
	 * can remove this eventually, only in a 2.53 and older, now its not written */
	G.fileflags &= ~G_FILE_RELATIVE_REMAP;

	/* match the read WM with current WM */
	wm_window_match_do(C, &wmbase); 
	WM_check(C); /* opens window(s), checks keymaps */

	G.main->name[0]= '\0';

	wm_init_userdef(C);
	
	/* When loading factory settings, the reset solid OpenGL lights need to be applied. */
	if (!G.background) GPU_default_lights();
	
	/* XXX */
	G.save_over = 0;	// start with save preference untitled.blend
	G.fileflags &= ~G_FILE_AUTOPLAY;	/*  disable autoplay in startup.blend... */
//	mainwindow_set_filename_to_title("");	// empty string re-initializes title to "Blender"
	
//	refresh_interface_font();
	
//	undo_editmode_clear();
	BKE_reset_undo();
	BKE_write_undo(C, "original");	/* save current state */

	ED_editors_init(C);
	DAG_on_load_update(CTX_data_main(C), TRUE);

#ifdef WITH_PYTHON
	if(CTX_py_init_get(C)) {
		/* sync addons, these may have changed from the defaults */
		BPY_string_exec(C, "__import__('addon_utils').reset_all()");

		BPY_driver_reset();
		BPY_modules_load_user(C);
	}
#endif

	WM_event_add_notifier(C, NC_WM|ND_FILEREAD, NULL);

	/* in background mode the scene will stay NULL */
	if(!G.background) {
		CTX_wm_window_set(C, NULL); /* exits queues */
	}

	return TRUE;
}

int WM_read_homefile_exec(bContext *C, wmOperator *op)
{
	int from_memory= strcmp(op->type->idname, "WM_OT_read_factory_settings") == 0;
	return WM_read_homefile(C, op->reports, from_memory) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void WM_read_history(void)
{
	char name[FILE_MAX];
	LinkNode *l, *lines;
	struct RecentFile *recent;
	char *line;
	int num;
	char *cfgdir = BLI_get_folder(BLENDER_CONFIG, NULL);

	if (!cfgdir) return;

	BLI_make_file_string("/", name, cfgdir, BLENDER_HISTORY_FILE);

	lines= BLI_read_file_as_lines(name);

	G.recent_files.first = G.recent_files.last = NULL;

	/* read list of recent opend files from recent-files.txt to memory */
	for (l= lines, num= 0; l && (num<U.recent_files); l= l->next) {
		line = l->link;
		if (line[0] && BLI_exists(line)) {
			recent = (RecentFile*)MEM_mallocN(sizeof(RecentFile),"RecentFile");
			BLI_addtail(&(G.recent_files), recent);
			recent->filepath = BLI_strdup(line);
			num++;
		}
	}
	
	BLI_free_file_lines(lines);

}

static void write_history(void)
{
	struct RecentFile *recent, *next_recent;
	char name[FILE_MAXDIR+FILE_MAXFILE];
	char *user_config_dir;
	FILE *fp;
	int i;

	/* will be NULL in background mode */
	user_config_dir = BLI_get_folder_create(BLENDER_USER_CONFIG, NULL);
	if(!user_config_dir)
		return;

	BLI_make_file_string("/", name, user_config_dir, BLENDER_HISTORY_FILE);

	recent = G.recent_files.first;
	/* refresh recent-files.txt of recent opened files, when current file was changed */
	if(!(recent) || (strcmp(recent->filepath, G.main->name)!=0)) {
		fp= fopen(name, "w");
		if (fp) {
			/* add current file to the beginning of list */
			recent = (RecentFile*)MEM_mallocN(sizeof(RecentFile),"RecentFile");
			recent->filepath = BLI_strdup(G.main->name);
			BLI_addhead(&(G.recent_files), recent);
			/* write current file to recent-files.txt */
			fprintf(fp, "%s\n", recent->filepath);
			recent = recent->next;
			i=1;
			/* write rest of recent opened files to recent-files.txt */
			while((i<U.recent_files) && (recent)){
				/* this prevents to have duplicities in list */
				if (strcmp(recent->filepath, G.main->name)!=0) {
					fprintf(fp, "%s\n", recent->filepath);
					recent = recent->next;
				}
				else {
					next_recent = recent->next;
					MEM_freeN(recent->filepath);
					BLI_freelinkN(&(G.recent_files), recent);
					recent = next_recent;
				}
				i++;
			}
			fclose(fp);
		}

		/* also update most recent files on System */
		GHOST_addToSystemRecentFiles(G.main->name);
	}
}

static void do_history(char *name, ReportList *reports)
{
	char tempname1[FILE_MAXDIR+FILE_MAXFILE], tempname2[FILE_MAXDIR+FILE_MAXFILE];
	int hisnr= U.versions;
	
	if(U.versions==0) return;
	if(strlen(name)<2) return;
		
	while(hisnr > 1) {
		BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr-1);
		BLI_snprintf(tempname2, sizeof(tempname2), "%s%d", name, hisnr);
	
		if(BLI_rename(tempname1, tempname2))
			BKE_report(reports, RPT_ERROR, "Unable to make version backup");
			
		hisnr--;
	}

	/* is needed when hisnr==1 */
	BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr);

	if(BLI_rename(name, tempname1))
		BKE_report(reports, RPT_ERROR, "Unable to make version backup");
}

static ImBuf *blend_file_thumb(Scene *scene, int **thumb_pt)
{
	/* will be scaled down, but gives some nice oversampling */
	ImBuf *ibuf;
	int *thumb;
	char err_out[256]= "unknown";

	*thumb_pt= NULL;
	
	if(G.background || scene->camera==NULL)
		return NULL;

	/* gets scaled to BLEN_THUMB_SIZE */
	ibuf= ED_view3d_draw_offscreen_imbuf_simple(scene, BLEN_THUMB_SIZE * 2, BLEN_THUMB_SIZE * 2, IB_rect, OB_SOLID, err_out);
	
	if(ibuf) {		
		float aspect= (scene->r.xsch*scene->r.xasp) / (scene->r.ysch*scene->r.yasp);

		/* dirty oversampling */
		IMB_scaleImBuf(ibuf, BLEN_THUMB_SIZE, BLEN_THUMB_SIZE);

		/* add pretty overlay */
		IMB_overlayblend_thumb(ibuf->rect, ibuf->x, ibuf->y, aspect);
		
		/* first write into thumb buffer */
		thumb= MEM_mallocN(((2 + (BLEN_THUMB_SIZE * BLEN_THUMB_SIZE))) * sizeof(int), "write_file thumb");

		thumb[0] = BLEN_THUMB_SIZE;
		thumb[1] = BLEN_THUMB_SIZE;

		memcpy(thumb + 2, ibuf->rect, BLEN_THUMB_SIZE * BLEN_THUMB_SIZE * sizeof(int));
	}
	else {
		/* '*thumb_pt' needs to stay NULL to prevent a bad thumbnail from being handled */
		fprintf(stderr, "blend_file_thumb failed to create thumbnail: %s\n", err_out);
		thumb= NULL;
	}
	
	/* must be freed by caller */
	*thumb_pt= thumb;
	
	return ibuf;
}

/* easy access from gdb */
int write_crash_blend(void)
{
	char path[FILE_MAX];
	BLI_strncpy(path, G.main->name, sizeof(path));
	BLI_replace_extension(path, sizeof(path), "_crash.blend");
	if(BLO_write_file(G.main, path, G.fileflags, NULL, NULL)) {
		printf("written: %s\n", path);
		return 1;
	}
	else {
		printf("failed: %s\n", path);
		return 0;
	}
}

int WM_write_file(bContext *C, const char *target, int fileflags, ReportList *reports, int copy)
{
	Library *li;
	int len;
	char di[FILE_MAX];

	int *thumb= NULL;
	ImBuf *ibuf_thumb= NULL;

	len = strlen(target);
	
	if (len == 0) {
		BKE_report(reports, RPT_ERROR, "Path is empty, cannot save");
		return -1;
	}

	if (len >= FILE_MAX) {
		BKE_report(reports, RPT_ERROR, "Path too long, cannot save");
		return -1;
	}
 
	BLI_strncpy(di, target, FILE_MAX);
	BLI_replace_extension(di, FILE_MAX, ".blend");
	/* dont use 'target' anymore */
	
	/* send the OnSave event */
	for (li= G.main->library.first; li; li= li->id.next) {
		if (strcmp(li->filepath, di) == 0) {
			BKE_reportf(reports, RPT_ERROR, "Can't overwrite used library '%.200s'", di);
			return -1;
		}
	}

	/* operator now handles overwrite checks */

	if (G.fileflags & G_AUTOPACK) {
		packAll(G.main, reports);
	}
	
	ED_object_exit_editmode(C, EM_DO_UNDO);
	ED_sculpt_force_update(C);

	/* don't forget not to return without! */
	WM_cursor_wait(1);
	
	/* blend file thumbnail */
	ibuf_thumb= blend_file_thumb(CTX_data_scene(C), &thumb);

	/* rename to .blend1, do this as last before write */
	do_history(di, reports);

	if (BLO_write_file(CTX_data_main(C), di, fileflags, reports, thumb)) {
		if(!copy) {
			G.relbase_valid = 1;
			BLI_strncpy(G.main->name, di, sizeof(G.main->name));	/* is guaranteed current file */
	
			G.save_over = 1; /* disable untitled.blend convention */
		}

		if(fileflags & G_FILE_COMPRESS) G.fileflags |= G_FILE_COMPRESS;
		else G.fileflags &= ~G_FILE_COMPRESS;
		
		if(fileflags & G_FILE_AUTOPLAY) G.fileflags |= G_FILE_AUTOPLAY;
		else G.fileflags &= ~G_FILE_AUTOPLAY;

		write_history();

		/* run this function after because the file cant be written before the blend is */
		if (ibuf_thumb) {
			ibuf_thumb= IMB_thumb_create(di, THB_NORMAL, THB_SOURCE_BLEND, ibuf_thumb);
			IMB_freeImBuf(ibuf_thumb);
		}

		if(thumb) MEM_freeN(thumb);
	}
	else {
		if(ibuf_thumb) IMB_freeImBuf(ibuf_thumb);
		if(thumb) MEM_freeN(thumb);
		
		WM_cursor_wait(0);
		return -1;
	}

	WM_cursor_wait(0);
	
	return 0;
}

/* operator entry */
int WM_write_homefile(bContext *C, wmOperator *op)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *win= CTX_wm_window(C);
	char tstr[FILE_MAXDIR+FILE_MAXFILE];
	int fileflags;
	
	/* check current window and close it if temp */
	if(win->screen->temp)
		wm_window_close(C, wm, win);
	
	BLI_make_file_string("/", tstr, BLI_get_folder_create(BLENDER_USER_CONFIG, NULL), BLENDER_STARTUP_FILE);
	printf("trying to save homefile at %s ", tstr);
	
	/*  force save as regular blend file */
	fileflags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_AUTOPLAY | G_FILE_LOCK | G_FILE_SIGN);

	if(BLO_write_file(CTX_data_main(C), tstr, fileflags, op->reports, NULL) == 0) {
		printf("fail\n");
		return OPERATOR_CANCELLED;
	}
	
	printf("ok\n");

	G.save_over= 0;

	return OPERATOR_FINISHED;
}

/************************ autosave ****************************/

void wm_autosave_location(char *filename)
{
	char pidstr[32];
#ifdef WIN32
	char *savedir;
#endif

	BLI_snprintf(pidstr, sizeof(pidstr), "%d.blend", abs(getpid()));

#ifdef WIN32
	/* XXX Need to investigate how to handle default location of '/tmp/'
	 * This is a relative directory on Windows, and it may be
	 * found. Example:
	 * Blender installed on D:\ drive, D:\ drive has D:\tmp\
	 * Now, BLI_exists() will find '/tmp/' exists, but
	 * BLI_make_file_string will create string that has it most likely on C:\
	 * through get_default_root().
	 * If there is no C:\tmp autosave fails. */
	if (!BLI_exists(U.tempdir)) {
		savedir = BLI_get_folder_create(BLENDER_USER_AUTOSAVE, NULL);
		BLI_make_file_string("/", filename, savedir, pidstr);
		return;
	}
#endif
	
	BLI_make_file_string("/", filename, U.tempdir, pidstr);
}

void WM_autosave_init(wmWindowManager *wm)
{
	wm_autosave_timer_ended(wm);

	if(U.flag & USER_AUTOSAVE)
		wm->autosavetimer= WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime*60.0);
}

void wm_autosave_timer(const bContext *C, wmWindowManager *wm, wmTimer *UNUSED(wt))
{
	wmWindow *win;
	wmEventHandler *handler;
	char filename[FILE_MAX];
	int fileflags;

	WM_event_remove_timer(wm, NULL, wm->autosavetimer);

	/* if a modal operator is running, don't autosave, but try again in 10 seconds */
	for(win=wm->windows.first; win; win=win->next) {
		for(handler=win->modalhandlers.first; handler; handler=handler->next) {
			if(handler->op) {
				wm->autosavetimer= WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, 10.0);
				return;
			}
		}
	}
	
	wm_autosave_location(filename);

	/*  force save as regular blend file */
	fileflags = G.fileflags & ~(G_FILE_COMPRESS|G_FILE_AUTOPLAY |G_FILE_LOCK|G_FILE_SIGN);

	/* no error reporting to console */
	BLO_write_file(CTX_data_main(C), filename, fileflags, NULL, NULL);

	/* do timer after file write, just in case file write takes a long time */
	wm->autosavetimer= WM_event_add_timer(wm, NULL, TIMERAUTOSAVE, U.savetime*60.0);
}

void wm_autosave_timer_ended(wmWindowManager *wm)
{
	if(wm->autosavetimer) {
		WM_event_remove_timer(wm, NULL, wm->autosavetimer);
		wm->autosavetimer= NULL;
	}
}

void wm_autosave_delete(void)
{
	char filename[FILE_MAX];
	
	wm_autosave_location(filename);

	if(BLI_exists(filename)) {
		char str[FILE_MAXDIR+FILE_MAXFILE];
		BLI_make_file_string("/", str, U.tempdir, "quit.blend");

		/* if global undo; remove tempsave, otherwise rename */
		if(U.uiflag & USER_GLOBALUNDO) BLI_delete(filename, 0, 0);
		else BLI_rename(filename, str);
	}
}

void wm_autosave_read(bContext *C, ReportList *reports)
{
	char filename[FILE_MAX];

	wm_autosave_location(filename);
	WM_read_file(C, filename, reports);
}

