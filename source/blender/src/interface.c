/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* 
     a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt

 */
 

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif // INTERNATIONAL

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BKE_blender.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_butspace.h"

#include "BSE_view.h"

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"

/* naming conventions:
 * 
 * uiBlahBlah()		external function
 * ui_blah_blah()	internal function
 */

/***/
/* ************ GLOBALS ************* */

float UIwinmat[4][4];
static int UIlock= 0, UIafterval;
static char *UIlockstr=NULL;
static void (*UIafterfunc)(void *arg, int event);
static void *UIafterfunc_arg;

static uiFont UIfont[UI_ARRAY];  // no init needed
static uiBut *UIbuttip;

/* ************* PROTOTYPES ***************** */

static void ui_set_but_val(uiBut *but, double value);
static void ui_set_ftf_font(uiBlock *block);

/* ****************************** */

static int uibut_contains_pt(uiBut *but, short *pt)
{
	return ((but->x1<pt[0] && but->x2>=pt[0]) && 
			(but->y1<pt[1] && but->y2>=pt[1]));
}

static void uibut_do_func(uiBut *but)
{
	if (but->func) {
		but->func(but->func_arg1, but->func_arg2);
	}
}

/* ************* window matrix ************** */


void ui_graphics_to_window(int win, float *x, float *y)	/* for rectwrite  */
{
	float gx, gy;
	int sx, sy;
	int getsizex, getsizey;

	bwin_getsize(win, &getsizex, &getsizey);
	bwin_getsuborigin(win, &sx, &sy);

	gx= *x;
	gy= *y;
	*x= sx + getsizex*(0.5+ 0.5*(gx*UIwinmat[0][0]+ gy*UIwinmat[1][0]+ UIwinmat[3][0]));
	*y= sy + getsizey*(0.5+ 0.5*(gx*UIwinmat[0][1]+ gy*UIwinmat[1][1]+ UIwinmat[3][1]));
}



void ui_window_to_graphics(int win, float *x, float *y)	/* for mouse cursor */
{
	float a, b, c, d, e, f, px, py;
	int getsizex, getsizey;
		
	bwin_getsize(win, &getsizex, &getsizey);

	a= .5*getsizex*UIwinmat[0][0];
	b= .5*getsizex*UIwinmat[1][0];
	c= .5*getsizex*(1.0+UIwinmat[3][0]);

	d= .5*getsizey*UIwinmat[0][1];
	e= .5*getsizey*UIwinmat[1][1];
	f= .5*getsizey*(1.0+UIwinmat[3][1]);
	
	px= *x;
	py= *y;
	
	*y=  (a*(py-f) + d*(c-px))/(a*e-d*b);
	*x= (px- b*(*y)- c)/a;
	
}


/* ************* SAVE UNDER ************ */

typedef struct {
	short x, y, sx, sy, oldwin;
	int oldcursor;
	unsigned int *rect;
} uiSaveUnder;


static void ui_paste_under(uiSaveUnder *su)
{
	
	if(su) {
		glDisable(GL_DITHER);
		glRasterPos2f( su->x-0.5,  su->y-0.5 );
		glDrawPixels(su->sx, su->sy, GL_RGBA, GL_UNSIGNED_BYTE, su->rect);
		glEnable(GL_DITHER);

		if(su->oldwin) {
			mywinset(su->oldwin);
			if (su->oldcursor) {
				set_cursor(su->oldcursor);
			}
		}
		
		MEM_freeN(su->rect);
		MEM_freeN(su);
	}
}


static uiSaveUnder *ui_save_under(int x, int y, int sx, int sy)
{
	uiSaveUnder *su=NULL;
	
	if(sx>1 && sy>1) {
		
		su= MEM_callocN(sizeof(uiSaveUnder), "save under");	
		
		su->rect= MEM_mallocN(sx*sy*4, "temp_frontbuffer_image");
		su->x= x;
		su->y= y;
		su->sx= sx;
		su->sy= sy;
		glReadPixels(x, y, sx, sy, GL_RGBA, GL_UNSIGNED_BYTE, su->rect);
	}
	
	return su;
}


static uiSaveUnder *ui_bgnpupdraw(int startx, int starty, int endx, int endy, int cursor)
{
	uiSaveUnder *su;
	short oldwin;
	
	#if defined(__sgi) || defined(__sun)
	/* this is a dirty patch: gets sometimes the backbuffer */
	my_get_frontbuffer_image(0, 0, 1, 1);
	my_put_frontbuffer_image();
	#endif

	oldwin= mywinget();

	mywinset(G.curscreen->mainwin);
	
	/* tinsy bit larger, 1 pixel on the edge */
	
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	
	/* for geforce and other cards */
	glFinish();

	su= ui_save_under(startx-1, starty-1, endx-startx+2, endy-starty+6);
	if(su) su->oldwin= oldwin;
	
	if(su && cursor) {
		su->oldcursor= get_cursor();
		set_cursor(CURSOR_STD);
	}
	
	return su;
}

static void ui_endpupdraw(uiSaveUnder *su)
{

	/* for geforce and other cards */

	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	
	glFinish();

	if(su) {
		ui_paste_under(su);
	}
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);
}



/* ******************* block calc ************************* */

void uiTextBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	int i = 0, j;
	
	bt= block->buttons.first;
	while(bt) {
		if(bt->type!=SEPR) {
			j= BIF_GetStringWidth(bt->font, bt->drawstr, (U.transopts & TR_BUTTONS));

			if(j > i) i = j;
		}
		bt= bt->next;
	}

	
	bt= block->buttons.first;
	while(bt) {
		bt->x2 = i + addval;
		ui_check_but(bt);	// clips text again
		bt= bt->next;
	}
}


void uiBoundsBlock(uiBlock *block, int addval)
{
	uiBut *bt;
	
	if(block->buttons.first==NULL) {
		if(block->panel) {
			block->minx= 0.0; block->maxx= block->panel->sizex;
			block->miny= 0.0; block->maxy= block->panel->sizey;
		}
	}
	else {
	
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
		
		block->minx -= addval;
		block->miny -= addval;
		block->maxx += addval;
		block->maxy += addval;
	}
}

static void ui_positionblock(uiBlock *block, uiBut *but)
{
	/* position block relative to but */
	uiBut *bt;
	rctf butrct;
	int xsize, ysize, xof=0, yof=0, centre;
	short dir1= 0, dir2=0;
	
	/* first transform to screen coords, assuming matrix is stil OK */
	/* the UIwinmat is in panelspace */

	butrct.xmin= but->x1; butrct.xmax= but->x2;
	butrct.ymin= but->y1; butrct.ymax= but->y2;

	ui_graphics_to_window(block->win, &butrct.xmin, &butrct.ymin);
	ui_graphics_to_window(block->win, &butrct.xmax, &butrct.ymax);
	block->parentrct= butrct;	// will use that for pulldowns later

	/* calc block rect */
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
	
	ui_graphics_to_window(block->win, &block->minx, &block->miny);
	ui_graphics_to_window(block->win, &block->maxx, &block->maxy);

	block->minx-= 2.0; block->miny-= 2.0;
	block->maxx+= 2.0; block->maxy+= 2.0;
	
	xsize= block->maxx - block->minx+4; // 4 for shadow
	ysize= block->maxy - block->miny+4;

	if(but) {
		short left=0, right=0, top=0, down=0;

		if(block->direction & UI_CENTRE) centre= ysize/2;
		else centre= 0;

		if( butrct.xmin-xsize > 0.0) left= 1;
		if( butrct.xmax+xsize < G.curscreen->sizex) right= 1;
		if( butrct.ymin-ysize+centre > 0.0) down= 1;
		if( butrct.ymax+ysize-centre < G.curscreen->sizey) top= 1;
		
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
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-centre;
			else yof= butrct.ymax - block->maxy+centre;
		}
		else if(dir1==UI_RIGHT) {
			xof= butrct.xmax - block->minx;
			if(dir2==UI_TOP) yof= butrct.ymin - block->miny-centre;
			else yof= butrct.ymax - block->maxy+centre;
		}
		else if(dir1==UI_TOP) {
			yof= butrct.ymax - block->miny-1;
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
			yof= butrct.ymin - block->maxy+1;
			if(dir2==UI_RIGHT) xof= butrct.xmax - block->maxx;
			else xof= butrct.xmin - block->minx;
			// changed direction?
			if((dir1 & block->direction)==0) {
				if(block->direction & UI_SHIFT_FLIPPED)
					xof+= dir2==UI_LEFT?25:-25;
				uiBlockFlipOrder(block);
			}
		}

		// apply requested offset in the block
		xof += block->xofs/block->aspect;
		yof += block->yofs/block->aspect;
		
	}
	
	/* apply */
	bt= block->buttons.first;
	while(bt) {
		
		ui_graphics_to_window(block->win, &bt->x1, &bt->y1);
		ui_graphics_to_window(block->win, &bt->x2, &bt->y2);

		bt->x1 += xof;
		bt->x2 += xof;
		bt->y1 += yof;
		bt->y2 += yof;

		bt->aspect= 1.0;
		
		bt= bt->next;
	}
	
	block->minx += xof;
	block->miny += yof;
	block->maxx += xof;
	block->maxy += yof;
	
	/* safety calculus */
	if(but) {
		float midx= (block->parentrct.xmin+block->parentrct.xmax)/2.0;
		float midy= (block->parentrct.ymin+block->parentrct.ymax)/2.0;
		
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
	}
	else {
		block->safety.xmin= block->minx-40;
		block->safety.ymin= block->miny-40;
		block->safety.xmax= block->maxx+40;
		block->safety.ymax= block->maxy+40;
	}

}


void ui_autofill(uiBlock *block)
{
	uiBut *but;
	float *maxw, *maxh, startx = 0, starty, height = 0;
	float totmaxh;
	int rows=0, /*  cols=0, */ i, lasti;
	
	/* first count rows */
	but= block->buttons.last;
	rows= but->x1+1;

	/* calculate max width / height for each row */
	maxw= MEM_callocN(sizeof(float)*rows, "maxw");
	maxh= MEM_callocN(sizeof(float)*rows, "maxh");
	but= block->buttons.first;
	while(but) {
		i= but->x1;
		if( maxh[i] < but->y2) maxh[i]= but->y2;
		maxw[i] += but->x2;
		but= but->next;
	}
	
	totmaxh= 0.0;
	for(i=0; i<rows; i++) totmaxh+= maxh[i];
	
	/* apply widths/heights */
	starty= block->maxy;
	but= block->buttons.first;
	lasti= -1;
	while(but) {
		
		i= but->x1;

		if(i!=lasti) {
			startx= block->minx;
			height= (maxh[i]*(block->maxy-block->miny))/totmaxh;
			starty-= height;
			lasti= i;
		}
		
		but->y1= starty+but->aspect;
		but->y2= but->y1+height-but->aspect;
		
		but->x2= (but->x2*(block->maxx-block->minx))/maxw[i];
		but->x1= startx+but->aspect;
		
		startx+= but->x2;
		but->x2+= but->x1-but->aspect;
		
		ui_check_but(but);
		
		but= but->next;
	}
	
	MEM_freeN(maxw); MEM_freeN(maxh);	
	block->autofill= 0;
}

/* ************** LINK LINE DRAWING  ************* */

/* link line drawing is not part of buttons or theme.. so we stick with it here */

static void ui_draw_linkline(uiBut *but, uiLinkLine *line)
{
	float vec1[2], vec2[2];

	if(line->from==NULL || line->to==NULL) return;
	
	if(but->block->frontbuf==UI_NEED_DRAW_FRONT) {
		but->block->frontbuf= UI_HAS_DRAW_FRONT;
	
		glDrawBuffer(GL_FRONT);
		if(but->win==curarea->headwin) curarea->head_swap= WIN_FRONT_OK;
		else curarea->win_swap= WIN_FRONT_OK;
	}

	vec1[0]= (line->from->x1+line->from->x2)/2.0;
	vec1[1]= (line->from->y1+line->from->y2)/2.0;
	vec2[0]= (line->to->x1+line->to->x2)/2.0;
	vec2[1]= (line->to->y1+line->to->y2)/2.0;
	
	if(line->flag & UI_SELECT) BIF_ThemeColorShade(but->themecol, 80);
	else glColor3ub(0,0,0);
	fdrawline(vec1[0], vec1[1], vec2[0], vec2[1]);
}

static void ui_draw_links(uiBlock *block)
{
	uiBut *but;
	uiLinkLine *line;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				ui_draw_linkline(but, line);
				line= line->next;
			}
		}
		but= but->next;
	}	
}

/* ************** BLOCK DRAWING FUNCTION ************* */


