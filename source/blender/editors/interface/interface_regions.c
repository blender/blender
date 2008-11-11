
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"
#include "wm_window.h"

#include "BIF_gl.h"

#include "UI_text.h"
#include "UI_interface.h"

#include "ED_screen.h"

#include "interface.h"

#define MENU_BUTTON_HEIGHT	20
#define B_NOP              	-1
#define MENU_SHADOW_LEFT	-1
#define MENU_SHADOW_BOTTOM	-10
#define MENU_SHADOW_RIGHT	10
#define MENU_SHADOW_TOP		1

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
	ED_region_exit(C, ar);
	BKE_area_region_free(ar);
	BLI_freelinkN(&sc->regionbase, ar);
}

/************************* Creating Tooltips **********************/

typedef struct uiTooltipData {
	rctf bbox;
	struct BMF_Font *font;
	char *tip;
	float aspect;
} uiTooltipData;

static void ui_tooltip_region_draw(const bContext *C, ARegion *ar)
{
	uiTooltipData *data;
	int x1, y1, x2, y2;

	data= ar->regiondata;

	x1= ar->winrct.xmin;
	y1= ar->winrct.ymin;
	x2= ar->winrct.xmax;
	y2= ar->winrct.ymax;

	/* draw drop shadow */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4ub(0, 0, 0, 20);
	
	gl_round_box(GL_POLYGON, 3, 3, x2-x1-3, y2-y1-2, 2.0);
	gl_round_box(GL_POLYGON, 3, 2, x2-x1-2, y2-y1-2, 3.0);
	
	glColor4ub(0, 0, 0, 8);
	
	gl_round_box(GL_POLYGON, 3, 1, x2-x1-1, y2-y1-3, 4.0);
	gl_round_box(GL_POLYGON, 3, 0, x2-x1-0, y2-y1-3, 5.0);

	glDisable(GL_BLEND);
	
	/* draw background */
	glColor3f(1.0f, 1.0f, 0.8666f);
	glRectf(0, 4, x2-x1-4, y2-y1);
	
	/* draw text */
	glColor3ub(0,0,0);

	/* set the position for drawing text +4 in from the left edge, and leaving
	 * an equal gap between the top of the background box and the top of the
	 * string's bbox, and the bottom of the background box, and the bottom of
	 * the string's bbox */
	ui_rasterpos_safe(4, ((y2-data->bbox.ymax)+(y1+data->bbox.ymin))/2 - data->bbox.ymin - y1, data->aspect);
	UI_SetScale(1.0);

	UI_DrawString(data->font, data->tip, ui_translate_tooltips());
}

static void ui_tooltip_region_free(ARegion *ar)
{
	uiTooltipData *data;

	data= ar->regiondata;
	MEM_freeN(data->tip);
	MEM_freeN(data);
}

ARegion *ui_tooltip_create(bContext *C, ARegion *butregion, uiBut *but)
{
	static ARegionType type={NULL, NULL, NULL, NULL, NULL};
	ARegion *ar;
	uiTooltipData *data;
	int x1, x2, y1, y2, winx, winy;

	if(!but->tip || strlen(but->tip)==0)
		return NULL;

	/* create area region */
	ar= ui_add_temporary_region(C->window->screen);

	type.draw= ui_tooltip_region_draw;
	type.free= ui_tooltip_region_free;
	ar->type= &type;

	/* create tooltip data */
	data= MEM_callocN(sizeof(uiTooltipData), "uiTooltipData");
	data->tip= BLI_strdup(but->tip);
	data->font= but->font;
	data->aspect= but->aspect;
	UI_GetBoundingBox(data->font, data->tip, ui_translate_tooltips(), &data->bbox);

	ar->regiondata= data;

	/* compute position */
	x1= (but->x1+but->x2)/2;
	x2= x1+but->aspect*((data->bbox.xmax-data->bbox.xmin) + 8);
	y2= but->y1-10;
	y1= y2-but->aspect*((data->bbox.ymax+(data->bbox.ymax-data->bbox.ymin)));

	y2 += 4;
	x2 += 4;

	if(butregion) {
		x1 += butregion->winrct.xmin;
		x2 += butregion->winrct.xmin;
		y1 += butregion->winrct.ymin;
		y2 += butregion->winrct.ymin;
	}

	wm_window_get_size(C->window, &winx, &winy);

	if(x2 > winx) {
		x1 -= x2-winx;
		x2= winx;
	}
	if(y1 < 0) {
		y1 += 36;
		y2 += 36;
	}

	ar->winrct.xmin= x1;
	ar->winrct.ymin= y1;
	ar->winrct.xmax= x2;
	ar->winrct.ymax= y2;

	/* notify change and redraw */
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);

	return ar;
}

