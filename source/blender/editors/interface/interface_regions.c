/**
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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_view2d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_draw.h"
#include "wm_subwindow.h"
#include "wm_window.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "BLF_api.h"

#include "ED_screen.h"

#include "interface_intern.h"

#define MENU_BUTTON_HEIGHT	20
#define MENU_SEPR_HEIGHT	6
#define B_NOP              	-1
#define MENU_SHADOW_SIDE	8
#define MENU_SHADOW_BOTTOM	10
#define MENU_TOP			8

/*********************** Menu Data Parsing ********************* */

typedef struct {
	char *str;
	int retval;
	int icon;
} MenuEntry;

typedef struct {
	char *instr;
	char *title;
	int titleicon;
	
	MenuEntry *items;
	int nitems, itemssize;
} MenuData;

static MenuData *menudata_new(char *instr)
{
	MenuData *md= MEM_mallocN(sizeof(*md), "MenuData");

	md->instr= instr;
	md->title= NULL;
	md->titleicon= 0;
	md->items= NULL;
	md->nitems= md->itemssize= 0;
	
	return md;
}

static void menudata_set_title(MenuData *md, char *title, int titleicon)
{
	if (!md->title)
		md->title= title;
	if (!md->titleicon)
		md->titleicon= titleicon;
}

static void menudata_add_item(MenuData *md, char *str, int retval, int icon)
{
	if (md->nitems==md->itemssize) {
		int nsize= md->itemssize?(md->itemssize<<1):1;
		MenuEntry *oitems= md->items;
		
		md->items= MEM_mallocN(nsize*sizeof(*md->items), "md->items");
		if (oitems) {
			memcpy(md->items, oitems, md->nitems*sizeof(*md->items));
			MEM_freeN(oitems);
		}
		
		md->itemssize= nsize;
	}
	
	md->items[md->nitems].str= str;
	md->items[md->nitems].retval= retval;
	md->items[md->nitems].icon= icon;
	md->nitems++;
}

void menudata_free(MenuData *md)
{
	MEM_freeN(md->instr);
	if (md->items)
		MEM_freeN(md->items);
	MEM_freeN(md);
}

	/**
	 * Parse menu description strings, string is of the
	 * form "[sss%t|]{(sss[%xNN]|), (%l|)}", ssss%t indicates the
	 * menu title, sss or sss%xNN indicates an option, 
	 * if %xNN is given then NN is the return value if
	 * that option is selected otherwise the return value
	 * is the index of the option (starting with 1). %l
	 * indicates a seperator.
	 * 
	 * @param str String to be parsed.
	 * @retval new menudata structure, free with menudata_free()
	 */
MenuData *decompose_menu_string(char *str) 
{
	char *instr= BLI_strdup(str);
	MenuData *md= menudata_new(instr);
	char *nitem= NULL, *s= instr;
	int nicon=0, nretval= 1, nitem_is_title= 0;
	
	while (1) {
		char c= *s;

		if (c=='%') {
			if (s[1]=='x') {
				nretval= atoi(s+2);

				*s= '\0';
				s++;
			} else if (s[1]=='t') {
				nitem_is_title= 1;

				*s= '\0';
				s++;
			} else if (s[1]=='l') {
				nitem= "%l";
				s++;
			} else if (s[1]=='i') {
				nicon= atoi(s+2);
				
				*s= '\0';
				s++;
			}
		} else if (c=='|' || c=='\0') {
			if (nitem) {
				*s= '\0';

				if (nitem_is_title) {
					menudata_set_title(md, nitem, nicon);
					nitem_is_title= 0;
				} else {
					/* prevent separator to get a value */
					if(nitem[0]=='%' && nitem[1]=='l')
						menudata_add_item(md, nitem, -1, nicon);
					else
						menudata_add_item(md, nitem, nretval, nicon);
					nretval= md->nitems+1;
				} 
				
				nitem= NULL;
				nicon= 0;
			}
			
			if (c=='\0')
				break;
		} else if (!nitem)
			nitem= s;
		
		s++;
	}
	
	return md;
}

void ui_set_name_menu(uiBut *but, int value)
{
	MenuData *md;
	int i;
	
	md= decompose_menu_string(but->str);
	for (i=0; i<md->nitems; i++)
		if (md->items[i].retval==value)
			strcpy(but->drawstr, md->items[i].str);
	
	menudata_free(md);
}

/******************** Creating Temporary regions ******************/

ARegion *ui_add_temporary_region(bScreen *sc)
{
	ARegion *ar;

	ar= MEM_callocN(sizeof(ARegion), "area region");
	BLI_addtail(&sc->regionbase, ar);

	ar->regiontype= RGN_TYPE_TEMPORARY;
	ar->alignment= RGN_ALIGN_FLOAT;

	return ar;
}

void ui_remove_temporary_region(bContext *C, bScreen *sc, ARegion *ar)
{
	if(CTX_wm_window(C))
		wm_draw_region_clear(CTX_wm_window(C), ar);

	ED_region_exit(C, ar);
	BKE_area_region_free(NULL, ar);		/* NULL: no spacetype */
	BLI_freelinkN(&sc->regionbase, ar);
}

/************************* Creating Tooltips **********************/

typedef struct uiTooltipData {
	rcti bbox;
	uiFontStyle fstyle;
	char *tip;
} uiTooltipData;

static void ui_tooltip_region_draw(const bContext *C, ARegion *ar)
{
	uiTooltipData *data= ar->regiondata;
	
	ui_draw_menu_back(U.uistyles.first, NULL, &data->bbox);
	
	/* draw text */
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	uiStyleFontSet(&data->fstyle);
	uiStyleFontDraw(&data->fstyle, &data->bbox, data->tip);
}

static void ui_tooltip_region_free(ARegion *ar)
{
	uiTooltipData *data;

	data= ar->regiondata;
	MEM_freeN(data->tip);
	MEM_freeN(data);
	ar->regiondata= NULL;
}

ARegion *ui_tooltip_create(bContext *C, ARegion *butregion, uiBut *but)
{
	uiStyle *style= U.uistyles.first;	// XXX pass on as arg
	static ARegionType type;
	ARegion *ar;
	uiTooltipData *data;
	float fonth, fontw, aspect= but->block->aspect;
	float x1f, x2f, y1f, y2f;
	int x1, x2, y1, y2, winx, winy, ofsx, ofsy;

	if(!but->tip || strlen(but->tip)==0)
		return NULL;

	/* create area region */
	ar= ui_add_temporary_region(CTX_wm_screen(C));

	memset(&type, 0, sizeof(ARegionType));
	type.draw= ui_tooltip_region_draw;
	type.free= ui_tooltip_region_free;
	ar->type= &type;

	/* create tooltip data */
	data= MEM_callocN(sizeof(uiTooltipData), "uiTooltipData");
	data->tip= BLI_strdup(but->tip);
	
	/* set font, get bb */
	data->fstyle= style->widget; /* copy struct */
	data->fstyle.align= UI_STYLE_TEXT_CENTER;
	ui_fontscale(&data->fstyle.points, aspect);
	uiStyleFontSet(&data->fstyle);
	fontw= aspect * BLF_width(data->tip);
	fonth= aspect * BLF_height(data->tip);

	ar->regiondata= data;

	/* compute position */
	ofsx= (but->block->panel)? but->block->panel->ofsx: 0;
	ofsy= (but->block->panel)? but->block->panel->ofsy: 0;

	x1f= (but->x1+but->x2)/2.0f + ofsx - 16.0f*aspect;
	x2f= x1f + fontw + 16.0f*aspect;
	y2f= but->y1 + ofsy - 15.0f*aspect;
	y1f= y2f - fonth - 10.0f*aspect;
	
	/* copy to int, gets projected if possible too */
	x1= x1f; y1= y1f; x2= x2f; y2= y2f; 
	
	if(butregion) {
		/* XXX temp, region v2ds can be empty still */
		if(butregion->v2d.cur.xmin != butregion->v2d.cur.xmax) {
			UI_view2d_to_region_no_clip(&butregion->v2d, x1f, y1f, &x1, &y1);
			UI_view2d_to_region_no_clip(&butregion->v2d, x2f, y2f, &x2, &y2);
		}

		x1 += butregion->winrct.xmin;
		x2 += butregion->winrct.xmin;
		y1 += butregion->winrct.ymin;
		y2 += butregion->winrct.ymin;
	}

	wm_window_get_size(CTX_wm_window(C), &winx, &winy);

	if(x2 > winx) {
		/* super size */
		if(x2 > winx + x1) {
			x2= winx;
			x1= 0;
		}
		else {
			x1 -= x2-winx;
			x2= winx;
		}
	}
	if(y1 < 0) {
		y1 += 36;
		y2 += 36;
	}

	/* widget rect, in region coords */
	data->bbox.xmin= MENU_SHADOW_SIDE;
	data->bbox.xmax= x2-x1 + MENU_SHADOW_SIDE;
	data->bbox.ymin= MENU_SHADOW_BOTTOM;
	data->bbox.ymax= y2-y1 + MENU_SHADOW_BOTTOM;
	
	/* region bigger for shadow */
	ar->winrct.xmin= x1 - MENU_SHADOW_SIDE;
	ar->winrct.xmax= x2 + MENU_SHADOW_SIDE;
	ar->winrct.ymin= y1 - MENU_SHADOW_BOTTOM;
	ar->winrct.ymax= y2 + MENU_TOP;

	/* adds subwindow */
	ED_region_init(C, ar);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	return ar;
}

