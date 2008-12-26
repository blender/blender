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
#include "DNA_oops_types.h"
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
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "ED_space_api.h"
#include "ED_screen.h"

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

void UI_table_draw(const bContext *C, uiTable *table)
{
	ARegion *ar= CTX_wm_region(C);
	uiBlock *block;
	View2D *v2d;
	rcti *rct, cellrct;
	int y, row, col;
	
	v2d= &ar->v2d;
	rct= &table->rct;
	
	block= uiBeginBlock(C, ar, "table outliner", UI_EMBOSST, UI_HELV);
	
	for(y=rct->ymax, row=0; y>rct->ymin; y-=ROW_HEIGHT, row++) {
		if(row%2 == 0) {
			UI_ThemeColorShade(TH_BACK, 6);
			glRecti(v2d->cur.xmin, y-ROW_HEIGHT, v2d->cur.xmax, y);
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

	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


/* ************************ main outliner area region *********************** */

typedef struct CellRNA {
	SpaceOops *space;
	StructRNA *srna;
	PropertyRNA *prop;
	PointerRNA ptr;
	int lastrow, index;

	CollectionPropertyIterator iter;
} CellRNA;

static void rna_back_cb(bContext *C, void *arg_unused, void *arg_unused2)
{
	SpaceOops *soutliner= (SpaceOops*)CTX_wm_space_data(C);
	char *newpath;

	newpath= RNA_path_back(soutliner->rnapath);
	if(soutliner->rnapath)
		MEM_freeN(soutliner->rnapath);
	soutliner->rnapath= newpath;
}

static void rna_pointer_cb(bContext *C, void *arg_prop, void *arg_index)
{
	SpaceOops *soutliner= (SpaceOops*)CTX_wm_space_data(C);
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
	type= RNA_property_type(&cell->ptr, prop);
	subtype= RNA_property_subtype(&cell->ptr, prop);
	arraylength= RNA_property_array_length(&cell->ptr, prop);

	if(cell->index == -1) {
		uiDefBut(block, LABEL, 0, (char*)RNA_property_ui_name(&cell->ptr, prop), rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, (char*)RNA_property_ui_description(&cell->ptr, prop));
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

	RNA_property_collection_lookup_int(&cell->ptr, cell->prop, cell->index, &lookup);

	if(lookup.data) {
		nameprop= RNA_struct_name_property(&lookup);

		if(nameprop)
			nameptr= RNA_property_string_get_alloc(&lookup, nameprop, name, sizeof(name));

		if(!nameprop || strlen(nameptr) == 0)
			sprintf(nameptr, "%d", cell->index+1);
	}

	but= uiDefBut(block, BUT, 0, nameptr, rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin, 0, 0, 0, 0, 0, "");
	uiButSetFlag(but, UI_TEXT_LEFT);

	if(nameptr != name)
		MEM_freeN(nameptr);

	uiButSetFunc(but, rna_pointer_cb, cell->prop, SET_INT_IN_POINTER(cell->index));
}

static uiBut *rna_auto_but(uiBlock *block, PointerRNA *ptr, PropertyRNA *prop, int index, int x1, int y1, int x2, int y2)
{
	uiBut *but;
	const char *propname= RNA_property_identifier(ptr, prop);

	switch(RNA_property_type(ptr, prop)) {
		case PROP_BOOLEAN: {
			int value, length;

			length= RNA_property_array_length(ptr, prop);

			if(length)
				value= RNA_property_boolean_get_array(ptr, prop, index);
			else
				value= RNA_property_boolean_get(ptr, prop);

			but= uiDefButR(block, TOG, 0, (value)? "True": "False", x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		}
		case PROP_INT:
		case PROP_FLOAT:
			but= uiDefButR(block, NUM, 0, "", x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_ENUM:
			but= uiDefButR(block, MENU, 0, NULL, x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_STRING:
			but= uiDefButR(block, TEX, 0, "", x1, y1, x2, y2, ptr, propname, index, 0, 0, -1, -1, NULL);
			break;
		case PROP_POINTER: {
			PointerRNA pptr;
			PropertyRNA *nameprop;
			char name[256]= "", *nameptr= name;

			RNA_property_pointer_get(ptr, prop, &pptr);

			if(pptr.data) {
				nameprop= RNA_struct_name_property(&pptr);
				if(pptr.type && nameprop)
					nameptr= RNA_property_string_get_alloc(&pptr, nameprop, name, sizeof(name));
				else
					strcpy(nameptr, "->");
			}

			but= uiDefButR(block, BUT, 0, nameptr, x1, y1, x2, y2, ptr, propname, index, 0, 0, 0, 0, NULL);
			uiButSetFlag(but, UI_TEXT_LEFT);

			if(nameptr != name)
				MEM_freeN(nameptr);

			break;
		}
		default:
			but= NULL;
			break;
	}

	return but;
}

static void rna_but(CellRNA *cell, rcti *rct, uiBlock *block)
{
	uiBut *but;
	PropertyRNA *prop;
	PropertyType type;
	int arraylength, index;

	prop= cell->prop;
	type= RNA_property_type(&cell->ptr, prop);
	arraylength= RNA_property_array_length(&cell->ptr, prop);

	if(type == PROP_COLLECTION) {
		/* item in a collection */
		if(cell->index >= 0)
			rna_collection_but(cell, rct, block);
	}
	else {
		/* other cases */
		index= (arraylength)? cell->index: 0;

		if(index >= 0) {
			but= rna_auto_but(block, &cell->ptr, prop, index, rct->xmin, rct->ymin, rct->xmax-rct->xmin, rct->ymax-rct->ymin);

			if(type == PROP_POINTER)
				uiButSetFunc(but, rna_pointer_cb, prop, SET_INT_IN_POINTER(0));
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

			type= RNA_property_type(&cell->ptr, cell->prop);
			if(type == PROP_COLLECTION)
				length= RNA_property_collection_length(&cell->ptr, cell->prop);
			else
				length= RNA_property_array_length(&cell->ptr, cell->prop);

			/* verify if we need to go to the next property */
			if(type == PROP_COLLECTION && cell->index < length);
			else if(length && cell->index < length);
			else {
				RNA_property_collection_next(&cell->iter);
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

static void outliner_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Outliner", SPACE_OOPS, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
							   
}

static void outliner_rna_draw(const bContext *C, ARegion *ar)
{
	uiTable *table;
	rcti rct;
	CellRNA cell;
	PropertyRNA *prop, *iterprop;
	PointerRNA newptr;
	float col[3];
	int rows, cols, awidth, aheight, width, height;
	SpaceOops *soutliner= (SpaceOops*)CTX_wm_space_data(C);
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;

	/* clear */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	awidth= width= ar->winx;
	aheight= height= ar->winy;
	
	/* create table */
	cell.space= soutliner;
	cell.lastrow= -1;
	RNA_main_pointer_create(CTX_data_main(C), &cell.ptr);
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

	iterprop= RNA_struct_iterator_property(&cell.ptr);
	RNA_property_collection_begin(&cell.ptr, iterprop, &cell.iter);

	for(; cell.iter.valid; RNA_property_collection_next(&cell.iter)) {
		prop= cell.iter.ptr.data;

		rows += 1 + RNA_property_array_length(&cell.ptr, prop);
		if(RNA_property_type(&cell.ptr, prop) == PROP_COLLECTION)
			rows += RNA_property_collection_length(&cell.ptr, prop);
	}

	RNA_property_collection_end(&cell.iter);
	
	/* determine extents of data
	 *	- height must be at least the height of the mask area
	 *	- width is columns + 1, as otherwise, part of last column 
	 * 	  will be obscured by scrollers
	 */
	if ((rows*ROW_HEIGHT) > height)
		height= rows * ROW_HEIGHT;
	width= (cols + 1) * COLUMN_WIDTH;
	
	/* update size of tot-rect (extents of data/viewable area) */
	UI_view2d_totRect_set(v2d, width, height);
	
	rct.xmin= 0;
	rct.ymin= -height;
	rct.xmax= width;
	rct.ymax= 0;
	
	/* set matrix for 2d-view controls */
	UI_view2d_view_ortho(C, v2d);
	
	/* create and draw table */
	table= UI_table_create(rows, 2, &rct, rna_table_cell_func, &cell);

	RNA_property_collection_begin(&cell.ptr, iterprop, &cell.iter);
	UI_table_draw(C, table);
	RNA_property_collection_end(&cell.iter);

	UI_table_free(table);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void outliner_tree_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	draw_outliner(C);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void outliner_main_area_draw(const bContext *C, ARegion *ar)
{
	SpaceOops *so= (SpaceOops *)CTX_wm_space_data(C);
	
	if(so->type==SO_RNA)
		outliner_rna_draw(C, ar);
	else
		outliner_tree_draw(C, ar);
}


static void outliner_main_area_free(ARegion *ar)
{
}

/* ************************ header outliner area region *********************** */

/* add handlers, stuff you only do once or on area/region changes */
static void outliner_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void outliner_header_area_draw(const bContext *C, ARegion *ar)
{
	float col[3];
	
	if(ED_screen_area_active(C))
		UI_GetThemeColor3fv(TH_HEADER, col);
	else
		UI_GetThemeColor3fv(TH_HEADERDESEL, col);
	
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);

	outliner_header_buttons(C, ar);
}

static void outliner_header_area_free(ARegion *ar)
{
}

/* ******************** default callbacks for outliner space ***************** */

static SpaceLink *outliner_new(const bContext *C)
{
	ARegion *ar;
	SpaceOops *soutliner;

	soutliner= MEM_callocN(sizeof(SpaceOops), "initoutliner");
	soutliner->spacetype= SPACE_OOPS;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for outliner");
	
	BLI_addtail(&soutliner->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for outliner");
	
	BLI_addtail(&soutliner->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM_O);
	ar->v2d.align = (V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_POS_Y);
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_KEEPZOOM|V2D_KEEPASPECT);
	ar->v2d.keeptot= V2D_KEEPTOT_STRICT;
	ar->v2d.minzoom= ar->v2d.maxzoom= 1.0f;
	
	return (SpaceLink*)soutliner;
}

static void free_oops(Oops *oops)	/* also oops itself */
{
	BLI_freelistN(&oops->link);
	MEM_freeN(oops);
}

static void outliner_free_tree(ListBase *lb)
{
	
	while(lb->first) {
		TreeElement *te= lb->first;
		
		outliner_free_tree(&te->subtree);
		BLI_remlink(lb, te);
		MEM_freeN(te);
	}
}

/* not spacelink itself */
static void outliner_free(SpaceLink *sl)
{
	SpaceOops *soutliner= (SpaceOops*)sl;
	Oops *oops;

	if(soutliner->rnapath) {
		MEM_freeN(soutliner->rnapath);
		soutliner->rnapath= NULL;
	}

	while( (oops= soutliner->oops.first) ) {
		BLI_remlink(&soutliner->oops, oops);
		free_oops(oops);
	}
	
	outliner_free_tree(&soutliner->tree);
	if(soutliner->treestore) {
		if(soutliner->treestore->data) MEM_freeN(soutliner->treestore->data);
		MEM_freeN(soutliner->treestore);
	}
	
}

/* spacetype; init callback */
static void outliner_init(wmWindowManager *wm, ScrArea *sa)
{
	
}

static SpaceLink *outliner_duplicate(SpaceLink *sl)
{
	SpaceOops *soutliner= (SpaceOops *)sl;
	SpaceOops *soutlinern= MEM_dupallocN(soutliner);

	if(soutlinern->rnapath)
		soutlinern->rnapath= MEM_dupallocN(soutlinern->rnapath);
	
	soutlinern->oops.first= soutlinern->oops.last= NULL;
	soutlinern->tree.first= soutlinern->tree.last= NULL;
	soutlinern->treestore= NULL;
	
	return (SpaceLink *)soutlinern;
}

/* only called once, from space_api/spacetypes.c */
void ED_spacetype_outliner(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype time");
	ARegionType *art;
	
	st->spaceid= SPACE_OOPS;
	strncpy(st->name, "Outliner", BKE_ST_MAXNAME);
	
	st->new= outliner_new;
	st->free= outliner_free;
	st->init= outliner_init;
	st->duplicate= outliner_duplicate;
	st->operatortypes= outliner_operatortypes;
	st->keymap= outliner_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= outliner_main_area_init;
	art->draw= outliner_main_area_draw;
	art->free= outliner_main_area_free;
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= outliner_header_area_init;
	art->draw= outliner_header_area_draw;
	art->free= outliner_header_area_free;
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