void ui_tooltip_free(bContext *C, ARegion *ar)
{
	ui_remove_temporary_region(C, C->window->screen, ar);

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
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

	for(block=ar->uiblocks.first; block; block=block->next) {
		wm_subwindow_getmatrix(C->window, ar->swinid, block->winmat);
		uiDrawBlock(block);
	}
}

static void ui_block_region_free(ARegion *ar)
{
	uiFreeBlocks(&ar->uiblocks);
}

uiMenuBlockHandle *ui_menu_block_create(bContext *C, ARegion *butregion, uiBut *but, uiBlockFuncFP block_func, void *arg)
{
	static ARegionType type={NULL, NULL, NULL, NULL, NULL};
	ARegion *ar;
	uiBlock *block;
	uiBut *bt;
	uiMenuBlockHandle *handle;
	uiSafetyRct *saferct;

	/* create handle */
	handle= MEM_callocN(sizeof(uiMenuBlockHandle), "uiMenuBlockHandle");

	/* create area region */
	ar= ui_add_temporary_region(C->window->screen);

	type.draw= ui_block_region_draw;
	type.free= ui_block_region_free;
	ar->type= &type;

	WM_event_add_keymap_handler(&ar->handlers, &C->wm->uikeymap);

	handle->region= ar;
	ar->regiondata= handle;

	/* create ui block */
	block= block_func(C->window, handle, arg);
	block->handle= handle;

	/* if this is being created from a button */
	if(but) {
		if(ELEM(but->type, BLOCK, PULLDOWN))
			block->xofs = -2;	/* for proper alignment */

		/* only used for automatic toolbox, so can set the shift flag */
		if(but->flag & UI_MAKE_TOP) {
			block->direction= UI_TOP|UI_SHIFT_FLIPPED;
			uiBlockFlipOrder(block);
		}
		if(but->flag & UI_MAKE_DOWN) block->direction= UI_DOWN|UI_SHIFT_FLIPPED;
		if(but->flag & UI_MAKE_LEFT) block->direction |= UI_LEFT;
		if(but->flag & UI_MAKE_RIGHT) block->direction |= UI_RIGHT;

		ui_block_position(C->window, butregion, but, block);
	}
	else {
		/* keep a list of these, needed for pulldown menus */
		saferct= MEM_callocN(sizeof(uiSafetyRct), "uiSafetyRct");
		saferct->safety= block->safety;
		BLI_addhead(&block->saferct, saferct);
	}

	/* the block and buttons were positioned in window space as in 2.4x, now
	 * these menu blocks are regions so we bring it back to region space.
	 * additionally we add some padding for the menu shadow */
	ar->winrct.xmin= block->minx + MENU_SHADOW_LEFT;
	ar->winrct.xmax= block->maxx + MENU_SHADOW_RIGHT;
	ar->winrct.ymin= block->miny + MENU_SHADOW_BOTTOM;
	ar->winrct.ymax= block->maxy + MENU_SHADOW_TOP;

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

	/* notify change and redraw */
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);

	SWAP(ARegion*, C->region, ar); /* XXX 2.50 bad context swapping */
	WM_operator_invoke(C, WM_operatortype_find("ED_UI_OT_menu_block_handle"), NULL);
	SWAP(ARegion*, C->region, ar);

	return handle;
}

void ui_menu_block_free(bContext *C, uiMenuBlockHandle *handle)
{
	ui_remove_temporary_region(C, C->window->screen, handle->region);
	MEM_freeN(handle);

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
}