void ui_tooltip_free(bContext *C, ARegion *ar)
{
	ui_remove_temporary_region(C, CTX_wm_screen(C), ar);
}

/************************* Creating Menu Blocks **********************/

/* position block relative to but, result is in window space */
static void ui_block_position(wmWindow *window, ARegion *butregion, uiBut *but, uiBlock *block)
{
	uiBut *bt;
	uiSafetyRct *saferct;
	rctf butrct;
	float aspect;
	int xsize, ysize, xof=0, yof=0, center;
	short dir1= 0, dir2=0;
	
	/* transform to window coordinates, using the source button region/block */
	butrct.xmin= but->x1; butrct.xmax= but->x2;
	butrct.ymin= but->y1; butrct.ymax= but->y2;

	ui_block_to_window_fl(butregion, but->block, &butrct.xmin, &butrct.ymin);
	ui_block_to_window_fl(butregion, but->block, &butrct.xmax, &butrct.ymax);

	/* calc block rect */
	if(block->minx == 0.0f && block->maxx == 0.0f) {
		if(block->buttons.first) {
			block->minx= block->miny= 10000;
			block->maxx= block->maxy= -10000;
			
			bt= block->buttons.first;
			while(bt) {
				if(bt->x1 < block->minx) block->minx= bt->x1;
				if(bt->y1 < block->miny) block->miny= bt->y1;

				if(bt->x2 > block->maxx) block->maxx= bt->x2;
				if(bt->y2 > block->maxy) block->maxy= bt->y2;
				
				bt= bt->next;
			}
		}
		else {
			/* we're nice and allow empty blocks too */
			block->minx= block->miny= 0;
			block->maxx= block->maxy= 20;
		}
	}
	
	aspect= (float)(block->maxx - block->minx + 4);
	ui_block_to_window_fl(butregion, but->block, &block->minx, &block->miny);
	ui_block_to_window_fl(butregion, but->block, &block->maxx, &block->maxy);

	//block->minx-= 2.0; block->miny-= 2.0;
	//block->maxx+= 2.0; block->maxy+= 2.0;
	
	xsize= block->maxx - block->minx+4; // 4 for shadow
	ysize= block->maxy - block->miny+4;
	aspect/= (float)xsize;

	if(but) {
		int left=0, right=0, top=0, down=0;
		int winx, winy;

		wm_window_get_size(window, &winx, &winy);

		if(block->direction & UI_CENTER) center= ysize/2;
		else center= 0;

		if( butrct.xmin-xsize > 0.0) left= 1;
		if( butrct.xmax+xsize < winx) right= 1;
		if( butrct.ymin-ysize+center > 0.0) down= 1;
		if( butrct.ymax+ysize-center < winy) top= 1;
		
		dir1= block->direction & UI_DIRECTION;

		/* secundary directions */
		if(dir1 & (UI_TOP|UI_DOWN)) {
			if(dir1 & UI_LEFT) dir2= UI_LEFT;
			else if(dir1 & UI_RIGHT) dir2= UI_RIGHT;
			dir1 &= (UI_TOP|UI_DOWN);
		}

		if(dir2==0) if(dir1==UI_LEFT || dir1==UI_RIGHT) dir2= UI_DOWN;
		if(dir2==0) if(dir1==UI_TOP || dir1==UI_DOWN) dir2= UI_LEFT;
		
		/* no space at all? dont change */
		if(left || right) {
			if(dir1==UI_LEFT && left==0) dir1= UI_RIGHT;
			if(dir1==UI_RIGHT && right==0) dir1= UI_LEFT;
			/* this is aligning, not append! */
			if(dir2==UI_LEFT && right==0) dir2= UI_RIGHT;
			if(dir2==UI_RIGHT && left==0) dir2= UI_LEFT;
		}
		if(down || top) {
			if(dir1==UI_TOP && top==0) dir1= UI_DOWN;
			if(dir1==UI_DOWN && down==0) dir1= UI_TOP;
			if(dir2==UI_TOP && top==0) dir2= UI_DOWN;
			if(dir2==UI_DOWN && down==0) dir2= UI_TOP;
		}
		
		if(dir1==UI_LEFT) {
			xof= butrct.xmin - block->maxx;
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-center;
			else yof= butrct.ymax - block->maxy+center;
		}
		else if(dir1==UI_RIGHT) {
			xof= butrct.xmax - block->minx;
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-center;
			else yof= butrct.ymax - block->maxy+center;
		}
		else if(dir1==UI_TOP) {
			yof= butrct.ymax - block->miny;
			if(dir2==UI_RIGHT) xof= butrct.xmax - block->maxx;
			else xof= butrct.xmin - block->minx;
			// changed direction? 
			if((dir1 & block->direction)==0) {
				if(block->direction & UI_SHIFT_FLIPPED)
					xof+= dir2==UI_LEFT?25:-25;
				uiBlockFlipOrder(block);
			}
		}
		else if(dir1==UI_DOWN) {
			yof= butrct.ymin - block->maxy;
			if(dir2==UI_RIGHT) xof= butrct.xmax - block->maxx;
			else xof= butrct.xmin - block->minx;
			// changed direction?
			if((dir1 & block->direction)==0) {
				if(block->direction & UI_SHIFT_FLIPPED)
					xof+= dir2==UI_LEFT?25:-25;
				uiBlockFlipOrder(block);
			}
		}

		/* and now we handle the exception; no space below or to top */
		if(top==0 && down==0) {
			if(dir1==UI_LEFT || dir1==UI_RIGHT) {
				// align with bottom of screen 
				yof= ysize;
			}
		}
		
		/* or no space left or right */
		if(left==0 && right==0) {
			if(dir1==UI_TOP || dir1==UI_DOWN) {
				// align with left size of screen 
				xof= -block->minx+5;
			}
		}
		
		// apply requested offset in the block
		xof += block->xofs/block->aspect;
		yof += block->yofs/block->aspect;
	}
	
	/* apply */
	
	for(bt= block->buttons.first; bt; bt= bt->next) {
		ui_block_to_window_fl(butregion, but->block, &bt->x1, &bt->y1);
		ui_block_to_window_fl(butregion, but->block, &bt->x2, &bt->y2);

		bt->x1 += xof;
		bt->x2 += xof;
		bt->y1 += yof;
		bt->y2 += yof;

		bt->aspect= 1.0;
		// ui_check_but recalculates drawstring size in pixels
		ui_check_but(bt);
	}
	
	block->minx += xof;
	block->miny += yof;
	block->maxx += xof;
	block->maxy += yof;

	/* safety calculus */
	if(but) {
		float midx= (butrct.xmin+butrct.xmax)/2.0;
		float midy= (butrct.ymin+butrct.ymax)/2.0;
		
		/* when you are outside parent button, safety there should be smaller */
		
		// parent button to left
		if( midx < block->minx ) block->safety.xmin= block->minx-3; 
		else block->safety.xmin= block->minx-40;
		// parent button to right
		if( midx > block->maxx ) block->safety.xmax= block->maxx+3; 
		else block->safety.xmax= block->maxx+40;
		
		// parent button on bottom
		if( midy < block->miny ) block->safety.ymin= block->miny-3; 
		else block->safety.ymin= block->miny-40;
		// parent button on top
		if( midy > block->maxy ) block->safety.ymax= block->maxy+3; 
		else block->safety.ymax= block->maxy+40;
		
		// exception for switched pulldowns...
		if(dir1 && (dir1 & block->direction)==0) {
			if(dir2==UI_RIGHT) block->safety.xmax= block->maxx+3; 
			if(dir2==UI_LEFT) block->safety.xmin= block->minx-3; 
		}
		block->direction= dir1;
	}
	else {
		block->safety.xmin= block->minx-40;
		block->safety.ymin= block->miny-40;
		block->safety.xmax= block->maxx+40;
		block->safety.ymax= block->maxy+40;
	}

	/* keep a list of these, needed for pulldown menus */
	saferct= MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
	saferct->parent= butrct;
	saferct->safety= block->safety;
	BLI_freelistN(&block->saferct);
	if(but)
		BLI_duplicatelist(&block->saferct, &but->block->saferct);
	BLI_addhead(&block->saferct, saferct);
}

static void ui_block_region_draw(const bContext *C, ARegion *ar)
{
	uiBlock *block;

	for(block=ar->uiblocks.first; block; block=block->next)
		uiDrawBlock(C, block);
}