void uiDrawBlock(uiBlock *block)
{
	uiBut *but;

	if(block->autofill) ui_autofill(block);
	if(block->minx==0.0 && block->maxx==0.0) uiBoundsBlock(block, 0);

	uiPanelPush(block); // panel matrix
	
	if(block->flag & UI_BLOCK_LOOP) {
		uiDrawMenuBox(block->minx, block->miny, block->maxx, block->maxy);
	}
	else if(block->panel) ui_draw_panel(block);
	
	if(block->drawextra) block->drawextra();

	for (but= block->buttons.first; but; but= but->next) {
		ui_draw_but(but);
	}

	ui_draw_links(block);

	uiPanelPop(block); // matrix restored
}

/* ************* MENUBUTS *********** */

typedef struct {
	char *str;
	int retval;
	int icon;
} MenuEntry;

typedef struct {
	char *instr;
	char *title;
	
	MenuEntry *items;
	int nitems, itemssize;
} MenuData;

static MenuData *menudata_new(char *instr) {
	MenuData *md= MEM_mallocN(sizeof(*md), "MenuData");

	md->instr= instr;
	md->title= NULL;
	md->items= NULL;
	md->nitems= md->itemssize= 0;
	
	return md;
}

static void menudata_set_title(MenuData *md, char *title) {
	if (!md->title)
		md->title= title;
}

static void menudata_add_item(MenuData *md, char *str, int retval, int icon) {
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

static void menudata_free(MenuData *md) {
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
static MenuData *decompose_menu_string(char *str) 
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
				s++;
			}
		} else if (c=='|' || c=='\0') {
			if (nitem) {
				*s= '\0';

				if (nitem_is_title) {
					menudata_set_title(md, nitem);
					nitem_is_title= 0;
				} else {
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

static void ui_set_name_menu(uiBut *but, int value)
{
	MenuData *md;
	int i;
	
	md= decompose_menu_string(but->str);
	for (i=0; i<md->nitems; i++)
		if (md->items[i].retval==value)
			strcpy(but->drawstr, md->items[i].str);
	menudata_free(md);
}

static void ui_warp_pointer(short x, short y)
{
	/* OSX has very poor mousewarp support, it sends events;
	   this causes a menu being pressed immediately ... */
	#ifndef __APPLE__
	warp_pointer(x, y);
	#endif
}


static int ui_do_but_MENU(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	double fvalue;
	int width, height=0, a, xmax, starty;
	short startx;
	int columns=1, rows=0, boxh, event;
	short  x1, y1, active= -1;
	short mval[2];
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(but->str);

	/* columns and row calculation */
	columns= (md->nitems+20)/20;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<md->nitems) rows++;
		
	/* size and location */
	if(md->title)
		width= 1.5*but->aspect*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= but->aspect*BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & TR_MENUS));
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width < (but->x2 - but->x1)) width = (but->x2 - but->x1);
	if (width<50) width=50;

	boxh= TBOXH;
	
	height= rows*boxh;
	if (md->title) height+= boxh;
	
	getmouseco_sc(mval);
	
	/* find active item */
	fvalue= ui_get_but_val(but);
	for(active=0; active<md->nitems; active++) {
		if( md->items[active].retval== (int)fvalue ) break;
	}
	/* no active item? */
	if(active==md->nitems) {
		if(md->title) active= -1;
		else active= 0;
	}

	/* for now disabled... works confusing because you think it's a title or so.... */
	active= -1;

	/* here we go! */
	startx= but->x1;
	starty= but->y1;
	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, block->font+1);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, block->font);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {
		
		x1= but->x1 + width*((int)(md->nitems-a-1)/rows);
		y1= but->y1 - boxh*(a%rows) + (rows-1)*boxh; 

		if (strcmp(md->items[md->nitems-a-1].str, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1,(short)(width-(rows>1)), (short)(boxh-1), NULL, 0.0, 0.0, 0, 0, "");
		}
		else if(md->items[md->nitems-a-1].icon) {
			uiBut *bt= uiDefIconTextBut(block, BUTM|but->pointype, but->retval, md->items[md->nitems-a-1].icon ,md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
			if(active==a) bt->flag |= UI_ACTIVE;
		}
		else {
			uiBut *bt= uiDefBut(block, BUTM|but->pointype, but->retval, md->items[md->nitems-a-1].str, x1, y1,(short)(width-(rows>1)), (short)(boxh-1), but->poin, (float) md->items[md->nitems-a-1].retval, 0.0, 0, 0, "");
			if(active==a) bt->flag |= UI_ACTIVE;
		}
	}
	
	block->direction= UI_TOP;
	ui_positionblock(block, but);
	block->win= G.curscreen->mainwin;
	event= uiDoBlocks(&listb, 0);
	
	menudata_free(md);
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);
	
	uibut_do_func(but);

	return event;	
}



/* ************* EVENTS ************* */

void uiGetMouse(int win, short *adr)
{
	int x, y;
	float xwin, ywin;
	
	getmouseco_sc(adr);
	if (win == G.curscreen->mainwin) return;
	
	bwin_getsuborigin(win, &x, &y);

	adr[0]-= x;
	adr[1]-= y;

	xwin= adr[0];
	ywin= adr[1];

	ui_window_to_graphics(win, &xwin, &ywin);

	adr[0]= (short)(xwin+0.5);
	adr[1]= (short)(ywin+0.5);
}

static void ui_is_but_sel(uiBut *but)
{
	double value;
	int lvalue;
	short push=0, true=1;

	value= ui_get_but_val(but);

	if( but->type==TOGN ) true= 0;

	if( but->bit ) {
		lvalue= (int)value;
		if( BTST(lvalue, (but->bitnr)) ) push= true;
		else push= !true;
	}
	else {
		switch(but->type) {
		case BUT:
			push= 0;
			break;
		case KEYEVT:
			if (value==-1) push= 1;
			break;
		case TOG:
		case TOGR:
		case TOG3:
		case ICONTOG:
			if(value!=but->min) push= 1;
			break;
		case TOGN:
			if(value==0.0) push= 1;
			break;
		case ROW:
			if(value == but->max) push= 1;
			break;
		case COL:
			push= 1;
			break;
		default:
			push= 2;
			break;
		}
	}
	
	if(push==2);
	else if(push==1) but->flag |= UI_SELECT;
	else but->flag &= ~UI_SELECT;
}

static int ui_do_but_BUT(uiBut *but)
{
	int activated;
	
	do {
		int oflag= but->flag;
		short mval[2];
			
		uiGetMouse(mywinget(), mval);

		if (uibut_contains_pt(but, mval))
			but->flag |= UI_SELECT;
		else
			but->flag &= ~UI_SELECT;

		if (but->flag != oflag) {
			ui_draw_but(but);
			glFinish(); // flush display in subloops
		}
		
		PIL_sleep_ms(10);
	} while (get_mbut() & L_MOUSE);

	activated= (but->flag & UI_SELECT);

	if(activated) {
		uibut_do_func(but);
	}
	
	but->flag &= ~UI_SELECT;
	ui_draw_but(but);

	return activated?but->retval:0;
}

static int ui_do_but_KEYEVT(uiBut *but)
{
	unsigned short event= 0;
	short val;

		/* flag for ui_check_but */
	ui_set_but_val(but, -1);
	ui_check_but(but);
	ui_draw_but(but);

	do {
		event= extern_qread(&val);
	} while (!event || !val || ELEM(event, MOUSEX, MOUSEY));

	if (!key_event_to_string(event)[0]) event= 0;

	ui_set_but_val(but, (double) event);
	ui_check_but(but);
	ui_draw_but(but);
	
	return (event!=0);
}

