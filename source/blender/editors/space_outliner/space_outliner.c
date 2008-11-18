/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_vec_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "ED_area.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_text.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "outliner_intern.h"

#define SET_INT_IN_POINTER(i) ((void*)(intptr_t)(i))
#define GET_INT_FROM_POINTER(i) ((int)(intptr_t)(i))

#define ROW_HEIGHT 		19
#define COLUMN_WIDTH	150

typedef void (*uiTableCellFunc)(void *userdata, int row, int col, struct rcti *rct, struct uiBlock *block);

typedef struct uiTable {
	rcti rct;
	int rows, cols;

	uiTableCellFunc cellfunc;
	void *userdata;
} uiTable;

uiTable *UI_table_create(int rows, int cols, rcti *rct, uiTableCellFunc cellfunc, void *userdata)
{
	uiTable *table;

	table= MEM_callocN(sizeof(uiTable), "uiTable");
	table->rct= *rct;
	table->cellfunc= cellfunc;
	table->rows= rows;
	table->cols= cols;
	table->userdata= userdata;

	return table;
}

void UI_table_free(uiTable *table)
{
	MEM_freeN(table);
}

void UI_table_draw(wmWindow *window, ARegion *region, uiTable *table)
{
	uiBlock *block;
	rcti *rct, cellrct;
	int y, row, col;

	rct= &table->rct;

	block= uiBeginBlock(window, region, "table outliner", UI_EMBOSST, UI_HELV);

	for(y=rct->ymax, row=0; y>rct->ymin; y-=ROW_HEIGHT, row++) {
		if(row%2 == 0) {
			UI_ThemeColorShade(TH_BACK, 6);
			glRecti(rct->xmin, y-ROW_HEIGHT, rct->xmax, y);
		}

		if(row >= table->rows)
			continue;

		for(col=0; col<table->cols; col++) {
			cellrct.xmin= rct->xmin+COLUMN_WIDTH*col + 1;
			cellrct.xmax= rct->xmin+COLUMN_WIDTH*(col+1);
			cellrct.ymin= y-ROW_HEIGHT;
			cellrct.ymax= y;

			table->cellfunc(table->userdata, row, col, &cellrct, block);
		}
	}

	UI_ThemeColorShadeAlpha(TH_BACK, -15, -200);

	for(col=0; col<table->cols; col++)
		fdrawline(rct->xmin+COLUMN_WIDTH*(col+1), rct->ymin, rct->xmin+COLUMN_WIDTH*(col+1), rct->ymax);

	uiEndBlock(block);
	uiDrawBlock(block);
}


/* ************************ main outliner area region *********************** */

typedef struct CellRNA {
	SpaceOops *space;
	StructRNA *srna;
	PropertyRNA *prop;
	PointerRNA ptr;
	int lastrow, index;

	CollectionPropertyIterator iter;
	PropertyRNA *iterprop;
} CellRNA;

static void rna_back_cb(void *arg_buts, void *arg_unused)
{
	SpaceOops *soutliner= arg_buts;
	char *newpath;

	newpath= RNA_path_back(soutliner->rnapath);
	if(soutliner->rnapath)
		MEM_freeN(soutliner->rnapath);
	soutliner->rnapath= newpath;
}

static void rna_pointer_cb(void *arg_buts, void *arg_prop, void *arg_index)
{
	SpaceOops *soutliner= arg_buts;
	PropertyRNA *prop= arg_prop;
	char *newpath;
	int index= GET_INT_FROM_POINTER(arg_index);;

	newpath= RNA_path_append(soutliner->rnapath, NULL, prop, index, NULL);
	if(soutliner->rnapath)
		MEM_freeN(soutliner->rnapath);
	soutliner->rnapath= newpath;
}

static void rna_label(CellRNA *cell, rcti *rct, uiBlock *block)
{
	PropertySubType subtype;
	PropertyType type;
	PropertyRNA *prop;
	char *vectoritem[4]= {"x", "y", "z", "w"};
	char *quatitem[4]= {"w", "x", "y", "z"};
	char *coloritem[4]= {"r", "g", "b", "a"};
	char item[32];
	int arraylength;

	prop= cell->prop;
	type= RNA_property_type(prop, &cell->ptr);
	subtype= RNA_property_subtype(prop, &cell->ptr);
	arraylength= RNA_property_array_length(prop, &cell->ptr);

	if(cell->index == -1) {
		uiDefBut(block, LABEL, 0, (char*)RNA_property_ui_name(prop, &cell->ptr), rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, (char*)RNA_property_ui_description(prop, &cell->ptr));
	}
	else if (type != PROP_COLLECTION) {
		if(arraylength == 4 && subtype == PROP_ROTATION)
			sprintf(item, "    %s", quatitem[cell->index]);
		else if(arraylength <= 4 && (subtype == PROP_VECTOR || subtype == PROP_ROTATION))
			sprintf(item, "    %s", vectoritem[cell->index]);
		else if(arraylength <= 4 && subtype == PROP_COLOR)
			sprintf(item, "    %s", coloritem[cell->index]);
		else
			sprintf(item, "    %d", cell->index+1);

		uiDefBut(block, LABEL, 0, item, rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, "");
	}
}