uiPopupBlockHandle *ui_popup_block_create(bContext *C, ARegion *butregion, uiBut *but, uiBlockCreateFunc create_func, uiBlockHandleCreateFunc handle_create_func, void *arg)
{
	wmWindow *window= CTX_wm_window(C);
	static ARegionType type;
	ARegion *ar;
	uiBlock *block;
	uiBut *bt;
	uiPopupBlockHandle *handle;
	uiSafetyRct *saferct;

	/* create handle */
	handle= MEM_callocN(sizeof(uiPopupBlockHandle), "uiPopupBlockHandle");

	/* store context for operator */
	handle->ctx_area= CTX_wm_area(C);
	handle->ctx_region= CTX_wm_region(C);
	
	/* create area region */
	ar= ui_add_temporary_region(CTX_wm_screen(C));

	memset(&type, 0, sizeof(ARegionType));
	type.draw= ui_block_region_draw;
	ar->type= &type;

	UI_add_region_handlers(&ar->handlers);

	handle->region= ar;
	ar->regiondata= handle;

	/* create ui block */
	if(create_func)
		block= create_func(C, handle->region, arg);
	else
		block= handle_create_func(C, handle, arg);
	block->handle= handle;

	if(!block->endblock)
		uiEndBlock(C, block);

	/* if this is being created from a button */
	if(but) {
		if(ELEM3(but->type, BLOCK, PULLDOWN, HMENU))
			block->xofs = -2;	/* for proper alignment */

		/* only used for automatic toolbox, so can set the shift flag */
		if(but->flag & UI_MAKE_TOP) {
			block->direction= UI_TOP|UI_SHIFT_FLIPPED;
			uiBlockFlipOrder(block);
		}
		if(but->flag & UI_MAKE_DOWN) block->direction= UI_DOWN|UI_SHIFT_FLIPPED;
		if(but->flag & UI_MAKE_LEFT) block->direction |= UI_LEFT;
		if(but->flag & UI_MAKE_RIGHT) block->direction |= UI_RIGHT;

		ui_block_position(window, butregion, but, block);
	}
	else {
		/* keep a list of these, needed for pulldown menus */
		saferct= MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
		saferct->safety= block->safety;
		BLI_addhead(&block->saferct, saferct);
		block->flag |= UI_BLOCK_POPUP;
	}

	/* the block and buttons were positioned in window space as in 2.4x, now
	 * these menu blocks are regions so we bring it back to region space.
	 * additionally we add some padding for the menu shadow or rounded menus */
	ar->winrct.xmin= block->minx - MENU_SHADOW_SIDE;
	ar->winrct.xmax= block->maxx + MENU_SHADOW_SIDE;
	ar->winrct.ymin= block->miny - MENU_SHADOW_BOTTOM;
	ar->winrct.ymax= block->maxy + MENU_TOP;

	block->minx -= ar->winrct.xmin;
	block->maxx -= ar->winrct.xmin;
	block->miny -= ar->winrct.ymin;
	block->maxy -= ar->winrct.ymin;

	for(bt= block->buttons.first; bt; bt= bt->next) {
		bt->x1 -= ar->winrct.xmin;
		bt->x2 -= ar->winrct.xmin;
		bt->y1 -= ar->winrct.ymin;
		bt->y2 -= ar->winrct.ymin;
	}

	block->flag |= UI_BLOCK_LOOP|UI_BLOCK_MOVEMOUSE_QUIT;

	/* adds subwindow */
	ED_region_init(C, ar);

	/* get winmat now that we actually have the subwindow */
	wmSubWindowSet(window, ar->swinid);
			// XXX ton, AA pixel space...
	wmOrtho2(0.0, (float)ar->winrct.xmax-ar->winrct.xmin+1, 0.0, (float)ar->winrct.ymax-ar->winrct.ymin+1);
	
	wm_subwindow_getmatrix(window, ar->swinid, block->winmat);
	
	/* notify change and redraw */
	ED_region_tag_redraw(ar);

	return handle;
}

void ui_popup_block_free(bContext *C, uiPopupBlockHandle *handle)
{
	ui_remove_temporary_region(C, CTX_wm_screen(C), handle->region);
	MEM_freeN(handle);
}

/***************************** Menu Button ***************************/

uiBlock *ui_block_func_MENU(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	uiBut *bt;
	MenuData *md;
	ListBase lb;
	float aspect;
	int width, height, boxh, columns, rows, startx, starty, x1, y1, xmax, a;

	/* create the block */
	block= uiBeginBlock(C, handle->region, "menu", UI_EMBOSSP);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;

	/* compute menu data */
	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if(columns<1)
		columns= 1;
	if(columns>8)
		columns= (md->nitems+25)/25;
	
	rows= md->nitems/columns;
	if(rows<1)
		rows= 1;
	while(rows*columns<md->nitems)
		rows++;
		
	/* prevent scaling up of pupmenu */
	aspect= but->aspect;
	if(aspect < 1.0f)
		aspect = 1.0f;

	/* size and location */
	if(md->title)
		width= 1.5*aspect*strlen(md->title)+UI_GetStringWidth(md->title);
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= aspect*UI_GetStringWidth(md->items[a].str);
		if(md->items[a].icon)
			xmax += 20*aspect;
		if(xmax>width)
			width= xmax;
	}

	width+= 10;
	if(width < (but->x2 - but->x1))
		width = (but->x2 - but->x1);
	if(width<50)
		width=50;

	boxh= MENU_BUTTON_HEIGHT;
	
	height= rows*boxh;
	if(md->title)
		height+= boxh;

	/* here we go! */
	startx= but->x1;
	starty= but->y1;
	
	if(md->title) {
		uiBut *bt;

		if (md->titleicon) {
			bt= uiDefIconTextBut(block, LABEL, 0, md->titleicon, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		} else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
	}

	for(a=0; a<md->nitems; a++) {
		
		x1= startx + width*((int)(md->nitems-a-1)/rows);
		y1= starty - boxh*(rows - ((md->nitems - a - 1)%rows)) + (rows*boxh);

		if (strcmp(md->items[md->nitems-a-1].str, "%l")==0) {
			bt= uiDefBut(block, SEPR, B_NOP, "", x1, y1,(short)(width-(rows>1)), (short)(boxh-1), NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(md->items[md->nitems-a-1].icon) {
			bt= uiDefIconTextButF(block, BUTM|FLO, B_NOP, md->items[md->nitems-a-1].icon ,md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), &handle->retvalue, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
		}
		else {
			bt= uiDefButF(block, BUTM|FLO, B_NOP, md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), &handle->retvalue, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
		}
	}
	
	menudata_free(md);

	/* the code up here has flipped locations, because of change of preferred order */
	/* thats why we have to switch list order too, to make arrowkeys work */
	
	lb.first= lb.last= NULL;
	bt= block->buttons.first;
	while(bt) {
		uiBut *next= bt->next;
		BLI_remlink(&block->buttons, bt);
		BLI_addhead(&lb, bt);
		bt= next;
	}
	block->buttons= lb;

	block->direction= UI_TOP;
	uiEndBlock(C, block);

	return block;
}

uiBlock *ui_block_func_ICONROW(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	int a;
	
	block= uiBeginBlock(C, handle->region, "menu", UI_EMBOSSP);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	
	for(a=(int)but->hardmin; a<=(int)but->hardmax; a++) {
		uiDefIconButF(block, BUTM|FLO, B_NOP, but->icon+(a-but->hardmin), 0, (short)(18*a), (short)(but->x2-but->x1-4), 18, &handle->retvalue, (float)a, 0.0, 0, 0, "");
	}

	block->direction= UI_TOP;	

	uiEndBlock(C, block);

	return block;
}

uiBlock *ui_block_func_ICONTEXTROW(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	MenuData *md;
	int width, xmax, ypos, a;

	block= uiBeginBlock(C, handle->region, "menu", UI_EMBOSSP);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;

	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
	if(md->title)
		width= 2*strlen(md->title)+UI_GetStringWidth(md->title);
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(md->items[a].str);
		if(xmax>width) width= xmax;
	}

	width+= 30;
	if (width<50) width=50;

	ypos = 1;

	/* loop through the menu options and draw them out with icons & text labels */
	for(a=0; a<md->nitems; a++) {

		/* add a space if there's a separator (%l) */
	        if (strcmp(md->items[a].str, "%l")==0) {
			ypos +=3;
		}
		else {
			uiDefIconTextButF(block, BUTM|FLO, B_NOP, (short)((but->icon)+(md->items[a].retval-but->hardmin)), md->items[a].str, 0, ypos,(short)width, 19, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			ypos += 20;
		}
	}
	
	if(md->title) {
		uiBut *bt;

		bt= uiDefBut(block, LABEL, 0, md->title, 0, ypos, (short)width, 19, NULL, 0.0, 0.0, 0, 0, "");
		bt->flag= UI_TEXT_LEFT;
	}
	
	menudata_free(md);

	block->direction= UI_TOP;

	uiBoundsBlock(block, 3);
	uiEndBlock(C, block);

	return block;
}

static void ui_warp_pointer(short x, short y)
{
	/* XXX 2.50 which function to use for this? */
#if 0
	/* OSX has very poor mousewarp support, it sends events;
	   this causes a menu being pressed immediately ... */
	#ifndef __APPLE__
	warp_pointer(x, y);
	#endif
#endif
}

/********************* Color Button ****************/

/* picker sizes S hsize, F full size, D spacer, B button/pallette height  */
#define SPICK	110.0
#define FPICK	180.0
#define DPICK	6.0
#define BPICK	24.0

#define UI_PALETTE_TOT 16
/* note; in tot+1 the old color is stored */
static float palette[UI_PALETTE_TOT+1][3]= {
{0.93, 0.83, 0.81}, {0.88, 0.89, 0.73}, {0.69, 0.81, 0.57}, {0.51, 0.76, 0.64}, 
{0.37, 0.56, 0.61}, {0.33, 0.29, 0.55}, {0.46, 0.21, 0.51}, {0.40, 0.12, 0.18}, 
{1.0, 1.0, 1.0}, {0.85, 0.85, 0.85}, {0.7, 0.7, 0.7}, {0.56, 0.56, 0.56}, 
{0.42, 0.42, 0.42}, {0.28, 0.28, 0.28}, {0.14, 0.14, 0.14}, {0.0, 0.0, 0.0}
};  

/* for picker, while editing hsv */
void ui_set_but_hsv(uiBut *but)
{
	float col[3];
	
	hsv_to_rgb(but->hsv[0], but->hsv[1], but->hsv[2], col, col+1, col+2);
	ui_set_but_vectorf(but, col);
}

static void update_picker_hex(uiBlock *block, float *rgb)
{
	uiBut *bt;
	char col[16];
	
	sprintf(col, "%02X%02X%02X", (unsigned int)(rgb[0]*255.0), (unsigned int)(rgb[1]*255.0), (unsigned int)(rgb[2]*255.0));
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(strcmp(bt->str, "Hex: ")==0) {
			strcpy(bt->poin, col);
			ui_check_but(bt);
			break;
		}
	}
}

void ui_update_block_buts_hsv(uiBlock *block, float *hsv)
{
	uiBut *bt;
	float r, g, b;
	float rgb[3];
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], &r, &g, &b);
	
	rgb[0] = r; rgb[1] = g; rgb[2] = b;
	update_picker_hex(block, rgb);

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(bt->type==HSVCUBE) {
			VECCOPY(bt->hsv, hsv);
			ui_set_but_hsv(bt);
		}
		else if(bt->str[1]==' ') {
			if(bt->str[0]=='R') {
				ui_set_but_val(bt, r);
			}
			else if(bt->str[0]=='G') {
				ui_set_but_val(bt, g);
			}
			else if(bt->str[0]=='B') {
				ui_set_but_val(bt, b);
			}
			else if(bt->str[0]=='H') {
				ui_set_but_val(bt, hsv[0]);
			}
			else if(bt->str[0]=='S') {
				ui_set_but_val(bt, hsv[1]);
			}
			else if(bt->str[0]=='V') {
				ui_set_but_val(bt, hsv[2]);
			}
		}		
	}
}

