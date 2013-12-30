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

/** \file blender/blenkernel/intern/blender.c
 *  \ingroup bke
 */


#ifndef _WIN32 
#  include <unistd.h> // for read close
#else
#  include <io.h> // for open close read
#  define open _open
#  define read _read
#  define close _close
#  define write _write
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>  /* for open */
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender.h"
#include "BKE_bpath.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"

#include "RE_pipeline.h"

#include "BLF_api.h"

#include "BLO_undofile.h"
#include "BLO_readfile.h" 
#include "BLO_writefile.h" 

#include "RNA_access.h"

#include "WM_api.h" // XXXXX BAD, very BAD dependency (bad level call) - remove asap, elubie

#include "IMB_colormanagement.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

Global G;
UserDef U;
/* ListBase = {NULL, NULL}; */

char versionstr[48] = "";

/* ********** free ********** */

/* only to be called on exit blender */
void free_blender(void)
{
	/* samples are in a global list..., also sets G.main->sound->sample NULL */
	BKE_main_free(G.main);
	G.main = NULL;

	BKE_spacetypes_free();      /* after free main, it uses space callbacks */
	
	IMB_exit();
	BKE_images_exit();
	DAG_exit();

	BKE_brush_system_exit();

	BLI_callback_global_finalize();

	BKE_sequencer_cache_destruct();
	IMB_moviecache_destruct();
	
	free_nodesystem();
}

void initglobals(void)
{
	memset(&G, 0, sizeof(Global));
	
	U.savetime = 1;

	G.main = BKE_main_new();

	strcpy(G.ima, "//");

	if (BLENDER_SUBVERSION)
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d.%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION);
	else
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);

#ifdef _WIN32
	G.windowstate = 0;
#endif

#ifndef WITH_PYTHON_SECURITY /* default */
	G.f |= G_SCRIPT_AUTOEXEC;
#else
	G.f &= ~G_SCRIPT_AUTOEXEC;
#endif
}

/***/

static void clear_global(void) 
{
//	extern short winqueue_break;	/* screen.c */

	BKE_main_free(G.main);          /* free all lib data */
	
//	free_vertexpaint();

	G.main = NULL;
}

static bool clean_paths_visit_cb(void *UNUSED(userdata), char *path_dst, const char *path_src)
{
	strcpy(path_dst, path_src);
	BLI_clean(path_dst);
	return !STREQ(path_dst, path_src);
}

/* make sure path names are correct for OS */
static void clean_paths(Main *main)
{
	Scene *scene;

	BKE_bpath_traverse_main(main, clean_paths_visit_cb, BKE_BPATH_TRAVERSE_SKIP_MULTIFILE, NULL);

	for (scene = main->scene.first; scene; scene = scene->id.next) {
		BLI_clean(scene->r.pic);
	}
}

/* context matching */
/* handle no-ui case */

/* note, this is called on Undo so any slow conversion functions here
 * should be avoided or check (mode!='u') */

