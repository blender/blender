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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#pragma warning (once : 4761)
#endif

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"
#include "BLI_storage_types.h"
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_icons.h"
#include "BKE_utildefines.h"
#include "BIF_filelist.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_resources.h"
#include "BIF_language.h"

#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_previewrender.h"
#include "BIF_fsmenu.h"
#include "BIF_space.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BSE_drawimasel.h"
#include "BSE_drawipo.h" /* for v2d functions */ 
#include "BSE_view.h"

#include "BLO_readfile.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "PIL_time.h"

#include "blendef.h"
#include "mydevice.h"

#include "interface.h"	/* urm...  for rasterpos_safe, roundbox */

#define BUTTONWIDTH 20
#define BOOKMARKWIDTH_MAX 240

void calc_imasel_rcts(SpaceImaSel *simasel, int winx, int winy)
{	
	int width = (int)16.0f*simasel->aspect;
	int numtiles;
	int numfiles = 0;
	int tilewidth = simasel->prv_w + TILE_BORDER_X*4;
	int tileheight = simasel->prv_h + TILE_BORDER_Y*4 + U.fontsize;

	// complete area of the space
	simasel->v2d.mask.xmin= simasel->v2d.mask.ymin = 0;
	simasel->v2d.mask.xmax= winx;
	simasel->v2d.mask.ymax= winy;

	// vertical scroll bar
	simasel->v2d.vert= simasel->v2d.mask;
	simasel->v2d.vert.xmax -= TILE_BORDER_X + 2;
	simasel->v2d.vert.xmin= simasel->v2d.vert.xmax- width - TILE_BORDER_X - 2;
	simasel->v2d.vert.ymax -= IMASEL_BUTTONS_HEIGHT + TILE_BORDER_Y + 2;
	simasel->v2d.vert.ymin += TILE_BORDER_Y + 2;
	// simasel->v2d.mask.xmax= simasel->v2d.vert.xmin;
	
	if ((simasel->flag & FILE_BOOKMARKS) && (simasel->type != FILE_MAIN)) {
		int bmwidth = (simasel->v2d.vert.xmin - simasel->v2d.mask.xmin)/4.0f;
		if (bmwidth > BOOKMARKWIDTH_MAX) bmwidth = BOOKMARKWIDTH_MAX;

		simasel->bookmarkrect.xmin = simasel->v2d.mask.xmin + TILE_BORDER_X;
		simasel->bookmarkrect.xmax = simasel->v2d.mask.xmin + bmwidth - TILE_BORDER_X;
		simasel->bookmarkrect.ymax = simasel->v2d.mask.ymax - IMASEL_BUTTONS_HEIGHT - TILE_BORDER_Y;	
		simasel->bookmarkrect.ymin = simasel->v2d.mask.ymin + TILE_BORDER_Y;

		simasel->viewrect.xmin = simasel->bookmarkrect.xmax + TILE_BORDER_X;
		simasel->viewrect.xmax = simasel->v2d.vert.xmin - TILE_BORDER_X;
		simasel->viewrect.ymax = simasel->v2d.mask.ymax - IMASEL_BUTTONS_HEIGHT - TILE_BORDER_Y;	
		simasel->viewrect.ymin = simasel->v2d.mask.ymin + TILE_BORDER_Y;	
	} else {
		simasel->viewrect.xmin = simasel->v2d.mask.xmin + TILE_BORDER_X;
		simasel->viewrect.xmax = simasel->v2d.vert.xmin - TILE_BORDER_X;
		simasel->viewrect.ymax = simasel->v2d.mask.ymax - IMASEL_BUTTONS_HEIGHT - TILE_BORDER_Y;	
		simasel->viewrect.ymin = simasel->v2d.mask.ymin + TILE_BORDER_Y;		
	}

	simasel->numtilesx = (simasel->viewrect.xmax - simasel->viewrect.xmin) / tilewidth;
	simasel->numtilesy = (simasel->viewrect.ymax - simasel->viewrect.ymin) / tileheight;
	numtiles = simasel->numtilesx*simasel->numtilesy;

	if (simasel->files) {
		numfiles = BIF_filelist_numfiles(simasel->files);
	}
	if (numtiles > numfiles) numtiles = numfiles;

	simasel->scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
	if (numtiles < numfiles) {
		simasel->scrollheight = ((float)numtiles / (float)numfiles)*simasel->scrollarea;
		simasel->scrollarea -= simasel->scrollheight;	
	} else {
		simasel->scrollheight = simasel->scrollarea;
	}
	if (simasel->scrollarea < 0) simasel->scrollarea = 0;
}