/***************************** Menu Button ***************************/

uiBlock *ui_block_func_MENU(wmWindow *window, uiMenuBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	uiBut *bt;
	MenuData *md;
	ListBase lb;
	float aspect;
	int width, height, boxh, columns, rows, startx, starty, x1, y1, xmax, a;

	/* create the block */
	block= uiBeginBlock(window, handle->region, "menu", UI_EMBOSSP, UI_HELV);
	block->dt= UI_EMBOSSP;
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;

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
		width= 1.5*aspect*strlen(md->title)+UI_GetStringWidth(block->curfont, md->title, ui_translate_menus());
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= aspect*UI_GetStringWidth(block->curfont, md->items[a].str, ui_translate_menus());
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
		uiSetCurFont(block, block->font+1);
		if (md->titleicon) {
			bt= uiDefIconTextBut(block, LABEL, 0, md->titleicon, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		} else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		uiSetCurFont(block, block->font);
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
	uiEndBlock(block);

	return block;
}

uiBlock *ui_block_func_ICONROW(wmWindow *window, uiMenuBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	int a;
	
	block= uiBeginBlock(window, handle->region, "menu", UI_EMBOSSP, UI_HELV);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;
	
	for(a=(int)but->min; a<=(int)but->max; a++) {
		uiDefIconButF(block, BUTM|FLO, B_NOP, but->icon+(a-but->min), 0, (short)(18*a), (short)(but->x2-but->x1-4), 18, &handle->retvalue, (float)a, 0.0, 0, 0, "");
	}

	block->direction= UI_TOP;	

	uiEndBlock(block);

	return block;
}

uiBlock *ui_block_func_ICONTEXTROW(wmWindow *window, uiMenuBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	MenuData *md;
	int width, xmax, ypos, a;

	block= uiBeginBlock(window, handle->region, "menu", UI_EMBOSSP, UI_HELV);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;

	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
	if(md->title)
		width= 2*strlen(md->title)+UI_GetStringWidth(block->curfont, md->title, ui_translate_menus());
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(block->curfont, md->items[a].str, ui_translate_menus());
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
			uiDefIconTextButF(block, BUTM|FLO, B_NOP, (short)((but->icon)+(md->items[a].retval-but->min)), md->items[a].str, 0, ypos,(short)width, 19, &handle->retvalue, (float) md->items[a].retval, 0.0, 0, 0, "");
			ypos += 20;
		}
	}
	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		bt= uiDefBut(block, LABEL, 0, md->title, 0, ypos, (short)width, 19, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, block->font);
		bt->flag= UI_TEXT_LEFT;
	}
	
	menudata_free(md);

	block->direction= UI_TOP;

	uiBoundsBlock(block, 3);
	uiEndBlock(block);

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
static void do_palette_cb(void *bt1, void *col1)
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
static void do_palette1_cb(void *bt1, void *hsv1)
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
static void do_palette2_cb(void *bt1, void *col1)
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

static void do_palette_hex_cb(void *bt1, void *hexcl)
{
	uiBut *but1= (uiBut *)bt1;
	char *hexcol= (char *)hexcl;
	
	ui_update_block_buts_hex(but1->block, hexcol);	
}

/* used for both 3d view and image window */
static void do_palette_sample_cb(void *bt1, void *col1)	/* frontbuf */
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

uiBlock *ui_block_func_COL(wmWindow *window, uiMenuBlockHandle *handle, void *arg_but)
{
	uiBut *but= arg_but;
	uiBlock *block;
	static float hsvcol[3], oldcol[3];
	static char hexcol[128];
	
	block= uiBeginBlock(window, handle->region, "colorpicker", UI_EMBOSS, UI_HELV);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_KEEP_OPEN;
	block->themecol= TH_BUT_NUM;
	
	VECCOPY(handle->retvec, but->editvec);
	uiBlockPickerButtons(block, handle->retvec, hsvcol, oldcol, hexcol, 'p', 0);

	/* and lets go */
	block->direction= UI_TOP;
	uiBoundsBlock(block, 3);
	
	return block;
}

