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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_area.h"
#include "ED_screen.h"
#include "ED_screen_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_subwindow.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BPY_extern.h"

#include "screen_intern.h"

/* general area and region code */

static void region_draw_emboss(ARegion *ar)
{
	short winx, winy;
	
	winx= ar->winrct.xmax-ar->winrct.xmin;
	winy= ar->winrct.ymax-ar->winrct.ymin;
	
	/* set transp line */
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	/* right  */
	glColor4ub(0,0,0, 50);
	sdrawline(winx, 0, winx, winy);
	
	/* bottom  */
	glColor4ub(0,0,0, 80);
	sdrawline(0, 0, winx, 0);
	
	/* top  */
	glColor4ub(255,255,255, 60);
	sdrawline(0, winy, winx, winy);

	/* left  */
	glColor4ub(255,255,255, 50);
	sdrawline(0, 0, 0, winy);
	
	glDisable( GL_BLEND );
}


void ED_region_do_listen(ARegion *ar, wmNotifier *note)
{
	
	/* generic notes first */
	switch(note->type) {
		case WM_NOTE_WINDOW_REDRAW:
			ar->do_draw= 1;
			break;
		case WM_NOTE_SCREEN_CHANGED:
			ar->do_draw= ar->do_refresh= 1;
			break;
		default:
			if(ar->type->listener)
				ar->type->listener(ar, note);
	}
}

void ED_region_do_draw(bContext *C, ARegion *ar)
{
	ARegionType *at= ar->type;
	
	wm_subwindow_set(C->window, ar->swinid);
	
	if(ar->swinid && at->draw) {
		at->draw(C, ar);
	}
	else {
		float fac= 0.1*ar->swinid;
		
		glClearColor(0.5, fac, 1.0f-fac, 0.0); 
		glClear(GL_COLOR_BUFFER_BIT);
		
		fac= BLI_frand();
		glColor3f(fac, fac, fac);
		glRecti(2,  2,  12,  12);
		
		region_draw_emboss(ar);
	}
	
	ar->do_draw= 0;
}

void ED_region_do_refresh(bContext *C, ARegion *ar)
{
	ARegionType *at= ar->type;

	/* refresh can be called before window opened */
	if(ar->swinid)
		wm_subwindow_set(C->window, ar->swinid);
	
	if (at->refresh) {
		at->refresh(C, ar);
	}
	
	ar->do_refresh= 0;
}

/* *************************************************************** */


static int rct_fits(rcti *rect, char dir, int size)
{
	if(dir=='h') {
		return rect->xmax-rect->xmin - size;
	}
	else { // 'v'
		return rect->ymax-rect->ymin - size;
	}
}