void draw_imasel_scroll(SpaceImaSel *simasel)
{
	rcti scrollbar;
	rcti scrollhandle;

	scrollbar.xmin= simasel->v2d.cur.xmin + simasel->v2d.vert.xmin;		
	scrollbar.ymin = simasel->v2d.cur.ymin + simasel->v2d.vert.ymin;
	scrollbar.xmax= simasel->v2d.cur.xmin + simasel->v2d.vert.xmax;
	scrollbar.ymax = simasel->v2d.cur.ymin + simasel->v2d.vert.ymax;

	scrollhandle.xmin= scrollbar.xmin;		
	scrollhandle.ymin = scrollbar.ymax - simasel->scrollpos -1;
	scrollhandle.xmax= scrollbar.xmax-1;
	scrollhandle.ymax = scrollbar.ymax - simasel->scrollpos - simasel->scrollheight;

	BIF_ThemeColor(TH_SHADE1);
	glRecti(scrollbar.xmin,  scrollbar.ymin, scrollbar.xmax, scrollbar.ymax);
	uiEmboss(scrollbar.xmin-2,  scrollbar.ymin-2, scrollbar.xmax+2, scrollbar.ymax+2, 1);

	BIF_ThemeColor(TH_SHADE2);
	glRecti(scrollhandle.xmin,  scrollhandle.ymin,  scrollhandle.xmax,  scrollhandle.ymax);
	
	uiEmboss(scrollhandle.xmin, scrollhandle.ymin, scrollhandle.xmax, scrollhandle.ymax, 1); 
}

static void draw_tile(SpaceImaSel *simasel, short sx, short sy, int colorid)
{
	/* TODO: BIF_ThemeColor seems to need this to show the color, not sure why? - elubie */
	glEnable(GL_BLEND);
	glColor4ub(0, 0, 0, 100);
	glDisable(GL_BLEND);
	
	BIF_ThemeColor4(colorid);
	uiSetRoundBox(15);	
	uiRoundBox(sx+TILE_BORDER_X, sy - simasel->prv_h - TILE_BORDER_Y*3 - U.fontsize, sx + simasel->prv_w + TILE_BORDER_X*3, sy, 6);
}

static float shorten_string(SpaceImaSel *simasel, char* string, float w)
{	
	short shortened = 0;
	float sw = 0;
	
	sw = BIF_GetStringWidth(simasel->curfont, string, 0);
	while (sw>w) {
		int slen = strlen(string);
		string[slen-1] = '\0';
		sw = BIF_GetStringWidth(simasel->curfont, string, 0);
		shortened = 1;
	}
	if (shortened) {
		int slen = strlen(string);
		if (slen > 3) {
			BLI_strncpy(string+slen-3, "...", 4);				
		}
	}
	return sw;
}

static void draw_file(SpaceImaSel *simasel, short sx, short sy, struct direntry *file)
{
	short soffs;
	char fname[FILE_MAXFILE];
	float sw;
	float x,y;

	BLI_strncpy(fname,file->relname, FILE_MAXFILE);
	sw = shorten_string(simasel, fname, simasel->prv_w );
	soffs = (simasel->prv_w + TILE_BORDER_X*4 - sw) / 2;	
	x = (float)(sx+soffs);
	y = (float)(sy - simasel->prv_h - TILE_BORDER_Y*2 - U.fontsize);

	ui_rasterpos_safe(x, y, simasel->aspect);
	/* handling of international fonts.
	    TODO: proper support for utf8 in languages different from ja_JP abd zh_CH
	    needs update of iconv in lib/windows to support getting the system language string
	*/
	#ifdef WITH_ICONV
		{
			struct LANGMenuEntry *lme;
       		lme = find_language(U.language);

			if ((lme !=NULL) && (!strcmp(lme->code, "ja_JP") || 
				!strcmp(lme->code, "zh_CN")))
			{
				BIF_RasterPos(x, y);
#ifdef WIN32
				BIF_DrawString(simasel->curfont, fname, ((U.transopts & USER_TR_MENUS) | CONVERT_TO_UTF8));
#else
				BIF_DrawString(simasel->curfont, fname, (U.transopts & USER_TR_MENUS));
#endif
			} else {
				BMF_DrawString(simasel->curfont, fname);
			}
		}
#else
			BMF_DrawString(simasel->curfont, fname);
#endif /* WITH_ICONV */
}

