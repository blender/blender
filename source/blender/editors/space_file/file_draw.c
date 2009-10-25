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

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BLF_api.h"

#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_datafiles.h"

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
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "fsmenu.h"
#include "filelist.h"

#include "file_intern.h"	// own include

/* ui geometry */
#define IMASEL_BUTTONS_HEIGHT 40
#define IMASEL_BUTTONS_MARGIN 6
#define TILE_BORDER_X 8
#define TILE_BORDER_Y 8

/* button events */
enum {
	B_REDR 	= 0,
	B_FS_EXEC,
	B_FS_CANCEL,
	B_FS_PARENT,
	B_FS_DIRNAME,
	B_FS_FILENAME
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
		case B_FS_FILENAME:
			file_filename_exec(C, NULL);
			break;
		case B_FS_DIRNAME:
			file_directory_exec(C, NULL);
			break;
	}
}

/* Note: This function uses pixelspace (0, 0, winx, winy), not view2d. 
 * The controls are laid out as follows:
 *
 * -------------------------------------------
 * | Directory input               | execute |
 * -------------------------------------------
 * | Filename input        | + | - | cancel  |
 * -------------------------------------------
 *
 * The input widgets will stretch to fill any excess space.
 * When there isn't enough space for all controls to be shown, they are
 * hidden in this order: x/-, execute/cancel, input widgets.
 */