static void ui_update_block_buts_hex(uiBlock *block, char *hexcol)
{
	uiBut *bt;
	float r=0, g=0, b=0;
	float h, s, v;
	
	
	// this updates button strings, is hackish... but button pointers are on stack of caller function
	hex_to_rgb(hexcol, &r, &g, &b);
	rgb_to_hsv(r, g, b, &h, &s, &v);

	for(bt= block->buttons.first; bt; bt= bt->next) {
		if(bt->type==HSVCUBE) {
			bt->hsv[0] = h;
			bt->hsv[1] = s;			
			bt->hsv[2] = v;
			ui_set_but_hsv(bt);
		}
		else if(bt->str[1]==' ') {
			if(bt->str[0]=='R') {
				ui_set_but_val(bt, r);
			}
			else if(bt->str[0]=='G') {
				ui_set_but_val(bt, g);
			}
			else if(bt->str[0]=='B') {
				ui_set_but_val(bt, b);
			}
			else if(bt->str[0]=='H') {
				ui_set_but_val(bt, h);
			}
			else if(bt->str[0]=='S') {
				ui_set_but_val(bt, s);
			}
			else if(bt->str[0]=='V') {
				ui_set_but_val(bt, v);
			}
		}
	}
}

/* bt1 is palette but, col1 is original color */
/* callback to copy from/to palette */
static void do_palette_cb(bContext *C, void *bt1, void *col1)
{
	uiBut *but1= (uiBut *)bt1;
	float *col= (float *)col1;
	float *fp, hsv[3];
	
	fp= (float *)but1->poin;
	
	/* XXX 2.50 bad access, how to solve?
	 *
	if( (get_qual() & LR_CTRLKEY) ) {
		VECCOPY(fp, col);
	}
	else*/ {
		VECCOPY(col, fp);
	}
	
	rgb_to_hsv(col[0], col[1], col[2], hsv, hsv+1, hsv+2);
	ui_update_block_buts_hsv(but1->block, hsv);
	update_picker_hex(but1->block, col);
}

/* bt1 is num but, hsv1 is pointer to original color in hsv space*/
/* callback to handle changes in num-buts in picker */
static void do_palette1_cb(bContext *C, void *bt1, void *hsv1)
{
	uiBut *but1= (uiBut *)bt1;
	float *hsv= (float *)hsv1;
	float *fp= NULL;
	
	if(but1->str[1]==' ') {
		if(but1->str[0]=='R') fp= (float *)but1->poin;
		else if(but1->str[0]=='G') fp= ((float *)but1->poin)-1;
		else if(but1->str[0]=='B') fp= ((float *)but1->poin)-2;
	}
	if(fp) {
		rgb_to_hsv(fp[0], fp[1], fp[2], hsv, hsv+1, hsv+2);
	} 
	ui_update_block_buts_hsv(but1->block, hsv);
}

/* bt1 is num but, col1 is pointer to original color */
/* callback to handle changes in num-buts in picker */
static void do_palette2_cb(bContext *C, void *bt1, void *col1)
{
	uiBut *but1= (uiBut *)bt1;
	float *rgb= (float *)col1;
	float *fp= NULL;
	
	if(but1->str[1]==' ') {
		if(but1->str[0]=='H') fp= (float *)but1->poin;
		else if(but1->str[0]=='S') fp= ((float *)but1->poin)-1;
		else if(but1->str[0]=='V') fp= ((float *)but1->poin)-2;
	}
	if(fp) {
		hsv_to_rgb(fp[0], fp[1], fp[2], rgb, rgb+1, rgb+2);
	} 
	ui_update_block_buts_hsv(but1->block, fp);
}

static void do_palette_hex_cb(bContext *C, void *bt1, void *hexcl)
{
	uiBut *but1= (uiBut *)bt1;
	char *hexcol= (char *)hexcl;
	
	ui_update_block_buts_hex(but1->block, hexcol);	
}

/* used for both 3d view and image window */
static void do_palette_sample_cb(bContext *C, void *bt1, void *col1)	/* frontbuf */
{
	/* XXX 2.50 this should become an operator? */
#if 0
	uiBut *but1= (uiBut *)bt1;
	uiBut *but;
	float tempcol[4];
	int x=0, y=0;
	short mval[2];
	float hsv[3];
	short capturing;
	int oldcursor;
	Window *win;
	unsigned short dev;
	
	oldcursor=get_cursor();
	win=winlay_get_active_window();
	
	while (get_mbut() & L_MOUSE) UI_wait_for_statechange();
	
	SetBlenderCursor(BC_EYEDROPPER_CURSOR);
	
	/* loop and wait for a mouse click */
	capturing = TRUE;
	while(capturing) {
		char ascii;
		short val;
		
		dev = extern_qread_ext(&val, &ascii);
		
		if(dev==INPUTCHANGE) break;
		if(get_mbut() & R_MOUSE) break;
		else if(get_mbut() & L_MOUSE) {
			uiGetMouse(mywinget(), mval);
			x= mval[0]; y= mval[1];
			
			capturing = FALSE;
			break;
		}
		else if(dev==ESCKEY) break;
	}
	window_set_cursor(win, oldcursor);
	
	if(capturing) return;
	
	if(x<0 || y<0) return;
	
	/* if we've got a glick, use OpenGL to sample the color under the mouse pointer */
	glReadBuffer(GL_FRONT);
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, tempcol);
	glReadBuffer(GL_BACK);
	
	/* and send that color back to the picker */
	rgb_to_hsv(tempcol[0], tempcol[1], tempcol[2], hsv, hsv+1, hsv+2);
	ui_update_block_buts_hsv(but1->block, hsv);
	update_picker_hex(but1->block, tempcol);
	
	for (but= but1->block->buttons.first; but; but= but->next) {
		ui_check_but(but);
		ui_draw_but(but);
	}
	
	but= but1->block->buttons.first;
	ui_block_flush_back(but->block);
#endif
}