static int ui_do_but_TOG(uiBlock *block, uiBut *but)
{
	uiBut *bt;
	double value;
	int w, lvalue, push;
	
	value= ui_get_but_val(but);
	lvalue= (int)value;
	
	if(but->bit) {
		w= BTST(lvalue, but->bitnr);
		if(w) lvalue = BCLR(lvalue, but->bitnr);
		else lvalue = BSET(lvalue, but->bitnr);
		
		if(but->type==TOGR) {
			if( (get_qual() & LR_SHIFTKEY)==0 ) {
				lvalue= 1<<(but->bitnr);
	
				ui_set_but_val(but, (double)lvalue);

				bt= block->buttons.first;
				while(bt) {
					if( bt!=but && bt->poin==but->poin ) {
						ui_is_but_sel(bt);
						ui_draw_but(bt);
					}
					bt= bt->next;
				}
			}
			else {
				if(lvalue==0) lvalue= 1<<(but->bitnr);
			}
		}
		ui_set_but_val(but, (double)lvalue);
		if(but->type==ICONTOG) ui_check_but(but);
		// no frontbuffer draw for this one
		if((but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
	}
	else {
		
		if(value==0.0) push= 1; 
		else push= 0;
		
		if(but->type==TOGN) push= !push;
		ui_set_but_val(but, (double)push);
		if(but->type==ICONTOG) ui_check_but(but);		
		// no frontbuffer draw for this one
		if((but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
	}
	
	/* no while loop...this button is used for viewmove */

	uibut_do_func(but);

	return but->retval;
}

static int ui_do_but_ROW(uiBlock *block, uiBut *but)
{
	uiBut *bt;
	
	ui_set_but_val(but, but->max);
	ui_draw_but(but);

	bt= block->buttons.first;
	while(bt) {
		if( bt!=but && bt->type==ROW ) {
			if(bt->min==but->min) {
				ui_is_but_sel(bt);
				ui_draw_but(bt);
			}
		}
		bt= bt->next;
	}
	return but->retval;
}

static int ui_do_but_TEX(uiBut *but)
{
	unsigned short dev;
	short x, mval[2], len=0, dodraw;
	char *str, backstr[UI_MAX_DRAW_STR];
	
	str= (char *)but->poin;
	
	but->flag |= UI_SELECT;

	uiGetMouse(mywinget(), mval);

	/* calculate cursor pos with current mousecoords */
	BLI_strncpy(backstr, but->drawstr, UI_MAX_DRAW_STR);
	but->pos= strlen(backstr)-but->ofs;

	while((but->aspect*BIF_GetStringWidth(but->font, backstr+but->ofs, (U.transopts & TR_BUTTONS)) + but->x1) > mval[0]) {
		if (but->pos <= 0) break;
		but->pos--;
		backstr[but->pos+but->ofs] = 0;
	}
	
	but->pos -= strlen(but->str);
	but->pos += but->ofs;
	if(but->pos<0) but->pos= 0;

	/* backup */
	BLI_strncpy(backstr, but->poin, UI_MAX_DRAW_STR);

	ui_draw_but(but);
	glFinish(); // flush display in subloops
	
	while (get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	len= strlen(str);
	but->min= 0.0;
	
	while(TRUE) {
		char ascii;
		short val;

		dodraw= 0;
		dev = extern_qread_ext(&val, &ascii);

		if(dev==INPUTCHANGE) break;
		else if(get_mbut() & L_MOUSE) break;
		else if(get_mbut() & R_MOUSE) break;
		else if(dev==ESCKEY) break;
		else if(dev==MOUSEX) val= 0;
		else if(dev==MOUSEY) val= 0;

		if(ascii) {
			if( ascii>31 && ascii<127) {
				if(len < but->max) {
					for(x= but->max; x>but->pos; x--)
						str[x]= str[x-1];
					str[but->pos]= ascii;
					but->pos++; 
					len++;
					str[len]= '\0';
					dodraw= 1;
				}
			}
		}
		else if(val) {
		
			if(dev==RIGHTARROWKEY) {
				if(G.qual & LR_SHIFTKEY) but->pos= strlen(str);
				else but->pos++;
				if(but->pos>strlen(str)) but->pos= strlen(str);
				dodraw= 1;
			}
			else if(dev==LEFTARROWKEY) {
				if(G.qual & LR_SHIFTKEY) but->pos= 0;
				else if(but->pos>0) but->pos--;
				dodraw= 1;
			}
			else if(dev==PADENTER || dev==RETKEY) {
				break;
			}
			else if(dev==DELKEY) {
					if(but->pos>=0 && but->pos<strlen(str)) {
							for(x=but->pos; x<=strlen(str); x++)
									str[x]= str[x+1];
							str[--len]='\0';
							dodraw= 1;
					}
			}
			else if(dev==BACKSPACEKEY) {
				if(len!=0) {
					if(get_qual() & LR_SHIFTKEY) {
						str[0]= 0;
						but->pos= 0;
						len= 0;
						dodraw= 1;
					}
					else if(but->pos>0) {
						for(x=but->pos; x<=strlen(str); x++)
							str[x-1]= str[x];
						but->pos--;
						str[--len]='\0';
						dodraw= 1;
					}
				} 
			}
		}
		if(dodraw) {
			ui_check_but(but);
			ui_draw_but(but);
			glFinish(); // flush display in subloops
		}
	}
	
	if(dev==ESCKEY) strcpy(but->poin, backstr);
	but->pos= -1;
	but->flag &= ~UI_SELECT;

	uibut_do_func(but);

	ui_check_but(but);
	ui_draw_but(but);
	
	if(dev!=ESCKEY) return but->retval;
	else return 0;
}


static int uiActAsTextBut(uiBut *but)
{
	double value;
	float min, max;
	int temp, retval, textleft;
	char str[UI_MAX_DRAW_STR], *point;
	
	
	value= ui_get_but_val(but);
	if( but->pointype==FLO ) {
		if(but->a2) { /* amount of digits defined */
			if(but->a2==1) sprintf(str, "%.1f", value);
			else if(but->a2==2) sprintf(str, "%.2f", value);
			else if(but->a2==3) sprintf(str, "%.3f", value);
			else sprintf(str, "%.4f", value);
		}
		else sprintf(str, "%.3f", value);
	}
	else {
		sprintf(str, "%d", (int)value);
	}
	point= but->poin;
	but->poin= str;
	min= but->min;
	max= but->max;
	but->min= 0.0;
	but->max= 15.0;
	temp= but->type;
	but->type= TEX;
	textleft= but->flag & UI_TEXT_LEFT;
	but->flag |= UI_TEXT_LEFT;
	ui_check_but(but);
	
	retval= ui_do_but_TEX(but);
	
	but->type= temp;
	but->poin= point;
	but->min= min;
	but->max= max;
	if(textleft==0) but->flag &= ~UI_TEXT_LEFT;

	if( but->pointype==FLO ) value= atof(str);
	else value= atoi(str);

	if(value<min) value= min;
	if(value>max) value= max;

	ui_set_but_val(but, value);
	ui_check_but(but);
	ui_draw_but(but);
	
	return retval;
}

static int ui_do_but_NUM(uiBut *but)
{
	double value;
	float deler, fstart, f, tempf;
	int lvalue, temp; /*  , firsttime=1; */
	short qual, sx, mval[2], pos=0;
	

	but->flag |= UI_SELECT;
	ui_draw_but(but);
	glFinish(); // flush display before subloop
	
	uiGetMouse(mywinget(), mval);
	value= ui_get_but_val(but);
	
	sx= mval[0];
	fstart= (value - but->min)/(but->max-but->min);
	f= fstart;
	
	temp= (int)value;
	tempf= value;
	
	if(get_qual() & LR_SHIFTKEY) {	/* make it textbut */
		if( uiActAsTextBut(but) ) return but->retval;
		else return 0;
	}
	
	/* firsttime: this button can be approached with enter as well */
	while (get_mbut() & L_MOUSE) {
		qual= get_qual();
		
		deler= 500;
		if( but->pointype!=FLO ) {

			if( (but->max-but->min)<100 ) deler= 200.0;
			if( (but->max-but->min)<25 ) deler= 50.0;

		}
		if(qual & LR_SHIFTKEY) deler*= 10.0;
		if(qual & LR_ALTKEY) deler*= 20.0;

		uiGetMouse(mywinget(), mval);
		
		if(mval[0] != sx) {
		
			f+= ((float)(mval[0]-sx))/deler;
			if(f>1.0) f= 1.0;
			if(f<0.0) f= 0.0;
			sx= mval[0];
			tempf= ( but->min + f*(but->max-but->min));
			
			if( but->pointype!=FLO ) {
				
				temp= floor(tempf+.5);
				
				if(tempf==but->min || tempf==but->max);
				else if(qual & LR_CTRLKEY) temp= 10*(temp/10);
				
				if( temp>=but->min && temp<=but->max) {
				
					value= ui_get_but_val(but);
					lvalue= (int)value;
					
					if(temp != lvalue ) {
						pos= 1;
						ui_set_but_val(but, (double)temp);
						ui_check_but(but);
						ui_draw_but(but);
						glFinish(); // flush display in subloops
						
						uibut_do_func(but);
					}
				}

			}
			else {
				temp= 0;
				if(qual & LR_CTRLKEY) {
					if(tempf==but->min || tempf==but->max);
					else if(but->max-but->min < 2.10) tempf= 0.1*floor(10*tempf);
					else if(but->max-but->min < 21.0) tempf= floor(tempf);
					else tempf= 10.0*floor(tempf/10.0);
				}

				if( tempf>=but->min && tempf<=but->max) {
					value= ui_get_but_val(but);
					
					if(tempf != value ) {
						pos= 1;
						ui_set_but_val(but, tempf);
						ui_check_but(but);
						ui_draw_but(but);
						glFinish(); // flush display in subloops
					}
				}

			}
		}
		BIF_wait_for_statechange();
	}
	
	if(pos==0) {  /* plus 1 or minus 1 */
		if( but->pointype!=FLO ) {

			if(sx<(but->x1+but->x2)/2) temp--;
			else temp++;
			
			if( temp>=but->min && temp<=but->max)
				ui_set_but_val(but, (double)temp);

		}
		else {
		
			if(sx<(but->x1+but->x2)/2) tempf-= 0.01*but->a1;
				else tempf+= 0.01*but->a1;

			if (tempf < but->min) tempf = but->min;
			if (tempf > but->max) tempf = but->max;

			ui_set_but_val(but, tempf);

		}
	}
	
	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	
	glFinish(); // flush display in subloops
	
	return but->retval;
}

static int ui_do_but_TOG3(uiBut *but)
{ 

	if( but->pointype==SHO ) {
		short *sp= (short *)but->poin;
		
		if( BTST(sp[1], but->bitnr)) {
			sp[1]= BCLR(sp[1], but->bitnr);
			sp[0]= BCLR(sp[0], but->bitnr);
		}
		else if( BTST(sp[0], but->bitnr)) {
			sp[1]= BSET(sp[1], but->bitnr);
		} else {
			sp[0]= BSET(sp[0], but->bitnr);
		}
	}
	else {
		if( BTST(*(but->poin+2), but->bitnr)) {
			*(but->poin+2)= BCLR(*(but->poin+2), but->bitnr);
			*(but->poin)= BCLR(*(but->poin), but->bitnr);
		}
		else if( BTST(*(but->poin), but->bitnr)) {
			*(but->poin+2)= BSET(*(but->poin+2), but->bitnr);
		} else {
			*(but->poin)= BSET(*(but->poin), but->bitnr);
		}
	}
	
	ui_is_but_sel(but);
	ui_draw_but(but);
	
	return but->retval;
}

static int ui_do_but_ICONROW(uiBut *but)
{
	ListBase listb= {NULL, NULL};
	uiBlock *block;
	int a;
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);
	
	/* here we go! */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;
	
	for(a=(int)but->min; a<=(int)but->max; a++) {
		uiDefIconBut(block, BUTM|but->pointype, but->retval, but->icon+(a-but->min), 0, (short)(18*a), (short)(but->x2-but->x1-4), 18, but->poin, (float)a, 0.0, 0, 0, "");
	}
	block->direction= UI_TOP;	
	ui_positionblock(block, but);
	
	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;
	
	uiDoBlocks(&listb, 0);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);	
	
	return but->retval;
}

static int ui_do_but_ICONTEXTROW(uiBut *but)
{
	uiBlock *block;
	ListBase listb={NULL, NULL};
	int width, a, xmax, ypos;
	MenuData *md;

	but->flag |= UI_SELECT;
	ui_draw_but(but);

	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, but->win);
	block->flag= UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT;
	block->themecol= TH_MENU_ITEM;

	md= decompose_menu_string(but->str);

	/* size and location */
	/* expand menu width to fit labels */
	if(md->title)
		width= 2*strlen(md->title)+BIF_GetStringWidth(block->curfont, md->title, (U.transopts & TR_MENUS));
	else
		width= 0;

	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(block->curfont, md->items[a].str, (U.transopts & TR_MENUS));
		if(xmax>width) width= xmax;
	}

	width+= 30;
	if (width<50) width=50;

	ypos = 0;

	/* loop through the menu options and draw them out with icons & text labels */
	for(a=0; a<md->nitems; a++) {

		/* add a space if there's a separator (%l) */
	        if (strcmp(md->items[a].str, "%l")==0) {
			ypos +=3;
		}
		else {
			uiDefIconTextBut(block, BUTM|but->pointype, but->retval, (short)((but->icon)+(md->items[a].retval-but->min)), md->items[a].str, 0, ypos,(short)width, 19, but->poin, (float) md->items[a].retval, 0.0, 0, 0, "");
			ypos += 20;
		}
	}

	block->direction= UI_TOP;
	ui_positionblock(block, but);

	/* the block is made with but-win, but is handled in mainwin space...
	   this is needs better implementation */
	block->win= G.curscreen->mainwin;

	uiBoundsBlock(block, 3);

	uiDoBlocks(&listb, 0);
	
	menudata_free(md);

	but->flag &= ~UI_SELECT;
	ui_check_but(but);
	ui_draw_but(but);

	uibut_do_func(but);

	return but->retval;

}

static int ui_do_but_IDPOIN(uiBut *but)
{
	char str[UI_MAX_DRAW_STR];
	ID *id;
	
	id= *but->idpoin_idpp;
	if(id) strcpy(str, id->name+2);
	else str[0]= 0;
	
	but->type= TEX;
	but->poin= str;
	but->min= 0.0;
	but->max= 22.0;
	ui_do_but_TEX(but);
	but->poin= NULL;
	but->type= IDPOIN;
	
	but->idpoin_func(str, but->idpoin_idpp);
	ui_check_but(but);
	ui_draw_but(but);
	
	return but->retval;
}

static int ui_do_but_SLI(uiBut *but)
{
	float f, fstart, tempf = 0.0, deler, value;
	int sx, h, temp, pos=0, lvalue, redraw;
	short mval[2], qual;
	float curmatrix[4][4];

	value= ui_get_but_val(but);
	uiGetMouse(mywinget(), mval);

	sx= mval[0];
	h= but->y2-but->y1;
	fstart= but->max-but->min;
	fstart= (value - but->min)/fstart;
	temp= 32767;

	if( but->type==NUMSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else if( but->type==HSVSLI) deler= ( (but->x2-but->x1)/2 - 5.0*but->aspect);
	else deler= (but->x2-but->x1- 5.0*but->aspect);
	

	while (get_mbut() & L_MOUSE) {
	
		qual= get_qual();
		uiGetMouse(mywinget(), mval);
		
		f= (float)(mval[0]-sx)/deler +fstart;
		
		if(qual & LR_CTRLKEY) {
			if(qual & LR_SHIFTKEY) f= floor(f*100.0)/100.0;
			else f= floor(f*10.0)/10.0;
		} 
		else if (qual & LR_SHIFTKEY) {
			f= (f-fstart)/10.0 + fstart;
		}
		
		CLAMP(f, 0.0, 1.0);
		tempf= but->min+f*(but->max-but->min);
		
		temp= floor(tempf+.5);
		
		value= ui_get_but_val(but);
		lvalue= (int) value;
		
		if( but->pointype!=FLO )
			redraw= (temp != lvalue);
		else
			redraw= (tempf != value);

		if (redraw) {
			pos= 1;
			
			ui_set_but_val(but, tempf);
			ui_check_but(but);
			ui_draw_but(but);
			glFinish(); // flush display in subloops
			
			if(but->a1) {	/* color number */
				uiBut *bt= but->prev;
				while(bt) {
					if(bt->retval == but->a1) ui_draw_but(bt);
					bt= bt->prev;
				}
				bt= but->next;
				while(bt) {
					if(bt->retval == but->a1) ui_draw_but(bt);
					bt= bt->next;
				}
			}
			/* save current window matrix (global UIwinmat)
			   because button callback function MIGHT change it
			   - which has until now occured through the Python API
			*/
			Mat4CpyMat4(curmatrix, UIwinmat);
			uibut_do_func(but);
			Mat4CpyMat4(UIwinmat, curmatrix);
		} 
		else BIF_wait_for_statechange();
	}

	
	if(temp!=32767 && pos==0) {  /* plus 1 or minus 1 */
		
		if( but->type==SLI) f= (float)(mval[0]-but->x1)/(but->x2-but->x1-h);
		else f= (float)(mval[0]- (but->x1+but->x2)/2)/( (but->x2-but->x1)/2 - h);
		
		f= but->min+f*(but->max-but->min);
		
		if( but->pointype!=FLO ) {

			if(f<temp) temp--;
			else temp++;
			if( temp>=but->min && temp<=but->max)
				ui_set_but_val(but, (float)temp);
		
		} 
		else {

			if(f<tempf) tempf-=.01;
			else tempf+=.01;
			if( tempf>=but->min && tempf<=but->max)
				ui_set_but_val(but, tempf);

		}
	}
	ui_check_but(but);
	ui_draw_but(but);
	glFinish(); // flush display in subloops
	
	return but->retval;
}

static int ui_do_but_NUMSLI(uiBut *but)
{
	short mval[2];

	/* first define if it's a slider or textbut */
	uiGetMouse(mywinget(), mval);
	
	if(mval[0]>= -6+(but->x1+but->x2)/2 ) {	/* slider */
		but->flag |= UI_SELECT;
		ui_draw_but(but);
		ui_do_but_SLI(but);
		but->flag &= ~UI_SELECT;
	}
	else {
		uiActAsTextBut(but);
	}

	while(get_mbut() & L_MOUSE) BIF_wait_for_statechange();
	
	ui_draw_but(but);
	
	/* hsv patch */
	if(but->type==HSVSLI) {
	
		if(but->str[0]=='H') {
			ui_draw_but(but->next);
			ui_draw_but(but->next->next);
		} 
		else if(but->str[0]=='S') {
			ui_draw_but(but->next);
			ui_draw_but(but->prev);
		} 
		else if(but->str[0]=='V') {
			ui_draw_but(but->prev);
			ui_draw_but(but->prev->prev);
		}
	}
	
	return but->retval;
}

static int ui_do_but_BLOCK(uiBut *but)
{
	uiBlock *block;
	uiBut *bt;
	
	but->flag |= UI_SELECT;
	ui_draw_but(but);	

	block= but->block_func(but->poin);

	block->xofs = -2;	/* for proper alignment */
	
	/* only used for automatic toolbox, so can set the shift flag */
	if(but->flag & UI_MAKE_TOP) {
		block->direction= UI_TOP|UI_SHIFT_FLIPPED;
		uiBlockFlipOrder(block);
	}
	if(but->flag & UI_MAKE_DOWN) block->direction= UI_DOWN|UI_SHIFT_FLIPPED;
	if(but->flag & UI_MAKE_LEFT) block->direction |= UI_LEFT;
	if(but->flag & UI_MAKE_RIGHT) block->direction |= UI_RIGHT;
	
	ui_positionblock(block, but);
	block->flag |= UI_BLOCK_LOOP;
	
	/* blocks can come from a normal window, but we go to screenspace */
	block->win= G.curscreen->mainwin;
	for(bt= block->buttons.first; bt; bt= bt->next) bt->win= block->win;
	bwin_getsinglematrix(block->win, block->winmat);
	
	/* postpone draw, this will cause a new window matrix, first finish all other buttons */
	block->flag |= UI_BLOCK_REDRAW;
	
	but->flag &= ~UI_SELECT;
	uibut_do_func(but);
	
	return 0;
}

static int ui_do_but_BUTM(uiBut *but)
{

	ui_set_but_val(but, but->min);
	UIafterfunc= but->butm_func;
	UIafterfunc_arg= but->butm_func_arg;
	UIafterval= but->a2;
	
	return but->retval;
}

static int ui_do_but_LABEL(uiBut *but)
{
	uibut_do_func(but);
	return but->retval;
}

static uiBut *ui_get_valid_link_button(uiBlock *block, uiBut *but, short *mval)
{
	uiBut *bt;
	
		/* find button to link to */
	for (bt= block->buttons.first; bt; bt= bt->next)
		if(bt!=but && uibut_contains_pt(bt, mval))
			break;

	if (bt) {
		if (but->type==LINK && bt->type==INLINK) {
			if( but->link->tocode == (int)bt->min ) {
				return bt;
			}
		}
		else if(but->type==INLINK && bt->type==LINK) {
			if( bt->link->tocode == (int)but->min ) {
				return bt;
			}
		}
	}

	return NULL;
}

static int ui_is_a_link(uiBut *from, uiBut *to)
{
	uiLinkLine *line;
	uiLink *link;
	
	link= from->link;
	if(link) {
		line= link->lines.first;
		while(line) {
			if(line->from==from && line->to==to) return 1;
			line= line->next;
		}
	}
	return 0;
}

static uiBut *ui_find_inlink(uiBlock *block, void *poin)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==INLINK) {
			if(but->poin == poin) return but;
		}
		but= but->next;
	}
	return NULL;
}