static void setup_app_data(bContext *C, BlendFileData *bfd, const char *filepath)
{
	bScreen *curscreen = NULL;
	Scene *curscene = NULL;
	int recover;
	char mode;

	/* 'u' = undo save, 'n' = no UI load */
	if (bfd->main->screen.first == NULL) mode = 'u';
	else if (G.fileflags & G_FILE_NO_UI) mode = 'n';
	else mode = 0;

	recover = (G.fileflags & G_FILE_RECOVER);

	/* Free all render results, without this stale data gets displayed after loading files */
	if (mode != 'u') {
		RE_FreeAllRenderResults();
	}

	/* Only make filepaths compatible when loading for real (not undo) */
	if (mode != 'u') {
		clean_paths(bfd->main);
	}

	/* XXX here the complex windowmanager matching */
	
	/* no load screens? */
	if (mode) {
		/* comes from readfile.c */
		SWAP(ListBase, G.main->wm, bfd->main->wm);
		SWAP(ListBase, G.main->screen, bfd->main->screen);
		SWAP(ListBase, G.main->script, bfd->main->script);
		
		/* we re-use current screen */
		curscreen = CTX_wm_screen(C);
		/* but use new Scene pointer */
		curscene = bfd->curscene;
		if (curscene == NULL) curscene = bfd->main->scene.first;
		/* empty file, we add a scene to make Blender work */
		if (curscene == NULL) curscene = BKE_scene_add(bfd->main, "Empty");
		
		/* and we enforce curscene to be in current screen */
		if (curscreen) curscreen->scene = curscene;  /* can run in bgmode */

		/* clear_global will free G.main, here we can still restore pointers */
		blo_lib_link_screen_restore(bfd->main, curscreen, curscene);
	}
	
	/* free G.main Main database */
//	CTX_wm_manager_set(C, NULL);
	clear_global();
	
	/* clear old property update cache, in case some old references are left dangling */
	RNA_property_update_cache_free();
	
	G.main = bfd->main;

	CTX_data_main_set(C, G.main);

	sound_init_main(G.main);
	
	if (bfd->user) {
		
		/* only here free userdef themes... */
		BKE_userdef_free();
		
		U = *bfd->user;
		MEM_freeN(bfd->user);
	}
	
	/* case G_FILE_NO_UI or no screens in file */
	if (mode) {
		/* leave entire context further unaltered? */
		CTX_data_scene_set(C, curscene);
	}
	else {
		G.winpos = bfd->winpos;
		G.displaymode = bfd->displaymode;
		G.fileflags = bfd->fileflags;
		CTX_wm_manager_set(C, G.main->wm.first);
		CTX_wm_screen_set(C, bfd->curscreen);
		CTX_data_scene_set(C, bfd->curscene);
		CTX_wm_area_set(C, NULL);
		CTX_wm_region_set(C, NULL);
		CTX_wm_menu_set(C, NULL);
	}
	
	/* this can happen when active scene was lib-linked, and doesn't exist anymore */
	if (CTX_data_scene(C) == NULL) {
		/* in case we don't even have a local scene, add one */
		if (!G.main->scene.first)
			BKE_scene_add(G.main, "Scene");

		CTX_data_scene_set(C, G.main->scene.first);
		CTX_wm_screen(C)->scene = CTX_data_scene(C);
		curscene = CTX_data_scene(C);
	}

	/* special cases, override loaded flags: */
	if (G.f != bfd->globalf) {
		const int flags_keep = (G_SWAP_EXCHANGE | G_SCRIPT_AUTOEXEC | G_SCRIPT_OVERRIDE_PREF);
		bfd->globalf = (bfd->globalf & ~flags_keep) | (G.f & flags_keep);
	}


	G.f = bfd->globalf;

#ifdef WITH_PYTHON
	/* let python know about new main */
	BPY_context_update(C);
#endif

	if (!G.background) {
		//setscreen(G.curscreen);
	}
	
	/* FIXME: this version patching should really be part of the file-reading code,
	 * but we still get too many unrelated data-corruption crashes otherwise... */
	if (G.main->versionfile < 250)
		do_versions_ipos_to_animato(G.main);
	
	G.main->recovered = 0;
	
	/* startup.blend or recovered startup */
	if (bfd->filename[0] == 0) {
		G.main->name[0] = 0;
	}
	else if (recover && G.relbase_valid) {
		/* in case of autosave or quit.blend, use original filename instead
		 * use relbase_valid to make sure the file is saved, else we get <memory2> in the filename */
		filepath = bfd->filename;
		G.main->recovered = 1;
	
		/* these are the same at times, should never copy to the same location */
		if (G.main->name != filepath)
			BLI_strncpy(G.main->name, filepath, FILE_MAX);
	}
	
	/* baseflags, groups, make depsgraph, etc */
	/* first handle case if other windows have different scenes visible */
	if (mode == 0) {
		wmWindowManager *wm = G.main->wm.first;
		
		if (wm) {
			wmWindow *win;
			
			for (win = wm->windows.first; win; win = win->next) {
				if (win->screen && win->screen->scene) /* zealous check... */
					if (win->screen->scene != CTX_data_scene(C))
						BKE_scene_set_background(G.main, win->screen->scene);
			}
		}
	}
	BKE_scene_set_background(G.main, CTX_data_scene(C));

	if (mode != 'u') {
		IMB_colormanagement_check_file_config(G.main);
	}

	MEM_freeN(bfd);

}

