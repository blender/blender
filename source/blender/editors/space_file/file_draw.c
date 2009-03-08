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

#include "BLI_blenlib.h"
#include "BLI_storage_types.h"
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
 
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "RNA_access.h"

#include "ED_fileselect.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "fsmenu.h"
#include "filelist.h"

#include "file_intern.h"	// own include

/* ui geometry */
#define IMASEL_BUTTONS_HEIGHT 60
#define TILE_BORDER_X 8
#define TILE_BORDER_Y 8

/* button events */
enum {
	B_REDR 	= 0,
	B_FS_EXEC,
	B_FS_CANCEL,
	B_FS_PARENT,
} eFile_ButEvents;

static void do_file_buttons(bContext *C, void *arg, int event)
{
	switch(event) {
		case B_FS_EXEC:
			file_exec(C, NULL);	/* file_ops.c */
			break;
		case B_FS_CANCEL:
			file_cancel_exec(C, NULL); /* file_ops.c */
			break;
		case B_FS_PARENT:
			file_parent_exec(C, NULL); /* file_ops.c */
			break;
	}
}

/* note; this function uses pixelspace (0, 0, winx, winy), not view2d */
void file_draw_buttons(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	uiBlock *block;
	int loadbutton;
	char name[20];
	char *menu;
	float slen;
	float parentbut_width = 20;
	float bookmarkbut_width = 0.0f;
	float file_start_width = 0.0f;

	int filebuty1, filebuty2;

	float xmin = 10;
	float xmax = ar->winx - 10;

	filebuty1= ar->winy - IMASEL_BUTTONS_HEIGHT;
	filebuty2= filebuty1+IMASEL_BUTTONS_HEIGHT/2 -6;

	/* HEADER */
	sprintf(name, "win %p", ar);
	block = uiBeginBlock(C, ar, name, UI_EMBOSS, UI_HELV);
	uiBlockSetHandleFunc(block, do_file_buttons, NULL);

	/* XXXX
	uiSetButLock( filelist_gettype(simasel->files)==FILE_MAIN && simasel->returnfunc, NULL); 
	*/

	/* space available for load/save buttons? */
	slen = UI_GetStringWidth(G.font, sfile->params->title, 0);
	loadbutton= slen > 60 ? slen + 20 : MAX2(80, 20+UI_GetStringWidth(G.font, params->title, 0));
	if(ar->winx > loadbutton+20) {
		if(params->title[0]==0) {
			loadbutton= 0;
		}
	}
	else {
		loadbutton= 0;
	}

	/* XXX to channel region */
	menu= fsmenu_build_menu();

	if (menu[0]&& (params->type != FILE_MAIN)) {
		bookmarkbut_width = parentbut_width;
		file_start_width = parentbut_width;
	}

	uiDefBut(block, TEX, 0 /* XXX B_FS_FILENAME */,"",	xmin+file_start_width+bookmarkbut_width+2, filebuty1, xmax-xmin-loadbutton-file_start_width-bookmarkbut_width, 21, params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	uiDefBut(block, TEX, 0 /* XXX B_FS_DIRNAME */,"",	xmin+parentbut_width, filebuty2, xmax-xmin-loadbutton-parentbut_width, 21, params->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");

	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT, B_FS_EXEC, params->title,	xmax-loadbutton, filebuty2, loadbutton, 21, params->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
		uiDefBut(block, BUT, B_FS_CANCEL, "Cancel",		xmax-loadbutton, filebuty1, loadbutton, 21, params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}

	/* menu[0] = NULL happens when no .Bfs is there, and first time browse
	   disallow external directory browsing for databrowse */

	if(menu[0] && (params->type != FILE_MAIN))	{ 
		uiDefButS(block, MENU, 0 /* B_FS_DIR_MENU */, menu, xmin, filebuty1, parentbut_width, 21, &params->menu, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, 0 /* B_FS_BOOKMARK */, "B", xmin+22, filebuty1, bookmarkbut_width, 21, 0, 0, 0, 0, 0, "Bookmark current directory");
	}

	MEM_freeN(menu);

	uiDefBut(block, BUT, B_FS_PARENT, "P", xmin, filebuty2, parentbut_width, 21, 0, 0, 0, 0, 0, "Move to the parent directory (PKEY)");	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


static void draw_tile(short sx, short sy, short width, short height, int colorid, int shade)
{
	/* TODO: BIF_ThemeColor seems to need this to show the color, not sure why? - elubie */
	//glEnable(GL_BLEND);
	//glColor4ub(0, 0, 0, 100);
	//glDisable(GL_BLEND);
	/* I think it was a missing glDisable() - ton */
	
	UI_ThemeColorShade(colorid, shade);
	uiSetRoundBox(15);	
	// glRecti(sx, sy - height, sx + width, sy);

	uiRoundBox(sx, sy - height, sx + width, sy, 6);
}

static float shorten_string(char* string, float w)
{	
	short shortened = 0;
	float sw = 0;
	
	sw = UI_GetStringWidth(G.font, string,0);
	while (sw>w) {
		int slen = strlen(string);
		string[slen-1] = '\0';
		sw = UI_GetStringWidth(G.font, string,0);
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

static void file_draw_string(short sx, short sy, char* string, short width, short height)
{
	short soffs;
	char fname[FILE_MAXFILE];
	float sw;
	float x,y;

	BLI_strncpy(fname,string, FILE_MAXFILE);
	sw = shorten_string(fname, width );
	soffs = (width - sw) / 2;
	x = (float)(sx);
	y = (float)(sy-height);

	// XXX was using ui_rasterpos_safe
	glRasterPos2f(x, y);
	UI_RasterPos(x, y);

	/* XXX TODO: handling of international fonts.
	    TODO: proper support for utf8 in languages different from ja_JP abd zh_CH
	    needs update of iconv in lib/windows to support getting the system language string
	*/
	UI_DrawString(G.font, fname, 0);

}

/* returns max number of rows in view */
static int file_view_rows(SpaceFile* sfile, View2D *v2d)
{
	int height= (v2d->cur.ymax - v2d->cur.ymin - 2*sfile->tile_border_y);
	return height / (sfile->tile_h + 2*sfile->tile_border_y);
}

/* returns max number of columns in view */
static int file_view_columns(SpaceFile* sfile, View2D *v2d)
{
	int width= (v2d->cur.xmax - v2d->cur.xmin - 2*sfile->tile_border_x);
	return width / (sfile->tile_w + 2*sfile->tile_border_x);
}

void file_calc_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	View2D *v2d= &ar->v2d;
	int width=0, height=0;
	int rows, columns;

	if (params->display == FILE_IMGDISPLAY) {
		sfile->prv_w = 96;
		sfile->prv_h = 96;
		sfile->tile_border_x = 4;
		sfile->tile_border_y = 4;
		sfile->prv_border_x = 4;
		sfile->prv_border_y = 4;
		sfile->tile_w = sfile->prv_w + 2*sfile->prv_border_x;
		sfile->tile_h = sfile->prv_h + 4*sfile->prv_border_y + U.fontsize*3/2;
		width= (v2d->cur.xmax - v2d->cur.xmin - 2*sfile->tile_border_x);
		columns= file_view_columns(sfile, v2d);
		if(columns)
			rows= filelist_numfiles(sfile->files)/columns + 1; // XXX dirty, modulo is zero
		else
			rows= filelist_numfiles(sfile->files) + 1; // XXX dirty, modulo is zero
		height= rows*(sfile->tile_h+2*sfile->tile_border_y) + sfile->tile_border_y*2;
	} else {
		sfile->prv_w = 0;
		sfile->prv_h = 0;
		sfile->tile_border_x = 8;
		sfile->tile_border_y = 2;
		sfile->prv_border_x = 0;
		sfile->prv_border_y = 0;
		sfile->tile_w = 240;
		sfile->tile_h = U.fontsize*3/2;
		height= v2d->cur.ymax - v2d->cur.ymin;
		rows = file_view_rows(sfile, v2d);
		if(rows)
			columns = filelist_numfiles(sfile->files)/rows + 1; // XXX dirty, modulo is zero
		else
			columns = filelist_numfiles(sfile->files) + 1; // XXX dirty, modulo is zero
			
		width = columns * (sfile->tile_w + 2*sfile->tile_border_x) + sfile->tile_border_x*2;
	}

	UI_view2d_totRect_set(v2d, width, height);
}

void file_draw_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params= ED_fileselect_get_params(sfile);
	View2D *v2d= &ar->v2d;
	static double lasttime= 0;
	struct FileList* files = sfile->files;
	int numfiles;
	struct direntry *file;

	short sx, sy;
	int do_load = 1;
	
	ImBuf* imb=0;
	int i;
	short type;
	int colorid = 0;
	int todo;
	int offset;
	int columns;
	int rows;

	if (!files) return;

	type = filelist_gettype(files);	
	filelist_imgsize(files,sfile->prv_w,sfile->prv_h);
	numfiles = filelist_numfiles(files);
	
	todo = 0;
	if (lasttime < 0.001) lasttime = PIL_check_seconds_timer();

	sx = v2d->cur.xmin + sfile->tile_border_x;
	sy = v2d->cur.ymax - sfile->tile_border_y;
	columns = file_view_columns(sfile, v2d);
	rows = file_view_rows(sfile, v2d);

	offset = columns*(-v2d->cur.ymax-sfile->tile_border_y)/(sfile->tile_h+sfile->tile_border_y);
	offset = (offset/columns-1)*columns;
	if (offset<0) offset=0;
	for (i=offset; (i < numfiles) && (i < (offset+(rows+2)*columns)); ++i)
	{
		sx = v2d->tot.xmin + sfile->tile_border_x + ((i)%columns)*(sfile->tile_w+2*sfile->tile_border_x);
		sy = v2d->tot.ymax - sfile->tile_border_y - ((i)/columns)*(sfile->tile_h+2*sfile->tile_border_y);
		file = filelist_file(files, i);				

		if (params->active_file == i) {
			colorid = TH_ACTIVE;
			draw_tile(sx - 1, sy, sfile->tile_w + 1, sfile->tile_h, colorid,0);
		} else if (file->flags & ACTIVE) {
			colorid = TH_HILITE;
			draw_tile(sx - 1, sy, sfile->tile_w + 1, sfile->tile_h, colorid,0);
		} else {
			colorid = TH_BACK;
			draw_tile(sx, sy, sfile->tile_w, sfile->tile_h, colorid, -5);
		}

#if 0
		if ( type == FILE_MAIN) {
			ID *id;
			int icon_id = 0;
			int idcode;
			idcode= groupname_to_code(sfile->dir);
			if (idcode == ID_MA || idcode == ID_TE || idcode == ID_LA || idcode == ID_WO || idcode == ID_IM) {
				id = (ID *)file->poin;
				icon_id = BKE_icon_getid(id);
			}		
			if (icon_id) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				if (do_load) {
					UI_icon_draw_preview(sx+2*TILE_BORDER_X, sy-simasel->prv_w-TILE_BORDER_X, icon_id, 0);			
				} else {
					UI_icon_draw_preview(sx+2*TILE_BORDER_X, sy-simasel->prv_w-TILE_BORDER_X, icon_id, 1);
					todo++;
				}
				
				glDisable(GL_BLEND);
			}		
		}
		else {
#endif
			if ( (file->flags & IMAGEFILE) /* || (file->flags & MOVIEFILE) */)
			{
				if (do_load) {					
					filelist_loadimage(files, i);				
				} else {
					todo++;
				}
				imb = filelist_getimage(files, i);
			} else {
				imb = filelist_getimage(files, i);
			}

			if (imb) {		
				float fx = ((float)sfile->prv_w - (float)imb->x)/2.0f;
				float fy = ((float)sfile->prv_h - (float)imb->y)/2.0f;
				short dx = (short)(fx + 0.5f + sfile->prv_border_x);
				short dy = (short)(fy + 0.5f - sfile->prv_border_y);
				
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);													
				// glaDrawPixelsSafe((float)sx+8 + dx, (float)sy - imgwidth + dy - 8, imb->x, imb->y, imb->x, GL_RGBA, GL_UNSIGNED_BYTE, imb->rect);
				glColor4f(1.0, 1.0, 1.0, 1.0);
				glaDrawPixelsTex((float)sx + dx, (float)sy - sfile->prv_h + dy, imb->x, imb->y,GL_UNSIGNED_BYTE, imb->rect);
				glDisable(GL_BLEND);
				imb = 0;
			}
#if 0
		}		
#endif
		if (type == FILE_MAIN) {
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);			
		}
		else {
			if (S_ISDIR(file->type)) {
				glColor4f(1.0f, 1.0f, 0.9f, 1.0f);
			}
			else if (file->flags & IMAGEFILE) {
				UI_ThemeColor(TH_SEQ_IMAGE);
			}
			else if (file->flags & MOVIEFILE) {
				UI_ThemeColor(TH_SEQ_MOVIE);
			}
			else if (file->flags & BLENDERFILE) {
				UI_ThemeColor(TH_SEQ_SCENE);
			}
			else {
				if (params->active_file == i) {
					UI_ThemeColor(TH_GRID); /* grid used for active text */
				} else if (file->flags & ACTIVE) {
					UI_ThemeColor(TH_TEXT_HI);			
				} else {
					UI_ThemeColor(TH_TEXT);
				}
			}
		}
			
		file_draw_string(sx + sfile->prv_border_x, sy+U.fontsize*3/2, file->relname, sfile->tile_w, sfile->tile_h);

		if (!sfile->loadimage_timer)
			sfile->loadimage_timer= WM_event_add_window_timer(CTX_wm_window(C), TIMER1, 1.0/30.0);	/* max 30 frames/sec. */

	}

}