void file_draw_buttons(const bContext *C, ARegion *ar)
{
	/* Button layout. */
	const int min_x      = 10;
	const int max_x      = ar->winx - 10;
	const int line1_y    = IMASEL_BUTTONS_HEIGHT/2 + IMASEL_BUTTONS_MARGIN*2;
	const int line2_y    = IMASEL_BUTTONS_MARGIN;
	const int input_minw = 20;
	const int btn_h      = UI_UNIT_Y;
	const int btn_fn_w   = UI_UNIT_X;
	const int btn_minw   = 80;
	const int btn_margin = 20;
	const int separator  = 4;

	/* Additional locals. */
	char  name[20];
	int loadbutton;
	int fnumbuttons;
	int available_w = max_x - min_x;
	int line1_w     = available_w;
	int line2_w     = available_w;
	
	uiBut*            but;
	uiBlock*          block;
	SpaceFile*        sfile  = CTX_wm_space_file(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	
	/* Initialize UI block. */
	sprintf(name, "win %p", ar);
	block = uiBeginBlock(C, ar, name, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_file_buttons, NULL);
	
	/* Is there enough space for the execute / cancel buttons? */
	loadbutton = UI_GetStringWidth(sfile->params->title) + btn_margin;
	if (loadbutton < btn_minw) {
		loadbutton = MAX2(btn_minw, 
		                  btn_margin + UI_GetStringWidth(params->title));
	}
	
	if (available_w <= loadbutton + separator + input_minw 
	 || params->title[0] == 0) {
		loadbutton = 0;
	} else {
		line1_w -= (loadbutton + separator);
		line2_w  = line1_w;
	}

	/* Is there enough space for file number increment/decrement buttons? */
	fnumbuttons = 2 * btn_fn_w;
	if (!loadbutton || line2_w <= fnumbuttons + separator + input_minw) {
		fnumbuttons = 0;
	} else {
		line2_w -= (fnumbuttons + separator);
	}
	
	/* Text input fields for directory and file. */
	if (available_w > 0) {
		but = uiDefBut(block, TEX, B_FS_DIRNAME, "",
		         min_x, line1_y, line1_w, btn_h, 
		         params->dir, 0.0, (float)FILE_MAX-1, 0, 0, 
		         "File path.");
		uiButSetCompleteFunc(but, autocomplete_directory, NULL);
		uiDefBut(block, TEX, B_FS_FILENAME, "",
		         min_x, line2_y, line2_w, btn_h,
		         params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, 
		         "File name.");
	}
	
	/* Filename number increment / decrement buttons. */
	if (fnumbuttons) {
		uiBlockBeginAlign(block);
		but = uiDefIconButO(block, BUT, "FILE_OT_filenum", 0, ICON_ZOOMOUT,
		        min_x + line2_w + separator, line2_y, 
		        btn_fn_w, btn_h, 
		        "Decrement the filename number.");    
		RNA_int_set(uiButGetOperatorPtrRNA(but), "increment", -1); 
	
		but = uiDefIconButO(block, BUT, "FILE_OT_filenum", 0, ICON_ZOOMIN, 
		        min_x + line2_w + separator + btn_fn_w, line2_y, 
		        btn_fn_w, btn_h, 
		        "Increment the filename number.");    
		RNA_int_set(uiButGetOperatorPtrRNA(but), "increment", 1); 
		uiBlockEndAlign(block);
	}
	
	/* Execute / cancel buttons. */
	if(loadbutton) {
		uiDefBut(block, BUT, B_FS_EXEC, params->title, 
		  max_x - loadbutton, line1_y, loadbutton, btn_h, 
		  params->dir, 0.0, (float)FILE_MAXFILE-1, 0, 0, params->title);
		uiDefBut(block, BUT, B_FS_CANCEL, "Cancel", 
		  max_x - loadbutton, line2_y, loadbutton, btn_h, 
		  params->file, 0.0, (float)FILE_MAXFILE-1, 0, 0, "Cancel.");
	}
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


static void draw_tile(int sx, int sy, int width, int height, int colorid, int shade)
{	
	UI_ThemeColorShade(colorid, shade);
	uiSetRoundBox(15);	
	uiRoundBox(sx, sy - height, sx + width, sy, 6);
}

#define FILE_SHORTEN_END				0
#define FILE_SHORTEN_FRONT				1

static float shorten_string(char* string, float w, int flag)
{	
	char temp[FILE_MAX];
	short shortened = 0;
	float sw = 0;
	float pad = 0;

	if (w <= 0) {
		*string = '\0';
		return 0.0;
	}

	sw = file_string_width(string);
	if (flag == FILE_SHORTEN_FRONT) {
		char *s = string;
		BLI_strncpy(temp, "...", 4);
		pad = file_string_width(temp);
		while ((*s) && (sw+pad>w)) {
			s++;
			sw = file_string_width(s);
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
			sw = file_string_width(s);
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

static void file_draw_icon(int sx, int sy, int icon, int width, int height)
{
	float x,y;
	float alpha=1.0f;
	
	x = (float)(sx);
	y = (float)(sy-height);
	
	if (icon == ICON_FILE_BLANK) alpha = 0.375f;
	
	glEnable(GL_BLEND);
	
	UI_icon_draw_aspect(x, y, icon, 1.f, alpha);
}


static void file_draw_string(int sx, int sy, const char* string, float width, int height, int flag)
{
	int soffs;
	char fname[FILE_MAXFILE];
	float sw;
	float x,y;

	BLI_strncpy(fname,string, FILE_MAXFILE);
	sw = shorten_string(fname, width, flag );

	soffs = (width - sw) / 2;
	x = (float)(sx);
	y = (float)(sy-height);

	BLF_position(x, y, 0);
	BLF_draw(fname);
}

void file_calc_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	View2D *v2d= &ar->v2d;
	
	ED_fileselect_init_layout(sfile, ar);
	/* +SCROLL_HEIGHT is bad hack to work around issue in UI_view2d_totRect_set */
	UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height+V2D_SCROLL_HEIGHT);
}

static void file_draw_preview(int sx, int sy, ImBuf *imb, FileLayout *layout, short dropshadow)
{
	if (imb) {
			float fx, fy;
			float dx, dy;
			int xco, yco;
			float scaledx, scaledy;
			float scale;
			int ex, ey;

			if ( (imb->x > layout->prv_w) || (imb->y > layout->prv_h) ) {
				if (imb->x > imb->y) {
					scaledx = (float)layout->prv_w;
					scaledy =  ( (float)imb->y/(float)imb->x )*layout->prv_w;
					scale = scaledx/imb->x;
				}
				else {
					scaledy = (float)layout->prv_h;
					scaledx =  ( (float)imb->x/(float)imb->y )*layout->prv_h;
					scale = scaledy/imb->y;
				}
			} else {
				scaledx = (float)imb->x;
				scaledy = (float)imb->y;
				scale = 1.0;
			}
			ex = (int)scaledx;
			ey = (int)scaledy;
			fx = ((float)layout->prv_w - (float)ex)/2.0f;
			fy = ((float)layout->prv_h - (float)ey)/2.0f;
			dx = (fx + 0.5f + layout->prv_border_x);
			dy = (fy + 0.5f - layout->prv_border_y);
			xco = (float)sx + dx;
			yco = (float)sy - layout->prv_h + dy;

			glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);
			
			/* shadow */
			if (dropshadow)
				uiDrawBoxShadow(220, xco, yco, xco + ex, yco + ey);
			
			glEnable(GL_BLEND);
			
			/* the image */
			glColor4f(1.0, 1.0, 1.0, 1.0);
			glaDrawPixelsTexScaled(xco, yco, imb->x, imb->y, GL_UNSIGNED_BYTE, imb->rect, scale, scale);
			
			/* border */
			if (dropshadow) {
				glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
				fdrawbox(xco, yco, xco + ex, yco + ey);
			}
			
			glDisable(GL_BLEND);
			imb = 0;
		}
}

static void renamebutton_cb(bContext *C, void *arg1, char *oldname)
{
	char newname[FILE_MAX+12];
	char orgname[FILE_MAX+12];
	char filename[FILE_MAX+12];
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	ARegion* ar = CTX_wm_region(C);

	struct direntry *file = (struct direntry *)arg1;

	BLI_make_file_string(G.sce, orgname, sfile->params->dir, oldname);
	BLI_strncpy(filename, file->relname, sizeof(filename));
	BLI_make_file_string(G.sce, newname, sfile->params->dir, filename);

	if( strcmp(orgname, newname) != 0 ) {
		if (!BLI_exists(newname)) {
			BLI_rename(orgname, newname);
			/* to make sure we show what is on disk */
			filelist_free(sfile->files);
		} else {
			BLI_strncpy(file->relname, oldname, strlen(oldname)+1);
		}
		
		ED_region_tag_redraw(ar);
	}
}


static void draw_background(FileLayout *layout, View2D *v2d)
{
	int i;
	int sy;

	/* alternating flat shade background */
	for (i=0; (i <= layout->rows); i+=2)
	{
		sy = v2d->cur.ymax - i*(layout->tile_h+2*layout->tile_border_y) - layout->tile_border_y;

		UI_ThemeColorShade(TH_BACK, -7);
		glRectf(v2d->cur.xmin, sy, v2d->cur.xmax, sy+layout->tile_h+2*layout->tile_border_y);
		
	}
}

static void draw_dividers(FileLayout *layout, View2D *v2d)
{
	int sx;

	/* vertical column dividers */
	sx = v2d->tot.xmin;
	while (sx < v2d->cur.xmax) {
		sx += (layout->tile_w+2*layout->tile_border_x);
		
		UI_ThemeColorShade(TH_BACK, 30);
		sdrawline(sx+1,  v2d->cur.ymax - layout->tile_border_y ,  sx+1,  v2d->cur.ymin); 
		UI_ThemeColorShade(TH_BACK, -30);
		sdrawline(sx,  v2d->cur.ymax - layout->tile_border_y ,  sx,  v2d->cur.ymin); 
	}
}

void file_draw_list(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	FileLayout* layout= ED_fileselect_get_layout(sfile, ar);
	View2D *v2d= &ar->v2d;
	struct FileList* files = sfile->files;
	struct direntry *file;
	ImBuf *imb;
	int numfiles;
	int numfiles_layout;
	int colorid = 0;
	int sx, sy;
	int offset;
	int i;
	float sw, spos;
	short is_icon;

	numfiles = filelist_numfiles(files);
	
	if (params->display != FILE_IMGDISPLAY) {

		draw_background(layout, v2d);
	
		draw_dividers(layout, v2d);
	}

	offset = ED_fileselect_layout_offset(layout, ar->v2d.cur.xmin, -ar->v2d.cur.ymax);
	if (offset<0) offset=0;

	numfiles_layout = ED_fileselect_layout_numfiles(layout, ar);

	for (i=offset; (i < numfiles) && (i<offset+numfiles_layout); ++i)
	{
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx += v2d->tot.xmin+2;
		sy = v2d->tot.ymax - sy;

		file = filelist_file(files, i);	
		
		if (!(file->flags & EDITING)) {
			if (params->active_file == i) {
				if (file->flags & ACTIVE) colorid= TH_HILITE;
				else colorid = TH_BACK;
				draw_tile(sx-2, sy-3, layout->tile_w+2, sfile->layout->tile_h+layout->tile_border_y, colorid,20);
			} else if (file->flags & ACTIVE) {
				colorid = TH_HILITE;
				draw_tile(sx-2, sy-3, layout->tile_w+2, sfile->layout->tile_h+layout->tile_border_y, colorid,0);
			} 
		}

		spos = sx;

		if ( FILE_IMGDISPLAY == params->display ) {
			if ( (file->flags & IMAGEFILE) /* || (file->flags & MOVIEFILE) */) {			
				filelist_loadimage(files, i);
			}
			is_icon = 0;
			imb = filelist_getimage(files, i);
			if (!imb) {
				imb = filelist_geticon(files,i);
				is_icon = 1;
			}
			
			file_draw_preview(sx, sy, imb, layout, !is_icon && (file->flags & IMAGEFILE));

		} else {
				file_draw_icon(spos, sy-3, get_file_icon(file), ICON_DEFAULT_WIDTH, ICON_DEFAULT_WIDTH);
				spos += ICON_DEFAULT_WIDTH + 4;
		}

		UI_ThemeColor4(TH_TEXT);

		sw = file_string_width(file->relname);
		if (file->flags & EDITING) {
			int but_width = (FILE_IMGDISPLAY == params->display) ? layout->tile_w : layout->column_widths[COLUMN_NAME];
			uiBlock *block = uiBeginBlock(C, ar, "FileName", UI_EMBOSS);
			uiBut *but = uiDefBut(block, TEX, 1, "", spos, sy-layout->tile_h-3, 
				but_width, layout->textheight*2, file->relname, 1.0f, (float)FILE_MAX,0,0,"");
			uiButSetRenameFunc(but, renamebutton_cb, file);
			if ( 0 == uiButActiveOnly(C, block, but)) {
				file->flags &= ~EDITING;
			}
			uiEndBlock(C, block);
			uiDrawBlock(C, block);
		} else {
			float name_width = (FILE_IMGDISPLAY == params->display) ? layout->tile_w : sw;
			file_draw_string(spos, sy, file->relname, name_width, layout->tile_h, FILE_SHORTEN_END);
		}

		uiSetRoundBox(0);

		if (params->display == FILE_SHORTDISPLAY) {
			spos += layout->column_widths[COLUMN_NAME] + 12;
			if (!(file->type & S_IFDIR)) {
				sw = file_string_width(file->size);
				file_draw_string(spos, sy, file->size, sw+1, layout->tile_h, FILE_SHORTEN_END);	
				spos += layout->column_widths[COLUMN_SIZE] + 12;
			}
		} else if (params->display == FILE_LONGDISPLAY) {
			spos += layout->column_widths[COLUMN_NAME] + 12;

#ifndef WIN32
			/* rwx rwx rwx */
			sw = file_string_width(file->mode1);
			file_draw_string(spos, sy, file->mode1, sw, layout->tile_h, FILE_SHORTEN_END); 
			spos += layout->column_widths[COLUMN_MODE1] + 12;

			sw = file_string_width(file->mode2);
			file_draw_string(spos, sy, file->mode2, sw, layout->tile_h, FILE_SHORTEN_END);
			spos += layout->column_widths[COLUMN_MODE2] + 12;

			sw = file_string_width(file->mode3);
			file_draw_string(spos, sy, file->mode3, sw, layout->tile_h, FILE_SHORTEN_END);
			spos += layout->column_widths[COLUMN_MODE3] + 12;

			sw = file_string_width(file->owner);
			file_draw_string(spos, sy, file->owner, sw, layout->tile_h, FILE_SHORTEN_END);
			spos += layout->column_widths[COLUMN_OWNER] + 12;
#endif

			sw = file_string_width(file->date);
			file_draw_string(spos, sy, file->date, sw, layout->tile_h, FILE_SHORTEN_END);
			spos += layout->column_widths[COLUMN_DATE] + 12;

			sw = file_string_width(file->time);
			file_draw_string(spos, sy, file->time, sw, layout->tile_h, FILE_SHORTEN_END); 
			spos += layout->column_widths[COLUMN_TIME] + 12;

			if (!(file->type & S_IFDIR)) {
				sw = file_string_width(file->size);
				file_draw_string(spos, sy, file->size, sw, layout->tile_h, FILE_SHORTEN_END);
				spos += layout->column_widths[COLUMN_SIZE] + 12;
			}
		}
	}

	if (!sfile->loadimage_timer)
		sfile->loadimage_timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER1, 1.0/30.0);	/* max 30 frames/sec. */
}