static int handle_subversion_warning(Main *main, ReportList *reports)
{
	if (main->minversionfile > BLENDER_VERSION ||
	    (main->minversionfile == BLENDER_VERSION &&
	     main->minsubversionfile > BLENDER_SUBVERSION))
	{
		BKE_reportf(reports, RPT_ERROR, "File written by newer Blender binary (%d.%d), expect loss of data!",
		            main->minversionfile, main->minsubversionfile);
	}

	return 1;
}

static void keymap_item_free(wmKeyMapItem *kmi)
{
	if (kmi->properties) {
		IDP_FreeProperty(kmi->properties);
		MEM_freeN(kmi->properties);
	}
	if (kmi->ptr)
		MEM_freeN(kmi->ptr);
}

void BKE_userdef_free(void)
{
	wmKeyMap *km;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	bAddon *addon, *addon_next;

	for (km = U.user_keymaps.first; km; km = km->next) {
		for (kmdi = km->diff_items.first; kmdi; kmdi = kmdi->next) {
			if (kmdi->add_item) {
				keymap_item_free(kmdi->add_item);
				MEM_freeN(kmdi->add_item);
			}
			if (kmdi->remove_item) {
				keymap_item_free(kmdi->remove_item);
				MEM_freeN(kmdi->remove_item);
			}
		}

		for (kmi = km->items.first; kmi; kmi = kmi->next)
			keymap_item_free(kmi);

		BLI_freelistN(&km->diff_items);
		BLI_freelistN(&km->items);
	}
	
	for (addon = U.addons.first; addon; addon = addon_next) {
		addon_next = addon->next;
		if (addon->prop) {
			IDP_FreeProperty(addon->prop);
			MEM_freeN(addon->prop);
		}
		MEM_freeN(addon);
	}

	BLI_freelistN(&U.autoexec_paths);

	BLI_freelistN(&U.uistyles);
	BLI_freelistN(&U.uifonts);
	BLI_freelistN(&U.themes);
	BLI_freelistN(&U.user_keymaps);
}

/* handle changes in settings that need recalc */
void BKE_userdef_state(void)
{
	/* prevent accidents */
	if (U.pixelsize == 0) U.pixelsize = 1;
	
	BLF_default_dpi(U.pixelsize * U.dpi);
	U.widget_unit = (U.pixelsize * U.dpi * 20 + 36) / 72;

}

int BKE_read_file(bContext *C, const char *filepath, ReportList *reports)
{
	BlendFileData *bfd;
	int retval = BKE_READ_FILE_OK;

	if (strstr(filepath, BLENDER_STARTUP_FILE) == NULL) /* don't print user-pref loading */
		printf("read blend: %s\n", filepath);

	bfd = BLO_read_from_file(filepath, reports);
	if (bfd) {
		if (bfd->user) retval = BKE_READ_FILE_OK_USERPREFS;
		
		if (0 == handle_subversion_warning(bfd->main, reports)) {
			BKE_main_free(bfd->main);
			MEM_freeN(bfd);
			bfd = NULL;
			retval = BKE_READ_FILE_FAIL;
		}
		else
			setup_app_data(C, bfd, filepath);  // frees BFD
	}
	else
		BKE_reports_prependf(reports, "Loading '%s' failed: ", filepath);
		
	return (bfd ? retval : BKE_READ_FILE_FAIL);
}