/* color picker, Gimp version. mode: 'f' = floating panel, 'p' =  popup */
/* col = read/write to, hsv/old/hexcol = memory for temporal use */
void uiBlockPickerButtons(uiBlock *block, float *col, float *hsv, float *old, char *hexcol, char mode, short retval)
{
	uiBut *bt;
	float h, offs;
	int a;

	VECCOPY(old, col);	// old color stored there, for palette_cb to work
	
	// the cube intersection
	bt= uiDefButF(block, HSVCUBE, retval, "",	0,DPICK+BPICK,FPICK,FPICK, col, 0.0, 0.0, 2, 0, "");
	uiButSetFlag(bt, UI_NO_HILITE);

	bt= uiDefButF(block, HSVCUBE, retval, "",	0,0,FPICK,BPICK, col, 0.0, 0.0, 3, 0, "");
	uiButSetFlag(bt, UI_NO_HILITE);

	// palette
	
	uiBlockSetEmboss(block, UI_EMBOSSP);
	
	bt=uiDefButF(block, COL, retval, "",		FPICK+DPICK, 0, BPICK,BPICK, old, 0.0, 0.0, -1, 0, "Old color, click to restore");
	uiButSetFunc(bt, do_palette_cb, bt, col);
	uiDefButF(block, COL, retval, "",		FPICK+DPICK, BPICK+DPICK, BPICK,60-BPICK-DPICK, col, 0.0, 0.0, -1, 0, "Active color");

	h= (DPICK+BPICK+FPICK-64)/(UI_PALETTE_TOT/2.0);
	uiBlockBeginAlign(block);
	for(a= -1+UI_PALETTE_TOT/2; a>=0; a--) {
		bt= uiDefButF(block, COL, retval, "",	FPICK+DPICK, 65.0+(float)a*h, BPICK/2, h, palette[a+UI_PALETTE_TOT/2], 0.0, 0.0, -1, 0, "Click to choose, hold CTRL to store in palette");
		uiButSetFunc(bt, do_palette_cb, bt, col);
		bt= uiDefButF(block, COL, retval, "",	FPICK+DPICK+BPICK/2, 65.0+(float)a*h, BPICK/2, h, palette[a], 0.0, 0.0, -1, 0, "Click to choose, hold CTRL to store in palette");		
		uiButSetFunc(bt, do_palette_cb, bt, col);
	}
	uiBlockEndAlign(block);
	
	uiBlockSetEmboss(block, UI_EMBOSS);

	// buttons
	rgb_to_hsv(col[0], col[1], col[2], hsv, hsv+1, hsv+2);
	sprintf(hexcol, "%02X%02X%02X", (unsigned int)(col[0]*255.0), (unsigned int)(col[1]*255.0), (unsigned int)(col[2]*255.0));	

	offs= FPICK+2*DPICK+BPICK;

	/* note; made this a TOG now, with NULL pointer. Is because BUT now gets handled with a afterfunc */
	bt= uiDefIconTextBut(block, TOG, UI_RETURN_OK, ICON_EYEDROPPER, "Sample", offs+55, 170, 85, 20, NULL, 0, 0, 0, 0, "Sample the color underneath the following mouse click (ESC or RMB to cancel)");
	uiButSetFunc(bt, do_palette_sample_cb, bt, col);
	uiButSetFlag(bt, UI_TEXT_LEFT);
	
	bt= uiDefBut(block, TEX, retval, "Hex: ", offs, 140, 140, 20, hexcol, 0, 8, 0, 0, "Hex triplet for color (#RRGGBB)");
	uiButSetFunc(bt, do_palette_hex_cb, bt, hexcol);

	uiBlockBeginAlign(block);
	bt= uiDefButF(block, NUMSLI, retval, "R ",	offs, 110, 140,20, col, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	bt= uiDefButF(block, NUMSLI, retval, "G ",	offs, 90, 140,20, col+1, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	bt= uiDefButF(block, NUMSLI, retval, "B ",	offs, 70, 140,20, col+2, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette1_cb, bt, hsv);
	
	uiBlockBeginAlign(block);
	bt= uiDefButF(block, NUMSLI, retval, "H ",	offs, 40, 140,20, hsv, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	bt= uiDefButF(block, NUMSLI, retval, "S ",	offs, 20, 140,20, hsv+1, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	bt= uiDefButF(block, NUMSLI, retval, "V ",	offs, 0, 140,20, hsv+2, 0.0, 1.0, 10, 3, "");
	uiButSetFunc(bt, do_palette2_cb, bt, col);
	uiBlockEndAlign(block);
}

uiBlock *ui_block_func_COL(bContext *C, uiPopupBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	static float hsvcol[3], oldcol[3];
	static char hexcol[128];
	
	block= uiBeginBlock(C, handle->region, "colorpicker", UI_EMBOSS);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_KEEP_OPEN;
	
	VECCOPY(handle->retvec, but->editvec);
	uiBlockPickerButtons(block, handle->retvec, hsvcol, oldcol, hexcol, 'p', 0);

	/* and lets go */
	block->direction= UI_TOP;
	uiBoundsBlock(block, 3);
	
	return block;
}

/* ******************** PUPmenu ****************** */

static int pupmenu_set= 0;

void uiPupMenuSetActive(int val)
{
	pupmenu_set= val;
}

/* value== -1 read, otherwise set */
static int pupmenu_memory(char *str, int value)
{
	static char mem[256], first=1;
	int val=0, nr=0;
	
	if(first) {
		memset(mem, 0, 256);
		first= 0;
	}
	while(str[nr]) {
		val+= str[nr];
		nr++;
	}

	if(value >= 0) mem[ val & 255 ]= value;
	else return mem[ val & 255 ];
	
	return 0;
}

#define PUP_LABELH	6

typedef struct uiPupMenuInfo {
	char *instr;
	int mx, my;
	int startx, starty;
	int maxrow;
} uiPupMenuInfo;

uiBlock *ui_block_func_PUPMENU(bContext *C, uiPopupBlockHandle *handle, void *arg_info)
{
	uiBlock *block;
	uiPupMenuInfo *info;
	int columns, rows, mousemove[2]= {0, 0}, mousewarp= 0;
	int width, height, xmax, ymax, maxrow;
	int a, startx, starty, endx, endy, x1, y1;
	int lastselected;
	MenuData *md;

	info= arg_info;
	maxrow= info->maxrow;
	height= 0;

	/* block stuff first, need to know the font */
	block= uiBeginBlock(C, handle->region, "menu", UI_EMBOSSP);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->direction= UI_DOWN;
	
	md= decompose_menu_string(info->instr);

	rows= md->nitems;
	columns= 1;

	/* size and location, title slightly bigger for bold */
	if(md->title) {
		width= 2*strlen(md->title)+UI_GetStringWidth(md->title);
		width /= columns;
	}
	else width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(md->items[a].str);
		if(xmax>width) width= xmax;

		if(strcmp(md->items[a].str, "%l")==0) height+= PUP_LABELH;
		else height+= MENU_BUTTON_HEIGHT;
	}

	width+= 10;
	if (width<50) width=50;
	
	wm_window_get_size(CTX_wm_window(C), &xmax, &ymax);

	/* set first item */
	lastselected= 0;
	if(pupmenu_set) {
		lastselected= pupmenu_set-1;
		pupmenu_set= 0;
	}
	else if(md->nitems>1) {
		lastselected= pupmenu_memory(info->instr, -1);
	}

	startx= info->mx-(0.8*(width));
	starty= info->my-height+MENU_BUTTON_HEIGHT/2;
	if(lastselected>=0 && lastselected<md->nitems) {
		for(a=0; a<md->nitems; a++) {
			if(a==lastselected) break;
			if( strcmp(md->items[a].str, "%l")==0) starty+= PUP_LABELH;
			else starty+=MENU_BUTTON_HEIGHT;
		}
		
		//starty= info->my-height+MENU_BUTTON_HEIGHT/2+lastselected*MENU_BUTTON_HEIGHT;
	}
	
	if(startx<10) {
		startx= 10;
	}
	if(starty<10) {
		mousemove[1]= 10-starty;
		starty= 10;
	}
	
	endx= startx+width*columns;
	endy= starty+height;

	if(endx>xmax) {
		endx= xmax-10;
		startx= endx-width*columns;
	}
	if(endy>ymax-20) {
		mousemove[1]= ymax-endy-20;
		endy= ymax-20;
		starty= endy-height;
	}

	if(mousemove[0] || mousemove[1]) {
		ui_warp_pointer(info->mx+mousemove[0], info->my+mousemove[1]);
		mousemove[0]= info->mx;
		mousemove[1]= info->my;
		mousewarp= 1;
	}

	/* here we go! */
	if(md->title) {
		uiBut *bt;
		char titlestr[256];

		if(md->titleicon) {
			width+= 20;
			sprintf(titlestr, " %s", md->title);
			uiDefIconTextBut(block, LABEL, 0, md->titleicon, titlestr, startx, (short)(starty+height), width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+height), columns*width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		
		//uiDefBut(block, SEPR, 0, "", startx, (short)(starty+height)-MENU_SEPR_HEIGHT, width, MENU_SEPR_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
	}

	x1= startx + width*((int)a/rows);
	y1= starty + height - MENU_BUTTON_HEIGHT; // - MENU_SEPR_HEIGHT;
		
	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		int icon = md->items[a].icon;

		if(strcmp(name, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, PUP_LABELH, NULL, 0, 0.0, 0, 0, "");
			y1 -= PUP_LABELH;
		}
		else if (icon) {
			uiDefIconButF(block, BUTM, B_NOP, icon, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else {
			uiDefButF(block, BUTM, B_NOP, name, x1, y1, width, MENU_BUTTON_HEIGHT-1, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= MENU_BUTTON_HEIGHT;
		}
	}
	
	uiBoundsBlock(block, 1);
	uiEndBlock(C, block);

	menudata_free(md);

	/* XXX 2.5 need to store last selected */
#if 0
	/* calculate last selected */
	if(event & ui_return_ok) {
		lastselected= 0;
		for(a=0; a<md->nitems; a++) {
			if(val==md->items[a].retval) lastselected= a;
		}
		
		pupmenu_memory(info->instr, lastselected);
	}
#endif
	
	/* XXX 2.5 need to warp back */
#if 0
	if(mousemove[1] && (event & ui_return_out)==0)
		ui_warp_pointer(mousemove[0], mousemove[1]);
	return val;
#endif

	return block;
}

uiBlock *ui_block_func_PUPMENUCOL(bContext *C, uiPopupBlockHandle *handle, void *arg_info)
{
	uiBlock *block;
	uiPupMenuInfo *info;
	int columns, rows, mousemove[2]= {0, 0}, mousewarp;
	int width, height, xmax, ymax, maxrow;
	int a, startx, starty, endx, endy, x1, y1;
	float fvalue;
	MenuData *md;

	info= arg_info;
	maxrow= info->maxrow;
	height= 0;

	/* block stuff first, need to know the font */
	block= uiBeginBlock(C, handle->region, "menu", UI_EMBOSSP);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->direction= UI_DOWN;
	
	md= decompose_menu_string(info->instr);

	/* columns and row calculation */
	columns= (md->nitems+maxrow)/maxrow;
	if (columns<1) columns= 1;
	
	if(columns > 8) {
		maxrow += 5;
		columns= (md->nitems+maxrow)/maxrow;
	}
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<(md->nitems+columns) ) rows++;

	/* size and location, title slightly bigger for bold */
	if(md->title) {
		width= 2*strlen(md->title)+UI_GetStringWidth(md->title);
		width /= columns;
	}
	else width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(md->items[a].str);
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width<50) width=50;
	
	height= rows*MENU_BUTTON_HEIGHT;
	if (md->title) height+= MENU_BUTTON_HEIGHT;
	
	wm_window_get_size(CTX_wm_window(C), &xmax, &ymax);

	/* find active item */
	fvalue= handle->retvalue;
	for(a=0; a<md->nitems; a++) {
		if( md->items[a].retval== (int)fvalue ) break;
	}

	/* no active item? */
	if(a==md->nitems) {
		if(md->title) a= -1;
		else a= 0;
	}

	if(a>0)
		startx = info->mx-width/2 - ((int)(a)/rows)*width;
	else
		startx= info->mx-width/2;
	starty = info->my-height + MENU_BUTTON_HEIGHT/2 + ((a)%rows)*MENU_BUTTON_HEIGHT;

	if (md->title) starty+= MENU_BUTTON_HEIGHT;
	
	if(startx<10) {
		mousemove[0]= 10-startx;
		startx= 10;
	}
	if(starty<10) {
		mousemove[1]= 10-starty;
		starty= 10;
	}
	
	endx= startx+width*columns;
	endy= starty+height;

	if(endx>xmax) {
		mousemove[0]= xmax-endx-10;
		endx= xmax-10;
		startx= endx-width*columns;
	}
	if(endy>ymax) {
		mousemove[1]= ymax-endy-10;
		endy= ymax-10;
		starty= endy-height;
	}

	if(mousemove[0] || mousemove[1]) {
		ui_warp_pointer(info->mx+mousemove[0], info->my+mousemove[1]);
		mousemove[0]= info->mx;
		mousemove[1]= info->my;
		mousewarp= 1;
	}

	/* here we go! */
	if(md->title) {
		uiBut *bt;

		if(md->titleicon) {
		}
		else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*MENU_BUTTON_HEIGHT), columns*width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
	}

	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		int icon = md->items[a].icon;

		x1= startx + width*((int)a/rows);
		y1= starty - MENU_BUTTON_HEIGHT*(a%rows) + (rows-1)*MENU_BUTTON_HEIGHT; 
		
		if(strcmp(name, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, PUP_LABELH, NULL, 0, 0.0, 0, 0, "");
			y1 -= PUP_LABELH;
		}
		else if (icon) {
			uiDefIconButF(block, BUTM, B_NOP, icon, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else {
			uiDefButF(block, BUTM, B_NOP, name, x1, y1, width, MENU_BUTTON_HEIGHT-1, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= MENU_BUTTON_HEIGHT;
		}
	}
	
	uiBoundsBlock(block, 1);
	uiEndBlock(C, block);
	
	menudata_free(md);
	
	/* XXX 2.5 need to warp back */
#if 0
	if((event & UI_RETURN_OUT)==0)
		ui_warp_pointer(mousemove[0], mousemove[1]);
#endif

	return block;
}

/************************** Menu Definitions ***************************/

/* prototype */
static uiBlock *ui_block_func_MENU_ITEM(bContext *C, uiPopupBlockHandle *handle, void *arg_info);

#define MAX_MENU_STR	64

/* type, internal */
#define MENU_ITEM_TITLE				0
#define MENU_ITEM_ITEM				1
#define MENU_ITEM_SEPARATOR			2
#define MENU_ITEM_OPNAME			10
#define MENU_ITEM_OPNAME_BOOL		11
#define MENU_ITEM_OPNAME_ENUM		12
#define MENU_ITEM_OPNAME_INT		13
#define MENU_ITEM_OPNAME_FLOAT		14
#define MENU_ITEM_OPNAME_STRING		15
#define MENU_ITEM_RNA_BOOL			20
#define MENU_ITEM_RNA_ENUM			21
#define MENU_ITEM_LEVEL				30
#define MENU_ITEM_LEVEL_OPNAME_ENUM	31
#define MENU_ITEM_LEVEL_RNA_ENUM	32

struct uiMenuItem {
	struct uiMenuItem *next, *prev;
	
	int type;
	int icon;
	char name[MAX_MENU_STR];
	
	char *opname;	/* static string */
	char *propname;	/* static string */
	
	int retval, enumval, boolval, intval;
	float fltval;
	char *strval;
	int opcontext;
	uiMenuHandleFunc eventfunc;
	void *argv;
	uiMenuCreateFunc newlevel;
	PointerRNA rnapoin;
	
	ListBase items;
};

typedef struct uiMenuInfo {
	uiMenuItem *head;
	int mx, my, popup, slideout;
	int startx, starty;
} uiMenuInfo;

/************************ Menu Definitions to uiBlocks ***********************/

const char *ui_menu_enumpropname(char *opname, const char *propname, int retval)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna)
		return "";
	
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);
	
	if(prop) {
		const EnumPropertyItem *item;
		int totitem, i;
		
		RNA_property_enum_items(&ptr, prop, &item, &totitem);
		
		for (i=0; i<totitem; i++) {
			if(item[i].value==retval)
				return item[i].name;
		}
	}

	return "";
}

typedef struct MenuItemLevel {
	int opcontext;
	char *opname;
	char *propname;
	PointerRNA rnapoin;
} MenuItemLevel;

/* make a menu level from enum properties */
static void menu_item_enum_opname_menu(bContext *C, uiMenuItem *head, void *arg)
{
	MenuItemLevel *lvl= (MenuItemLevel*)(((uiBut*)arg)->func_argN);

	head->opcontext= lvl->opcontext;
	uiMenuItemsEnumO(head, lvl->opname, lvl->propname);
}

static void menu_item_enum_rna_menu(bContext *C, uiMenuItem *head, void *arg)
{
	MenuItemLevel *lvl= (MenuItemLevel*)(((uiBut*)arg)->func_argN);

	head->opcontext= lvl->opcontext;
	uiMenuItemsEnumR(head, &lvl->rnapoin, lvl->propname);
}

static uiBlock *ui_block_func_MENU_ITEM(bContext *C, uiPopupBlockHandle *handle, void *arg_info)
{
	uiBlock *block;
	uiBut *but;
	uiMenuInfo *info= arg_info;
	uiMenuItem *head, *item;
	MenuItemLevel *lvl;
	ScrArea *sa;
	ARegion *ar;
	static int counter= 0;
	int width, height, icon;
	int startx, starty, x1, y1;
	char str[16];
	
	head= info->head;
	height= 0;
	
	/* block stuff first, need to know the font */
	sprintf(str, "tb %d", counter++);
	block= uiBeginBlock(C, handle->region, str, UI_EMBOSSP);
	uiBlockSetButmFunc(block, head->eventfunc, head->argv);
	block->direction= UI_DOWN;

	width= 50; // fixed with, uiMenuPopupBoundsBlock will compute actual width

	for(item= head->items.first; item; item= item->next) {
		if(0) height+= PUP_LABELH; // XXX sepr line
		else height+= MENU_BUTTON_HEIGHT;
	}

	startx= 0;
	starty= 0;
	
	/* here we go! */
	if(head->name[0]) {
		char titlestr[256];
		
		if(head->icon) {
			width+= 20;
			sprintf(titlestr, " %s", head->name);
			uiDefIconTextBut(block, LABEL, 0, head->icon, titlestr, startx, (short)(starty+height), width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			but= uiDefBut(block, LABEL, 0, head->name, startx, (short)(starty+height), width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
			but->flag= UI_TEXT_LEFT;
		}
		
		//uiDefBut(block, SEPR, 0, "", startx, (short)(starty+height)-MENU_SEPR_HEIGHT, width, MENU_SEPR_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
	}
	
	x1= startx;
	y1= starty + height - MENU_BUTTON_HEIGHT; // - MENU_SEPR_HEIGHT;
	
	for(item= head->items.first; item; item= item->next) {
		
		if(item->type==MENU_ITEM_LEVEL) {
			uiDefIconTextMenuBut(block, item->newlevel, NULL, ICON_RIGHTARROW_THIN, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL);
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_LEVEL_OPNAME_ENUM) {
			but= uiDefIconTextMenuBut(block, menu_item_enum_opname_menu, NULL, ICON_RIGHTARROW_THIN, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL);

			/* XXX warning, abuse of func_arg! */
			lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
			lvl->opname= item->opname;
			lvl->propname= item->propname;
			lvl->opcontext= item->opcontext;

			but->poin= (char*)but;
			but->func_argN= lvl;
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_LEVEL_RNA_ENUM) {
			but= uiDefIconTextMenuBut(block, menu_item_enum_rna_menu, NULL, ICON_RIGHTARROW_THIN, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL);

			/* XXX warning, abuse of func_arg! */
			lvl= MEM_callocN(sizeof(MenuItemLevel), "MenuItemLevel");
			lvl->rnapoin= item->rnapoin;
			lvl->propname= item->propname;
			lvl->opcontext= item->opcontext;

			but->poin= (char*)but;
			but->func_argN= lvl;
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME_BOOL) {
			but= uiDefIconTextButO(block, BUTM, item->opname, item->opcontext, item->icon, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, "");
			RNA_boolean_set(uiButGetOperatorPtrRNA(but), item->propname, item->boolval);
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME_ENUM) {
			const char *name;
			char bname[64];
			
			/* If no name is given, use the enum name */
			if (item->name[0] == '\0')
				name= ui_menu_enumpropname(item->opname, item->propname, item->enumval);
			else
				name= item->name;
			
			BLI_strncpy(bname, name, sizeof(bname));
			
			but= uiDefIconTextButO(block, BUTM, item->opname, item->opcontext, item->icon, bname, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, "");
			RNA_enum_set(uiButGetOperatorPtrRNA(but), item->propname, item->enumval);
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME_INT) {
			but= uiDefIconTextButO(block, BUTM, item->opname, head->opcontext, item->icon, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, "");
			RNA_int_set(uiButGetOperatorPtrRNA(but), item->propname, item->intval);
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME_FLOAT) {
			but= uiDefIconTextButO(block, BUTM, item->opname, item->opcontext, item->icon, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, "");
			RNA_float_set(uiButGetOperatorPtrRNA(but), item->propname, item->fltval);
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME_STRING) {
			but= uiDefIconTextButO(block, BUTM, item->opname, item->opcontext, item->icon, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, "");
			RNA_string_set(uiButGetOperatorPtrRNA(but), item->propname, item->strval);
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_OPNAME) {
			uiDefIconTextButO(block, BUTM, item->opname, item->opcontext, item->icon, NULL, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL);
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_RNA_BOOL) {
			PropertyRNA *prop= RNA_struct_find_property(&item->rnapoin, item->propname);

			if(prop && RNA_property_type(prop) == PROP_BOOLEAN) {
				icon= (RNA_property_boolean_get(&item->rnapoin, prop))? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT;
				uiDefIconTextButR(block, TOG, 0, icon, NULL, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, &item->rnapoin, item->propname, 0, 0, 0, 0, 0, NULL);
			}
			else {
				uiBlockSetButLock(block, 1, "");
				uiDefIconTextBut(block, BUT, 0, ICON_BLANK1, item->propname, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL, 0.0, 0.0, 0, 0, "");
				uiBlockClearButLock(block);
			}

			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type==MENU_ITEM_RNA_ENUM) {
			PropertyRNA *prop= RNA_struct_find_property(&item->rnapoin, item->propname);

			if(prop && RNA_property_type(prop) == PROP_ENUM) {
				icon= (RNA_property_enum_get(&item->rnapoin, prop) == item->enumval)? ICON_CHECKBOX_HLT: ICON_CHECKBOX_DEHLT;
				uiDefIconTextButR(block, ROW, 0, icon, NULL, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, &item->rnapoin, item->propname, 0, 0, item->enumval, 0, 0, NULL);
			}
			else {
				uiBlockSetButLock(block, 1, "");
				uiDefIconTextBut(block, BUT, 0, ICON_BLANK1, item->propname, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, NULL, 0.0, 0.0, 0, 0, "");
				uiBlockClearButLock(block);
			}
			
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else if(item->type == MENU_ITEM_ITEM) {
			uiDefIconTextButF(block, BUTM, B_NOP, item->icon, item->name, x1, y1, width+16, MENU_BUTTON_HEIGHT-1, &handle->retvalue, 0.0, 0.0, 0, item->retval, "");
			y1 -= MENU_BUTTON_HEIGHT;
		}
		else {
			uiDefBut(block, SEPR, 0, "", x1, y1, width+16, MENU_SEPR_HEIGHT-1, NULL, 0.0, 0.0, 0, 0, "");
			y1 -= MENU_SEPR_HEIGHT;
		}
	}

	if(info->popup) {
		uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT|UI_BLOCK_RET_1);
		uiBlockSetDirection(block, UI_DOWN);

		/* here we set an offset for the mouse position */
		uiMenuPopupBoundsBlock(block, 1, 0, -height+MENU_BUTTON_HEIGHT/2);
	}
	else {
		/* for a header menu we set the direction automatic */
		if(!info->slideout) {
			sa= CTX_wm_area(C);
			ar= CTX_wm_region(C);

			if(sa && sa->headertype==HEADERDOWN) {
				if(ar && ar->regiontype == RGN_TYPE_HEADER) {
					uiBlockSetDirection(block, UI_TOP);
					uiBlockFlipOrder(block);
				}
			}
		}

		uiTextBoundsBlock(block, 50);
	}

	/* if menu slides out of other menu, override direction */
	if(info->slideout)
		uiBlockSetDirection(block, UI_RIGHT);

	uiEndBlock(C, block);
	
	return block;
}

uiPopupBlockHandle *ui_popup_menu_create(bContext *C, ARegion *butregion, uiBut *but, uiMenuCreateFunc menu_func, void *arg)
{
	uiPopupBlockHandle *handle;
	uiMenuItem *head;
	uiMenuInfo info;
	
	head= MEM_callocN(sizeof(uiMenuItem), "menu dummy");
	head->opcontext= WM_OP_INVOKE_REGION_WIN; 

	menu_func(C, head, arg);
	
	memset(&info, 0, sizeof(info));
	info.head= head;
	info.slideout= (but && (but->block->flag & UI_BLOCK_LOOP));
	
	handle= ui_popup_block_create(C, butregion, but, NULL, ui_block_func_MENU_ITEM, &info);
	
	BLI_freelistN(&head->items);
	MEM_freeN(head);

	return handle;
}

/*************************** Menu Creating API **************************/

/* internal add func */
static uiMenuItem *ui_menu_add_item(uiMenuItem *head, const char *name, int icon, int argval)
{
	uiMenuItem *item= MEM_callocN(sizeof(uiMenuItem), "menu item");
	
	BLI_strncpy(item->name, name, MAX_MENU_STR);
	if(icon)
		item->icon= icon;
	else
		item->icon= ICON_BLANK1;
	item->retval= argval;
	
	item->opcontext= head->opcontext; 
	
	BLI_addtail(&head->items, item);
	
	return item;
}

/* set callback for regular items */
void uiMenuFunc(uiMenuItem *head, void (*eventfunc)(bContext *, void *, int), void *argv)
{
	head->eventfunc= eventfunc;
	head->argv= argv;
}

/* optionally set different context for all items in one level */
void uiMenuContext(uiMenuItem *head, int opcontext)
{
	head->opcontext= opcontext;
}


/* regular item, with retval */
void uiMenuItemVal(uiMenuItem *head, const char *name, int icon, int argval)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, argval);
	
	item->type = MENU_ITEM_ITEM;
}

/* regular operator item */
void uiMenuItemO(uiMenuItem *head, int icon, char *opname)
{
	uiMenuItem *item= ui_menu_add_item(head, "", icon, 0);
	
	item->opname= opname; // static!
	item->type = MENU_ITEM_OPNAME;
}

/* single operator item with property */
void uiMenuItemEnumO(uiMenuItem *head, const char *name, int icon, char *opname, char *propname, int value)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, 0);
	
	item->opname= opname; // static!
	item->propname= propname; // static!
	item->enumval= value;
	item->type = MENU_ITEM_OPNAME_ENUM;
}

