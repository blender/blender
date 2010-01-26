/**
 * $Id: 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_object.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"

#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"

/* ***************** generic editmode undo system ********************* */
/*

Add this in your local code:

void undo_editmode_push(bContext *C, char *name, 
		void * (*getdata)(bContext *C),     // use context to retrieve current editdata
		void (*freedata)(void *), 			// pointer to function freeing data
		void (*to_editmode)(void *, void *),        // data to editmode conversion
		void * (*from_editmode)(void *))      // editmode to data conversion
		int  (*validate_undo)(void *, void *))      // check if undo data is still valid


Further exported for UI is:

void undo_editmode_step(bContext *C, int step);	 // undo and redo
void undo_editmode_clear(void)				// free & clear all data
void undo_editmode_menu(void)				// history menu


*/
/* ********************************************************************* */

/* ****** XXX ***** */
void error() {}
/* ****** XXX ***** */


#define MAXUNDONAME	64
typedef struct UndoElem {
	struct UndoElem *next, *prev;
	ID id;			// copy of editmode object ID
	Object *ob;		// pointer to edited object
	int type;		// type of edited object
	void *undodata;
	uintptr_t undosize;
	char name[MAXUNDONAME];
	void * (*getdata)(bContext *C);
	void (*freedata)(void *);
	void (*to_editmode)(void *, void *);
	void * (*from_editmode)(void *);
	int (*validate_undo)(void *, void *);
} UndoElem;

static ListBase undobase={NULL, NULL};
static UndoElem *curundo= NULL;


/* ********************* xtern api calls ************* */

static void undo_restore(UndoElem *undo, void *editdata)
{
	if(undo) {
		undo->to_editmode(undo->undodata, editdata);	
	}
}

/* name can be a dynamic string */
void undo_editmode_push(bContext *C, char *name, 
						void * (*getdata)(bContext *C),
						void (*freedata)(void *), 
						void (*to_editmode)(void *, void *),  
						void *(*from_editmode)(void *),
						int (*validate_undo)(void *, void *))
{
	UndoElem *uel;
	Object *obedit= CTX_data_edit_object(C);
	void *editdata;
	int nr;
	uintptr_t memused, totmem, maxmem;

	/* at first here was code to prevent an "original" key to be insterted twice
	   this was giving conflicts for example when mesh changed due to keys or apply */
	
	/* remove all undos after (also when curundo==NULL) */
	while(undobase.last != curundo) {
		uel= undobase.last;
		uel->freedata(uel->undodata);
		BLI_freelinkN(&undobase, uel);
	}
	
	/* make new */
	curundo= uel= MEM_callocN(sizeof(UndoElem), "undo editmode");
	strncpy(uel->name, name, MAXUNDONAME-1);
	BLI_addtail(&undobase, uel);
	
	uel->getdata= getdata;
	uel->freedata= freedata;
	uel->to_editmode= to_editmode;
	uel->from_editmode= from_editmode;
	uel->validate_undo= validate_undo;
	
	/* limit amount to the maximum amount*/
	nr= 0;
	uel= undobase.last;
	while(uel) {
		nr++;
		if(nr==U.undosteps) break;
		uel= uel->prev;
	}
	if(uel) {
		while(undobase.first!=uel) {
			UndoElem *first= undobase.first;
			first->freedata(first->undodata);
			BLI_freelinkN(&undobase, first);
		}
	}

	/* copy  */
	memused= MEM_get_memory_in_use();
	editdata= getdata(C);
	curundo->undodata= curundo->from_editmode(editdata);
	curundo->undosize= MEM_get_memory_in_use() - memused;
	curundo->ob= obedit;
	curundo->id= obedit->id;
	curundo->type= obedit->type;

	if(U.undomemory != 0) {
		/* limit to maximum memory (afterwards, we can't know in advance) */
		totmem= 0;
		maxmem= ((uintptr_t)U.undomemory)*1024*1024;

		uel= undobase.last;
		while(uel && uel->prev) {
			totmem+= uel->undosize;
			if(totmem>maxmem) break;
			uel= uel->prev;
		}

		if(uel) {
			if(uel->prev && uel->prev->prev)
				uel= uel->prev;

			while(undobase.first!=uel) {
				UndoElem *first= undobase.first;
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
	Object *obedit= CTX_data_edit_object(C);
	
	/* global undo changes pointers, so we also allow identical names */
	/* side effect: when deleting/renaming object and start editing new one with same name */
	
	uel= undobase.first; 
	while(uel) {
		void *editdata= uel->getdata(C);
		int isvalid= 0;
		next= uel->next;
		
		/* for when objects are converted, renamed, or global undo changes pointers... */
		if(uel->type==obedit->type) {
			if(strcmp(uel->id.name, obedit->id.name)==0) {
				if(uel->validate_undo==NULL)
					isvalid= 1;
				else if(uel->validate_undo(uel->undodata, editdata))
					isvalid= 1;
			}
		}
		if(isvalid) 
			uel->ob= obedit;
		else {
			if(uel == curundo)
				curundo= NULL;

			uel->freedata(uel->undodata);
			BLI_freelinkN(&undobase, uel);
		}
		
		uel= next;
	}
	
	if(curundo == NULL) curundo= undobase.last;
}

/* 1= an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void undo_editmode_step(bContext *C, int step)
{
	Object *obedit= CTX_data_edit_object(C);
	
	/* prevent undo to happen on wrong object, stack can be a mix */
	undo_clean_stack(C);
	
	if(step==0) {
		undo_restore(curundo, curundo->getdata(C));
	}
	else if(step==1) {
		
		if(curundo==NULL || curundo->prev==NULL) error("No more steps to undo");
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", curundo->name);
			curundo= curundo->prev;
			undo_restore(curundo, curundo->getdata(C));
		}
	}
	else {
		/* curundo has to remain current situation! */
		
		if(curundo==NULL || curundo->next==NULL) error("No more steps to redo");
		else {
			undo_restore(curundo->next, curundo->getdata(C));
			curundo= curundo->next;
			if(G.f & G_DEBUG) printf("redo %s\n", curundo->name);
		}
	}
	
	DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);

	/* XXX notifiers */
}

