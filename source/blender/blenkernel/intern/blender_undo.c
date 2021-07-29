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

/** \file blender/blenkernel/intern/blender_undo.c
 *  \ingroup bke
 *
 * Blend file undo (known as 'Global Undo').
 * DNA level diffing for undo.
 */

#ifndef _WIN32
#  include <unistd.h> // for read close
#else
#  include <io.h> // for open close read
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>  /* for open */
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#include "BKE_blender_undo.h"  /* own include */
#include "BKE_blendfile.h"
#include "BKE_appdir.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "RE_pipeline.h"

#include "BLO_undofile.h"
#include "BLO_readfile.h"
#include "BLO_writefile.h"

/* -------------------------------------------------------------------- */

/** \name Global Undo
 * \{ */

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

/**
 * Avoid bad-level call to #WM_jobs_kill_all_except()
 */
static void (*undo_wm_job_kill_callback)(struct bContext *C) = NULL;

void BKE_undo_callback_wm_kill_jobs_set(void (*callback)(struct bContext *C))
{
	undo_wm_job_kill_callback = callback;
}

static int read_undosave(bContext *C, UndoElem *uel)
{
	char mainstr[sizeof(G.main->name)];
	int success = 0, fileflags;

	/* This is needed so undoing/redoing doesn't crash with threaded previews going */
	undo_wm_job_kill_callback(C);

	BLI_strncpy(mainstr, G.main->name, sizeof(mainstr));    /* temporal store */

	fileflags = G.fileflags;
	G.fileflags |= G_FILE_NO_UI;

	if (UNDO_DISK)
		success = (BKE_blendfile_read(C, uel->str, NULL, 0) != BKE_BLENDFILE_READ_FAIL);
	else
		success = BKE_blendfile_read_from_memfile(C, &uel->memfile, NULL, 0);

	/* restore */
	BLI_strncpy(G.main->name, mainstr, sizeof(G.main->name)); /* restore */
	G.fileflags = fileflags;

	if (success) {
		/* important not to update time here, else non keyed tranforms are lost */
		DAG_on_visible_update(G.main, false);
	}

	return success;
}

/* name can be a dynamic string */
void BKE_undo_write(bContext *C, const char *name)
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
		BLO_memfile_free(&uel->memfile);
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
			BLO_memfile_merge(&first->memfile, &first->next->memfile);
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
		BLI_make_file_string("/", filepath, BKE_tempdir_session(), numstr);

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
				BLO_memfile_merge(&first->memfile, &first->next->memfile);
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

void BKE_undo_reset(void)
{
	UndoElem *uel;

	uel = undobase.first;
	while (uel) {
		BLO_memfile_free(&uel->memfile);
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
bool BKE_undo_is_valid(const char *name)
{
	if (name) {
		UndoElem *uel = BLI_rfindstring(&undobase, name, offsetof(UndoElem, name));
		return uel && uel->prev;
	}

	return undobase.last != undobase.first;
}

/* get name of undo item, return null if no item with this index */
/* if active pointer, set it to 1 if true */
const char *BKE_undo_get_name(int nr, bool *r_active)
{
	UndoElem *uel = BLI_findlink(&undobase, nr);

	if (r_active) *r_active = false;

	if (uel) {
		if (r_active && (uel == curundo)) {
			*r_active = true;
		}
		return uel->name;
	}
	return NULL;
}

/* return the name of the last item */
const char *BKE_undo_get_name_last(void)
{
	UndoElem *uel = undobase.last;
	return (uel ? uel->name : NULL);
}

/**
 * Saves .blend using undo buffer.
 *
 * \return success.
 */
bool BKE_undo_save_file(const char *filename)
{
	UndoElem *uel;
	MemFileChunk *chunk;
	int file, oflags;

	if ((U.uiflag & USER_GLOBALUNDO) == 0) {
		return false;
	}

	uel = curundo;
	if (uel == NULL) {
		fprintf(stderr, "No undo buffer to save recovery file\n");
		return false;
	}

	/* note: This is currently used for autosave and 'quit.blend', where _not_ following symlinks is OK,
	 * however if this is ever executed explicitly by the user, we may want to allow writing to symlinks.
	 */

	oflags = O_BINARY | O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_NOFOLLOW
	/* use O_NOFOLLOW to avoid writing to a symlink - use 'O_EXCL' (CVE-2008-1103) */
	oflags |= O_NOFOLLOW;
#else
	/* TODO(sergey): How to deal with symlinks on windows? */
#  ifndef _MSC_VER
#    warning "Symbolic links will be followed on undo save, possibly causing CVE-2008-1103"
#  endif
#endif
	file = BLI_open(filename,  oflags, 0666);

	if (file == -1) {
		fprintf(stderr, "Unable to save '%s': %s\n",
		        filename, errno ? strerror(errno) : "Unknown error opening file");
		return false;
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
		return false;
	}
	return true;
}

/* sets curscene */
Main *BKE_undo_get_main(Scene **r_scene)
{
	Main *mainp = NULL;
	BlendFileData *bfd = BLO_read_from_memfile(G.main, G.main->name, &curundo->memfile, NULL, BLO_READ_SKIP_NONE);

	if (bfd) {
		mainp = bfd->main;
		if (r_scene) {
			*r_scene = bfd->curscene;
		}

		MEM_freeN(bfd);
	}

	return mainp;
}

/** \} */