/* single operator item with property */
void uiMenuItemIntO(uiMenuItem *head, const char *name, int icon, char *opname, char *propname, int value)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, 0);
	
	item->opname= opname; // static!
	item->propname= propname; // static!
	item->intval= value;
	item->type = MENU_ITEM_OPNAME_INT;
}

/* single operator item with property */
void uiMenuItemFloatO(uiMenuItem *head, const char *name, int icon, char *opname, char *propname, float value)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, 0);
	
	item->opname= opname; // static!
	item->propname= propname; // static!
	item->fltval= value;
	item->type = MENU_ITEM_OPNAME_FLOAT;
}

/* single operator item with property */
void uiMenuItemBooleanO(uiMenuItem *head, const char *name, int icon, char *opname, char *propname, int value)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, 0);
	
	item->opname= opname; // static!
	item->propname= propname; // static!
	item->boolval= value;
	item->type = MENU_ITEM_OPNAME_BOOL;
}

/* single operator item with property */
void uiMenuItemStringO(uiMenuItem *head, const char *name, int icon, char *opname, char *propname, char *value)
{
	uiMenuItem *item= ui_menu_add_item(head, name, icon, 0);
	
	item->opname= opname; // static!
	item->propname= propname; // static!
	item->strval= value;
	item->type = MENU_ITEM_OPNAME_STRING;
}