void file_draw_list(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	struct FileList* files = sfile->files;
	struct direntry *file;
	int numfiles;
	int colorid = 0;
	short sx, sy;
	int offset;
	short type;
	int i;
	int rows;
	float sw;

	numfiles = filelist_numfiles(files);
	type = filelist_gettype(files);	

	sx = ar->v2d.tot.xmin + sfile->tile_border_x/2;
	sy = ar->v2d.cur.ymax - sfile->tile_border_y;

	rows = (ar->v2d.cur.ymax - ar->v2d.cur.ymin - 2*sfile->tile_border_y) / (sfile->tile_h+sfile->tile_border_y);
	offset = rows*(sx - sfile->tile_border_x)/(sfile->tile_w+sfile->tile_border_x);
	offset = (offset/rows-1)*rows;

	while (sx < ar->v2d.cur.xmax) {
		sx += (sfile->tile_w+sfile->tile_border_x);
		glColor4ub(0xB0,0xB0,0xB0, 0xFF);
		sdrawline(sx+1,  ar->v2d.cur.ymax - sfile->tile_border_y ,  sx+1,  ar->v2d.cur.ymin + sfile->tile_border_y); 
		glColor4ub(0x30,0x30,0x30, 0xFF);
		sdrawline(sx,  ar->v2d.cur.ymax - sfile->tile_border_y ,  sx,  ar->v2d.cur.ymin + sfile->tile_border_y); 
	}

	sx = ar->v2d.cur.xmin + sfile->tile_border_x;
	sy = ar->v2d.cur.ymax - sfile->tile_border_y;
	if (offset<0) offset=0;
	for (i=offset; (i < numfiles); ++i)
	{
		sy = ar->v2d.tot.ymax-sfile->tile_border_y - (i%rows)*(sfile->tile_h+sfile->tile_border_y);
		sx = 2 + ar->v2d.tot.xmin +sfile->tile_border_x + (i/rows)*(sfile->tile_w+sfile->tile_border_x);

		file = filelist_file(files, i);	

		if (params->active_file == i) {
			if (file->flags & ACTIVE) colorid= TH_HILITE;
			else colorid = TH_BACK;
			draw_tile(sx-2, sy-3, sfile->tile_w+2, sfile->tile_h, colorid,20);
		} else if (file->flags & ACTIVE) {
			colorid = TH_HILITE;
			draw_tile(sx-2, sy-3, sfile->tile_w+2, sfile->tile_h, colorid,0);
		} else {
			/*
			colorid = TH_PANEL;
			draw_tile(sx, sy, sfile->tile_w, sfile->tile_h, colorid);
			*/
		}
		if (type == FILE_MAIN) {
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);			
		}
		else {
			if (S_ISDIR(file->type))
				UI_ThemeColor4(TH_TEXT_HI);			
			else
				UI_ThemeColor4(TH_TEXT);
		}
		
		sw = UI_GetStringWidth(G.font, file->size, 0);
		file_draw_string(sx, sy, file->relname, sfile->tile_w - sw - 5, sfile->tile_h);
		file_draw_string(sx + sfile->tile_w - sw, sy, file->size, sfile->tile_w - sw, sfile->tile_h);
	}
}