static void ui_add_link_line(ListBase *listb, uiBut *but, uiBut *bt)
{
	uiLinkLine *line;
	
	line= MEM_callocN(sizeof(uiLinkLine), "linkline");
	BLI_addtail(listb, line);
	line->from= but;
	line->to= bt;
}


void uiComposeLinks(uiBlock *block)
{
	uiBut *but, *bt;
	uiLink *link;
	void ***ppoin;
	int a;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK) {
			link= but->link;
			
			/* for all pointers in the array */
			if(link) {
				if(link->ppoin) {
					ppoin= link->ppoin;
					for(a=0; a < *(link->totlink); a++) {
						bt= ui_find_inlink(block, (*ppoin)[a] );
						if(bt) {
							ui_add_link_line(&link->lines, but, bt);
						}
					}
				}
				else if(link->poin) {
					bt= ui_find_inlink(block, *(link->poin) );
					if(bt) {
						ui_add_link_line(&link->lines, but, bt);
					}
				}
			}
		}
		but= but->next;
	}
}

static void ui_add_link(uiBut *from, uiBut *to)
{
	/* in 'from' we have to add a link to 'to' */
	uiLink *link;
	void **oldppoin;
	int a;
	
	if(ui_is_a_link(from, to)) {
		printf("already exists\n");
		return;
	}
	
	link= from->link;

	/* are there more pointers allowed? */
	if(link->ppoin) {
		oldppoin= *(link->ppoin);
		
		(*(link->totlink))++;
		*(link->ppoin)= MEM_callocN( *(link->totlink)*sizeof(void *), "new link");

		for(a=0; a< (*(link->totlink))-1; a++) {
			(*(link->ppoin))[a]= oldppoin[a];
		}
		(*(link->ppoin))[a]= to->poin;
		
		if(oldppoin) MEM_freeN(oldppoin);
	}
	else {
		*(link->poin)= to->poin;
	}
	
}

static int ui_do_but_LINK(uiBlock *block, uiBut *but)
{
	/* 
	 * This button only visualizes, the dobutton mode
	 * can add a new link, but then the whole system
	 * should be redrawn/initialized. 
	 * 
	 */
	uiBut *bt=0, *bto=NULL;
	short sval[2], mval[2], mvalo[2], first= 1;

	uiGetMouse(curarea->win, sval);
	mvalo[0]= sval[0];
	mvalo[1]= sval[1];
	
	while (get_mbut() & L_MOUSE) {
		uiGetMouse(curarea->win, mval);

		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || first) {			
				/* clear completely, because of drawbuttons */
			bt= ui_get_valid_link_button(block, but, mval);
			if(bt) {
				bt->flag |= UI_ACTIVE;
				ui_draw_but(bt);
			}
			if(bto && bto!=bt) {
				bto->flag &= ~UI_ACTIVE;
				ui_draw_but(bto);
			}
			bto= bt;

			if (!first) {
				glutil_draw_front_xor_line(sval[0], sval[1], mvalo[0], mvalo[1]);
			}
			glutil_draw_front_xor_line(sval[0], sval[1], mval[0], mval[1]);

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];

			first= 0;
		}
		else BIF_wait_for_statechange();		
	}
	
	if (!first) {
		glutil_draw_front_xor_line(sval[0], sval[1], mvalo[0], mvalo[1]);
	}

	if(bt) {
		if(but->type==LINK) ui_add_link(but, bt);
		else ui_add_link(bt, but);

		scrarea_queue_winredraw(curarea);
	}

	return 0;
}


/* ************************************************ */

void uiSetButLock(int val, char *lockstr)
{
	UIlock |= val;
	if (val) UIlockstr= lockstr;
}

void uiClearButLock()
{
	UIlock= 0;
	UIlockstr= NULL;
}

/* ********************** NEXT/PREV for arrowkeys etc ************** */

static uiBut *ui_but_prev(uiBut *but)
{
	while(but->prev) {
		but= but->prev;
		if(but->type!=LABEL && but->type!=SEPR) return but;
	}
	return NULL;
}

static uiBut *ui_but_next(uiBut *but)
{
	while(but->next) {
		but= but->next;
		if(but->type!=LABEL && but->type!=SEPR) return but;
	}
	return NULL;
}

static uiBut *ui_but_first(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.first;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR) return but;
		but= but->next;
	}
	return NULL;
}

static uiBut *ui_but_last(uiBlock *block)
{
	uiBut *but;
	
	but= block->buttons.last;
	while(but) {
		if(but->type!=LABEL && but->type!=SEPR) return but;
		but= but->prev;
	}
	return NULL;
}

/* *************************************************************** */

static void setup_file(uiBlock *block)
{
	uiBut *but;
	FILE *fp;

	fp= fopen("butsetup","w");
	if(fp==NULL);
	else {
		but= block->buttons.first;
		while(but) {
			// if(but->rt[3]==1) {
			ui_check_but(but);
			fprintf(fp,"%d,%d,%d,%d   %s %s\n", (int)but->x1, (int)but->y1, (int)( but->x2-but->x1), (int)(but->y2-but->y1), but->str, but->tip);
			// }
			but= but->next;
		}
		fclose(fp);
	}
}


static void edit_but(uiBlock *block, uiBut *but, uiEvent *uevent)
{
	short dx, dy, mval[2], mvalo[2], didit=0;
	
	getmouseco_sc(mvalo);
	while(TRUE) {
		if( !(get_mbut() & L_MOUSE) ) break;	
	
		getmouseco_sc(mval);
		dx= (mval[0]-mvalo[0]);
		dy= (mval[1]-mvalo[1]);
		
		if(dx!=0 || dy!=0) {
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			cpack(0xc0c0c0);
			glRectf(but->x1-2, but->y1-2, but->x2+2, but->y2+2); 
			
			if((uevent->qual & LR_SHIFTKEY)==0) {
				but->x1 += dx;
				but->y1 += dy;
			}
			but->x2 += dx;
			but->y2 += dy;
			
			ui_draw_but(but);
			glFinish();
			didit= 1;
			but->rt[3]= 1;
		}
		/* idle for this poor code */
		else PIL_sleep_ms(30);
	}
	if(didit) setup_file(block);
}

/* is called when LEFTMOUSE is pressed or released
 * return: butval or zero
 */
static int ui_do_button(uiBlock *block, uiBut *but, uiEvent *uevent)
{
	int retval= 0;

	if(but->lock) {
		if (but->lockstr) {
			error("%s", but->lockstr);
			return 0;
		}
	} 
	else {
		if( but->pointype ) {		/* there's a pointer needed */
			if(but->poin==0 ) {
				printf("DoButton pointer error: %s\n",but->str);
				return 0;
			}
		}
	}

	if(G.rt==1 && (uevent->qual & LR_CTRLKEY)) {
		edit_but(block, but, uevent);
		return 0;
	}
	
	block->flag |= UI_BLOCK_BUSY;

	switch(but->type) {
	case BUT:
		if(uevent->val) retval= ui_do_but_BUT(but);		
		break;

	case KEYEVT:
		if(uevent->val) retval= ui_do_but_KEYEVT(but);
		break;

	case TOG: 
	case TOGR: 
	case ICONTOG: 
	case TOGN:
		if(uevent->val) {
			retval= ui_do_but_TOG(block, but);
		}
		break;
		
	case ROW:
		if(uevent->val) retval= ui_do_but_ROW(block, but);
		break;

	case SCROLL:
		/* DrawBut(b, 1); */
		/* do_scrollbut(b); */
		/* DrawBut(b,0); */
		break;

	case NUM:
		if(uevent->val) retval= ui_do_but_NUM(but);
		break;
		
	case SLI:
	case NUMSLI:
	case HSVSLI:
		if(uevent->val) retval= ui_do_but_NUMSLI(but);
		break;
		
	case LABEL:	
		if(uevent->val) retval= ui_do_but_LABEL(but);
		break;
		
	case TOG3:	
		if(uevent->val) retval= ui_do_but_TOG3(but);
		break;
		
	case TEX:
		if(uevent->val) retval= ui_do_but_TEX(but);
		break;
		
	case MENU:
		if(uevent->val) retval= ui_do_but_MENU(but);
		break;
		
	case ICONROW:
		if(uevent->val) retval= ui_do_but_ICONROW(but);		
		break;
		
	case ICONTEXTROW:
		if(uevent->val) retval= ui_do_but_ICONTEXTROW(but);
		break;

	case IDPOIN:
		if(uevent->val) retval= ui_do_but_IDPOIN(but);	
		break;
	
	case BLOCK:
		if(uevent->val) {
			retval= ui_do_but_BLOCK(but);
			if(block->auto_open==0) block->auto_open= 1;
		}
		break;

	case BUTM:
		retval= ui_do_but_BUTM(but);
		break;

	case LINK:
	case INLINK:
		retval= ui_do_but_LINK(block, but);
		break;
	}
	
	block->flag &= ~UI_BLOCK_BUSY;

	return retval;
}