/* add all operator items with property */
void uiMenuItemsEnumO(uiMenuItem *head, char *opname, char *propname)
{
	wmOperatorType *ot= WM_operatortype_find(opname);
	PointerRNA ptr;
	PropertyRNA *prop;

	if(!ot || !ot->srna)
		return;
	
	RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
	prop= RNA_struct_find_property(&ptr, propname);
	
	if(prop && RNA_property_type(prop) == PROP_ENUM) {
		const EnumPropertyItem *item;
		int totitem, i;
		
		RNA_property_enum_items(&ptr, prop, &item, &totitem);
		
		for (i=0; i<totitem; i++)
			uiMenuItemEnumO(head, "", 0, opname, propname, item[i].value);
	}
}

/* rna property toggle */
void uiMenuItemBooleanR(uiMenuItem *head, PointerRNA *ptr, char *propname)
{
	uiMenuItem *item= ui_menu_add_item(head, "", 0, 0);
	
	item->propname= propname; // static!
	item->rnapoin= *ptr;
	item->type = MENU_ITEM_RNA_BOOL;
}

void uiMenuItemEnumR(uiMenuItem *head, PointerRNA *ptr, char *propname, int value)
{
	uiMenuItem *item= ui_menu_add_item(head, "", 0, 0);
	
	item->propname= propname; // static!
	item->rnapoin= *ptr;
	item->enumval= value;
	item->type = MENU_ITEM_RNA_ENUM;
}

