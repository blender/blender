/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/file_draw.c
 *  \ingroup spfile
 */


#include <math.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BLF_api.h"
#include "BLF_translation.h"

#include "IMB_imbuf_types.h"
 
#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "RNA_access.h"

#include "ED_fileselect.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_types.h"

#include "fsmenu.h"
#include "filelist.h"

#include "file_intern.h"	// own include

/* button events */
enum {
	B_FS_DIRNAME,
	B_FS_FILENAME
} /*eFile_ButEvents*/;


static void do_file_buttons(bContext *C, void *UNUSED(arg), int event)
{
	switch(event) {
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
	const int max_x      = ar->winx - 10;
	const int line1_y    = ar->winy - (IMASEL_BUTTONS_HEIGHT/2 + IMASEL_BUTTONS_MARGIN);
	const int line2_y    = line1_y - (IMASEL_BUTTONS_HEIGHT/2 + IMASEL_BUTTONS_MARGIN);
	const int input_minw = 20;
	const int btn_h      = UI_UNIT_Y;
	const int btn_fn_w   = UI_UNIT_X;
	const int btn_minw   = 80;
	const int btn_margin = 20;
	const int separator  = 4;

	/* Additional locals. */
	char uiblockstr[32];
	int loadbutton;
	int fnumbuttons;
	int min_x       = 10;
	int chan_offs	= 0;
	int available_w = max_x - min_x;
	int line1_w     = available_w;
	int line2_w     = available_w;
	
	uiBut*            but;
	uiBlock*          block;
	SpaceFile*        sfile  = CTX_wm_space_file(C);
	FileSelectParams* params = ED_fileselect_get_params(sfile);
	ARegion*		  artmp;
	
	/* Initialize UI block. */
	BLI_snprintf(uiblockstr, sizeof(uiblockstr), "win %p", (void *)ar);
	block = uiBeginBlock(C, ar, uiblockstr, UI_EMBOSS);
	uiBlockSetHandleFunc(block, do_file_buttons, NULL);

	/* exception to make space for collapsed region icon */
	for (artmp=CTX_wm_area(C)->regionbase.first; artmp; artmp=artmp->next) {
		if (artmp->regiontype == RGN_TYPE_CHANNELS && artmp->flag & RGN_FLAG_HIDDEN) {
			chan_offs = 16;
			min_x += chan_offs;
			available_w -= chan_offs;
		}
	}
	
	/* Is there enough space for the execute / cancel buttons? */
	loadbutton = UI_GetStringWidth(sfile->params->title) + btn_margin;
	if (loadbutton < btn_minw) {
		loadbutton = MAX2(btn_minw, 
						  btn_margin + UI_GetStringWidth(params->title));
	}
	
	if (available_w <= loadbutton + separator + input_minw 
	 || params->title[0] == 0) {
		loadbutton = 0;
	}
	else {
		line1_w -= (loadbutton + separator);
		line2_w  = line1_w;
	}

	/* Is there enough space for file number increment/decrement buttons? */
	fnumbuttons = 2 * btn_fn_w;
	if (!loadbutton || line2_w <= fnumbuttons + separator + input_minw) {
		fnumbuttons = 0;
	}
	else {
		line2_w -= (fnumbuttons + separator);
	}
	
	/* Text input fields for directory and file. */
	if (available_w > 0) {
		int overwrite_alert= file_draw_check_exists(sfile);
		/* callbacks for operator check functions */
		uiBlockSetFunc(block, file_draw_check_cb, NULL, NULL);

		but = uiDefButTextO(block, TEX, "FILE_OT_directory", 0, "",
		                    min_x, line1_y, line1_w-chan_offs, btn_h,
		                    params->dir, 0.0, (float)FILE_MAX, 0, 0,
		                    TIP_("File path"));
		uiButSetCompleteFunc(but, autocomplete_directory, NULL);
		uiButSetFlag(but, UI_BUT_NO_UTF8);

		if ((params->flag & FILE_DIRSEL_ONLY) == 0) {
			but = uiDefBut(block, TEX, B_FS_FILENAME, "",
			               min_x, line2_y, line2_w-chan_offs, btn_h,
			               params->file, 0.0, (float)FILE_MAXFILE, 0, 0,
			               TIP_(overwrite_alert ?N_("File name, overwrite existing") : N_("File name")));
			uiButSetCompleteFunc(but, autocomplete_file, NULL);
			uiButSetFlag(but, UI_BUT_NO_UTF8);
			uiButClearFlag(but, UI_BUT_UNDO); /* operator button above does this automatic */

			/* check if this overrides a file and if the operator option is used */
			if (overwrite_alert) {
				uiButSetFlag(but, UI_BUT_REDALERT);
			}
		}
		
		/* clear func */
		uiBlockSetFunc(block, NULL, NULL, NULL);
	}
	
	/* Filename number increment / decrement buttons. */
	if (fnumbuttons && (params->flag & FILE_DIRSEL_ONLY) == 0) {
		uiBlockBeginAlign(block);
		but = uiDefIconButO(block, BUT, "FILE_OT_filenum", 0, ICON_ZOOMOUT,
		                    min_x + line2_w + separator - chan_offs, line2_y,
		                    btn_fn_w, btn_h,
		                    TIP_("Decrement the filename number"));
		RNA_int_set(uiButGetOperatorPtrRNA(but), "increment", -1);

		but = uiDefIconButO(block, BUT, "FILE_OT_filenum", 0, ICON_ZOOMIN,
		                    min_x + line2_w + separator + btn_fn_w - chan_offs, line2_y,
		                    btn_fn_w, btn_h,
		                    TIP_("Increment the filename number"));
		RNA_int_set(uiButGetOperatorPtrRNA(but), "increment", 1);
		uiBlockEndAlign(block);
	}
	
	/* Execute / cancel buttons. */
	if (loadbutton) {
		/* params->title is already translated! */
		uiDefButO(block, BUT, "FILE_OT_execute", WM_OP_EXEC_REGION_WIN, params->title,
		          max_x - loadbutton, line1_y, loadbutton, btn_h, "");
		uiDefButO(block, BUT, "FILE_OT_cancel", WM_OP_EXEC_REGION_WIN, IFACE_("Cancel"),
		          max_x - loadbutton, line2_y, loadbutton, btn_h, "");
	}
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
}


static void draw_tile(int sx, int sy, int width, int height, int colorid, int shade)
{
	UI_ThemeColorShade(colorid, shade);
	uiSetRoundBox(UI_CNR_ALL);
	uiRoundBox((float)sx, (float)(sy - height), (float)(sx + width), (float)sy, 5.0f);
}


static int get_file_icon(struct direntry *file)
{
	if (file->type & S_IFDIR) {
		if ( strcmp(file->relname, "..") == 0) {
				return  ICON_FILE_PARENT;
		}
		if (file->flags & BLENDERFILE) {
			return ICON_FILE_BLEND;
		}
		return ICON_FILE_FOLDER;
	}
	else if (file->flags & BLENDERFILE)
		return ICON_FILE_BLEND;
	else if (file->flags & BLENDERFILE_BACKUP)
		return ICON_FILE_BLEND;
	else if (file->flags & IMAGEFILE)
		return ICON_FILE_IMAGE;
	else if (file->flags & MOVIEFILE)
		return ICON_FILE_MOVIE;
	else if (file->flags & PYSCRIPTFILE)
		return ICON_FILE_SCRIPT;
	else if (file->flags & SOUNDFILE) 
		return ICON_FILE_SOUND;
	else if (file->flags & FTFONTFILE) 
		return ICON_FILE_FONT;
	else if (file->flags & BTXFILE) 
		return ICON_FILE_BLANK;
	else if (file->flags & COLLADAFILE) 
		return ICON_FILE_BLANK;
	else
		return ICON_FILE_BLANK;
}

static void file_draw_icon(uiBlock *block, char *path, int sx, int sy, int icon, int width, int height)
{
	uiBut *but;
	int x,y;
	/*float alpha=1.0f;*/
	
	x = sx;
	y = sy-height;
	
	/*if (icon == ICON_FILE_BLANK) alpha = 0.375f;*/

	but = uiDefIconBut(block, LABEL, 0, icon, x, y, width, height, NULL, 0.0f, 0.0f, 0.0f, 0.0f, "");
	uiButSetDragPath(but, path);
}


static void file_draw_string(int sx, int sy, const char* string, float width, int height, short align)
{
	uiStyle *style= UI_GetStyle();
	uiFontStyle fs = style->widgetlabel;
	rcti rect;
	char fname[FILE_MAXFILE];

	fs.align = align;

	BLI_strncpy(fname,string, FILE_MAXFILE);
	file_shorten_string(fname, width + 1.0f, 0);

	/* no text clipping needed, uiStyleFontDraw does it but is a bit too strict (for buttons it works) */
	rect.xmin = sx;
	rect.xmax = (int)(sx + ceil(width+4.0f));
	rect.ymin = sy - height;
	rect.ymax = sy;
	
	uiStyleFontDraw(&fs, &rect, fname);
}

void file_calc_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile= CTX_wm_space_file(C);
	View2D *v2d= &ar->v2d;
	
	ED_fileselect_init_layout(sfile, ar);
	/* +SCROLL_HEIGHT is bad hack to work around issue in UI_view2d_totRect_set */
	UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height+V2D_SCROLL_HEIGHT);
}