static void ui_delete_active_linkline(uiBlock *block)
{
	uiBut *but;
	uiLink *link;
	uiLinkLine *line, *nline;
	int a, b;
	
	but= block->buttons.first;
	while(but) {
		if(but->type==LINK && but->link) {
			line= but->link->lines.first;
			while(line) {
				
				nline= line->next;
				
				if(line->flag & UI_SELECT) {
					BLI_remlink(&but->link->lines, line);

					link= line->from->link;

					/* are there more pointers allowed? */
					if(link->ppoin) {
						
						if(*(link->totlink)==1) {
							*(link->totlink)= 0;
							MEM_freeN(*(link->ppoin));
							*(link->ppoin)= NULL;
						}
						else {
							b= 0;
							for(a=0; a< (*(link->totlink)); a++) {
								
								if( (*(link->ppoin))[a] != line->to->poin ) {
									(*(link->ppoin))[b]= (*(link->ppoin))[a];
									b++;
								}
							}	
							(*(link->totlink))--;
						}
					}
					else {
						*(link->poin)= NULL;
					}

					MEM_freeN(line);
				}
				line= nline;
			}
		}
		but= but->next;
	}
	
	/* temporal! these buttons can be everywhere... */
	allqueue(REDRAWBUTSLOGIC, 0);
}

static void ui_do_active_linklines(uiBlock *block, short *mval)
{
	uiBut *but;
	uiLinkLine *line, *act= NULL;
	float mindist= 12.0, fac, v1[2], v2[2], v3[3];
	int foundone= 0; 
	
	if(mval) {
		v1[0]= mval[0];
		v1[1]= mval[1];
		
		/* find a line close to the mouse */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				foundone= 1;
				line= but->link->lines.first;
				while(line) {
					v2[0]= line->from->x2;
					v2[1]= (line->from->y1+line->from->y2)/2.0;
					v3[0]= line->to->x1;
					v3[1]= (line->to->y1+line->to->y2)/2.0;
					
					fac= PdistVL2Dfl(v1, v2, v3);
					if(fac < mindist) {
						mindist= fac;
						act= line;
					}
					line= line->next;
				}
			}
			but= but->next;
		}
	}

	/* check for a 'found one' to prevent going to 'frontbuffer' mode.
	   this slows done gfx quite some, and at OSX the 'finish' forces a swapbuffer */
	if(foundone) {
	
		/* draw */
		but= block->buttons.first;
		while(but) {
			if(but->type==LINK && but->link) {
				line= but->link->lines.first;
				while(line) {
					if(line==act) {
						if((line->flag & UI_SELECT)==0) {
							line->flag |= UI_SELECT;
							ui_draw_linkline(but, line);
						}
					}
					else if(line->flag & UI_SELECT) {
						line->flag &= ~UI_SELECT;
						ui_draw_linkline(but, line);
					}
					line= line->next;
				}
			}
			but= but->next;
		}
	}
}

/* only to be used to prevent an 'outside' event when using nested pulldowns */
/* four checks:
  - while mouse moves in good x direction
  - while mouse motion x is bigger than y motion
  - while distance to center block diminishes
  - only for 1 second
  
  return 0: check outside
*/
static int ui_mouse_motion_towards_block(uiBlock *block, uiEvent *uevent)
{
	short mvalo[2], dx, dy, domx, domy, x1, y1;
	int disto, dist, counter=0;

	if((block->direction & UI_TOP) || (block->direction & UI_DOWN)) return 0;
	if(uevent->event!= MOUSEX && uevent->event!= MOUSEY) return 0;
	
	/* calculate dominant direction */
	domx= ( -uevent->mval[0] + (block->maxx+block->minx)/2 );
	domy= ( -uevent->mval[1] + (block->maxy+block->miny)/2 );
	/* we need some accuracy */
	if( abs(domx)<4 ) return 0;
	
	/* calculte old dist */
	disto= domx*domx + domy*domy;
	
	uiGetMouse(mywinget(), mvalo);
	
	while(TRUE) {
		uiGetMouse(mywinget(), uevent->mval);
		
		/* check inside, if so return */
		if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {		
			if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) {
				return 1;
			}
		}
		
		/* check direction */
		dx= uevent->mval[0] - mvalo[0];
		dy= uevent->mval[1] - mvalo[1];
				
		if( abs(dx)+abs(dy)>4 ) {  // threshold
			if( abs(dy) > abs(dx) ) {
				//printf("left because y>x direction\n");
				return 0;
			}
			
			if( dx>0 && domx>0);
			else if(dx<0 && domx<0);
			else {
				//printf("left because dominant direction\n");
				return 0;
			}
			
		}
		
		/* check dist */
		x1= ( -uevent->mval[0] + (block->maxx+block->minx)/2 );
		y1= ( -uevent->mval[1] + (block->maxy+block->miny)/2 );
		dist= x1*x1 + y1*y1;
		if(dist > disto) {
			//printf("left because distance\n");
			return 0;
		}
		else disto= dist;

		/* idle for this poor code */
		PIL_sleep_ms(10);
		counter++;
		if(counter > 100) {
			// printf("left because of timer (1 sec)\n");
			return 0;
		}
	}
	
	return 0;
}


static void ui_set_ftf_font(uiBlock *block)
{

#ifdef INTERNATIONAL
	if(block->aspect<1.15) {
		FTF_SetFontSize('l');
	}
	else if(block->aspect<1.59) {
		FTF_SetFontSize('m');
	}
	else {
		FTF_SetFontSize('s');
	}
#endif
}


/* return: 
 * UI_NOTHING	pass event to other ui's
 * UI_CONT		don't pass event to other ui's
 * UI_RETURN	something happened, return, swallow event
 */
static int ui_do_block(uiBlock *block, uiEvent *uevent)
{
	uiBut *but, *bt;
	int butevent, event, retval=UI_NOTHING, count, act=0;
	int inside= 0, active=0;
	
	if(block->win != mywinget()) return UI_NOTHING;

	/* filter some unwanted events */
	/* btw: we allow event==0 for first time in menus, draws the hilited item */
	if(uevent==0 || uevent->event==LEFTSHIFTKEY || uevent->event==RIGHTSHIFTKEY) return UI_NOTHING;
	
	if(block->flag & UI_BLOCK_ENTER_OK) {
		if(uevent->event == RETKEY && uevent->val) {
			// printf("qual: %d %d %d\n", uevent->qual, get_qual(), G.qual);
			if ((G.qual & LR_SHIFTKEY) == 0) {
				return UI_RETURN_OK;
			}
		}
	}		

	ui_set_ftf_font(block);	// sets just a pointer in ftf lib... the button dont have ftf handles

	Mat4CpyMat4(UIwinmat, block->winmat);
	uiPanelPush(block); // push matrix; no return without pop!

	uiGetMouse(mywinget(), uevent->mval);	/* transformed mouseco */

	/* check boundbox and panel events */
	if( block->minx <= uevent->mval[0] && block->maxx >= uevent->mval[0] ) {
		// inside block
		if( block->miny <= uevent->mval[1] && block->maxy >= uevent->mval[1] ) inside= 1;
		
		if(block->panel && block->panel->paneltab==NULL) {
			
			/* clicked at panel header? */
			if( block->panel->flag & PNL_CLOSEDX) {
				if(block->minx <= uevent->mval[0] && block->minx+PNL_HEADER >= uevent->mval[0]) 
					inside= 2;
			}
			else if( (block->maxy <= uevent->mval[1]) && (block->maxy+PNL_HEADER >= uevent->mval[1]) )
				inside= 2;
				
			if(inside) {	// this stuff should move to do_panel
				
				if(uevent->event==LEFTMOUSE) {
					if(inside==2) {
						uiPanelPop(block); 	// pop matrix; no return without pop!
						ui_do_panel(block, uevent);
						return UI_EXIT_LOOP;	// exit loops because of moving panels
					}
				}
				else if(uevent->event==ESCKEY) {
					if(block->handler) {
						rem_blockhandler(curarea, block->handler);
						addqueue(curarea->win, REDRAW, 1);
					}
				}
				else if(uevent->event==PADPLUSKEY || uevent->event==PADMINUS) {
					SpaceLink *sl= curarea->spacedata.first;
					if(curarea->spacetype!=SPACE_BUTS) {
						if(uevent->event==PADPLUSKEY) sl->blockscale+= 0.1;
						else sl->blockscale-= 0.1;
						CLAMP(sl->blockscale, 0.6, 1.0);
						addqueue(block->winq, REDRAW, 1);
						retval= UI_CONT;
					}
				}
			}
		}
	}
	
	switch(uevent->event) {
	case PAD8: case PAD2:
	case UPARROWKEY:
	case DOWNARROWKEY:
		if(inside || (block->flag & UI_BLOCK_LOOP)) {
			/* arrowkeys: only handle for block_loop blocks */
			event= 0;
			if(block->flag & UI_BLOCK_LOOP) {
				event= uevent->event;
				if(event==PAD8) event= UPARROWKEY;
				if(event==PAD2) event= DOWNARROWKEY;
			}
			else {
				if(uevent->event==PAD8) event= UPARROWKEY;
				if(uevent->event==PAD2) event= DOWNARROWKEY;
			}
			if(event && uevent->val) {
	
				but= block->buttons.first;
				while(but) {
					
					but->flag &= ~UI_MOUSE_OVER;
		
					if(but->flag & UI_ACTIVE) {
						but->flag &= ~UI_ACTIVE;
						ui_draw_but(but);
	
						bt= ui_but_prev(but);
						if(bt && event==UPARROWKEY) {
							bt->flag |= UI_ACTIVE;
							ui_draw_but(bt);
							break;
						}
						bt= ui_but_next(but);
						if(bt && event==DOWNARROWKEY) {
							bt->flag |= UI_ACTIVE;
							ui_draw_but(bt);
							break;
						}
					}
					but= but->next;
				}
	
				/* nothing done */
				if(but==NULL) {
				
					if(event==UPARROWKEY) but= ui_but_last(block);
					else but= ui_but_first(block);
					
					if(but) {
						but->flag |= UI_ACTIVE;
						ui_draw_but(but);
					}
				}
				retval= UI_CONT;
			}
		}
		break;
	
	case ONEKEY: act= 1;
	case TWOKEY: if(act==0) act= 2;
	case THREEKEY: if(act==0) act= 3;
	case FOURKEY: if(act==0) act= 4;
	case FIVEKEY: if(act==0) act= 5;
	case SIXKEY: if(act==0) act= 6;
	case SEVENKEY: if(act==0) act= 7;
	case EIGHTKEY: if(act==0) act= 8;
	case NINEKEY: if(act==0) act= 9;
	case ZEROKEY: if(act==0) act= 10;
	
		if( block->flag & UI_BLOCK_NUMSELECT ) {
			
			if(get_qual() & LR_ALTKEY) act+= 10;
			
			but= block->buttons.first;
			count= 0;
			while(but) {
				if( but->type!=LABEL && but->type!=SEPR) count++;
				if(count==act) {
					but->flag |= UI_ACTIVE;
					if(uevent->val==1) ui_draw_but(but);
					else {
						uevent->event= RETKEY;
						uevent->val= 1;			/* patch: to avoid UI_BLOCK_RET_1 type not working */
						addqueue(block->winq, RIGHTARROWKEY, 1);
					}
				}
				else if(but->flag & UI_ACTIVE) {
					but->flag &= ~UI_ACTIVE;
					ui_draw_but(but);
				}
				but= but->next;
			}
		}
		
		break;
		
	default:
		if (uevent->event!=RETKEY) {	/* when previous command was arrow */
			but= block->buttons.first;
			while(but) {
			
				but->flag &= ~UI_MOUSE_OVER;
				
				/* check boundbox */
				if (uibut_contains_pt(but, uevent->mval)) {
					but->flag |= UI_MOUSE_OVER;
					UIbuttip= but;
				}
				/* hilite case 1 */
				if(but->flag & UI_MOUSE_OVER) {
					if( (but->flag & UI_ACTIVE)==0) {
						but->flag |= UI_ACTIVE;
						if(but->type != LABEL && (but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
					}
				}
				/* hilite case 2 */
				if(but->flag & UI_ACTIVE) {
					if( (but->flag & UI_MOUSE_OVER)==0) {
						/* we dont clear active flag until mouse move, for Menu buttons to remain showing active item when opened */
						if (uevent->event==MOUSEY) {
							but->flag &= ~UI_ACTIVE;
							if(but->type != LABEL && (but->flag & UI_NO_HILITE)==0) ui_draw_but(but);
						}
					}
					else if(but->type==BLOCK || but->type==MENU) {	// automatic opens block button (pulldown)
						int time;
						if(uevent->event!=LEFTMOUSE ) {
							if(block->auto_open==2) time= 1;	// test for toolbox
							else if(block->auto_open) time= 5*U.menuthreshold2;
							else if(U.uiflag & MENUOPENAUTO) time= 5*U.menuthreshold1;
							else time= -1;

							for (; time>0; time--) {
								if (qtest()) break;
								else PIL_sleep_ms(20);
							}

							if(time==0) {
								uevent->val= 1;	// otherwise buttons dont react
								ui_do_button(block, but, uevent);
							}
						}
					}
					if(but->flag & UI_ACTIVE) active= 1;
				}
				
				but= but->next;
			}
			
			/* if there are no active buttons... otherwise clear lines */
			if(active) ui_do_active_linklines(block, 0);
			else ui_do_active_linklines(block, uevent->mval);			

		}
	}

	/* middlemouse exception, not for regular blocks */
	if( (block->flag & UI_BLOCK_LOOP) && uevent->event==MIDDLEMOUSE) uevent->event= LEFTMOUSE;

	/* the final dobutton */
	but= block->buttons.first;
	while(but) {
		if(but->flag & UI_ACTIVE) {
			
			/* UI_BLOCK_RET_1: not return when val==0 */
			
			if(uevent->val || (block->flag & UI_BLOCK_RET_1)==0) {
				if ELEM3(uevent->event, LEFTMOUSE, PADENTER, RETKEY) {
					/* when mouse outside, don't do button */
					if(inside || uevent->event!=LEFTMOUSE) {
						butevent= ui_do_button(block, but, uevent);
						if(butevent) addqueue(block->winq, UI_BUT_EVENT, (short)butevent);

						/* i doubt about the next line! */
						/* if(but->func) mywinset(block->win); */
			
						if( (block->flag & UI_BLOCK_LOOP) && but->type==BLOCK);
						else	
							if(/*but->func ||*/ butevent) retval= UI_RETURN_OK;
					}
				}
			}
		}
		
		but= but->next;
	}

	uiPanelPop(block); // pop matrix; no return without pop!

	/* the linkines... why not make buttons from it? Speed? Memory? */
	if(uevent->val && (uevent->event==XKEY || uevent->event==DELKEY)) 
		ui_delete_active_linkline(block);

	if(block->flag & UI_BLOCK_LOOP) {

		if(inside==0 && uevent->val==1) {
			if ELEM3(uevent->event, LEFTMOUSE, MIDDLEMOUSE, RIGHTMOUSE) {
				if(BLI_in_rctf(&block->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]));
				else return UI_RETURN_OUT;
			}
		}

		if(uevent->event==ESCKEY && uevent->val==1) return UI_RETURN_CANCEL;

		if((uevent->event==RETKEY || uevent->event==PADENTER) && uevent->val==1) return UI_RETURN_OK;
		
		/* check outside */
		if(inside==0) {
			/* strict check, and include the parent rect */
			if( BLI_in_rctf(&block->parentrct, (float)uevent->mval[0], (float)uevent->mval[1]));
			else if( ui_mouse_motion_towards_block(block, uevent));
			else if( BLI_in_rctf(&block->safety, (float)uevent->mval[0], (float)uevent->mval[1]));
			else return UI_RETURN_OUT;
		}
	}

	return retval;
}

static uiSaveUnder *ui_draw_but_tip(uiBut *but)
{
	uiSaveUnder *su;
	float x1, x2, y1, y2;
	
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		float llx,lly,llz,urx,ury,urz;  //for FTF_GetBoundingBox()

		if(U.transopts & TR_TOOLTIPS) {
			FTF_GetBoundingBox(but->tip, &llx,&lly,&llz,&urx,&ury,&urz, FTF_USE_GETTEXT | FTF_INPUT_UTF8);

			x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*FTF_GetStringWidth(but->tip, FTF_USE_GETTEXT | FTF_INPUT_UTF8);  //BMF_GetStringWidth(but->font, but->tip);
			y1= but->y1-(ury+FTF_GetSize()); y2= but->y1;
		} else {
			FTF_GetBoundingBox(but->tip, &llx,&lly,&llz,&urx,&ury,&urz, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);

			x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*FTF_GetStringWidth(but->tip, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);  //BMF_GetStringWidth(but->font, but->tip);
			y1= but->y1-(ury+FTF_GetSize()); y2= but->y1;
		}
	} else {
  		x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*BMF_GetStringWidth(but->font, but->tip);
  		y1= but->y1-19; y2= but->y1-2;
	}
#else
  	x1= (but->x1+but->x2)/2; x2= 10+x1+ but->aspect*BMF_GetStringWidth(but->font, but->tip);
  	y1= but->y1-19; y2= but->y1-2;
#endif

	/* for pulldown menus it doesnt work */
	if(mywinget()==G.curscreen->mainwin);
	else {
		ui_graphics_to_window(mywinget(), &x1, &y1);
		ui_graphics_to_window(mywinget(), &x2, &y2);
	}
	
	if(x2 > G.curscreen->sizex) {
		x1 -= x2-G.curscreen->sizex;
		x2= G.curscreen->sizex;
	}
	if(y1 < 0) {
		y1 += 36;
		y2 += 36;
	}

	// adjust tooltip heights
	if(mywinget()==G.curscreen->mainwin)
		y2 -= G.ui_international ? 4:1;		//tip is from pulldownmenu
	else if(curarea->win != mywinget())
		y2 -= G.ui_international ? 5:1;		//tip is from a windowheader
//	else y2 += 1;							//tip is from button area

	su= ui_bgnpupdraw((int)(x1-1), (int)(y1-2), (int)(x2+4), (int)(y2+4), 0);

	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4ub(0, 0, 0, 60);
	fdrawline(x1+3, y1-1, x2, y1-1);
	fdrawline(x2+1, y1, x2+1, y2-3);
	
	glColor4ub(0, 0, 0, 40);
	fdrawline(x1+3, y1-2, x2+1, y1-2);
	fdrawline(x2+2, y1-1, x2+2, y2-3);

	glColor4ub(0, 0, 0, 20);
	fdrawline(x1+3, y1-3, x2+2, y1-3);
	fdrawline(x2+3, y1-2, x2+3, y2-3);
	
	glColor4ub(0, 0, 0, 20);
	fdrawline(x2, y2, x2+3, y2-3);
	

	glDisable(GL_BLEND);
	
	glColor3ub(0xFF, 0xFF, 0xDD);
	glRectf(x1, y1, x2, y2);
	
	glColor3ub(0,0,0);
	glRasterPos2f( x1+3, y1+4);
	BIF_DrawString(but->font, but->tip, (U.transopts & TR_TOOLTIPS));
	
	glFinish();		/* to show it in the frontbuffer */
	return su;
}