/* add all rna items with property */
void uiMenuItemsEnumR(uiMenuItem *head, PointerRNA *ptr, char *propname)
{
	PropertyRNA *prop;

	prop= RNA_struct_find_property(ptr, propname);
	
	if(prop && RNA_property_type(prop) == PROP_ENUM) {
		const EnumPropertyItem *item;
		int totitem, i;
		
		RNA_property_enum_items(ptr, prop, &item, &totitem);
		
		for (i=0; i<totitem; i++)
			uiMenuItemEnumR(head, ptr, propname, item[i].value);
	}
}

/* generic new menu level */
void uiMenuLevel(uiMenuItem *head, const char *name, uiMenuCreateFunc newlevel)
{
	uiMenuItem *item= ui_menu_add_item(head, name, 0, 0);
	
	item->type = MENU_ITEM_LEVEL;
	item->newlevel= newlevel;
}

/* make a new level from enum properties */
void uiMenuLevelEnumO(uiMenuItem *head, char *opname, char *propname)
{
	uiMenuItem *item= ui_menu_add_item(head, "", 0, 0);
	wmOperatorType *ot;
	
	item->type = MENU_ITEM_LEVEL_OPNAME_ENUM;
	ot= WM_operatortype_find(opname);
	if(ot)
		BLI_strncpy(item->name, ot->name, MAX_MENU_STR);

	item->opname= opname; // static!
	item->propname= propname; // static!
}

/* make a new level from enum properties */
void uiMenuLevelEnumR(uiMenuItem *head, PointerRNA *ptr, char *propname)
{
	uiMenuItem *item= ui_menu_add_item(head, "", 0, 0);
	PropertyRNA *prop;
	
	item->type = MENU_ITEM_LEVEL_RNA_ENUM;
	prop= RNA_struct_find_property(ptr, propname);
	if(prop)
		BLI_strncpy(item->name, RNA_property_ui_name(prop), MAX_MENU_STR);

	item->rnapoin= *ptr;
	item->propname= propname; // static!
}

/* separator */
void uiMenuSeparator(uiMenuItem *head)
{
	uiMenuItem *item= ui_menu_add_item(head, "", 0, 0);
	
	item->type = MENU_ITEM_SEPARATOR;
}

/*************************** Popup Menu API **************************/

/* only return handler, and set optional title */
uiMenuItem *uiPupMenuBegin(const char *title, int icon)
{
	uiMenuItem *item= MEM_callocN(sizeof(uiMenuItem), "menu start");
	
	item->type = MENU_ITEM_TITLE;
	item->opcontext= WM_OP_EXEC_REGION_WIN; 
	item->icon= icon;
	
	/* NULL is no title */
	if(title)
		BLI_strncpy(item->name, title, MAX_MENU_STR);
	
	return item;
}

/* set the whole structure to work */
void uiPupMenuEnd(bContext *C, uiMenuItem *head)
{
	wmWindow *window= CTX_wm_window(C);
	uiMenuInfo info;
	uiPopupBlockHandle *menu;
	
	memset(&info, 0, sizeof(info));
	info.popup= 1;
	info.mx= window->eventstate->x;
	info.my= window->eventstate->y;
	info.head= head;
	
	menu= ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_MENU_ITEM, &info);
	menu->popup= 1;
	
	UI_add_popup_handlers(C, &window->handlers, menu);
	WM_event_add_mousemove(C);
	
	BLI_freelistN(&head->items);
	MEM_freeN(head);
}

/* ************** standard pupmenus *************** */

/* this one can called with operatortype name and operators */
static uiPopupBlockHandle *ui_pup_menu(bContext *C, int maxrow, uiMenuHandleFunc func, void *arg, char *str, ...)
{
	wmWindow *window= CTX_wm_window(C);
	uiPupMenuInfo info;
	uiPopupBlockHandle *menu;

	memset(&info, 0, sizeof(info));
	info.mx= window->eventstate->x;
	info.my= window->eventstate->y;
	info.maxrow= maxrow;
	info.instr= str;

	menu= ui_popup_block_create(C, NULL, NULL, NULL, ui_block_func_PUPMENU, &info);
	menu->popup= 1;

	UI_add_popup_handlers(C, &window->handlers, menu);
	WM_event_add_mousemove(C);

	menu->popup_func= func;
	menu->popup_arg= arg;
	
	return menu;
}


static void operator_name_cb(bContext *C, void *arg, int retval)
{
	const char *opname= arg;

	if(opname && retval > 0)
		WM_operator_name_call(C, opname, WM_OP_EXEC_DEFAULT, NULL);
}

static void vconfirm_opname(bContext *C, char *opname, char *title, char *itemfmt, va_list ap)
{
	char *s, buf[512];

	s= buf;
	if (title) s+= sprintf(s, "%s%%t|", title);
	vsprintf(s, itemfmt, ap);

	ui_pup_menu(C, 0, operator_name_cb, opname, buf);
}

static void operator_cb(bContext *C, void *arg, int retval)
{
	wmOperator *op= arg;
	
	if(op && retval > 0)
		WM_operator_call(C, op);
	else
		WM_operator_free(op);
}

static void confirm_cancel_operator(void *opv)
{
	WM_operator_free(opv);
}

static void confirm_operator(bContext *C, wmOperator *op, char *title, char *item)
{
	uiPopupBlockHandle *handle;
	char *s, buf[512];
	
	s= buf;
	if (title) s+= sprintf(s, "%s%%t|%s", title, item);
	
	handle= ui_pup_menu(C, 0, operator_cb, op, buf);
	handle->cancel_func= confirm_cancel_operator;
}


void uiPupMenuOkee(bContext *C, char *opname, char *str, ...)
{
	va_list ap;
	char titlestr[256];

	sprintf(titlestr, "OK? %%i%d", ICON_HELP);

	va_start(ap, str);
	vconfirm_opname(C, opname, titlestr, str, ap);
	va_end(ap);
}


void uiPupMenuSaveOver(bContext *C, wmOperator *op, char *filename)
{
	size_t len= strlen(filename);

	if(len==0)
		return;

	if(filename[len-1]=='/' || filename[len-1]=='\\') {
		uiPupMenuError(C, "Cannot overwrite a directory");
		WM_operator_free(op);
		return;
	}
	if(BLI_exists(filename)==0)
		operator_cb(C, op, 1);
	else
		confirm_operator(C, op, "Save over", filename);
}

void uiPupMenuNotice(bContext *C, char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	vconfirm_opname(C, NULL, NULL, str, ap);
	va_end(ap);
}

void uiPupMenuError(bContext *C, char *str, ...)
{
	va_list ap;
	char nfmt[256];
	char titlestr[256];

	sprintf(titlestr, "Error %%i%d", ICON_ERROR);

	sprintf(nfmt, "%s", str);

	va_start(ap, str);
	vconfirm_opname(C, NULL, titlestr, nfmt, ap);
	va_end(ap);
}

void uiPupMenuReports(bContext *C, ReportList *reports)
{
	Report *report;
	DynStr *ds;
	char *str;

	if(!reports || !reports->list.first)
		return;
	if(!CTX_wm_window(C))
		return;

	ds= BLI_dynstr_new();

	for(report=reports->list.first; report; report=report->next) {
		if(report->type >= RPT_ERROR)
			BLI_dynstr_appendf(ds, "Error %%i%d%%t|%s", ICON_ERROR, report->message);
		else if(report->type >= RPT_WARNING)
			BLI_dynstr_appendf(ds, "Warning %%i%d%%t|%s", ICON_ERROR, report->message);
	}

	str= BLI_dynstr_get_cstring(ds);
	ui_pup_menu(C, 0, NULL, NULL, str);
	MEM_freeN(str);

	BLI_dynstr_free(ds);
}

/*************************** Popup Block API **************************/

void uiPupBlockO(bContext *C, uiBlockCreateFunc func, void *arg, char *opname, int opcontext)
{
	wmWindow *window= CTX_wm_window(C);
	uiPopupBlockHandle *handle;
	
	handle= ui_popup_block_create(C, NULL, NULL, func, NULL, arg);
	handle->popup= 1;
	handle->opname= opname;
	handle->opcontext= opcontext;
	
	UI_add_popup_handlers(C, &window->handlers, handle);
	WM_event_add_mousemove(C);
}

void uiPupBlock(bContext *C, uiBlockCreateFunc func, void *arg)
{
	uiPupBlockO(C, func, arg, NULL, 0);
}

