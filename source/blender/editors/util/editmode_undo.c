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
 * The Original Code is Copyright (C) 2004 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/editmode_undo.c
 *  \ingroup edutil
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"

#include "ED_util.h"
#include "ED_mesh.h"


#include "util_intern.h"

/* ***************** generic editmode undo system ********************* */
/*
 * Add this in your local code:
 *
 * void undo_editmode_push(bContext *C, const char *name, 
 *      void * (*getdata)(bContext *C),     // use context to retrieve current editdata
 *      void (*freedata)(void *),           // pointer to function freeing data
 *      void (*to_editmode)(void *, void *),        // data to editmode conversion
 *      void * (*from_editmode)(void *))      // editmode to data conversion
 *      int  (*validate_undo)(void *, void *))      // check if undo data is still valid
 *
 *
 * Further exported for UI is:
 *
 * void undo_editmode_step(bContext *C, int step);	 // undo and redo
 * void undo_editmode_clear(void)				// free & clear all data
 * void undo_editmode_menu(void)				// history menu
 */

/* ********************************************************************* */

/* ****** XXX ***** */
static void error(const char *UNUSED(arg)) {}
/* ****** XXX ***** */

typedef struct UndoElem {
	struct UndoElem *next, *prev;
	ID id;          // copy of editmode object ID
	Object *ob;     // pointer to edited object
	int type;       // type of edited object
	void *undodata;
	uintptr_t undosize;
	char name[BKE_UNDO_STR_MAX];
	void * (*getdata)(bContext * C);
	void (*freedata)(void *);
	void (*to_editmode)(void *, void *, void *);
	void * (*from_editmode)(void *, void *);
	int (*validate_undo)(void *, void *);
} UndoElem;

static ListBase undobase = {NULL, NULL};
static UndoElem *curundo = NULL;


/* ********************* xtern api calls ************* */

static void undo_restore(UndoElem *undo, void *editdata, void *obdata)
{
	if (undo) {
		undo->to_editmode(undo->undodata, editdata, obdata);
	}
}

/* name can be a dynamic string */
void undo_editmode_push(bContext *C, const char *name, 
                        void * (*getdata)(bContext * C),
                        void (*freedata)(void *),
                        void (*to_editmode)(void *, void *, void *),
                        void *(*from_editmode)(void *, void *),
                        int (*validate_undo)(void *, void *))
{
	UndoElem *uel;
	Object *obedit = CTX_data_edit_object(C);
	void *editdata;
	int nr;
	uintptr_t memused, totmem, maxmem;

	/* at first here was code to prevent an "original" key to be inserted twice
	 * this was giving conflicts for example when mesh changed due to keys or apply */
	
	/* remove all undos after (also when curundo == NULL) */
	while (undobase.last != curundo) {
		uel = undobase.last;
		uel->freedata(uel->undodata);
		BLI_freelinkN(&undobase, uel);
	}
	
	/* make new */
	curundo = uel = MEM_callocN(sizeof(UndoElem), "undo editmode");
	BLI_strncpy(uel->name, name, sizeof(uel->name));
	BLI_addtail(&undobase, uel);
	
	uel->getdata = getdata;
	uel->freedata = freedata;
	uel->to_editmode = to_editmode;
	uel->from_editmode = from_editmode;
	uel->validate_undo = validate_undo;
	
	/* limit amount to the maximum amount*/
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
			first->freedata(first->undodata);
			BLI_freelinkN(&undobase, first);
		}
	}

	/* copy  */
	memused = MEM_get_memory_in_use();
	editdata = getdata(C);
	curundo->undodata = curundo->from_editmode(editdata, obedit->data);
	curundo->undosize = MEM_get_memory_in_use() - memused;
	curundo->ob = obedit;
	curundo->id = obedit->id;
	curundo->type = obedit->type;

	if (U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem = 0;
		maxmem = ((uintptr_t)U.undomemory) * 1024 * 1024;

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
				first->freedata(first->undodata);
				BLI_freelinkN(&undobase, first);
			}
		}
	}
}