static void region_rect_recursive(ARegion *ar, rcti *remainder)
{
	if(ar==NULL)
		return;
	
	/* clear state flag first */
	ar->flag &= ~RGN_FLAG_TOO_SMALL;
	
	if(ar->size<ar->minsize)
		ar->size= ar->minsize;
	
	/* hidden is user flag */
	if(ar->flag & RGN_FLAG_HIDDEN);
	/* remainder is too small for any usage */
	else if( rct_fits(remainder, 'v', 1)==0 || rct_fits(remainder, 'h', 1) < 0 ) {
		ar->flag |= RGN_FLAG_TOO_SMALL;
	}
	else if(ar->alignment==RGN_ALIGN_NONE) {
		/* typically last region */
		ar->winrct= *remainder;
		BLI_init_rcti(remainder, 0, 0, 0, 0);
	}
	else if(ar->alignment==RGN_ALIGN_TOP || ar->alignment==RGN_ALIGN_BOTTOM) {
		
		if( rct_fits(remainder, 'v', ar->minsize) < 0 ) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac= rct_fits(remainder, 'v', ar->size);
			
			if(fac < 0 )
				ar->size += fac;
			
			ar->winrct= *remainder;
			
			if(ar->alignment==RGN_ALIGN_TOP) {
				ar->winrct.ymin= ar->winrct.ymax - ar->size;
				remainder->ymax= ar->winrct.ymin-1;
			}
			else {
				ar->winrct.ymax= ar->winrct.ymin + ar->size;
				remainder->ymin= ar->winrct.ymax+1;
			}
		}
	}
	else if(ar->alignment==RGN_ALIGN_LEFT || ar->alignment==RGN_ALIGN_RIGHT) {
		
		if( rct_fits(remainder, 'h', ar->minsize) < 0 ) {
			ar->flag |= RGN_FLAG_TOO_SMALL;
		}
		else {
			int fac= rct_fits(remainder, 'h', ar->size);
			
			if(fac < 0 )
				ar->size += fac;
			
			ar->winrct= *remainder;
			
			if(ar->alignment==RGN_ALIGN_RIGHT) {
				ar->winrct.xmin= ar->winrct.xmax - ar->size;
				remainder->xmax= ar->winrct.xmin-1;
			}
			else {
				ar->winrct.xmax= ar->winrct.xmin + ar->size;
				remainder->xmin= ar->winrct.xmax+1;
			}
		}
	}
	else {
		/* percentage subdiv*/
		ar->winrct= *remainder;
		
		if(ar->alignment==RGN_ALIGN_HSPLIT) {
			ar->winrct.xmax= (remainder->xmin+remainder->xmax)/2;
			remainder->xmin= ar->winrct.xmax+1;
		}
		else {
			ar->winrct.ymax= (remainder->ymin+remainder->ymax)/2;
			remainder->ymin= ar->winrct.ymax+1;
		}
	}
	
	region_rect_recursive(ar->next, remainder);
}

static void area_calc_totrct(ScrArea *sa, int sizex, int sizey)
{
	
	if(sa->v1->vec.x>0) sa->totrct.xmin= sa->v1->vec.x+1;
	else sa->totrct.xmin= sa->v1->vec.x;
	if(sa->v4->vec.x<sizex-1) sa->totrct.xmax= sa->v4->vec.x-1;
	else sa->totrct.xmax= sa->v4->vec.x;
	
	if(sa->v1->vec.y>0) sa->totrct.ymin= sa->v1->vec.y+1;
	else sa->totrct.ymin= sa->v1->vec.y;
	if(sa->v2->vec.y<sizey-1) sa->totrct.ymax= sa->v2->vec.y-1;
	else sa->totrct.ymax= sa->v2->vec.y;
	
	/* for speedup */
	sa->winx= sa->totrct.xmax-sa->totrct.xmin+1;
	sa->winy= sa->totrct.ymax-sa->totrct.ymin+1;
}

#define AZONESPOT		12
void area_azone_initialize(ScrArea *sa) {
	AZone *az;
	if(sa->actionzones.first==NULL) {
		printf("area_azone_initialize\n");
		/* set action zones - should these actually be ARegions? With these we can easier check area hotzones */
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&(sa->actionzones), az);
		az->type= AZONE_TRI;
		az->x1= sa->v1->vec.x+1;
		az->y1= sa->v1->vec.y+1;
		az->x2= sa->v1->vec.x+AZONESPOT;
		az->y2= sa->v1->vec.y+AZONESPOT;
		az->pos= AZONE_SW;
		az->action= AZONE_SPLIT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&(sa->actionzones), az);
		az->type= AZONE_TRI;
		az->x1= sa->v3->vec.x-1;
		az->y1= sa->v3->vec.y-1;
		az->x2= sa->v3->vec.x-AZONESPOT;
		az->y2= sa->v3->vec.y-AZONESPOT;
		az->pos= AZONE_NE;
		az->action= AZONE_DRAG;
		
		/*az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_TRI;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_TRI;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_QUAD;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_QUAD;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_QUAD;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;
		
		az= (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
		BLI_addtail(&sa->azones, az);
		az->type= AZONE_QUAD;
		az->x1= as->v1->vec.x;
		az->y1= as->v1->vec.y;
		az->x2= as->v1->vec.x+AZONESPOT;
		az->y2= as->v1->vec.y+AZONESPOT;*/
	}
	
	for(az= sa->actionzones.first; az; az= az->next) {
		if(az->pos==AZONE_SW) {
			az->x1= sa->v1->vec.x+1;
			az->y1= sa->v1->vec.y+1;
			az->x2= sa->v1->vec.x+AZONESPOT;
			az->y2= sa->v1->vec.y+AZONESPOT;
		} else if (az->pos==AZONE_NE) {
			az->x1= sa->v3->vec.x-1;
			az->y1= sa->v3->vec.y-1;
			az->x2= sa->v3->vec.x-AZONESPOT;
			az->y2= sa->v3->vec.y-AZONESPOT;
		}
	}
}