static void file_draw_preview(uiBlock *block, struct direntry *file, int sx, int sy, ImBuf *imb, FileLayout *layout, short dropshadow)
{
	if (imb) {
		uiBut *but;
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
		}
		else {
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
		xco = sx + (int)dx;
		yco = sy - layout->prv_h + (int)dy;
		
		glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);
		
		/* shadow */
		if (dropshadow)
			uiDrawBoxShadow(220, (float)xco, (float)yco, (float)(xco + ex), (float)(yco + ey));

		glEnable(GL_BLEND);
		
		/* the image */
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glaDrawPixelsTexScaled((float)xco, (float)yco, imb->x, imb->y, GL_UNSIGNED_BYTE, imb->rect, scale, scale);
		
		/* border */
		if (dropshadow) {
			glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
			fdrawbox((float)xco, (float)yco, (float)(xco + ex), (float)(yco + ey));
		}
		
		/* dragregion */
		but = uiDefBut(block, LABEL, 0, "", xco, yco, ex, ey, NULL, 0.0, 0.0, 0, 0, "");
		uiButSetDragImage(but, file->path, get_file_icon(file), imb, scale);
		
		glDisable(GL_BLEND);
		imb = NULL;
	}
}

static void renamebutton_cb(bContext *C, void *UNUSED(arg1), char *oldname)
{
	char newname[FILE_MAX+12];
	char orgname[FILE_MAX+12];
	char filename[FILE_MAX+12];
	SpaceFile *sfile= (SpaceFile*)CTX_wm_space_data(C);
	ARegion* ar = CTX_wm_region(C);

	BLI_make_file_string(G.main->name, orgname, sfile->params->dir, oldname);
	BLI_strncpy(filename, sfile->params->renameedit, sizeof(filename));
	BLI_make_file_string(G.main->name, newname, sfile->params->dir, filename);

	if ( strcmp(orgname, newname) != 0 ) {
		if (!BLI_exists(newname)) {
			BLI_rename(orgname, newname);
			/* to make sure we show what is on disk */
			ED_fileselect_clear(C, sfile);
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
		sy = (int)v2d->cur.ymax - i*(layout->tile_h+2*layout->tile_border_y) - layout->tile_border_y;

		UI_ThemeColorShade(TH_BACK, -7);
		glRectf(v2d->cur.xmin, (float)sy, v2d->cur.xmax, (float)(sy+layout->tile_h+2*layout->tile_border_y));
		
	}
}

static void draw_dividers(FileLayout *layout, View2D *v2d)
{
	int sx;

	/* vertical column dividers */
	sx = (int)v2d->tot.xmin;
	while (sx < v2d->cur.xmax) {
		sx += (layout->tile_w+2*layout->tile_border_x);
		
		UI_ThemeColorShade(TH_BACK, 30);
		sdrawline(sx+1, (short)(v2d->cur.ymax - layout->tile_border_y),  sx+1,  (short)v2d->cur.ymin); 
		UI_ThemeColorShade(TH_BACK, -30);
		sdrawline(sx, (short)(v2d->cur.ymax - layout->tile_border_y),  sx,  (short)v2d->cur.ymin); 
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
	uiBlock *block = uiBeginBlock(C, ar, __func__, UI_EMBOSS);
	int numfiles;
	int numfiles_layout;
	int sx, sy;
	int offset;
	int textwidth, textheight;
	int i;
	short is_icon;
	short align;


	numfiles = filelist_numfiles(files);
	
	if (params->display != FILE_IMGDISPLAY) {

		draw_background(layout, v2d);
	
		draw_dividers(layout, v2d);
	}

	offset = ED_fileselect_layout_offset(layout, (int)ar->v2d.cur.xmin, (int)-ar->v2d.cur.ymax);
	if (offset<0) offset=0;

	numfiles_layout = ED_fileselect_layout_numfiles(layout, ar);

	/* adjust, so the next row is already drawn when scrolling */
	if (layout->flag & FILE_LAYOUT_HOR) {
		numfiles_layout += layout->rows;
	}
	else {
		numfiles_layout += layout->columns;
	}

	textwidth =( FILE_IMGDISPLAY == params->display) ? layout->tile_w : (int)layout->column_widths[COLUMN_NAME];
	textheight = (int)(layout->textheight*3.0/2.0 + 0.5);

	align = ( FILE_IMGDISPLAY == params->display) ? UI_STYLE_TEXT_CENTER : UI_STYLE_TEXT_LEFT;

	for (i=offset; (i < numfiles) && (i<offset+numfiles_layout); ++i)
	{
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx += (int)(v2d->tot.xmin+2.0f);
		sy = (int)(v2d->tot.ymax - sy);

		file = filelist_file(files, i);	
		
		UI_ThemeColor4(TH_TEXT);


		if (!(file->selflag & EDITING_FILE)) {
			if ((params->active_file == i) || (file->selflag & HILITED_FILE) || (file->selflag & SELECTED_FILE)) {
				int colorid = (file->selflag & SELECTED_FILE) ? TH_HILITE : TH_BACK;
				int shade = (params->active_file == i) || (file->selflag & HILITED_FILE) ? 20 : 0;
				draw_tile(sx, sy-1, layout->tile_w+4, sfile->layout->tile_h+layout->tile_border_y, colorid, shade);
			}
		}
		uiSetRoundBox(UI_CNR_NONE);

		if ( FILE_IMGDISPLAY == params->display ) {
			is_icon = 0;
			imb = filelist_getimage(files, i);
			if (!imb) {
				imb = filelist_geticon(files,i);
				is_icon = 1;
			}
			
			file_draw_preview(block, file, sx, sy, imb, layout, !is_icon && (file->flags & IMAGEFILE));
		}
		else {
			file_draw_icon(block, file->path, sx, sy-(UI_UNIT_Y / 6), get_file_icon(file), ICON_DEFAULT_WIDTH_SCALE, ICON_DEFAULT_HEIGHT_SCALE);
			sx += ICON_DEFAULT_WIDTH_SCALE + 4;
		}

		UI_ThemeColor4(TH_TEXT);

		if (file->selflag & EDITING_FILE) {
			uiBut *but = uiDefBut(block, TEX, 1, "", sx , sy-layout->tile_h-3, 
				textwidth, textheight, sfile->params->renameedit, 1.0f, (float)sizeof(sfile->params->renameedit),0,0,"");
			uiButSetRenameFunc(but, renamebutton_cb, file);
			uiButSetFlag(but, UI_BUT_NO_UTF8); /* allow non utf8 names */
			uiButClearFlag(but, UI_BUT_UNDO);
			if ( 0 == uiButActiveOnly(C, block, but)) {
				file->selflag &= ~EDITING_FILE;
			}
		}

		if (!(file->selflag & EDITING_FILE)) {
			int tpos = (FILE_IMGDISPLAY == params->display) ? sy - layout->tile_h + layout->textheight : sy;
			file_draw_string(sx+1, tpos, file->relname, (float)textwidth, textheight, align);
		}

		if (params->display == FILE_SHORTDISPLAY) {
			sx += (int)layout->column_widths[COLUMN_NAME] + 12;
			if (!(file->type & S_IFDIR)) {
				file_draw_string(sx, sy, file->size, layout->column_widths[COLUMN_SIZE], layout->tile_h, align);	
				sx += (int)layout->column_widths[COLUMN_SIZE] + 12;
			}
		}
		else if (params->display == FILE_LONGDISPLAY) {
			sx += (int)layout->column_widths[COLUMN_NAME] + 12;

#ifndef WIN32
			/* rwx rwx rwx */
			file_draw_string(sx, sy, file->mode1, layout->column_widths[COLUMN_MODE1], layout->tile_h, align); 
			sx += layout->column_widths[COLUMN_MODE1] + 12;

			file_draw_string(sx, sy, file->mode2, layout->column_widths[COLUMN_MODE2], layout->tile_h, align);
			sx += layout->column_widths[COLUMN_MODE2] + 12;

			file_draw_string(sx, sy, file->mode3, layout->column_widths[COLUMN_MODE3], layout->tile_h, align);
			sx += layout->column_widths[COLUMN_MODE3] + 12;

			file_draw_string(sx, sy, file->owner, layout->column_widths[COLUMN_OWNER] , layout->tile_h, align);
			sx += layout->column_widths[COLUMN_OWNER] + 12;
#endif

			file_draw_string(sx, sy, file->date, layout->column_widths[COLUMN_DATE], layout->tile_h, align);
			sx += (int)layout->column_widths[COLUMN_DATE] + 12;

			file_draw_string(sx, sy, file->time, layout->column_widths[COLUMN_TIME] , layout->tile_h, align); 
			sx += (int)layout->column_widths[COLUMN_TIME] + 12;

			if (!(file->type & S_IFDIR)) {
				file_draw_string(sx, sy, file->size, layout->column_widths[COLUMN_SIZE], layout->tile_h, align);
				sx += (int)layout->column_widths[COLUMN_SIZE] + 12;
			}
		}
	}

	uiEndBlock(C, block);
	uiDrawBlock(C, block);

}