void file_draw_fsmenu(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	char bookmark[FILE_MAX];
	int nentries = fsmenu_get_nentries();
	int linestep = U.fontsize*3/2;
	int i;
	short sx, sy;
	int bmwidth = ar->v2d.cur.xmax - ar->v2d.cur.xmin - 2*TILE_BORDER_X;
	int fontsize = U.fontsize;

	if (params->flag & FILE_BOOKMARKS) {
		sx = ar->v2d.cur.xmin + TILE_BORDER_X;
		sy = ar->v2d.cur.ymax-2*TILE_BORDER_Y;
		for (i=0; i< nentries && (sy > ar->v2d.cur.ymin) ;++i) {
			char *fname = fsmenu_get_entry(i);

			if (fname) {
				int sl;
				BLI_strncpy(bookmark, fname, FILE_MAX);
			
				sl = strlen(bookmark)-1;
				while (bookmark[sl] == '\\' || bookmark[sl] == '/') {
					bookmark[sl] = '\0';
					sl--;
				}
				if (params->active_bookmark == i ) {
					glColor4ub(0, 0, 0, 100);
					UI_ThemeColor(TH_HILITE);
					uiSetRoundBox(15);	
					uiRoundBox(sx, sy - linestep, sx + bmwidth, sy, 6);
					// glRecti(sx, sy - linestep, sx + bmwidth, sy);
					UI_ThemeColor(TH_TEXT_HI);
				} else {
					UI_ThemeColor(TH_TEXT);
				}

				file_draw_string(sx, sy, bookmark, bmwidth, fontsize);
				sy -= linestep;
			} else {
				glColor4ub(0xB0,0xB0,0xB0, 0xFF);
				sdrawline(sx,  sy-1-fontsize/2 ,  sx + bmwidth,  sy-1-fontsize/2); 
				glColor4ub(0x30,0x30,0x30, 0xFF);
				sdrawline(sx,  sy-fontsize/2 ,  sx + bmwidth,  sy - fontsize/2);
				sy -= linestep;
			}
		}
	}
}