int BKE_read_file_from_memory(bContext *C, const void *filebuf, int filelength, ReportList *reports, int update_defaults)
{
	BlendFileData *bfd;

	bfd = BLO_read_from_memory(filebuf, filelength, reports);
	if (bfd) {
		if (update_defaults)
			BLO_update_defaults_startup_blend(bfd->main);
		setup_app_data(C, bfd, "<memory2>");
	}
	else
		BKE_reports_prepend(reports, "Loading failed: ");

	return (bfd ? 1 : 0);
}

/* memfile is the undo buffer */
int BKE_read_file_from_memfile(bContext *C, MemFile *memfile, ReportList *reports)
{
	BlendFileData *bfd;

	bfd = BLO_read_from_memfile(CTX_data_main(C), G.main->name, memfile, reports);
	if (bfd) {
		/* remove the unused screens and wm */
		while (bfd->main->wm.first)
			BKE_libblock_free(&bfd->main->wm, bfd->main->wm.first);
		while (bfd->main->screen.first)
			BKE_libblock_free(&bfd->main->screen, bfd->main->screen.first);
		
		setup_app_data(C, bfd, "<memory1>");
	}
	else
		BKE_reports_prepend(reports, "Loading failed: ");

	return (bfd ? 1 : 0);
}

/* only read the userdef from a .blend */
int BKE_read_file_userdef(const char *filepath, ReportList *reports)
{
	BlendFileData *bfd;
	int retval = 0;
	
	bfd = BLO_read_from_file(filepath, reports);
	if (bfd->user) {
		retval = BKE_READ_FILE_OK_USERPREFS;
		
		/* only here free userdef themes... */
		BKE_userdef_free();
		
		U = *bfd->user;
		MEM_freeN(bfd->user);
	}
	BKE_main_free(bfd->main);
	MEM_freeN(bfd);
	
	return retval;
}

/* only write the userdef in a .blend */
int BKE_write_file_userdef(const char *filepath, ReportList *reports)
{
	Main *mainb = MEM_callocN(sizeof(Main), "empty main");
	int retval = 0;
	
	if (BLO_write_file(mainb, filepath, G_FILE_USERPREFS, reports, NULL)) {
		retval = 1;
	}
	
	MEM_freeN(mainb);
	
	return retval;
}

/* *****************  testing for break ************* */

static void (*blender_test_break_cb)(void) = NULL;

void set_blender_test_break_cb(void (*func)(void) )
{
	blender_test_break_cb = func;
}


int blender_test_break(void)
{
	if (!G.background) {
		if (blender_test_break_cb)
			blender_test_break_cb();
	}
	
	return (G.is_break == TRUE);
}


/* ***************** GLOBAL UNDO *************** */

#define UNDO_DISK   0

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char str[FILE_MAX];
	char name[BKE_UNDO_STR_MAX];
	MemFile memfile;
	uintptr_t undosize;
} UndoElem;

static ListBase undobase = {NULL, NULL};
static UndoElem *curundo = NULL;


static int read_undosave(bContext *C, UndoElem *uel)
{
	char mainstr[sizeof(G.main->name)];
	int success = 0, fileflags;
	
	/* This is needed so undoing/redoing doesn't crash with threaded previews going */
	WM_jobs_kill_all_except(CTX_wm_manager(C), CTX_wm_screen(C));

	BLI_strncpy(mainstr, G.main->name, sizeof(mainstr));    /* temporal store */

	fileflags = G.fileflags;
	G.fileflags |= G_FILE_NO_UI;

	if (UNDO_DISK) 
		success = (BKE_read_file(C, uel->str, NULL) != BKE_READ_FILE_FAIL);
	else
		success = BKE_read_file_from_memfile(C, &uel->memfile, NULL);

	/* restore */
	BLI_strncpy(G.main->name, mainstr, sizeof(G.main->name)); /* restore */
	G.fileflags = fileflags;

	if (success) {
		/* important not to update time here, else non keyed tranforms are lost */
		DAG_on_visible_update(G.main, FALSE);
	}

	return success;
}