void undo_editmode_clear(void)
{
	UndoElem *uel;
	
	uel= undobase.first;
	while(uel) {
		uel->freedata(uel->undodata);
		uel= uel->next;
	}
	BLI_freelistN(&undobase);
	curundo= NULL;
}

/* based on index nr it does a restore */
static void undo_number(bContext *C, int nr)
{
	UndoElem *uel;
	int a=1;
	
	for(uel= undobase.first; uel; uel= uel->next, a++) {
		if(a==nr) break;
	}
	curundo= uel;
	undo_editmode_step(C, 0);
}

void undo_editmode_name(bContext *C, const char *undoname)
{
	UndoElem *uel;
	
	for(uel= undobase.last; uel; uel= uel->prev) {
		if(strcmp(undoname, uel->name)==0)
			break;
	}
	if(uel && uel->prev) {
		curundo= uel->prev;
		undo_editmode_step(C, 0);
	}
}


/* ************** for interaction with menu/pullown */

void undo_editmode_menu(bContext *C)
{
	UndoElem *uel;
	DynStr *ds= BLI_dynstr_new();
	short event= 0;
	char *menu;

	undo_clean_stack(C);	// removes other objects from it
	
	BLI_dynstr_append(ds, "Editmode Undo History %t");
	
	for(uel= undobase.first; uel; uel= uel->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, uel->name);
	}
	
	menu= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	
// XXX	event= pupmenu_col(menu, 20);
	MEM_freeN(menu);
	
	if(event>0) undo_number(C, event);
}

static void do_editmode_undohistorymenu(bContext *C, void *arg, int event)
{
	Object *obedit= CTX_data_edit_object(C);
	
	if(obedit==NULL || event<1) return;

	undo_number(C, event-1);
	
}

uiBlock *editmode_undohistorymenu(bContext *C, ARegion *ar, void *arg_unused)
{
	uiBlock *block;
	UndoElem *uel;
	short yco = 20, menuwidth = 120;
	short item= 1;
	
	undo_clean_stack(C);	// removes other objects from it

	block= uiBeginBlock(C, ar, "view3d_edit_mesh_undohistorymenu", UI_EMBOSSP);
	uiBlockSetButmFunc(block, do_editmode_undohistorymenu, NULL);
	
	for(uel= undobase.first; uel; uel= uel->next, item++) {
		if (uel==curundo) uiDefBut(block, SEPR, 0, "",		0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, uel->name, 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, (float)item, "");
		if (uel==curundo) uiDefBut(block, SEPR, 0, "",		0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

void *undo_editmode_get_prev(Object *ob)
{
	UndoElem *ue= undobase.last;
	if(ue && ue->prev && ue->prev->ob==ob) return ue->prev->undodata;
	return NULL;
}