/* called in screen_refresh, or screens_init */
void ED_area_initialize(wmWindowManager *wm, wmWindow *win, ScrArea *sa)
{
	ARegion *ar;
	rcti rect;
	
	/* set typedefinitions */
	sa->type= BKE_spacetype_from_id(sa->spacetype);
	if(sa->type==NULL) {
		sa->spacetype= SPACE_VIEW3D;
		sa->type= BKE_spacetype_from_id(sa->spacetype);
	}
	
	area_calc_totrct(sa, win->sizex, win->sizey);
	
	/* regiontype callback, it should create/verify the amount of subregions with minsizes etc */
	if(sa->type->init)
		sa->type->init(sa);
	
	/* region rect sizes */
	rect= sa->totrct;
	region_rect_recursive(sa->regionbase.first, &rect);
	
	/* region windows */
	for(ar= sa->regionbase.first; ar; ar= ar->next) {
		if(ar->flag & (RGN_FLAG_HIDDEN|RGN_FLAG_TOO_SMALL)) {
			if(ar->swinid)
				wm_subwindow_close(win, ar->swinid);
			ar->swinid= 0;
		}
		else if(ar->swinid==0)
			ar->swinid= wm_subwindow_open(win, &ar->winrct);
		else 
			wm_subwindow_position(win, ar->swinid, &ar->winrct);
	}
	
	area_azone_initialize(sa);
}

/* sa2 to sa1, we swap spaces for fullscreen to keep all allocated data */
/* area vertices were set */

void area_copy_data(ScrArea *sa1, ScrArea *sa2, int swap_space)
{
	Panel *pa1, *pa2, *patab;
	ARegion *ar;
	AZone *az;
	
	sa1->headertype= sa2->headertype;
	sa1->spacetype= sa2->spacetype;
	
	if(swap_space) {
		SWAP(ListBase, sa1->spacedata, sa2->spacedata);
		/* exception: ensure preview is reset */
//		if(sa1->spacetype==SPACE_VIEW3D)
// XXX			BIF_view3d_previewrender_free(sa1->spacedata.first);
	}
	else {
		BKE_spacedata_freelist(&sa1->spacedata);
		BKE_spacedata_copylist(&sa1->spacedata, &sa2->spacedata);
	}
	
	BLI_freelistN(&sa1->panels);
	BLI_duplicatelist(&sa1->panels, &sa2->panels);
	
	/* copy panel pointers */
	for(pa1= sa1->panels.first; pa1; pa1= pa1->next) {
		
		patab= sa1->panels.first;
		pa2= sa2->panels.first;
		while(patab) {
			if( pa1->paneltab == pa2) {
				pa1->paneltab = patab;
				break;
			}
			patab= patab->next;
			pa2= pa2->next;
		}
	}
	
	/* regions */
	BLI_freelistN(&sa1->regionbase);
	BLI_duplicatelist(&sa1->regionbase, &sa2->regionbase);
	for(ar= sa1->regionbase.first; ar; ar= ar->next)
		ar->swinid= 0;
		
	/* scripts */
	BPY_free_scriptlink(&sa1->scriptlink);
	sa1->scriptlink= sa2->scriptlink;
	BPY_copy_scriptlink(&sa1->scriptlink);	/* copies internal pointers */
	
}