static void draw_imasel_bookmarks(ScrArea *sa, SpaceImaSel *simasel)
{
	char bookmark[FILE_MAX];
	float sw;

	if ((simasel->flag & FILE_BOOKMARKS) && (simasel->type != FILE_MAIN)) {
		int nentries = fsmenu_get_nentries();
		int i;
		short sx, sy;
		int bmwidth;
		int linestep = U.fontsize*3/2;
		
		sx = simasel->bookmarkrect.xmin + TILE_BORDER_X;
		sy = simasel->bookmarkrect.ymax - TILE_BORDER_Y - linestep;
		bmwidth = simasel->bookmarkrect.xmax - simasel->bookmarkrect.xmin - 2*TILE_BORDER_X;
		
		if (bmwidth < 0) return;

		for (i=0; i< nentries && sy > linestep ;++i) {
			char *fname = fsmenu_get_entry(i);
			char *sname = NULL;
			
			if (fname) {
				int sl;
				BLI_strncpy(bookmark, fname, FILE_MAX);
			
				sl = strlen(bookmark)-1;
				if (bookmark[sl] == '\\' || bookmark[sl] == '/') {
					bookmark[sl] = '\0';
					sl--;
				}
				while (sl) {
					if (bookmark[sl] == '\\' || bookmark[sl] == '/'){
						sl++;
						break;
					};
					sl--;
				}
				sname = &bookmark[sl];
				sw = shorten_string(simasel, sname, bmwidth);

				
				if (simasel->active_bookmark == i ) {
					glEnable(GL_BLEND);
					glColor4ub(0, 0, 0, 100);
					glDisable(GL_BLEND);
					BIF_ThemeColor(TH_HILITE);
					uiSetRoundBox(15);	
					uiRoundBox(simasel->bookmarkrect.xmin + TILE_BORDER_X - 1, sy - linestep*0.25, simasel->bookmarkrect.xmax - TILE_BORDER_X + 1, sy + linestep*0.75, 6);
					BIF_ThemeColor(TH_TEXT_HI);
				} else {
					BIF_ThemeColor(TH_TEXT);
				}
				ui_rasterpos_safe(sx, sy, simasel->aspect);

				/* handling of international fonts.
					TODO: proper support for utf8 in languages different from ja_JP abd zh_CH
					needs update of iconv in lib/windows to support getting the system language string
				*/
#ifdef WITH_ICONV
				{
					struct LANGMenuEntry *lme;
       				lme = find_language(U.language);

					if ((lme !=NULL) && (!strcmp(lme->code, "ja_JP") || 
						!strcmp(lme->code, "zh_CN")))
					{
						BIF_RasterPos(sx, sy);
#ifdef WIN32
						BIF_DrawString(simasel->curfont, sname, ((U.transopts & USER_TR_MENUS) | CONVERT_TO_UTF8));
#else
						BIF_DrawString(simasel->curfont, sname, (U.transopts & USER_TR_MENUS));
#endif
					} else {
						BMF_DrawString(simasel->curfont, sname);
					}
				}
#else
				BMF_DrawString(simasel->curfont, sname);
#endif /* WITH_ICONV */

				sy -= linestep;
			} else {
				cpack(0xB0B0B0);
				sdrawline(sx,  sy + U.fontsize/2 ,  sx + bmwidth,  sy + U.fontsize/2); 
				cpack(0x303030);				
				sdrawline(sx,  sy + 1 + U.fontsize/2 ,  sx + bmwidth,  sy + 1 + U.fontsize/2);
				sy -= linestep;
			}
		}

		uiEmboss(simasel->bookmarkrect.xmin, simasel->bookmarkrect.ymin, simasel->bookmarkrect.xmax-1, simasel->bookmarkrect.ymax-1, 1);
	}
}