static void rna_collection_but(CellRNA *cell, rcti *rct, uiBlock *block)
{
	uiBut *but;
	PointerRNA lookup;
	PropertyRNA *nameprop;
	char name[256]= "", *nameptr= name;

	RNA_property_collection_lookup_int(cell->prop, &cell->ptr, cell->index, &lookup);

	if(lookup.data) {
		nameprop= RNA_struct_name_property(&lookup);

		if(nameprop)
			nameptr= RNA_property_string_get_alloc(nameprop, &lookup, name, sizeof(name));
		else
			sprintf(nameptr, "%d", cell->index+1);
	}

	but= uiDefBut(block, BUT, 0, nameptr, rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, "");
	uiButSetFlag(but, UI_TEXT_LEFT);

	if(nameptr != name)
		MEM_freeN(nameptr);

	uiButSetFunc3(but, rna_pointer_cb, cell->space, cell->prop, SET_INT_IN_POINTER(cell->index));
}

static void rna_but(CellRNA *cell, rcti *rct, uiBlock *block)
{
	uiBut *but;
	PropertyRNA *prop;
	PropertyType type;
	int arraylength, index;

	prop= cell->prop;
	type= RNA_property_type(prop, &cell->ptr);
	arraylength= RNA_property_array_length(prop, &cell->ptr);

	if(type == PROP_COLLECTION) {
		/* item in a collection */
		if(cell->index >= 0)
			rna_collection_but(cell, rct, block);
	}
	else {
		/* other cases */
		index= (arraylength)? cell->index: 0;

		if(index >= 0) {
			but= uiDefRNABut(block, 0, &cell->ptr, prop, index, rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin);

			if(type == PROP_POINTER)
				uiButSetFunc3(but, rna_pointer_cb, cell->space, prop, SET_INT_IN_POINTER(0));
		}
	}
}

static void rna_path_but(CellRNA *cell, rcti *rct, uiBlock *block)
{
	uiBut *but;

	but= uiDefBut(block, BUT, 0, (cell->space->rnapath)? "..": ".", rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, "");
	uiButSetFlag(but, UI_TEXT_LEFT);
	uiButSetFunc(but, rna_back_cb, cell->space, NULL);
}

static void rna_table_cell_func(void *userdata, int row, int col, rcti *rct, uiBlock *block)
{
	CellRNA *cell= userdata;
	PropertyType type;
	int length;

	/* path button */
	if(row == 0) {
		if(col == 0)
			rna_path_but(cell, rct, block);

		return;
	}

	/* set next property for new row */
	if(row != cell->lastrow) {
		if(cell->prop) {
			cell->index++;

			type= RNA_property_type(cell->prop, &cell->ptr);
			if(type == PROP_COLLECTION)
				length= RNA_property_collection_length(cell->prop, &cell->ptr);
			else
				length= RNA_property_array_length(cell->prop, &cell->ptr);

			/* verify if we need to go to the next property */
			if(type == PROP_COLLECTION && cell->index < length);
			else if(length && cell->index < length);
			else {
				RNA_property_collection_next(cell->iterprop, &cell->iter);
				cell->prop= cell->iter.ptr.data;
				cell->index= -1;
			}
		}
		else {
			/* initialize */
			cell->prop= cell->iter.ptr.data;
			cell->index= -1;
		}

		cell->lastrow= row;
	}

	/* make button */
	if(col == 0)
		rna_label(cell, rct, block);
	else if(col == 1)
		rna_but(cell, rct, block);
}

static void outliner_main_area_draw(const bContext *C, ARegion *ar)
{
	uiTable *table;
	rcti rct;
	CellRNA cell;
	PropertyRNA *prop;
	PointerRNA newptr;
	float col[3];
	int rows, cols, width, height;
	SpaceOops *soutliner= C->area->spacedata.first;

	/* clear */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	width= ar->winrct.xmax - ar->winrct.xmin;
	height= ar->winrct.ymax - ar->winrct.ymin;
	
	/* create table */
	rct.xmin= 0;
	rct.ymin= 0;
	rct.xmax= width;
	rct.ymax= height;

	cell.space= soutliner;
	cell.lastrow= -1;
	RNA_pointer_main_get(G.main, &cell.ptr);
	cell.prop= NULL;

	/* solve RNA path or reset if fails */
	if(soutliner->rnapath) {
		if(!RNA_path_resolve(&cell.ptr, soutliner->rnapath, &newptr, &prop)) {
			newptr.data= NULL;
			printf("RNA outliner: failed resolving path. (%s)\n", soutliner->rnapath);
		}

		if(newptr.data && newptr.type) {
			cell.ptr= newptr;
		}
		else {
			MEM_freeN(soutliner->rnapath);
			soutliner->rnapath= NULL;
		}
	}

	/* compute number of rows and columns */
	rows= 1;
	cols= 2;

	cell.iterprop= RNA_struct_iterator_property(&cell.ptr);
	RNA_property_collection_begin(cell.iterprop, &cell.iter, &cell.ptr);

	for(; cell.iter.valid; RNA_property_collection_next(cell.iterprop, &cell.iter)) {
		prop= cell.iter.ptr.data;

		rows += 1 + RNA_property_array_length(prop, &cell.ptr);
		if(RNA_property_type(prop, &cell.ptr) == PROP_COLLECTION)
			rows += RNA_property_collection_length(prop, &cell.ptr);
	}

	RNA_property_collection_end(cell.iterprop, &cell.iter);

	/* create and draw table */
	table= UI_table_create(rows, 2, &rct, rna_table_cell_func, &cell);

	RNA_property_collection_begin(cell.iterprop, &cell.iter, &cell.ptr);
	UI_table_draw(C->window, ar, table);
	RNA_property_collection_end(cell.iterprop, &cell.iter);

	UI_table_free(table);
}

