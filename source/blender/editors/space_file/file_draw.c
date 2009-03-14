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
#include "BLI_dynstr.h"
#include "BLI_storage_types.h"
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BMF_Api.h"

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
#define IMASEL_BUTTONS_HEIGHT 40
#define TILE_BORDER_X 8
#define TILE_BORDER_Y 8

/* button events */
enum {
	B_REDR 	= 0,
	B_FS_EXEC,
	B_FS_CANCEL,
	B_FS_PARENT,
} eFile_ButEvents;

/* XXX very bad, need to check font code */
static int gFontsize=12;

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
	float slen;
	int filebuty1, filebuty2;

	float xmin = 8;
	float xmax = ar->winx - 10;

	filebuty1= ar->winy - IMASEL_BUTTONS_HEIGHT - 12;
	filebuty2= filebuty1 + IMASEL_BUTTONS_HEIGHT/2 + 4;

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

	uiDefBut(block, TEX, 0 /* XXX B_FS_FILENAME */,"",	xmin+2, filebuty1, xmax-xmin-loadbutton-4, 21, params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	uiDefBut(block, TEX, 0 /* XXX B_FS_DIRNAME */,"",	xmin+2, filebuty2, xmax-xmin-loadbutton-4, 21, params->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	
	if(loadbutton) {
		uiSetCurFont(block, UI_HELV);
		uiDefBut(block, BUT, B_FS_EXEC, params->title,	xmax-loadbutton, filebuty2, loadbutton, 21, params->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
		uiDefBut(block, BUT, B_FS_CANCEL, "Cancel",		xmax-loadbutton, filebuty1, loadbutton, 21, params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "");
	}

#if 0
	/* menu[0] = NULL happens when no .Bfs is there, and first time browse
	   disallow external directory browsing for databrowse */

	if(menu[0] && (params->type != FILE_MAIN))	{ 
		uiDefButS(block, MENU, 0 /* B_FS_DIR_MENU */, menu, xmin, filebuty2, fsmenubut_width, 21, &params->menu, 0, 0, 0, 0, "");
		uiDefBut(block, BUT, 0 /* B_FS_BOOKMARK */, "B", xmin, filebuty1, bookmarkbut_width, 21, 0, 0, 0, 0, 0, "Bookmark current directory");
	}

	MEM_freeN(menu);
#endif


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

#define FILE_SHORTEN_END				0
#define FILE_SHORTEN_FRONT				1
#define FILE_SHORTEN_FSIZE				2
#define FILE_SHORTEN_FSIZE_WIDTHONLY	3

static float shorten_string(char* string, float w, int flag)
{	
	char temp[FILE_MAX];
	short shortened = 0;
	float sw = 0;
	float pad = 0;

	sw = UI_GetStringWidth(G.font, string,0);
	if (flag == FILE_SHORTEN_FRONT) {
		char *s = string;
		BLI_strncpy(temp, "...", 4);
		pad = UI_GetStringWidth(G.font, temp,0);
		while (s && (sw+pad>w)) {
			s++;
			sw = UI_GetStringWidth(G.font, s,0);
			shortened = 1;
		}
		if (shortened) {
			int slen = strlen(s);			
			BLI_strncpy(temp+3, s, slen+1);
			temp[slen+4] = '\0';
			BLI_strncpy(string, temp, slen+4);
		}
	} else {
		char *s = string;
		while (sw>w) {
			int slen = strlen(string);
			string[slen-1] = '\0';
			sw = UI_GetStringWidth(G.font, s,0);
			shortened = 1;
		}
		if (shortened) {
			int slen = strlen(string);
			if (slen > 3) {
				BLI_strncpy(string+slen-3, "...", 4);				
			}
		}
	}
	
	return sw;
}

static float shorten_filesize(char* string, int flag)
{	
	float sw = 0;
	char tmp[FILE_MAX];
	int slen = strlen(string);
	char *s = string;
	int i;
	float size;

	/* note:
	 * input file size is stored as a string, 15 chars long including 
	 * whitespace at the start, and spaces in between sections.
	 * i.e. the maximum size is 999 999 999 999 bytes.
	 */
	
	/* get rid of whitespace and convert to a float representing size in bytes */
	for (i=0; i < slen; i++) {
		s++;
		if (s[0] != ' ')
			strncat(tmp, s, 1);
	}
	size = atof(tmp);
	
	if (size > 1024*1024*1024) {
		sprintf(tmp, "%.2f GB", size/(1024*1024*1024));	
	}
	else if (size > 1024*1024) {
		sprintf(tmp, "%.1f MB", size/(1024*1024));
	}
	else if (size > 1024) {
		sprintf(tmp, "%d KB", (int)(size/1024));
	}
	else {
		sprintf(tmp, "%d B", (int)size);
	}
	
	sw = UI_GetStringWidth(G.font, tmp, 0);
	
	if (flag != FILE_SHORTEN_FSIZE_WIDTHONLY)
		strcpy(string, tmp);
	
	return sw;
}

static int get_file_icon(struct direntry *file)
{
	if (file->type & S_IFDIR)
		return ICON_FILE_FOLDER;
	else if (file->flags & BLENDERFILE)
		return ICON_FILE_BLEND;
	else if (file->flags & IMAGEFILE)
		return ICON_FILE_IMAGE;
	else if (file->flags & MOVIEFILE)
		return ICON_FILE_MOVIE;
	else if (file->flags & PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (file->flags & PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (file->flags & SOUNDFILE) 
		return ICON_FILE_SOUND;
	else if (file->flags & FTFONTFILE) 
		return ICON_FILE_FONT;
	else
		return ICON_FILE_BLANK;
}

static void file_draw_icon(short sx, short sy, int icon, short width, short height)
{
	float x,y;
	int blend=0;
	
	x = (float)(sx);
	y = (float)(sy-height);
	
	if (icon == ICON_FILE_BLANK) blend = -80;
	
	glEnable(GL_BLEND);
	
	UI_icon_draw_aspect_blended(x, y, icon, 1.f, blend);
}

static void file_draw_string(short sx, short sy, const char* string, short width, short height, int flag)
{
	short soffs;
	char fname[FILE_MAXFILE];
	float sw;
	float x,y;

	BLI_strncpy(fname,string, FILE_MAXFILE);
	if (ELEM(flag, FILE_SHORTEN_END, FILE_SHORTEN_FRONT))
		sw = shorten_string(fname, width, flag );
	else if (flag == FILE_SHORTEN_FSIZE)
		sw = shorten_filesize(fname, flag);
	
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

void file_calc_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	View2D *v2d= &ar->v2d;
	
	ED_fileselect_init_layout(sfile, ar);
	UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height);
}

void file_draw_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params= ED_fileselect_get_params(sfile);
	FileLayout* layout= ED_fileselect_get_layout(sfile, ar);
	View2D *v2d= &ar->v2d;
	struct FileList* files = sfile->files;
	int numfiles;
	struct direntry *file;
	short sx, sy;
	ImBuf* imb=0;
	int i;
	int colorid = 0;
	int offset;
	int is_icon;

	if (!files) return;

	filelist_imgsize(files,sfile->layout->prv_w,sfile->layout->prv_h);
	numfiles = filelist_numfiles(files);

	sx = v2d->cur.xmin + layout->tile_border_x;
	sy = v2d->cur.ymax - layout->tile_border_y;
	
	offset = ED_fileselect_layout_offset(layout, 0, 0);
	if (offset<0) offset=0;
	for (i=offset; (i < numfiles) && (i < (offset+(layout->rows+2)*layout->columns)); ++i)
	{
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx += v2d->tot.xmin+2;
		sy = v2d->tot.ymax - sy;
		file = filelist_file(files, i);				

		if (file->flags & ACTIVE) {
			colorid = TH_HILITE;
			draw_tile(sx - 1, sy, sfile->layout->tile_w + 1, sfile->layout->tile_h, colorid,0);
		} else if (params->active_file == i) {
			colorid = TH_ACTIVE;
			draw_tile(sx - 1, sy, sfile->layout->tile_w + 1, sfile->layout->tile_h, colorid,0);
		}
		
		if ( (file->flags & IMAGEFILE) /* || (file->flags & MOVIEFILE) */)
		{			
			filelist_loadimage(files, i);
		}
		is_icon = 0;
		imb = filelist_getimage(files, i);
		if (!imb) {
			imb = filelist_geticon(files,i);
			is_icon = 1;
		}

		if (imb) {
			float fx = ((float)layout->prv_w - (float)imb->x)/2.0f;
			float fy = ((float)layout->prv_h - (float)imb->y)/2.0f;
			float dx = (fx + 0.5f + sfile->layout->prv_border_x);
			float dy = (fy + 0.5f - sfile->layout->prv_border_y);
			short xco = (float)sx + dx;
			short yco = (float)sy - sfile->layout->prv_h + dy;
			
			glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);
			
			/* shadow */
			if (!is_icon && (file->flags & IMAGEFILE))
				uiDrawBoxShadow(220, xco, yco, xco + imb->x, yco + imb->y);
			
			glEnable(GL_BLEND);
			
			/* the image */
			glColor4f(1.0, 1.0, 1.0, 1.0);
			glaDrawPixelsTex(xco, yco, imb->x, imb->y, GL_UNSIGNED_BYTE, imb->rect);
			
			/* border */
			if (!is_icon && (file->flags & IMAGEFILE)) {
				glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
				fdrawbox(xco, yco, xco + imb->x, yco + imb->y);
			}
			
			glDisable(GL_BLEND);
			imb = 0;
		}

		/* shadow */
		UI_ThemeColorShade(TH_BACK, -20);
		
		
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

		file_draw_string(sx + layout->prv_border_x, sy+4, file->relname, layout->tile_w, layout->tile_h, FILE_SHORTEN_END);

		if (!sfile->loadimage_timer)
			sfile->loadimage_timer= WM_event_add_window_timer(CTX_wm_window(C), TIMER1, 1.0/30.0);	/* max 30 frames/sec. */

	}

	uiSetRoundBox(0);
}


void file_draw_list(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	FileLayout* layout= ED_fileselect_get_layout(sfile, ar);
	View2D *v2d= &ar->v2d;
	struct FileList* files = sfile->files;
	struct direntry *file;
	int numfiles;
	int colorid = 0;
	short sx, sy;
	int offset;
	short type;
	int i;
	float sw, spos;

	numfiles = filelist_numfiles(files);
	type = filelist_gettype(files);	

	sx = ar->v2d.tot.xmin + layout->tile_border_x/2;
	sy = ar->v2d.cur.ymax - layout->tile_border_y;

	offset = ED_fileselect_layout_offset(layout, 0, 0);
	if (offset<0) offset=0;

	/* alternating flat shade background */
	for (i=offset; (i <= numfiles); ++i)
	{
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx = v2d->tot.xmin;
		sy = v2d->tot.ymax - (sy + layout->tile_border_y);
		//sy = v2d->tot.ymax - sy;
		
		if (i % 2) UI_ThemeColor(TH_BACK);
		else UI_ThemeColorShade(TH_BACK, -7);
		glRectf(v2d->tot.xmin, sy, v2d->tot.xmax, sy+layout->tile_h+2*layout->tile_border_y);
	}
	
	/* vertical column dividers */
	while (sx < ar->v2d.cur.xmax) {
		sx += (sfile->layout->tile_w+2*sfile->layout->tile_border_x);
		
		UI_ThemeColorShade(TH_BACK, 30);
		sdrawline(sx+1,  ar->v2d.cur.ymax - layout->tile_border_y ,  sx+1,  ar->v2d.cur.ymin); 
		UI_ThemeColorShade(TH_BACK, -30);
		sdrawline(sx,  ar->v2d.cur.ymax - layout->tile_border_y ,  sx,  ar->v2d.cur.ymin); 
	}

	sx = ar->v2d.cur.xmin + layout->tile_border_x;
	sy = ar->v2d.cur.ymax - layout->tile_border_y;

	for (i=offset; (i < numfiles); ++i)
	{
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx += v2d->tot.xmin+2;
		sy = v2d->tot.ymax - sy;

		file = filelist_file(files, i);	

		if (params->active_file == i) {
			if (file->flags & ACTIVE) colorid= TH_HILITE;
			else colorid = TH_BACK;
			draw_tile(sx-2, sy-3, layout->tile_w+2, sfile->layout->tile_h+layout->tile_border_y, colorid,20);
		} else if (file->flags & ACTIVE) {
			colorid = TH_HILITE;
			draw_tile(sx-2, sy-3, layout->tile_w+2, sfile->layout->tile_h+layout->tile_border_y, colorid,0);
		} else {
			/*
			colorid = TH_PANEL;
			draw_tile(sx, sy, sfile->tile_w, sfile->tile_h, colorid);
			*/
		}

		spos = sx;
		file_draw_icon(spos, sy-3, get_file_icon(file), ICON_DEFAULT_WIDTH, ICON_DEFAULT_WIDTH);
		spos += ICON_DEFAULT_WIDTH + 4;
		
		UI_ThemeColor4(TH_TEXT);
		
		sw = shorten_filesize(file->size, FILE_SHORTEN_FSIZE_WIDTHONLY);
		file_draw_string(spos, sy, file->relname, layout->tile_w - sw - 5, layout->tile_h, FILE_SHORTEN_END);
		
		spos += filelist_maxnamelen(sfile->files);
		if (params->display == FILE_SHOWSHORT) {
			if (!(file->type & S_IFDIR))
				file_draw_string(sx + layout->tile_w - layout->tile_border_x - sw, sy, file->size, sw, layout->tile_h, FILE_SHORTEN_FSIZE);
		} else {
#if 0 // XXX TODO: add this for non-windows systems
			/* rwx rwx rwx */
			spos += 20;
			sw = UI_GetStringWidth(G.font, file->mode1, 0);
			file_draw_string(spos, sy, file->mode1, sw, layout->tile_h); 
			
			spos += 30;
			sw = UI_GetStringWidth(G.font, file->mode2, 0);
			file_draw_string(spos, sy, file->mode2, sw, layout->tile_h);

			spos += 30;
			sw = UI_GetStringWidth(G.font, file->mode3, 0);
			file_draw_string(spos, sy, file->mode3, sw, layout->tile_h);
			
			spos += 30;
			sw = UI_GetStringWidth(G.font, file->owner, 0);
			file_draw_string(spos, sy, file->owner, sw, layout->tile_h);
#endif

			spos += 50;
			sw = UI_GetStringWidth(G.font, file->date, 0);
			file_draw_string(spos, sy, file->date, sw, layout->tile_h, FILE_SHORTEN_END);

			spos += 100;
			sw = UI_GetStringWidth(G.font, file->time, 0);
			file_draw_string(spos, sy, file->time, sw, layout->tile_h, FILE_SHORTEN_END); 
			
			if (!(file->type & S_IFDIR)) {
				sw = UI_GetStringWidth(G.font, file->size, 0);
				spos += 200-sw;
				file_draw_string(spos, sy, file->size, sw, layout->tile_h, FILE_SHORTEN_END);
			}
		}
	}
}

static void file_draw_fsmenu_category(const bContext *C, ARegion *ar, FSMenuCategory category, const char* category_name, short *starty)
{
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	struct FSMenu* fsmenu = fsmenu_get();
	char bookmark[FILE_MAX];
	int nentries = fsmenu_get_nentries(fsmenu, category);
	int linestep = gFontsize*2.0f;
	short sx, sy, xpos, ypos;
	int bmwidth = ar->v2d.cur.xmax - ar->v2d.cur.xmin - 2*TILE_BORDER_X - ICON_DEFAULT_WIDTH - 4;
	int fontsize = gFontsize;
	int i;

	
	sx = ar->v2d.cur.xmin + TILE_BORDER_X;
	sy = *starty;

	UI_ThemeColor(TH_TEXT_HI);
	file_draw_string(sx, sy, category_name, bmwidth, fontsize, FILE_SHORTEN_END);
	
	sy -= linestep;

	for (i=0; i< nentries && (sy > ar->v2d.cur.ymin) ;++i) {
		char *fname = fsmenu_get_entry(fsmenu, category, i);

		if (fname) {
			int sl;
			BLI_strncpy(bookmark, fname, FILE_MAX);
		
			sl = strlen(bookmark)-1;
			while (bookmark[sl] == '\\' || bookmark[sl] == '/') {
				bookmark[sl] = '\0';
				sl--;
			}
			if (fsmenu_is_selected(fsmenu, category, i) ) {
				UI_ThemeColor(TH_HILITE);
				/* uiSetRoundBox(15);	
				 * uiRoundBox(sx, sy - linestep, sx + bmwidth, sy, 4.0f); */
				glRectf(ar->v2d.cur.xmin, sy-linestep, ar->v2d.cur.xmax + 2*TILE_BORDER_X, sy);
				UI_ThemeColor(TH_TEXT);
			} else {
				UI_ThemeColor(TH_TEXT_HI);
			}

			xpos = sx;
			ypos = sy - (TILE_BORDER_Y * 0.5);
			
			file_draw_icon(xpos, ypos, ICON_FILE_FOLDER, ICON_DEFAULT_WIDTH, ICON_DEFAULT_WIDTH);
			xpos += ICON_DEFAULT_WIDTH + 4;
			file_draw_string(xpos, ypos, bookmark, bmwidth, fontsize, FILE_SHORTEN_FRONT);
			sy -= linestep;
			fsmenu_set_pos(fsmenu, category, i, xpos, ypos);
		}
	}

	*starty = sy;
}

void file_draw_fsmenu(const bContext *C, ARegion *ar)
{
	int linestep = gFontsize*2.0f;
	short sy= ar->v2d.cur.ymax-2*TILE_BORDER_Y;
	
	file_draw_fsmenu_category(C, ar, FS_CATEGORY_SYSTEM, "SYSTEM", &sy);
	sy -= linestep;
	file_draw_fsmenu_category(C, ar, FS_CATEGORY_BOOKMARKS, "BOOKMARKS", &sy);
	sy -= linestep;
	file_draw_fsmenu_category(C, ar, FS_CATEGORY_RECENT, "RECENT", &sy);
	
}