static void draw_imasel_previews(ScrArea *sa, SpaceImaSel *simasel)
{
	static double lasttime= 0;
	struct FileList* files = simasel->files;
	int numfiles;
	struct direntry *file;
	int numtiles;
	
	int tilewidth = simasel->prv_w + TILE_BORDER_X*4;
	int tileheight = simasel->prv_h + TILE_BORDER_Y*4 + U.fontsize;
	short sx, sy;
	int do_load = 1;
	
	ImBuf* imb=0;
	int i,j;
	short type;
	int colorid = 0;
	int todo;
	int fileoffset, rowoffset, columnoffset;
	float scrollofs;
	

	rcti viewrect = simasel->viewrect;

	if (!files) return;
	/* Reload directory */
	BLI_strncpy(simasel->dir, BIF_filelist_dir(files), FILE_MAX);	
	
	type = BIF_filelist_gettype(simasel->files);	
	
	if (BIF_filelist_empty(files))
	{
		unsigned int filter = 0;
		BIF_filelist_hidedot(simasel->files, simasel->flag & FILE_HIDE_DOT);
		if (simasel->flag & FILE_FILTER) {
			filter = simasel->filter ;
		} else {
			filter = 0;
		}

		BIF_filelist_setfilter(simasel->files, filter);
		BIF_filelist_readdir(files);
		
		if(simasel->sort!=FILE_SORTALPHA) BIF_filelist_sort(simasel->files, simasel->sort);		
	}

	BIF_filelist_imgsize(simasel->files,simasel->prv_w,simasel->prv_h);

	numfiles = BIF_filelist_numfiles(files);
	numtiles = simasel->numtilesx*simasel->numtilesy;

	if (numtiles > numfiles) numtiles = numfiles;
	
	todo = 0;
	if (lasttime < 0.001) lasttime = PIL_check_seconds_timer();

	
	if (simasel->numtilesx > 0) {
		/* calculate the offset to start drawing */
		if ((numtiles < numfiles) && (simasel->scrollarea > 0)) {
			fileoffset = numfiles*( (simasel->scrollpos) / simasel->scrollarea) + 0.5;		
		} else {
			fileoffset = 0;
		}
		rowoffset = (fileoffset / simasel->numtilesx)*simasel->numtilesx;
		columnoffset = fileoffset % simasel->numtilesx;
		scrollofs = (float)tileheight*(float)columnoffset/(float)simasel->numtilesx;
	} else {
		rowoffset = 0;
		scrollofs = 0;
	}
	/* add partially visible row */
	numtiles += simasel->numtilesx;
	for (i=rowoffset, j=0 ; (i < numfiles) && (j < numtiles); ++i, ++j)
	{
		sx = simasel->v2d.cur.xmin + viewrect.xmin + (j % simasel->numtilesx)*tilewidth;
		sy = simasel->v2d.cur.ymin + viewrect.ymax + (short)scrollofs - (viewrect.ymin + (j / simasel->numtilesx)*tileheight);

		file = BIF_filelist_file(files, i);				

		if (simasel->active_file == i) {
			colorid = TH_ACTIVE;
			draw_tile(simasel, sx, sy, colorid);
		} else if (file->flags & ACTIVE) {
			colorid = TH_HILITE;
			draw_tile(simasel, sx, sy, colorid);
		} else {
			/*
			colorid = TH_PANEL;
			draw_tile(simasel, sx, sy, colorid);
			*/
		}

		if ( type == FILE_MAIN) {
			ID *id;
			int icon_id = 0;
			int idcode;
			idcode= BIF_groupname_to_code(simasel->dir);
			if (idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO || idcode == ID_IM) {
				id = (ID *)file->poin;
				icon_id = BKE_icon_getid(id);
			}		
			if (icon_id) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				if (do_load) {
					BIF_icon_draw_preview(sx+2*TILE_BORDER_X, sy-simasel->prv_w-TILE_BORDER_X, icon_id, 0);			
				} else {
					BIF_icon_draw_preview(sx+2*TILE_BORDER_X, sy-simasel->prv_w-TILE_BORDER_X, icon_id, 1);
					todo++;
				}
				
				glDisable(GL_BLEND);
			}		
		}
		else {
			if ( (file->flags & IMAGEFILE) || (file->flags & MOVIEFILE))
			{
				if (do_load) {					
					BIF_filelist_loadimage(simasel->files, i);				
				} else {
					todo++;
				}
				imb = BIF_filelist_getimage(simasel->files, i);
			} else {
				imb = BIF_filelist_getimage(simasel->files, i);
			}

			if (imb) {		
					float fx = ((float)simasel->prv_w - (float)imb->x)/2.0f;
					float fy = ((float)simasel->prv_h - (float)imb->y)/2.0f;
					short dx = (short)(fx + 0.5f);
					short dy = (short)(fy + 0.5f);
					
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);													
					// glaDrawPixelsSafe((float)sx+8 + dx, (float)sy - imgwidth + dy - 8, imb->x, imb->y, imb->x, GL_RGBA, GL_UNSIGNED_BYTE, imb->rect);
					glColor4f(1.0, 1.0, 1.0, 1.0);
					glaDrawPixelsTex((float)sx+2*TILE_BORDER_X + dx, (float)sy - simasel->prv_h + dy - 2*TILE_BORDER_Y, imb->x, imb->y,GL_UNSIGNED_BYTE, imb->rect);
					// glDisable(GL_BLEND);
					imb = 0;
			}			
		}		

		if (type == FILE_MAIN) {
			glColor3f(1.0f, 1.0f, 1.0f);			
		}
		else {
			if (S_ISDIR(file->type)) {
				glColor3f(1.0f, 1.0f, 0.9f);
			}
			else if (file->flags & IMAGEFILE) {
				BIF_ThemeColor(TH_SEQ_IMAGE);
			}
			else if (file->flags & MOVIEFILE) {
				BIF_ThemeColor(TH_SEQ_MOVIE);
			}
			else if (file->flags & BLENDERFILE) {
				BIF_ThemeColor(TH_SEQ_SCENE);
			}
			else {
				if (simasel->active_file == i) {
					BIF_ThemeColor(TH_GRID); /* grid used for active text */
				} else if (file->flags & ACTIVE) {
					BIF_ThemeColor(TH_TEXT_HI);			
				} else {
					BIF_ThemeColor(TH_TEXT);
				}
			}
		}
			
		draw_file(simasel, sx, sy, file);
		
		if(do_load && (PIL_check_seconds_timer() - lasttime > 0.3)) {
			lasttime= PIL_check_seconds_timer();
			do_load = 0;
		}
	}

	if (!do_load && todo > 0) /* we broke off loading */
		addafterqueue(sa->win, RENDERPREVIEW, 1);
}