/* ******************** PUPmenu ****************** */

static int pupmenu_set= 0;

void pupmenu_set_active(int val)
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

uiBlock *ui_block_func_PUPMENU(wmWindow *window, uiMenuBlockHandle *handle, void *arg_info)
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
	block= uiBeginBlock(window, handle->region, "menu", UI_EMBOSSP, UI_HELV);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(info->instr);

	rows= md->nitems;
	columns= 1;

	/* size and location, title slightly bigger for bold */
	if(md->title) {
		width= 2*strlen(md->title)+UI_GetStringWidth(uiBlockGetCurFont(block), md->title, ui_translate_buttons());
		width /= columns;
	}
	else width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, ui_translate_buttons());
		if(xmax>width) width= xmax;

		if(strcmp(md->items[a].str, "%l")==0) height+= PUP_LABELH;
		else height+= MENU_BUTTON_HEIGHT;
	}

	width+= 10;
	if (width<50) width=50;
	
	wm_window_get_size(window, &xmax, &ymax);

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
		uiSetCurFont(block, UI_HELVB);

		if(md->titleicon) {
			width+= 20;
			sprintf(titlestr, " %s", md->title);
			uiDefIconTextBut(block, LABEL, 0, md->titleicon, titlestr, startx, (short)(starty+height), width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
		}
		else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+height), columns*width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		uiSetCurFont(block, UI_HELV);
	}

	x1= startx + width*((int)a/rows);
	y1= starty + height - MENU_BUTTON_HEIGHT;
		
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
	uiEndBlock(block);

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

uiBlock *ui_block_func_PUPMENUCOL(wmWindow *window, uiMenuBlockHandle *handle, void *arg_info)
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
	block= uiBeginBlock(window, handle->region, "menu", UI_EMBOSSP, UI_HELV);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
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
		width= 2*strlen(md->title)+UI_GetStringWidth(uiBlockGetCurFont(block), md->title, ui_translate_buttons());
		width /= columns;
	}
	else width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= UI_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, ui_translate_buttons());
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width<50) width=50;
	
	height= rows*MENU_BUTTON_HEIGHT;
	if (md->title) height+= MENU_BUTTON_HEIGHT;
	
	wm_window_get_size(window, &xmax, &ymax);

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
		uiSetCurFont(block, UI_HELVB);

		if(md->titleicon) {
		}
		else {
			bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*MENU_BUTTON_HEIGHT), columns*width, MENU_BUTTON_HEIGHT, NULL, 0.0, 0.0, 0, 0, "");
			bt->flag= UI_TEXT_LEFT;
		}
		uiSetCurFont(block, UI_HELV);
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
	uiEndBlock(block);

#if 0
	event= uiDoBlocks(&listb, 0, 1);
#endif
	
	menudata_free(md);
	
	/* XXX 2.5 need to warp back */
#if 0
	if((event & UI_RETURN_OUT)==0)
		ui_warp_pointer(mousemove[0], mousemove[1]);
#endif

	return block;
}

uiMenuBlockHandle *pupmenu_col(bContext *C, char *instr, int mx, int my, int maxrow)
{
	uiPupMenuInfo info;

	memset(&info, 0, sizeof(info));
	info.instr= instr;
	info.mx= mx;
	info.my= my;
	info.maxrow= maxrow;

	return ui_menu_block_create(C, NULL, NULL, ui_block_func_PUPMENUCOL, &info);
}

uiMenuBlockHandle *pupmenu(bContext *C, char *instr, int mx, int my)
{
	uiPupMenuInfo info;

	memset(&info, 0, sizeof(info));
	info.instr= instr;
	info.mx= mx;
	info.my= my;

	return ui_menu_block_create(C, NULL, NULL, ui_block_func_PUPMENU, &info);
}


void pupmenu_free(bContext *C, uiMenuBlockHandle *handle)
{
	ui_menu_block_free(C, handle);
}

/*************** Temporary Buttons Tests **********************/