static void ui_do_but_tip(void)
{
	uiSaveUnder *su;
	int time;
	
	if (UIbuttip && UIbuttip->tip && UIbuttip->tip[0]) {
			/* Pause for a moment to see if we
			 * should really display the tip
			 * or if the user will keep moving
			 * the pointer.
			 */
		for (time= 0; time<10; time++) {
			if (anyqtest())
				return;
			else
				PIL_sleep_ms(50);
		}
			
			/* Display the tip, and keep it displayed
			 * as long as the mouse remains on top
			 * of the button that owns it.
			 */
		uiPanelPush(UIbuttip->block); // panel matrix
		su= ui_draw_but_tip(UIbuttip);
		
		while (1) {
			char ascii;
			short val;
			unsigned short evt= extern_qread_ext(&val, &ascii);

			if (evt==MOUSEX || evt==MOUSEY) {
				short mouse[2];
				uiGetMouse(su->oldwin, mouse);
				
				if (!uibut_contains_pt(UIbuttip, mouse))
					break;
			} else {
				mainqpushback(evt, val, ascii);
				break;
			}
		}
		
		ui_endpupdraw(su);
		uiPanelPop(UIbuttip->block); // panel matrix
		UIbuttip= NULL;
	}
}

/* returns UI_NOTHING, if nothing happened */
int uiDoBlocks(ListBase *lb, int event)
{
	/* return when:  firstblock != BLOCK_LOOP
	 * 
	 * 'cont' is used to make sure you can press another button while a looping menu
	 * is active. otherwise you have to press twice...
	 */

	uiBlock *block, *first;
	uiEvent uevent;
	int retval= UI_NOTHING, cont= 1, dopop=0;

	if(lb->first==0) return UI_NOTHING;
	
	/* for every pixel both x and y events are generated, overloads the system! */
	if(event==MOUSEX) return UI_NOTHING;
		
	UIbuttip= NULL;
	UIafterfunc= NULL;	/* to prevent infinite loops, this shouldnt be a global! */
	
	uevent.qual= G.qual;
	uevent.event= event;
	uevent.val= 1;

	/* this is a caching mechanism, to prevent too many calls to glFrontBuffer and glFinish, which slows down interface */
	block= lb->first;
	while(block) {
		block->frontbuf= UI_NEED_DRAW_FRONT;	// signal
		block= block->next;
	}

	/* main loop, needed when you click outside a looping block (menu) then it uses that
	   event to immediately evaluate the other uiBlocks again.  */
	while(cont) {
	
		/* first loop, for the normal blocks */
		block= lb->first;
		while(block) {

			/* for pupmenus, the bgnpupdraw sets (and later restores) the active
			   window. Then mousecoords get transformed OK.
			   It looks double... but a call to ui_do_block otherwise doesnt get handled properly
			 */
			if(block->flag & UI_BLOCK_REDRAW) {
				if( block->flag & UI_BLOCK_LOOP) {
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-6, (int)block->maxx+6, (int)block->maxy+1, 1);
					block->frontbuf= UI_HAS_DRAW_FRONT;
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}

			retval= ui_do_block(block, &uevent);
			if(retval==UI_EXIT_LOOP) break;
			
			/* now a new block could be created for menus, this is 
			   inserted in the beginning of a list */
			
			/* is there a glfinish cached? */
			if(block->frontbuf == UI_HAS_DRAW_FRONT) {
				glFinish();
				glDrawBuffer(GL_BACK);
				block->frontbuf= UI_NEED_DRAW_FRONT;
			}
			
			/* to make sure the matrix of the panel works for menus too */
			dopop= 1;
			if(retval==UI_CONT || (retval & UI_RETURN)) break;
			first= lb->first; if(first->flag & UI_BLOCK_LOOP) break;
			
			block= block->next;
		}
	
		/* second loop, for menus (looping blocks). works for sub->menus too */
		block= lb->first;
		if(block==NULL || (block->flag & UI_BLOCK_LOOP)==0) cont= 0;
		
		while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
			if(block->auto_open==0) block->auto_open= 1;
			
			/* this here, for menu buts */
			if(block->flag & UI_BLOCK_REDRAW) {

				if( block->flag & UI_BLOCK_LOOP) {
					block->saveunder= ui_bgnpupdraw((int)block->minx-1, (int)block->miny-6, (int)block->maxx+6, (int)block->maxy+1, 1);
					block->frontbuf= UI_HAS_DRAW_FRONT;
				}
				uiDrawBlock(block);
				block->flag &= ~UI_BLOCK_REDRAW;
			}

			/* need to reveil drawing? (not in end of loop, because of free block */
			if(block->frontbuf == UI_HAS_DRAW_FRONT) {
				glFinish();
				block->frontbuf= UI_NEED_DRAW_FRONT;
			}

			uevent.event= extern_qread(&uevent.val);
			
			if(uevent.event) {
			
				retval= ui_do_block(block, &uevent);
			
				if(retval & UI_RETURN) {
					/* free this block */
					ui_endpupdraw(block->saveunder);
					
					BLI_remlink(lb, block);
					uiFreeBlock(block);
				}
				if(retval & (UI_RETURN_OK|UI_RETURN_CANCEL)) {
					/* free other menus */
					while( (block= lb->first) && (block->flag & UI_BLOCK_LOOP)) {
						ui_endpupdraw(block->saveunder);
						BLI_remlink(lb, block);
						uiFreeBlock(block);
					}
				}
			}
			
			/* tooltip */	
			if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
				if(U.flag & TOOLTIPS) ui_do_but_tip();
			}
		}
		
		/* else it does the first part of this loop again, maybe another menu needs to be opened */
		if(retval==UI_CONT || (retval & UI_RETURN_OK)) cont= 0;
	}
	
	/* cleanup frontbuffer & flags */
	block= lb->first;
	while(block) {
		if(block->frontbuf==UI_HAS_DRAW_FRONT) glFinish();
		block->frontbuf= 0;
		block= block->next;
	}
	
	/* afterfunc is used for fileloading too, so after this call, the blocks pointers are invalid */
	if(retval & UI_RETURN_OK) {
		if(UIafterfunc) UIafterfunc(UIafterfunc_arg, UIafterval);
		UIafterfunc= NULL;
	}

	/* tooltip */	
	if(retval==UI_NOTHING && (uevent.event==MOUSEX || uevent.event==MOUSEY)) {
		if(U.flag & TOOLTIPS) ui_do_but_tip();
	}

	/* doesnt harm :-) */
	glDrawBuffer(GL_BACK);

	return retval;
}

/* ************** DATA *************** */


double ui_get_but_val(uiBut *but)
{
	void *poin;
	double value = 0.0;

	poin= but->poin;

	if(but->type== HSVSLI) {
		float h, s, v, *fp= (float *) poin;
		
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);

		switch(but->str[0]) {
			case 'H': value= h; break;
			case 'S': value= s; break;
			case 'V': value= v; break;
		}
		
	} 
	else if( but->pointype == CHA ) {
		value= *(char *)poin;
	}
	else if( but->pointype == SHO ) {
		value= *(short *)poin;
	} 
	else if( but->pointype == INT ) {
		value= *(int *)poin;		
	} 
	else if( but->pointype == FLO ) {
		value= *(float *)poin;
	}

	return value;
}