/* in panel space! */
static void imasel_imgdraw(ScrArea *sa, uiBlock *block)
{
	SpaceImaSel *simasel= sa->spacedata.first;
	rctf dispf;
	rcti winrect;
	struct direntry *file;
	char path[FILE_MAX];
	float tsize;
	short ofsx=0;
	short ofsy=0;
	short ex, ey;
	float scaledx, scaledy;
	int index;	

	BLI_init_rctf(&dispf, 0.0f, (block->maxx - block->minx)-0.0f, 0.0f, (block->maxy - block->miny)-0.0f);
	ui_graphics_to_window_rct(sa->win, &dispf, &winrect);

	if (!simasel->img) {
		BLI_join_dirfile(path, simasel->dir, simasel->file);
		if (!BLI_exists(path))
			return;
	
		index = BIF_filelist_find(simasel->files, simasel->file);
		if (index >= 0) {
			file = BIF_filelist_file(simasel->files,index);
			if (file->flags & IMAGEFILE || file->flags & MOVIEFILE) {
				simasel->img = IMB_loadiffname(path, IB_rect);

				if (simasel->img) {
					tsize = MIN2(winrect.xmax - winrect.xmin,winrect.ymax - winrect.ymin);
					
					if (simasel->img->x > simasel->img->y) {
						scaledx = (float)tsize;
						scaledy =  ( (float)simasel->img->y/(float)simasel->img->x )*tsize;
						ofsy = (scaledx - scaledy) / 2.0;
						ofsx = 0;
					}
					else {
						scaledy = (float)tsize;
						scaledx =  ( (float)simasel->img->x/(float)simasel->img->y )*tsize;
						ofsx = (scaledy - scaledx) / 2.0;
						ofsy = 0;
					}
					ex = (short)scaledx;
					ey = (short)scaledy;

					IMB_scaleImBuf(simasel->img, ex, ey);
				}
			}
		}
	}
	if (simasel->img == NULL) 
		return;
	if(simasel->img->rect==NULL)
		return;

	/* correction for gla draw */
	BLI_translate_rcti(&winrect, -curarea->winrct.xmin, -curarea->winrct.ymin);
		
	glaDefine2DArea(&sa->winrct);
	glaDrawPixelsSafe(winrect.xmin+ofsx, winrect.ymin+ofsy, simasel->img->x, simasel->img->y, simasel->img->x, GL_RGBA, GL_UNSIGNED_BYTE, simasel->img->rect);	
}