/* name can be a dynamic string */
void BKE_write_undo(bContext *C, const char *name)
{
	uintptr_t maxmem, totmem, memused;
	int nr /*, success */ /* UNUSED */;
	UndoElem *uel;
	
	if ((U.uiflag & USER_GLOBALUNDO) == 0) {
		return;
	}

	if (U.undosteps == 0) {
		return;
	}
	
	/* remove all undos after (also when curundo == NULL) */
	while (undobase.last != curundo) {
		uel = undobase.last;
		BLI_remlink(&undobase, uel);
		BLO_free_memfile(&uel->memfile);
		MEM_freeN(uel);
	}
	
	/* make new */
	curundo = uel = MEM_callocN(sizeof(UndoElem), "undo file");
	BLI_strncpy(uel->name, name, sizeof(uel->name));
	BLI_addtail(&undobase, uel);
	
	/* and limit amount to the maximum */
	nr = 0;
	uel = undobase.last;
	while (uel) {
		nr++;
		if (nr == U.undosteps) break;
		uel = uel->prev;
	}
	if (uel) {
		while (undobase.first != uel) {
			UndoElem *first = undobase.first;
			BLI_remlink(&undobase, first);
			/* the merge is because of compression */
			BLO_merge_memfile(&first->memfile, &first->next->memfile);
			MEM_freeN(first);
		}
	}


	/* disk save version */
	if (UNDO_DISK) {
		static int counter = 0;
		char filepath[FILE_MAX];
		char numstr[32];
		int fileflags = G.fileflags & ~(G_FILE_HISTORY); /* don't do file history on undo */

		/* calculate current filepath */
		counter++;
		counter = counter % U.undosteps;
	
		BLI_snprintf(numstr, sizeof(numstr), "%d.blend", counter);
		BLI_make_file_string("/", filepath, BLI_temporary_dir(), numstr);
	
		/* success = */ /* UNUSED */ BLO_write_file(CTX_data_main(C), filepath, fileflags, NULL, NULL);
		
		BLI_strncpy(curundo->str, filepath, sizeof(curundo->str));
	}
	else {
		MemFile *prevfile = NULL;
		
		if (curundo->prev) prevfile = &(curundo->prev->memfile);
		
		memused = MEM_get_memory_in_use();
		/* success = */ /* UNUSED */ BLO_write_file_mem(CTX_data_main(C), prevfile, &curundo->memfile, G.fileflags);
		curundo->undosize = MEM_get_memory_in_use() - memused;
	}

	if (U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem = 0;
		maxmem = ((uintptr_t)U.undomemory) * 1024 * 1024;

		/* keep at least two (original + other) */
		uel = undobase.last;
		while (uel && uel->prev) {
			totmem += uel->undosize;
			if (totmem > maxmem) break;
			uel = uel->prev;
		}

		if (uel) {
			if (uel->prev && uel->prev->prev)
				uel = uel->prev;

			while (undobase.first != uel) {
				UndoElem *first = undobase.first;
				BLI_remlink(&undobase, first);
				/* the merge is because of compression */
				BLO_merge_memfile(&first->memfile, &first->next->memfile);
				MEM_freeN(first);
			}
		}
	}
}

/* 1 = an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void BKE_undo_step(bContext *C, int step)
{
	
	if (step == 0) {
		read_undosave(C, curundo);
	}
	else if (step == 1) {
		/* curundo should never be NULL, after restart or load file it should call undo_save */
		if (curundo == NULL || curundo->prev == NULL) {
			// XXX error("No undo available");
		}
		else {
			if (G.debug & G_DEBUG) printf("undo %s\n", curundo->name);
			curundo = curundo->prev;
			read_undosave(C, curundo);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if (curundo == NULL || curundo->next == NULL) {
			// XXX error("No redo available");
		}
		else {
			read_undosave(C, curundo->next);
			curundo = curundo->next;
			if (G.debug & G_DEBUG) printf("redo %s\n", curundo->name);
		}
	}
}