static void ui_set_but_val(uiBut *but, double value)
{
	void *poin;

	if(but->pointype==0) return;
	poin= but->poin;

	/* value is a hsv value: convert to rgb */
	if( but->type==HSVSLI ) {
		float h, s, v, *fp= (float *)but->poin;
		
		rgb_to_hsv(fp[0], fp[1], fp[2], &h, &s, &v);
		
		switch(but->str[0]) {
		case 'H': h= value; break;
		case 'S': s= value; break;
		case 'V': v= value; break;
		}
		
		hsv_to_rgb(h, s, v, fp, fp+1, fp+2);
		
	}
	else if( but->pointype==CHA )
		*((char *)poin)= (char)value;
	else if( but->pointype==SHO ) {
		/* gcc 3.2.1 seems to have problems 
		 * casting a double like 32772.0 to
		 * a short so we cast to an int, then 
		 to a short */
		int gcckludge;
		gcckludge = (int) value;
		*((short *)poin)= (short) gcckludge;
	}
	else if( but->pointype==INT )
		*((int *)poin)= (int)value;
	else if( but->pointype==FLO )
		*((float *)poin)= value;
	
	/* update select flag */
	ui_is_but_sel(but);

}

void uiSetCurFont(uiBlock *block, int index)
{
	
	ui_set_ftf_font(block);
	
	if(block->aspect<0.60) {
		block->curfont= UIfont[index].xl;
	}
	else if(block->aspect<1.15) {
		block->curfont= UIfont[index].large;
	}
	else if(block->aspect<1.59) {
		block->curfont= UIfont[index].medium;		
	}
	else {
		block->curfont= UIfont[index].small;		
	}

	if(block->curfont==NULL) block->curfont= UIfont[index].large;	
	if(block->curfont==NULL) block->curfont= UIfont[index].medium;	
	if(block->curfont==NULL) printf("error block no font %s\n", block->name);
	
}

void uiDefFont(unsigned int index, void *xl, void *large, void *medium, void *small)
{
	if(index>=UI_ARRAY) return;
	
	UIfont[index].xl= xl;
	UIfont[index].large= large;
	UIfont[index].medium= medium;
	UIfont[index].small= small;
}

static void ui_free_link(uiLink *link)
{
	if(link) {	
		BLI_freelistN(&link->lines);
		MEM_freeN(link);
	}
}

static void ui_free_but(uiBut *but)
{
	if(but->str && but->str != but->strdata) MEM_freeN(but->str);
	ui_free_link(but->link);

	MEM_freeN(but);
}

void uiFreeBlock(uiBlock *block)
{
	uiBut *but;

	if(block->flag & UI_BLOCK_BUSY) printf("attempt to free busy buttonblock: %p\n", block);	

	while( (but= block->buttons.first) ) {
		BLI_remlink(&block->buttons, but);	
		ui_free_but(but);
	}

	if(block->panel) block->panel->active= 0;

	
	MEM_freeN(block);
	UIbuttip= NULL;
}

void uiFreeBlocks(ListBase *lb)
{
	uiBlock *block;
	
	while( (block= lb->first) ) {
		BLI_remlink(lb, block);
		uiFreeBlock(block);
	}
}

void uiFreeBlocksWin(ListBase *lb, int win)
{
	uiBlock *block, *blockn;
	
	block= lb->first;
	while(block) {
		blockn= block->next;
		if(block->win==win) {
			BLI_remlink(lb, block);
			uiFreeBlock(block);
		}
		block= blockn;
	}
}

uiBlock *uiNewBlock(ListBase *lb, char *name, short dt, short font, short win)
{
	uiBlock *block;
	
	/* each listbase only has one block with this name */
	if(lb) {
		for (block= lb->first; block; block= block->next)
			if (BLI_streq(block->name, name))
				break;
		if (block) {
			BLI_remlink(lb, block);
			uiFreeBlock(block);
		}
	}
	
	block= MEM_callocN(sizeof(uiBlock), "uiBlock");
	if(lb) BLI_addhead(lb, block);		/* at the beginning of the list! for dynamical menus/blocks */

	strcpy(block->name, name);
	/* draw win */
	block->win= win;
	/* window where queue event should be added, pretty weak this way!
	   this is because the 'mainwin' pup menu's */
	block->winq= mywinget();
	block->dt= dt;
	block->themecol= TH_AUTO;
	
		/* aspect */
	bwin_getsinglematrix(win, block->winmat);

	if (win==G.curscreen->mainwin) {
		block->aspect= 1.0;
		block->auto_open= 2;
	} else {
		int getsizex, getsizey;

		bwin_getsize(win, &getsizex, &getsizey);
		block->aspect= 2.0/( (getsizex)*block->winmat[0][0]);
	}

	uiSetCurFont(block, font);

	return block;
}

uiBlock *uiGetBlock(char *name, ScrArea *sa)
{
	uiBlock *block= sa->uiblocks.first;
	
	while(block) {
		if( strcmp(name, block->name)==0 ) return block;
		block= block->next;
	}
	
	return NULL;
}


void ui_check_but(uiBut *but)
{
	/* if something changed in the button */
	ID *id;
	double value;
	float okwidth;
	short pos;
	
	ui_is_but_sel(but);
	
	
	if(but->type==NUMSLI || but->type==HSVSLI) 
		okwidth= -7 + (but->x2 - but->x1)/2.0;
	else 
		okwidth= -7 + (but->x2 - but->x1); 
	
	/* name: */
	switch( but->type ) {
	
	case MENU:
	case ICONTEXTROW:
		
		if(but->x2 - but->x1 > 24) {
			value= ui_get_but_val(but);
			ui_set_name_menu(but, (int)value);
		}
		break;
	
	case NUM:
	case NUMSLI:
	case HSVSLI:

		value= ui_get_but_val(but);

		if( but->pointype==FLO ) {
			if(but->a2) { /* amount of digits defined */
				if(but->a2==1) sprintf(but->drawstr, "%s%.1f", but->str, value);
				else if(but->a2==2) sprintf(but->drawstr, "%s%.2f", but->str, value);
				else if(but->a2==3) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.4f", but->str, value);
			}
			else {
				if(but->max<10.001) sprintf(but->drawstr, "%s%.3f", but->str, value);
				else sprintf(but->drawstr, "%s%.2f", but->str, value);
			}
		}
		else {
			sprintf(but->drawstr, "%s%d", but->str, (int)value);
		}
		break;

	case IDPOIN:
		id= *(but->idpoin_idpp);
		strcpy(but->drawstr, but->str);
		if(id) strcat(but->drawstr, id->name+2);
		break;
	
	case TEX:
		strcpy(but->drawstr, but->str);
		strcat(but->drawstr, but->poin);
		break;
	
	case KEYEVT:
		strcpy(but->drawstr, but->str);
		if (but->flag & UI_SELECT) {
			strcat(but->drawstr, "Press a key");
		} else {
			strcat(but->drawstr, key_event_to_string((short) ui_get_but_val(but)));
		}
		break;

	default:
		strcpy(but->drawstr, but->str);
		
	}

	if(but->drawstr[0]) {
		but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr, (U.transopts & TR_BUTTONS));
		// here should be check for less space for icon offsets...
		if(but->type==MENU) okwidth -= 20;
	}
	else
		but->strwidth= 0;

		/* automatic width */
	if(but->x2==0.0) {
		but->x2= (but->x1+but->strwidth+6); 
	}

	if(but->strwidth==0) but->drawstr[0]= 0;
	else if(but->type==BUTM);	// clip string
	else {

		/* calc but->ofs, to draw the string shorter if too long */
		but->ofs= 0;
		while(but->strwidth > (int)okwidth ) {
			but->ofs++;
	
			if(but->drawstr[but->ofs]) 
				but->strwidth= but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & TR_BUTTONS));
			else but->strwidth= 0;
	
			/* textbut exception */
			if(but->pos != -1) {
				pos= but->pos+strlen(but->str);
				if(pos-1 < but->ofs) {
					pos= but->ofs-pos+1;
					but->ofs -= pos;
					if(but->ofs<0) {
						but->ofs= 0;
						pos--;
					}
					but->drawstr[ strlen(but->drawstr)-pos ]= 0;
				}
			}
			
			if(but->strwidth < 10) break;
		}
		
		/* fix for buttons that better not have text cut off to the right */
		if(but->ofs) {
			if ELEM(but->type, NUM, TEX);	// only these cut off left
			else {
				but->drawstr[ strlen(but->drawstr)-but->ofs ]= 0;
				but->ofs= 0;
			}
		}
	}
	
	/* test for min and max, icon sliders, etc */
	
	switch( but->type ) {
	case NUM:
	case SLI:
	case SCROLL:
	case NUMSLI:
	case HSVSLI:
		value= ui_get_but_val(but);
		if(value < but->min) value= but->min;
		if(value > but->max) value= but->max;
		ui_set_but_val(but, value);
		break;
		
	case ICONTOG: 
		if(but->flag & UI_SELECT) but->iconadd= 1;
		else but->iconadd= 0;
		break;
		
	case ICONROW:
		value= ui_get_but_val(but);
		but->iconadd= (int)value- (int)(but->min);
		break;

	case ICONTEXTROW:
		value= ui_get_but_val(but);
		but->iconadd= (int)value- (int)(but->min);
		break;
	}
}

static int ui_auto_themecol(uiBut *but)
{

	switch(but->type) {
	case BUT:
		return TH_BUT_ACTION;
	case ROW:
	case TOG:
	case TOG3:
	case TOGR:
	case TOGN:
		return TH_BUT_SETTING;
	case SLI:
	case NUM:
	case NUMSLI:
	case HSVSLI:
		return TH_BUT_NUM;
	case TEX:
		return TH_BUT_TEXTFIELD;
	case BLOCK:
	case MENU:
	case BUTM:
		// (weak!) detect if it is a blockloop
		if(UIbuttip) return TH_MENU_ITEM;
		return TH_BUT_POPUP;
	default:
		return TH_BUT_NEUTRAL;
	}
}

static uiBut *ui_def_but(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but;
	short slen;
	
	if(type & BUTPOIN) {		/* a pointer is required */
		if(poin==0) {
				/* if pointer is zero, button is removed and not drawn */
			BIF_ThemeColor(block->themecol);
			glRects(x1,  y1,  x1+x2,  y1+y2);
			return NULL;
		}
	}

	but= MEM_callocN(sizeof(uiBut), "uiBut");

	but->type= type & BUTTYPE;
	but->pointype= type & BUTPOIN;
	but->bit= type & BIT;
	but->bitnr= type & 31;

	BLI_addtail(&block->buttons, but);

	but->retval= retval;
	if( strlen(str)>=UI_MAX_NAME_STR-1 ) {
		but->str= MEM_callocN( strlen(str)+2, "uiDefBut");
		strcpy(but->str, str);
	}
	else {
		but->str= but->strdata;
		strcpy(but->str, str);
	}
	but->x1= x1; 
	but->y1= y1;
	if(block->autofill) {
		but->x2= x2; 
		but->y2= y2;
	}
	else {
		but->x2= (x1+x2); 
		but->y2= (y1+y2);
	}
	but->poin= poin;
	but->min= min; 
	but->max= max;
	but->a1= a1; 
	but->a2= a2;
	but->tip= tip;
	
	but->font= block->curfont;
	
	but->lock= UIlock;
	but->lockstr= UIlockstr;

	but->aspect= block->aspect;
	but->win= block->win;
	but->block= block;		// pointer back, used for frontbuffer status

	if(block->themecol==TH_AUTO) but->themecol= ui_auto_themecol(but);
	else but->themecol= block->themecol;
	
	if (but->type==BUTM) {
		but->butm_func= block->butm_func;
		but->butm_func_arg= block->butm_func_arg;
	} else {
		but->func= block->func;
		but->func_arg1= block->func_arg1;
		but->func_arg2= block->func_arg2;
	}

	ui_set_embossfunc(but, block->dt);
	
	but->pos= -1;	/* cursor invisible */

	if(but->type==NUM) {	/* add a space to name */
		slen= strlen(but->str);
		if(slen>0 && slen<UI_MAX_NAME_STR-2) {
			if(but->str[slen-1]!=' ') {
				but->str[slen]= ' ';
				but->str[slen+1]= 0;
			}
		}
	}
	
	if ELEM8(but->type, HSVSLI , NUMSLI, MENU, TEX, LABEL, IDPOIN, BLOCK, BUTM) {
		but->flag |= UI_TEXT_LEFT;
	}
	
	return but;
}