static void imasel_panel_image(ScrArea *sa, short cntrl)
{
	uiBlock *block;
	SpaceImaSel *simasel= sa->spacedata.first;
	short w = 300;
	short h = 300;
	short offsx, offsy;

	if (simasel->img) {
		w = simasel->img->x;
		h = simasel->img->y;
	}
	
	offsx = -150 + (simasel->v2d.mask.xmax - simasel->v2d.mask.xmin)/2;
	offsy = -150 + (simasel->v2d.mask.ymax - simasel->v2d.mask.ymin)/2;

	block= uiNewBlock(&curarea->uiblocks, "imasel_panel_image", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(IMASEL_HANDLER_IMAGE);  // for close and esc
	if(uiNewPanel(curarea, block, "Image Preview", "Image Browser", offsx, offsy, w, h)==0) 
		return;
	uiBlockSetDrawExtraFunc(block, imasel_imgdraw); 
}

static void imasel_blockhandlers(ScrArea *sa)
{
	SpaceImaSel *simasel= sa->spacedata.first;
	short a;
		
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(simasel->blockhandler[a]) {

		case IMASEL_HANDLER_IMAGE:
			imasel_panel_image(sa, simasel->blockhandler[a+1]);
			break;
		
		}
		/* clear action value for event */
		simasel->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);
}