void BKE_reset_undo(void)
{
	UndoElem *uel;
	
	uel = undobase.first;
	while (uel) {
		BLO_free_memfile(&uel->memfile);
		uel = uel->next;
	}
	
	BLI_freelistN(&undobase);
	curundo = NULL;
}

/* based on index nr it does a restore */
void BKE_undo_number(bContext *C, int nr)
{
	curundo = BLI_findlink(&undobase, nr);
	BKE_undo_step(C, 0);
}

/* go back to the last occurance of name in stack */
void BKE_undo_name(bContext *C, const char *name)
{
	UndoElem *uel = BLI_rfindstring(&undobase, name, offsetof(UndoElem, name));

	if (uel && uel->prev) {
		curundo = uel->prev;
		BKE_undo_step(C, 0);
	}
}

/* name optional */
int BKE_undo_valid(const char *name)
{
	if (name) {
		UndoElem *uel = BLI_rfindstring(&undobase, name, offsetof(UndoElem, name));
		return uel && uel->prev;
	}
	
	return undobase.last != undobase.first;
}

/* get name of undo item, return null if no item with this index */
/* if active pointer, set it to 1 if true */
const char *BKE_undo_get_name(int nr, int *active)
{
	UndoElem *uel = BLI_findlink(&undobase, nr);
	
	if (active) *active = 0;
	
	if (uel) {
		if (active && uel == curundo)
			*active = 1;
		return uel->name;
	}
	return NULL;
}

/* saves .blend using undo buffer, returns 1 == success */
int BKE_undo_save_file(const char *filename)
{
	UndoElem *uel;
	MemFileChunk *chunk;
	const int flag = O_BINARY + O_WRONLY + O_CREAT + O_TRUNC + O_EXCL;
	int file;

	if ((U.uiflag & USER_GLOBALUNDO) == 0) {
		return 0;
	}

	uel = curundo;
	if (uel == NULL) {
		fprintf(stderr, "No undo buffer to save recovery file\n");
		return 0;
	}

	/* first try create the file, if it exists call without 'O_CREAT',
	 * to avoid writing to a symlink - use 'O_EXCL' (CVE-2008-1103) */
	errno = 0;
	file = BLI_open(filename, flag, 0666);
	if (file < 0) {
		if (errno == EEXIST) {
			errno = 0;
			file = BLI_open(filename, flag & ~O_CREAT, 0666);
		}
	}

	if (file == -1) {
		fprintf(stderr, "Unable to save '%s': %s\n",
		        filename, errno ? strerror(errno) : "Unknown error opening file");
		return 0;
	}

	for (chunk = uel->memfile.chunks.first; chunk; chunk = chunk->next) {
		if (write(file, chunk->buf, chunk->size) != chunk->size) {
			break;
		}
	}
	
	close(file);
	
	if (chunk) {
		fprintf(stderr, "Unable to save '%s': %s\n",
		        filename, errno ? strerror(errno) : "Unknown error writing file");
		return 0;
	}
	return 1;
}

/* sets curscene */
Main *BKE_undo_get_main(Scene **scene)
{
	Main *mainp = NULL;
	BlendFileData *bfd = BLO_read_from_memfile(G.main, G.main->name, &curundo->memfile, NULL);
	
	if (bfd) {
		mainp = bfd->main;
		if (scene)
			*scene = bfd->curscene;
		
		MEM_freeN(bfd);
	}
	
	return mainp;
}

/* ************** copy paste .blend, partial saves ********** */

/* assumes data is in G.main */

void BKE_copybuffer_begin(Main *bmain)
{
	/* set all id flags to zero; */
	BKE_main_id_flag_all(bmain, LIB_NEED_EXPAND | LIB_DOIT, false);
}

void BKE_copybuffer_tag_ID(ID *id)
{
	id->flag |= LIB_NEED_EXPAND | LIB_DOIT;
}

static void copybuffer_doit(void *UNUSED(handle), Main *UNUSED(bmain), void *vid)
{
	if (vid) {
		ID *id = vid;
		/* only tag for need-expand if not done, prevents eternal loops */
		if ((id->flag & LIB_DOIT) == 0)
			id->flag |= LIB_NEED_EXPAND | LIB_DOIT;
	}
}