uiBut *uiDefBut(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	ui_check_but(but);
	
	return but;
}
uiBut *uiDefButF(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|FLO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButI(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|INT, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButS(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|SHO, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefButC(uiBlock *block, int type, int retval, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefBut(block, type|CHA, retval, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

uiBut *uiDefIconBut(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, "", x1, y1, x2, y2, poin, min, max, a1, a2, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	ui_check_but(but);
	
	return but;
}

uiBut *uiDefIconButF(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|FLO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButI(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|INT, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButS(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|SHO, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconButC(uiBlock *block, int type, int retval, int icon, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconBut(block, type|CHA, retval, icon, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}

/* Button containing both string label and icon */
uiBut *uiDefIconTextBut(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, void *poin, float min, float max, float a1, float a2,  char *tip)
{
	uiBut *but= ui_def_but(block, type, retval, str, x1, y1, x2, y2, poin, min, max, a1, a2, tip);

	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;

	ui_check_but(but);

	return but;
}

uiBut *uiDefIconTextButF(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, float *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|FLO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButI(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, int *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|INT, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButS(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, short *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|SHO, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
uiBut *uiDefIconTextButC(uiBlock *block, int type, int retval, int icon, char *str, short x1, short y1, short x2, short y2, char *poin, float min, float max, float a1, float a2,  char *tip)
{
	return uiDefIconTextBut(block, type|CHA, retval, icon, str, x1, y1, x2, y2, (void*) poin, min, max, a1, a2, tip);
}
/* END Button containing both string label and icon */

void uiAutoBlock(uiBlock *block, float minx, float miny, float sizex, float sizey, int flag)
{
	block->minx= minx;
	block->maxx= minx+sizex;
	block->miny= miny;
	block->maxy= miny+sizey;
	
	block->autofill= flag;	/* also check for if it has to be done */

}

void uiSetButLink(uiBut *but, void **poin, void ***ppoin, short *tot, int from, int to)
{
	uiLink *link;
	
	link= but->link= MEM_callocN(sizeof(uiLink), "new uilink");
	
	link->poin= poin;
	link->ppoin= ppoin;
	link->totlink= tot;
	link->fromcode= from;
	link->tocode= to;
}

/* cruft to make uiBlock and uiBut private */

int uiBlocksGetYMin(ListBase *lb)
{
	uiBlock *block;
	int min= 0;
	
	for (block= lb->first; block; block= block->next)
		if (block==lb->first || block->miny<min)
			min= block->miny;
			
	return min;
}

int uiBlockGetCol(uiBlock *block)
{
	return block->themecol;
}
void uiBlockSetCol(uiBlock *block, int col)
{
	block->themecol= col;
}
void uiBlockSetEmboss(uiBlock *block, int emboss)
{
	block->dt= emboss;
}
void uiBlockSetDirection(uiBlock *block, int direction)
{
	block->direction= direction;
}
void uiBlockFlipOrder(uiBlock *block)
{
	uiBut *but;
	float centy, miny=10000, maxy= -10000;

	for(but= block->buttons.first; but; but= but->next) {
		if(but->y1 < miny) miny= but->y1;
		if(but->y2 > maxy) maxy= but->y2;
	}
	/* mirror trick */
	centy= (miny+maxy)/2.0;
	for(but= block->buttons.first; but; but= but->next) {
		but->y1 = centy-(but->y1-centy);
		but->y2 = centy-(but->y2-centy);
		SWAP(float, but->y1, but->y2);
	}
}


void uiBlockSetFlag(uiBlock *block, int flag)
{
	block->flag= flag;
}
void uiBlockSetXOfs(uiBlock *block, int xofs)
{
	block->xofs= xofs;
}
void* uiBlockGetCurFont(uiBlock *block)
{
	return block->curfont;
}

void uiButSetFlag(uiBut *but, int flag)
{
	but->flag|= flag;
}
void uiButClearFlag(uiBut *but, int flag)
{
	but->flag&= ~flag;
}

int uiButGetRetVal(uiBut *but)
{
	return but->retval;
}


void uiBlockSetButmFunc(uiBlock *block, void (*menufunc)(void *arg, int event), void *arg)
{
	block->butm_func= menufunc;
	block->butm_func_arg= arg;
}

void uiBlockSetFunc(uiBlock *block, void (*func)(void *arg1, void *arg2), void *arg1, void *arg2)
{
	block->func= func;
	block->func_arg1= arg1;
	block->func_arg2= arg2;
}

void uiBlockSetDrawExtraFunc(uiBlock *block, void (*func)())
{
	block->drawextra= func;
}

void uiButSetFunc(uiBut *but, void (*func)(void *arg1, void *arg2), void *arg1, void *arg2)
{
	but->func= func;
	but->func_arg1= arg1;
	but->func_arg2= arg2;
}

void uiDefIDPoinBut(uiBlock *block, uiIDPoinFuncFP func, int retval, char *str, short x1, short y1, short x2, short y2, void *idpp, char *tip)
{
	uiBut *but= ui_def_but(block, IDPOIN, retval, str, x1, y1, x2, y2, NULL, 0.0, 0.0, 0.0, 0.0, tip);
	but->idpoin_func= func;
	but->idpoin_idpp= (ID**) idpp;
	ui_check_but(but);
}

uiBut *uiDefBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	but->block_func= func;
	ui_check_but(but);
	return but;
}

/* Block button containing both string label and icon */
uiBut *uiDefIconTextBlockBut(uiBlock *block, uiBlockFuncFP func, void *arg, int icon, char *str, short x1, short y1, short x2, short y2, char *tip)
{
	uiBut *but= ui_def_but(block, BLOCK, 0, str, x1, y1, x2, y2, arg, 0.0, 0.0, 0.0, 0.0, tip);
	
	but->icon= (BIFIconID) icon;
	but->flag|= UI_HAS_ICON;

	but->flag|= UI_ICON_LEFT;
	but->flag|= UI_ICON_RIGHT;

	but->block_func= func;
	ui_check_but(but);
	
	return but;
}

void uiDefKeyevtButS(uiBlock *block, int retval, char *str, short x1, short y1, short x2, short y2, short *spoin, char *tip)
{
	uiBut *but= ui_def_but(block, KEYEVT|SHO, retval, str, x1, y1, x2, y2, spoin, 0.0, 0.0, 0.0, 0.0, tip);
	ui_check_but(but);
}

/* ******************** PUPmenu ****************** */

short pupmenu(char *instr)
{
	uiBlock *block;
	ListBase listb= {NULL, NULL};
	int event;
	static int lastselected= 0;
	short width, height=0, mousexmove = 0, mouseymove, xmax, ymax, mval[2], val= -1;
	short a, startx, starty, endx, endy, boxh=TBOXH, x1, y1;
	static char laststring[UI_MAX_NAME_STR];
	MenuData *md;

	/* block stuff first, need to know the font */
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(instr);

	/* size and location, title slightly bigger for bold */
	if(md->title) width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts && TR_BUTTONS));
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts && TR_BUTTONS));
		if(xmax>width) width= xmax;

		if( strcmp(name, "%l")==0) height+= 6;
		else height+= boxh;
	}

	width+= 10;
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

	getmouseco_sc(mval);
	
	if(strncmp(laststring, instr, UI_MAX_NAME_STR-1)!=0) lastselected= 0;
	BLI_strncpy(laststring, instr, UI_MAX_NAME_STR);
	
	startx= mval[0]-width/2;
	if(lastselected>=0 && lastselected<md->nitems) {
		starty= mval[1]-height+boxh/2+lastselected*boxh;
	}
	else starty= mval[1]-height/2;
	
	mouseymove= 0;
	
	if(startx<10) startx= 10;
	if(starty<10) {
		mouseymove= 10-starty;
		starty= 10;
	}
	
	endx= startx+width;
	endy= starty+height;
	if(endx>xmax) {
		endx= xmax-10;
		startx= endx-width;
	}
	if(endy>ymax-20) {
		mouseymove= ymax-endy-20;
		endy= ymax-20;
		starty= endy-height;
		
	}

	if(mouseymove) {
		ui_warp_pointer(mval[0], mouseymove+mval[1]);
		mousexmove= mval[0];
		mouseymove= mval[1];
	}
	
	/* here we go! */	
	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, UI_HELVB);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+md->nitems*boxh), width, boxh, NULL, 0.0, 0.0, 0, 0, "");
		bt->flag= UI_TEXT_LEFT;
		uiSetCurFont(block, UI_HELV);
	}

	y1= starty + boxh*(md->nitems-1);
	x1= startx;
	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		if( strcmp(name, "%l")==0) {
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, 6, NULL, 0, 0.0, 0, 0, "");
			y1 -= 6;
		}
		else {
			uiDefButS(block, BUTM, B_NOP, name, x1, y1, width, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= boxh;
		}
	}
	
	uiBoundsBlock(block, 2);

	event= uiDoBlocks(&listb, 0);

	/* calculate last selected */
	lastselected= 0;
	for(a=0; a<md->nitems; a++) {
		if(val==md->items[a].retval) lastselected= a;
	}

	menudata_free(md);
	
	if(mouseymove && (event & UI_RETURN_OUT)==0) ui_warp_pointer(mousexmove, mouseymove);
	return val;
}

short pupmenu_col(char *instr, int maxrow)
{
	uiBlock *block;
	ListBase listb= {NULL, NULL};
	int	columns, rows;
	short	mousemove[2], mval[2], event;
	int width, height, xmax, ymax, val= -1;
	int a, startx, starty, endx, endy, boxh=TBOXH, x1, y1;
	MenuData *md;
	
	block= uiNewBlock(&listb, "menu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_NUMSELECT);
	block->themecol= TH_MENU_ITEM;
	
	md= decompose_menu_string(instr);

	/* collumns and row calculation */
	columns= (md->nitems+maxrow)/maxrow;
	if (columns<1) columns= 1;
	
	rows= (int) md->nitems/columns;
	if (rows<1) rows= 1;
	
	while (rows*columns<(md->nitems+columns) ) rows++;
		
	/* size and location */
	if(md->title) width= 2*strlen(md->title)+BIF_GetStringWidth(uiBlockGetCurFont(block), md->title, (U.transopts & TR_BUTTONS));
	else width= 0;
	for(a=0; a<md->nitems; a++) {
		xmax= BIF_GetStringWidth(uiBlockGetCurFont(block), md->items[a].str, (U.transopts & TR_BUTTONS));
		if(xmax>width) width= xmax;
	}

	width+= 10;
	if (width<50) width=50;

	boxh= TBOXH;
	
	height= rows*boxh;
	if (md->title) height+= boxh;
	
	xmax = G.curscreen->sizex;
	ymax = G.curscreen->sizey;

	getmouseco_sc(mval);
	
	/* find active item */
#if 0
	fvalue= ui_get_but_val(but);
	for(a=0; a<md->nitems; a++) {
		if( md->items[a].retval== (int)fvalue ) break;
	}
#endif
	/* no active item? */
	if(a==md->nitems) {
		if(md->title) a= -1;
		else a= 0;
	}

	if(a>0)
		startx = mval[0]-width/2 - ((int)(a)/rows)*width;
	else
		startx= mval[0]-width/2;
	starty = mval[1]-height + boxh/2 + ((a)%rows)*boxh;

	if (md->title) starty+= boxh;
	
	mousemove[0]= mousemove[1]= 0;
	
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

	ui_warp_pointer(mval[0]+mousemove[0], mval[1]+mousemove[1]);

	mousemove[0]= mval[0];
	mousemove[1]= mval[1];

	/* here we go! */

	if(md->title) {
		uiBut *bt;
		uiSetCurFont(block, UI_HELVB);
		bt= uiDefBut(block, LABEL, 0, md->title, startx, (short)(starty+rows*boxh), (short)width, (short)boxh, NULL, 0.0, 0.0, 0, 0, "");
		uiSetCurFont(block, UI_HELV);
		bt->flag= UI_TEXT_LEFT;
	}

	for(a=0; a<md->nitems; a++) {
		char *name= md->items[a].str;
		
		x1= startx + width*((int)a/rows);
		y1= starty - boxh*(a%rows) + (rows-1)*boxh; 
		
		if( strcmp(name, "%l")==0){
			uiDefBut(block, SEPR, B_NOP, "", x1, y1, width, 6, NULL, 0, 0.0, 0, 0, "");
			y1 -= 6;
		}
		else {
			uiDefButS(block, BUTM, B_NOP, name, x1, y1, width, boxh-1, &val, (float) md->items[a].retval, 0.0, 0, 0, "");
			y1 -= boxh;
		}
		//uiDefButI(block, BUTM, B_NOP, md->items[a].str, x1, y1, (short)(width-(rows>1)), (short)(boxh-1), &val, (float)md->items[a].retval, 0.0, 0, 0, "");
	}
	
	uiBoundsBlock(block, 3);

	event= uiDoBlocks(&listb, 0);
	
	menudata_free(md);
	
	if((event & UI_RETURN_OUT)==0) ui_warp_pointer(mousemove[0], mousemove[1]);
	
	return val;	
}