static void draw_imasel_buttons(ScrArea *sa, SpaceImaSel* simasel)
{
	uiBlock *block;
	int loadbutton;
	char name[20];
	char *menu;
	float slen;
	float parentbut_width = 20;
	float bookmarkbut_width = 0.0f;
	float file_start_width = 0.0f;

	int filebuty1, filebuty2;

	float xmin = simasel->v2d.mask.xmin + 10;
	float xmax = simasel->v2d.mask.xmax - 10;

	filebuty1= simasel->v2d.mask.ymax - IMASEL_BUTTONS_HEIGHT;
	filebuty2= filebuty1+IMASEL_BUTTONS_HEIGHT/2 -6;

	/* HEADER */
	sprintf(name, "win %d", sa->win);
	block = uiNewBlock(&sa->uiblocks, name, UI_EMBOSS, UI_HELV, sa->win);
	
	uiSetButLock( BIF_filelist_gettype(simasel->files)==FILE_MAIN && simasel->returnfunc, NULL); 

	/* space available for load/save buttons? */
	slen = BIF_GetStringWidth(G.font, simasel->title, simasel->aspect);
	loadbutton= slen > 60 ? slen + 20 : 80; /* MAX2(80, 20+BIF_GetStringWidth(G.font, simasel->title)); */
	if(simasel->v2d.mask.xmax-simasel->v2d.mask.xmin > loadbutton+20) {
		if(simasel->title[0]==0) {
			loadbutton= 0;
		}
	}
	else {
		loadbutton= 0;
	}

	menu= fsmenu_build_menu();

	if (menu[0]&& (simasel->type != FILE_MAIN)) {
		bookmarkbut_width = parentbut_width;
		file_start_width = parentbut_width;
	}

	uiDefBut(block, TEX, B_FS_FILENAME,"",	xmin+file_start_width+bookmarkbut_width+2, filebuty1, xmax-xmin-loadbutton-file_start_width-bookmarkbut_width, 21, simasel->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	uiDefBut(block, TEX, B_FS_DIRNAME,"",	xmin+parentbut_width, filebuty2, xmax-xmin-loadbutton-parentbut_width, 21, simasel->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");

	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT,B_FS_LOAD, simasel->title,	xmax-loadbutton, filebuty2, loadbutton, 21, simasel->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
		uiDefBut(block, BUT,B_FS_CANCEL, "Cancel",		xmax-loadbutton, filebuty1, loadbutton, 21, simasel->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}

	/* menu[0] = NULL happens when no .Bfs is there, and first time browse
	   disallow external directory browsing for databrowse */
	if(menu[0] && (simasel->type != FILE_MAIN))	{ 
		uiDefButS(block, MENU,B_FS_DIR_MENU, menu, xmin, filebuty1, parentbut_width, 21, &simasel->menu, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, B_FS_BOOKMARK, "B", xmin+22, filebuty1, bookmarkbut_width, 21, 0, 0, 0, 0, 0, "Bookmark current directory");
	}
	MEM_freeN(menu);

	uiDefBut(block, BUT, B_FS_PARDIR, "P", xmin, filebuty2, parentbut_width, 21, 0, 0, 0, 0, 0, "Move to the parent directory (PKEY)");	

	uiDrawBlock(block);
}



/* ************** main drawing function ************** */

void drawimaselspace(ScrArea *sa, void *spacedata)
{
	float col[3];
	SpaceImaSel *simasel= curarea->spacedata.first;
	
	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);	
	
	/* HACK: somehow when going fullscreen, v2d isn't set correctly */
	simasel->v2d.cur.xmin= simasel->v2d.cur.ymin= 0.0f;
	simasel->v2d.cur.xmax= sa->winx;
	simasel->v2d.cur.ymax= sa->winy;	
	simasel->v2d.tot= simasel->v2d.cur;
	test_view2d(G.v2d, sa->winx, sa->winy);

	calc_imasel_rcts(simasel, sa->winx, sa->winy);

	myortho2(simasel->v2d.cur.xmin, simasel->v2d.cur.xmax, simasel->v2d.cur.ymin, simasel->v2d.cur.ymax);
	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();
	
	/* warning; blocks need to be freed each time, handlers dont remove  */
	uiFreeBlocksWin(&sa->uiblocks, sa->win); 

	/* aspect+font, set each time */
	simasel->aspect= (simasel->v2d.cur.xmax - simasel->v2d.cur.xmin)/((float)sa->winx);
	simasel->curfont= uiSetCurFont_ext(simasel->aspect);	
	
	if (!simasel->files) {
		simasel->files = BIF_filelist_new();
		BIF_filelist_setdir(simasel->files, simasel->dir);
		BIF_filelist_settype(simasel->files, simasel->type);
	}

	/* Buttons */
	draw_imasel_buttons(sa, simasel);	
	
	/* scrollbar */
	draw_imasel_scroll(simasel);	

	/* bookmarks */
	draw_imasel_bookmarks(sa, simasel);

	uiEmboss(simasel->viewrect.xmin, simasel->viewrect.ymin, simasel->v2d.mask.xmax-TILE_BORDER_X, simasel->viewrect.ymax, 1);


	glScissor(sa->winrct.xmin + simasel->viewrect.xmin , 
			  sa->winrct.ymin + simasel->viewrect.ymin, 
			  simasel->viewrect.xmax - simasel->viewrect.xmin , 
			  simasel->viewrect.ymax - simasel->viewrect.ymin);

	/* previews */	
	draw_imasel_previews(sa, simasel);
	
	/* BIF_ThemeColor(TH_HEADER);*/
	/* glRecti(simasel->viewrect.xmin,  simasel->viewrect.ymin,  simasel->viewrect.xmax,  simasel->viewrect.ymax);*/
	
	/* restore viewport (not needed yet) */
	mywinset(sa->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);

	draw_area_emboss(sa);

	imasel_blockhandlers(sa);

	curarea->win_swap= WIN_BACK_OK;
}