static void outliner_main_area_free(ARegion *ar)
{
	uiFreeBlocks(&ar->uiblocks);
}

/* ************************ header outliner area region *********************** */

static void outliner_header_area_draw(const bContext *C, ARegion *ar)
{
	SpaceOops *soutliner= C->area->spacedata.first;
	float col[3];
	int width, height;
	rctf bbox;
	char *path;

	path= (soutliner->rnapath)? soutliner->rnapath: "Main";

	/* clear */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(MAX2(col[0]-0.3f, 0.0f), MAX2(col[1]-0.3f, 0.0f), MAX2(col[2]-0.3f, 0.0f), 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	width= ar->winrct.xmax - ar->winrct.xmin;
	height= ar->winrct.ymax - ar->winrct.ymin;

	/* header text */
	UI_GetBoundingBox(UI_HELV, path, 0, &bbox);

	glColor3f(1.0f, 1.0f, 1.0f);
	UI_SetScale(1.0);
	UI_RasterPos(0.5f*(width - (bbox.xmax - bbox.xmin)), 0.5f*(height - (bbox.ymax - bbox.ymin)));
	UI_DrawString(UI_HELV, path, 0);
}

static void outliner_header_area_free(ARegion *ar)
{
	uiFreeBlocks(&ar->uiblocks);
}

/* ******************** default callbacks for outliner space ***************** */

static SpaceLink *outliner_new(void)
{
	SpaceOops *soutliner;

	soutliner= MEM_callocN(sizeof(SpaceOops), "initoutliner");

	return (SpaceLink*)soutliner;
}

/* not spacelink itself */
static void outliner_free(SpaceLink *sl)
{
	SpaceOops *soutliner= (SpaceOops*)sl;

	if(soutliner->rnapath) {
		MEM_freeN(soutliner->rnapath);
		soutliner->rnapath= NULL;
	}
}

/* spacetype; init callback */
static void outliner_init(wmWindowManager *wm, ScrArea *sa)
{
	ARegion *ar;
	
	/* link area to SpaceXXX struct */

	/* add handlers to area */
	/* define how many regions, the order and types */
	
	/* add types to regions */
	for(ar= sa->regionbase.first; ar; ar= ar->next) {
		if(ar->regiontype == RGN_TYPE_WINDOW) {
			static ARegionType mainart={NULL, NULL, NULL, NULL, NULL};

			mainart.draw= outliner_main_area_draw;
			mainart.free= outliner_main_area_free;

			ar->type= &mainart;

			WM_event_add_keymap_handler(&ar->handlers, &wm->uikeymap);
		}
		else if(ar->regiontype == RGN_TYPE_HEADER) {
			static ARegionType headerart={NULL, NULL, NULL, NULL, NULL};

			headerart.draw= outliner_header_area_draw;
			headerart.free= outliner_header_area_free;

			ar->type= &headerart;

			WM_event_add_keymap_handler(&ar->handlers, &wm->uikeymap);
		}
		else {
			static ARegionType headerart={NULL, NULL, NULL, NULL, NULL};
			ar->type= &headerart;
		}
	}
}

/* spacetype; context changed */
static void outliner_refresh(bContext *C, ScrArea *sa)
{
	
}

static SpaceLink *outliner_duplicate(SpaceLink *sl)
{
	SpaceOops *soutliner= (SpaceOops *)sl;
	SpaceOops *soutlinern= MEM_dupallocN(soutliner);
	
	return (SpaceLink *)soutlinern;
}

/* only called once, from screen/spacetypes.c */
void ED_spacetype_outliner(void)
{
	static SpaceType st;
	
	st.spaceid= SPACE_OOPS;
	
	st.new= outliner_new;
	st.free= outliner_free;
	st.init= outliner_init;
	st.refresh= outliner_refresh;
	st.duplicate= outliner_duplicate;
	st.operatortypes= outliner_operatortypes;
	st.keymap= outliner_keymap;
	
	BKE_spacetype_register(&st);
}