/* helper to remove clean other objects from undo stack */
static void undo_clean_stack(bContext *C)
{
	UndoElem *uel, *next;
	Object *obedit = CTX_data_edit_object(C);
	
	/* global undo changes pointers, so we also allow identical names */
	/* side effect: when deleting/renaming object and start editing new one with same name */
	
	uel = undobase.first;
	while (uel) {
		void *editdata = uel->getdata(C);
		bool is_valid = false;
		next = uel->next;
		
		/* for when objects are converted, renamed, or global undo changes pointers... */
		if (uel->type == obedit->type) {
			if (STREQ(uel->id.name, obedit->id.name)) {
				if (uel->validate_undo == NULL)
					is_valid = true;
				else if (uel->validate_undo(uel->undodata, editdata))
					is_valid = true;
			}
		}
		if (is_valid)
			uel->ob = obedit;
		else {
			if (uel == curundo)
				curundo = NULL;

			uel->freedata(uel->undodata);
			BLI_freelinkN(&undobase, uel);
		}
		
		uel = next;
	}
	
	if (curundo == NULL) curundo = undobase.last;
}

/* 1 = an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void undo_editmode_step(bContext *C, int step)
{
	Object *obedit = CTX_data_edit_object(C);
	
	/* prevent undo to happen on wrong object, stack can be a mix */
	undo_clean_stack(C);
	
	if (step == 0) {
		undo_restore(curundo, curundo->getdata(C), obedit->data);
	}
	else if (step == 1) {
		
		if (curundo == NULL || curundo->prev == NULL) {
			error("No more steps to undo");
		}
		else {
			if (G.debug & G_DEBUG) printf("undo %s\n", curundo->name);
			curundo = curundo->prev;
			undo_restore(curundo, curundo->getdata(C), obedit->data);
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if (curundo == NULL || curundo->next == NULL) {
			error("No more steps to redo");
		}
		else {
			undo_restore(curundo->next, curundo->getdata(C), obedit->data);
			curundo = curundo->next;
			if (G.debug & G_DEBUG) printf("redo %s\n", curundo->name);
		}
	}
	
	/* special case for editmesh, mode must be copied back to the scene */
	if (obedit->type == OB_MESH) {
		EDBM_selectmode_to_scene(C);
	}

	DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);

	/* XXX notifiers */
}

void undo_editmode_clear(void)
{
	UndoElem *uel;
	
	uel = undobase.first;
	while (uel) {
		uel->freedata(uel->undodata);
		uel = uel->next;
	}
	BLI_freelistN(&undobase);
	curundo = NULL;
}

/* based on index nr it does a restore */
void undo_editmode_number(bContext *C, int nr)
{
	UndoElem *uel;
	int a = 1;
	
	for (uel = undobase.first; uel; uel = uel->next, a++) {
		if (a == nr) break;
	}
	curundo = uel;
	undo_editmode_step(C, 0);
}

void undo_editmode_name(bContext *C, const char *undoname)
{
	UndoElem *uel;
	
	for (uel = undobase.last; uel; uel = uel->prev) {
		if (STREQ(undoname, uel->name))
			break;
	}
	if (uel && uel->prev) {
		curundo = uel->prev;
		undo_editmode_step(C, 0);
	}
}

/* undoname optionally, if NULL it just checks for existing undo steps */
bool undo_editmode_is_valid(const char *undoname)
{
	if (undoname) {
		UndoElem *uel;
		
		for (uel = undobase.last; uel; uel = uel->prev) {
			if (STREQ(undoname, uel->name))
				break;
		}
		return uel != NULL;
	}
	return undobase.last != undobase.first;
}


/* get name of undo item, return null if no item with this index */
/* if active pointer, set it to 1 if true */
const char *undo_editmode_get_name(bContext *C, int nr, bool *r_active)
{
	UndoElem *uel;
	
	/* prevent wrong numbers to be returned */
	undo_clean_stack(C);
	
	if (r_active) *r_active = false;
	
	uel = BLI_findlink(&undobase, nr);
	if (uel) {
		if (r_active && (uel == curundo)) {
			*r_active = true;
		}
		return uel->name;
	}
	return NULL;
}


void *undo_editmode_get_prev(Object *ob)
{
	UndoElem *ue = undobase.last;
	if (ue && ue->prev && ue->prev->ob == ob) return ue->prev->undodata;
	return NULL;
}
