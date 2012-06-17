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
#include <fcntl.h> // for open

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
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


#include "BLO_undofile.h"
#include "BLO_readfile.h" 
#include "BLO_writefile.h" 

#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "WM_api.h" // XXXXX BAD, very BAD dependency (bad level call) - remove asap, elubie

#ifdef WITH_PYTHON
#include "BPY_extern.h"
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
	free_main(G.main);
	G.main = NULL;

	BKE_spacetypes_free();      /* after free main, it uses space callbacks */
	
	IMB_exit();

	BLI_callback_global_finalize();

	seq_stripelem_cache_destruct();
	IMB_moviecache_destruct();
	
	free_nodesystem();	
}

void initglobals(void)
{
	memset(&G, 0, sizeof(Global));
	
	U.savetime = 1;

	G.main = MEM_callocN(sizeof(Main), "initglobals");

	strcpy(G.ima, "//");

	if (BLENDER_SUBVERSION)
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d.%d", BLENDER_VERSION / 100, BLENDER_VERSION % 100, BLENDER_SUBVERSION);
	else
		BLI_snprintf(versionstr, sizeof(versionstr), "v%d.%02d", BLENDER_VERSION / 100, BLENDER_VERSION % 100);

#ifdef _WIN32   // FULLSCREEN
	G.windowstate = G_WINDOWSTATE_USERDEF;
#endif

	G.charstart = 0x0000;
	G.charmin = 0x0000;
	G.charmax = 0xffff;

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

	free_main(G.main);          /* free all lib data */
	
//	free_vertexpaint();

	G.main = NULL;
}

static int clean_paths_visit_cb(void *UNUSED(userdata), char *path_dst, const char *path_src)
{
	strcpy(path_dst, path_src);
	BLI_clean(path_dst);
	return (strcmp(path_dst, path_src) == 0) ? FALSE : TRUE;
}

/* make sure path names are correct for OS */
static void clean_paths(Main *main)
{
	Scene *scene;

	BLI_bpath_traverse_main(main, clean_paths_visit_cb, BLI_BPATH_TRAVERSE_SKIP_MULTIFILE, NULL);

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
		extern void lib_link_screen_restore(Main *, bScreen *, Scene *);
		
		SWAP(ListBase, G.main->wm, bfd->main->wm);
		SWAP(ListBase, G.main->screen, bfd->main->screen);
		SWAP(ListBase, G.main->script, bfd->main->script);
		
		/* we re-use current screen */
		curscreen = CTX_wm_screen(C);
		/* but use new Scene pointer */
		curscene = bfd->curscene;
		if (curscene == NULL) curscene = bfd->main->scene.first;
		/* and we enforce curscene to be in current screen */
		if (curscreen) curscreen->scene = curscene;  /* can run in bgmode */

		/* clear_global will free G.main, here we can still restore pointers */
		lib_link_screen_restore(bfd->main, curscreen, curscene);
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
		CTX_wm_manager_set(C, bfd->main->wm.first);
		CTX_wm_screen_set(C, bfd->curscreen);
		CTX_data_scene_set(C, bfd->curscreen->scene);
		CTX_wm_area_set(C, NULL);
		CTX_wm_region_set(C, NULL);
		CTX_wm_menu_set(C, NULL);
	}
	
	/* this can happen when active scene was lib-linked, and doesn't exist anymore */
	if (CTX_data_scene(C) == NULL) {
		CTX_data_scene_set(C, bfd->main->scene.first);
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
	
	// FIXME: this version patching should really be part of the file-reading code, 
	// but we still get too many unrelated data-corruption crashes otherwise...
	if (G.main->versionfile < 250)
		do_versions_ipos_to_animato(G.main);
	
	if (recover && bfd->filename[0] && G.relbase_valid) {
		/* in case of autosave or quit.blend, use original filename instead
		 * use relbase_valid to make sure the file is saved, else we get <memory2> in the filename */
		filepath = bfd->filename;
	}
#if 0
	else if (!G.relbase_valid) {
		/* otherwise, use an empty string as filename, rather than <memory2> */
		filepath = "";
	}
#endif
	
	/* these are the same at times, should never copy to the same location */
	if (G.main->name != filepath)
		BLI_strncpy(G.main->name, filepath, FILE_MAX);

	/* baseflags, groups, make depsgraph, etc */
	BKE_scene_set_background(G.main, CTX_data_scene(C));
	
	MEM_freeN(bfd);

	(void)curscene; /* quiet warning */
}