static uiBlock *test_submenu(wmWindow *window, uiMenuBlockHandle *handle, void *arg)
{
	ARegion *ar= handle->region;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(window, ar, "test_viewmenu", UI_EMBOSSP, UI_HELV);
	//uiBlockSetButmFunc(block, do_test_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT,
					 "Only Selected Data Keys|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Marker|PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Marker|PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Key|Ctrl PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Key|Ctrl PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center View|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT,
					 "Lock Time to Other Windows|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	
	uiBlockSetDirection(block, UI_RIGHT);

	uiTextBoundsBlock(block, 50);
	uiEndBlock(block);
	
	return block;
}

static uiBlock *test_viewmenu(wmWindow *window, uiMenuBlockHandle *handle, void *arg_area)
{
	ScrArea *area= arg_area;
	ARegion *ar= handle->region;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiBeginBlock(window, ar, "test_viewmenu", UI_EMBOSSP, UI_HELV);
	//uiBlockSetButmFunc(block, do_test_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Seconds|T", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT,
					 "Only Selected Data Keys|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Marker|PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Marker|PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Next Key|Ctrl PageUp", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Jump To Prev Key|Ctrl PageDown", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center View|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, 
					 "Lock Time to Other Windows|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
    uiDefIconTextBlockBut(block, test_submenu, NULL, ICON_RIGHTARROW_THIN, "Sub Menu", 0, yco-=20, 120, 19, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	if(area->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);
	uiEndBlock(block);
	
	return block;
}

void uiTestRegion(const bContext *C)
{
	uiBlock *block;
	static float testcol[3];
	static char testtext[64];
	static float testnumf=5.0f;
	static short testchoice= 0, testtog= 0;
#if 0
	static CurveMapping *cumap= NULL;
	static ColorBand *coba= NULL;
#endif

	block= uiBeginBlock(C->window, C->region, "header buttons", UI_EMBOSS, UI_HELV);

	uiDefPulldownBut(block, test_viewmenu, C->area,  "View",
		13, 1, 50, 24, "");

	uiDefBut(block, BUT, 31415, "Type BUT",
		13+50+5, 3, 80, 20, NULL, 0, 0, 0, 0, "A tooltip.");
	uiDefButS(block, MENU, 31416, "Gather Method%t|Raytrace %x0|Approximate %x1",
		13+50+5+80+5, 3, 100, 20, &testchoice, 0, 0, 0, 0, "Method for occlusion gathering");
	uiDefButBitS(block, TOG, 1, 31417, "Pixel Cache",
		13+50+5+80+5+100+5, 3, 80, 20, &testtog, 0, 0, 0, 0, "Cache AO results in pixels and interpolate over neighbouring pixels for speedup.");

	uiDefBut(block, TEX, 31418, "Text: ",
		13+50+5+80+5+100+5+80+5, 3, 200, 20, testtext, 0, sizeof(testtext), 0, 0, "User defined text");

	uiDefButF(block, NUMSLI, 31419, "Slider: ",
		13+50+5+80+5+100+5+80+5+200+5, 3, 150, 20, &testnumf, 0.0, 10.0, 0, 0, "Some tooltip.");
	uiDefButF(block, NUM, 31419, "N: ",
		13+50+5+80+5+100+5+80+5+200+5+150+5, 3, 100, 20, &testnumf, 0.0, 10.0, 0, 0, "Some tooltip.");

    uiDefButF(block, COL, 3142, "",
		13+50+5+80+5+100+5+80+5+200+5+150+5+100+5, 3, 100, 20, testcol, 0, 0, 0, 0 /*B_BANDCOL*/, "");

#if 0
	if(!cumap) {
		cumap= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);  
		cumap->flag &= ~CUMA_DO_CLIP;
	}
	if(!coba)
		coba= add_colorband(0);

    uiDefBut(block, BUT_CURVE, 3143, "",
		13+400, 33, 100, 100, cumap, 0.0f, 1.0f, 0, 0, "");
    uiDefBut(block, BUT_COLORBAND, 3143, "",
		13+400+100+10, 33, 150, 30, coba, 0.0f, 1.0f, 0, 0, "");
#endif

	uiEndBlock(block);
	uiDrawBlock(block);
}