/* frees main in end */
int BKE_copybuffer_save(const char *filename, ReportList *reports)
{
	Main *mainb = MEM_callocN(sizeof(Main), "copybuffer");
	ListBase *lbarray[MAX_LIBARRAY], *fromarray[MAX_LIBARRAY];
	int a, retval;
	
	/* path backup/restore */
	void     *path_list_backup;
	const int path_list_flag = (BKE_BPATH_TRAVERSE_SKIP_LIBRARY | BKE_BPATH_TRAVERSE_SKIP_MULTIFILE);

	path_list_backup = BKE_bpath_list_backup(G.main, path_list_flag);

	BLO_main_expander(copybuffer_doit);
	BLO_expand_main(NULL, G.main);
	
	/* move over all tagged blocks */
	set_listbasepointers(G.main, fromarray);
	a = set_listbasepointers(mainb, lbarray);
	while (a--) {
		ID *id, *nextid;
		ListBase *lb1 = lbarray[a], *lb2 = fromarray[a];
		
		for (id = lb2->first; id; id = nextid) {
			nextid = id->next;
			if (id->flag & LIB_DOIT) {
				BLI_remlink(lb2, id);
				BLI_addtail(lb1, id);
			}
		}
	}
	
	
	/* save the buffer */
	retval = BLO_write_file(mainb, filename, G_FILE_RELATIVE_REMAP, reports, NULL);
	
	/* move back the main, now sorted again */
	set_listbasepointers(G.main, lbarray);
	a = set_listbasepointers(mainb, fromarray);
	while (a--) {
		ID *id;
		ListBase *lb1 = lbarray[a], *lb2 = fromarray[a];
		
		while ((id = BLI_pophead(lb2))) {
			BLI_addtail(lb1, id);
			id_sort_by_name(lb1, id);
		}
	}
	
	MEM_freeN(mainb);
	
	/* set id flag to zero; */
	BKE_main_id_flag_all(G.main, LIB_NEED_EXPAND | LIB_DOIT, false);
	
	if (path_list_backup) {
		BKE_bpath_list_restore(G.main, path_list_flag, path_list_backup);
		BKE_bpath_list_free(path_list_backup);
	}

	return retval;
}

/* return success (1) */
int BKE_copybuffer_paste(bContext *C, const char *libname, ReportList *reports)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Main *mainl = NULL;
	Library *lib;
	BlendHandle *bh;
		
	bh = BLO_blendhandle_from_file(libname, reports);
	
	if (bh == NULL) {
		/* error reports will have been made by BLO_blendhandle_from_file() */
		return 0;
	}

	BKE_scene_base_deselect_all(scene);
	
	/* tag everything, all untagged data can be made local
	 * its also generally useful to know what is new
	 *
	 * take extra care BKE_main_id_flag_all(bmain, LIB_LINK_TAG, false) is called after! */
	BKE_main_id_flag_all(bmain, LIB_PRE_EXISTING, true);
	
	/* here appending/linking starts */
	mainl = BLO_library_append_begin(bmain, &bh, libname);
	
	BLO_library_append_all(mainl, bh);

	BLO_library_append_end(C, mainl, &bh, 0, 0);
	
	/* mark all library linked objects to be updated */
	BKE_main_lib_objects_recalc_all(bmain);
	IMB_colormanagement_check_file_config(bmain);
	
	/* append, rather than linking */
	lib = BLI_findstring(&bmain->library, libname, offsetof(Library, filepath));
	BKE_library_make_local(bmain, lib, true);
	
	/* important we unset, otherwise these object wont
	 * link into other scenes from this blend file */
	BKE_main_id_flag_all(bmain, LIB_PRE_EXISTING, false);
	
	/* recreate dependency graph to include new objects */
	DAG_relations_tag_update(bmain);
	
	BLO_blendhandle_close(bh);
	/* remove library... */
	
	return 1;
}