static int handle_subversion_warning(Main *main, ReportList *reports)
{
	if (main->minversionfile > BLENDER_VERSION ||
	    (main->minversionfile == BLENDER_VERSION &&
	     main->minsubversionfile > BLENDER_SUBVERSION))
	{
		BKE_reportf(reports, RPT_ERROR, "File written by newer Blender binary: %d.%d, expect loss of data!",
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
	
	BLI_freelistN(&U.uistyles);
	BLI_freelistN(&U.uifonts);
	BLI_freelistN(&U.themes);
	BLI_freelistN(&U.user_keymaps);
	BLI_freelistN(&U.addons);
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
			free_main(bfd->main);
			MEM_freeN(bfd);
			bfd = NULL;
			retval = BKE_READ_FILE_FAIL;
		}
		else
			setup_app_data(C, bfd, filepath);  // frees BFD
	} 
	else
		BKE_reports_prependf(reports, "Loading %s failed: ", filepath);
		
	return (bfd ? retval : BKE_READ_FILE_FAIL);
}

int BKE_read_file_from_memory(bContext *C, char *filebuf, int filelength, ReportList *reports)
{
	BlendFileData *bfd;

	bfd = BLO_read_from_memory(filebuf, filelength, reports);
	if (bfd)
		setup_app_data(C, bfd, "<memory2>");
	else
		BKE_reports_prepend(reports, "Loading failed: ");

	return (bfd ? 1 : 0);
}

/* memfile is the undo buffer */
int BKE_read_file_from_memfile(bContext *C, MemFile *memfile, ReportList *reports)
{
	BlendFileData *bfd;

	bfd = BLO_read_from_memfile(CTX_data_main(C), G.main->name, memfile, reports);
	if (bfd)
		setup_app_data(C, bfd, "<memory1>");
	else
		BKE_reports_prepend(reports, "Loading failed: ");

	return (bfd ? 1 : 0);
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
	
	return (G.afbreek == 1);
}


/* ***************** GLOBAL UNDO *************** */

#define UNDO_DISK   0

#define MAXUNDONAME 64
typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char str[FILE_MAX];
	char name[MAXUNDONAME];
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
	WM_jobs_stop_all(CTX_wm_manager(C));

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
	
	if ( (U.uiflag & USER_GLOBALUNDO) == 0) return;
	if (U.undosteps == 0) return;
	
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

/* 1= an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void BKE_undo_step(bContext *C, int step)
{
	
	if (step == 0) {
		read_undosave(C, curundo);
	}
	else if (step == 1) {
		/* curundo should never be NULL, after restart or load file it should call undo_save */
		if (curundo == NULL || curundo->prev == NULL) ;  // XXX error("No undo available");
		else {
			if (G.debug & G_DEBUG) printf("undo %s\n", curundo->name);
			curundo = curundo->prev;
			read_undosave(C, curundo);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if (curundo == NULL || curundo->next == NULL) ;  // XXX error("No redo available");
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

char *BKE_undo_menu_string(void)
{
	UndoElem *uel;
	DynStr *ds = BLI_dynstr_new();
	char *menu;

	BLI_dynstr_append(ds, "Global Undo History %t");
	
	for (uel = undobase.first; uel; uel = uel->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, uel->name);
	}

	menu = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return menu;
}

/* saves quit.blend */
void BKE_undo_save_quit(void)
{
	UndoElem *uel;
	MemFileChunk *chunk;
	int file;
	char str[FILE_MAX];
	
	if ( (U.uiflag & USER_GLOBALUNDO) == 0) return;
	
	uel = curundo;
	if (uel == NULL) {
		printf("No undo buffer to save recovery file\n");
		return;
	}
	
	/* no undo state to save */
	if (undobase.first == undobase.last) return;
		
	BLI_make_file_string("/", str, BLI_temporary_dir(), "quit.blend");

	file = BLI_open(str, O_BINARY + O_WRONLY + O_CREAT + O_TRUNC, 0666);
	if (file == -1) {
		//XXX error("Unable to save %s, check you have permissions", str);
		return;
	}

	chunk = uel->memfile.chunks.first;
	while (chunk) {
		if (write(file, chunk->buf, chunk->size) != chunk->size) break;
		chunk = chunk->next;
	}
	
	close(file);
	
	if (chunk) ;  //XXX error("Unable to save %s, internal error", str);
	else printf("Saved session recovery to %s\n", str);
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

