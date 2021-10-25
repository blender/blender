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
#include <errno.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_fileops_types.h"
#include "BLI_math.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BLO_readfile.h"

#include "BLT_translation.h"

#include "IMB_imbuf_types.h"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"

#include "ED_fileselect.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "filelist.h"

#include "file_intern.h"    // own include

/* Dummy helper - we need dynamic tooltips here. */
static char *file_draw_tooltip_func(bContext *UNUSED(C), void *argN, const char *UNUSED(tip))
{
	char *dyn_tooltip = argN;
	return BLI_strdup(dyn_tooltip);
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
	const int line1_y    = ar->winy - (IMASEL_BUTTONS_HEIGHT / 2 + IMASEL_BUTTONS_MARGIN);
	const int line2_y    = line1_y - (IMASEL_BUTTONS_HEIGHT / 2 + IMASEL_BUTTONS_MARGIN);
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
	int chan_offs   = 0;
	int available_w = max_x - min_x;
	int line1_w     = available_w;
	int line2_w     = available_w;
	
	uiBut *but;
	uiBlock *block;
	SpaceFile *sfile  = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	ARegion *artmp;
	const bool is_browse_only = (sfile->op == NULL);
	
	/* Initialize UI block. */
	BLI_snprintf(uiblockstr, sizeof(uiblockstr), "win %p", (void *)ar);
	block = UI_block_begin(C, ar, uiblockstr, UI_EMBOSS);

	/* exception to make space for collapsed region icon */
	for (artmp = CTX_wm_area(C)->regionbase.first; artmp; artmp = artmp->next) {
		if (artmp->regiontype == RGN_TYPE_TOOLS && artmp->flag & RGN_FLAG_HIDDEN) {
			chan_offs = 16;
			min_x += chan_offs;
			available_w -= chan_offs;
		}
	}

	/* Is there enough space for the execute / cancel buttons? */


	if (is_browse_only) {
		loadbutton = 0;
	}
	else {
		const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
		loadbutton = UI_fontstyle_string_width(fstyle, params->title) + btn_margin;
		CLAMP_MIN(loadbutton, btn_minw);
		if (available_w <= loadbutton + separator + input_minw) {
			loadbutton = 0;
		}
	}

	if (loadbutton) {
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
		const struct FileDirEntry *file = sfile->files ? filelist_file(sfile->files, params->active_file) : NULL;
		int overwrite_alert = file_draw_check_exists(sfile);
		const bool is_active_dir = file && (file->typeflag & FILE_TYPE_FOLDER);

		/* callbacks for operator check functions */
		UI_block_func_set(block, file_draw_check_cb, NULL, NULL);

		but = uiDefBut(block, UI_BTYPE_TEXT, -1, "",
		               min_x, line1_y, line1_w - chan_offs, btn_h,
		               params->dir, 0.0, (float)FILE_MAX, 0, 0,
		               TIP_("File path"));
		UI_but_func_complete_set(but, autocomplete_directory, NULL);
		UI_but_flag_enable(but, UI_BUT_NO_UTF8);
		UI_but_flag_disable(but, UI_BUT_UNDO);
		UI_but_funcN_set(but, file_directory_enter_handle, NULL, but);

		/* TODO, directory editing is non-functional while a library is loaded
		 * until this is properly supported just disable it. */
		if (sfile->files && filelist_lib(sfile->files))
			UI_but_flag_enable(but, UI_BUT_DISABLED);

		if ((params->flag & FILE_DIRSEL_ONLY) == 0) {
			but = uiDefBut(block, UI_BTYPE_TEXT, -1, "",
			               min_x, line2_y, line2_w - chan_offs, btn_h,
			               is_active_dir ? (char *)"" : params->file,
			               0.0, (float)FILE_MAXFILE, 0, 0,
			               TIP_(overwrite_alert ? N_("File name, overwrite existing") : N_("File name")));
			UI_but_func_complete_set(but, autocomplete_file, NULL);
			UI_but_flag_enable(but, UI_BUT_NO_UTF8);
			UI_but_flag_disable(but, UI_BUT_UNDO);
			/* silly workaround calling NFunc to ensure this does not get called
			 * immediate ui_apply_but_func but only after button deactivates */
			UI_but_funcN_set(but, file_filename_enter_handle, NULL, but);

			/* check if this overrides a file and if the operator option is used */
			if (overwrite_alert) {
				UI_but_flag_enable(but, UI_BUT_REDALERT);
			}
		}
		
		/* clear func */
		UI_block_func_set(block, NULL, NULL, NULL);
	}
	
	/* Filename number increment / decrement buttons. */
	if (fnumbuttons && (params->flag & FILE_DIRSEL_ONLY) == 0) {
		UI_block_align_begin(block);
		but = uiDefIconButO(block, UI_BTYPE_BUT, "FILE_OT_filenum", 0, ICON_ZOOMOUT,
		                    min_x + line2_w + separator - chan_offs, line2_y,
		                    btn_fn_w, btn_h,
		                    TIP_("Decrement the filename number"));
		RNA_int_set(UI_but_operator_ptr_get(but), "increment", -1);

		but = uiDefIconButO(block, UI_BTYPE_BUT, "FILE_OT_filenum", 0, ICON_ZOOMIN,
		                    min_x + line2_w + separator + btn_fn_w - chan_offs, line2_y,
		                    btn_fn_w, btn_h,
		                    TIP_("Increment the filename number"));
		RNA_int_set(UI_but_operator_ptr_get(but), "increment", 1);
		UI_block_align_end(block);
	}
	
	/* Execute / cancel buttons. */
	if (loadbutton) {
		const struct FileDirEntry *file = sfile->files ? filelist_file(sfile->files, params->active_file) : NULL;
		char const *str_exec;

		if (file && FILENAME_IS_PARENT(file->relpath)) {
			str_exec = IFACE_("Parent Directory");
		}
		else if (file && file->typeflag & FILE_TYPE_DIR) {
			str_exec = IFACE_("Open Directory");
		}
		else {
			str_exec = params->title;  /* params->title is already translated! */
		}

		uiDefButO(block, UI_BTYPE_BUT, "FILE_OT_execute", WM_OP_EXEC_REGION_WIN, str_exec,
		          max_x - loadbutton, line1_y, loadbutton, btn_h, "");
		uiDefButO(block, UI_BTYPE_BUT, "FILE_OT_cancel", WM_OP_EXEC_REGION_WIN, IFACE_("Cancel"),
		          max_x - loadbutton, line2_y, loadbutton, btn_h, "");
	}
	
	UI_block_end(C, block);
	UI_block_draw(C, block);
}


static void draw_tile(int sx, int sy, int width, int height, int colorid, int shade)
{
	UI_ThemeColorShade(colorid, shade);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox((float)sx, (float)(sy - height), (float)(sx + width), (float)sy, 5.0f);
}


static void file_draw_icon(uiBlock *block, const char *path, int sx, int sy, int icon, int width, int height, bool drag)
{
	uiBut *but;
	int x, y;
	// float alpha = 1.0f;
	
	x = sx;
	y = sy - height;
	
	/*if (icon == ICON_FILE_BLANK) alpha = 0.375f;*/

	but = uiDefIconBut(block, UI_BTYPE_LABEL, 0, icon, x, y, width, height, NULL, 0.0f, 0.0f, 0.0f, 0.0f, NULL);
	UI_but_func_tooltip_set(but, file_draw_tooltip_func, BLI_strdup(path));

	if (drag) {
		/* path is no more static, cannot give it directly to but... */
		UI_but_drag_set_path(but, BLI_strdup(path), true);
	}
}


static void file_draw_string(int sx, int sy, const char *string, float width, int height, short align)
{
	uiStyle *style;
	uiFontStyle fs;
	rcti rect;
	char fname[FILE_MAXFILE];

	if (string[0] == '\0') {
		return;
	}

	style = UI_style_get();
	fs = style->widgetlabel;

	fs.align = align;

	BLI_strncpy(fname, string, FILE_MAXFILE);
	UI_text_clip_middle_ex(&fs, fname, width, UI_DPI_ICON_SIZE, sizeof(fname), '\0');

	/* no text clipping needed, UI_fontstyle_draw does it but is a bit too strict (for buttons it works) */
	rect.xmin = sx;
	rect.xmax = (int)(sx + ceil(width + 5.0f / UI_DPI_FAC));
	rect.ymin = sy - height;
	rect.ymax = sy;

	UI_fontstyle_draw(&fs, &rect, fname);
}

void file_calc_previews(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	View2D *v2d = &ar->v2d;
	
	ED_fileselect_init_layout(sfile, ar);
	UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height);
}

static void file_draw_preview(
        uiBlock *block, const char *path, int sx, int sy, const float icon_aspect,
        ImBuf *imb, const int icon, FileLayout *layout, const bool is_icon, const int typeflags, const bool drag)
{
	uiBut *but;
	float fx, fy;
	float dx, dy;
	int xco, yco;
	float ui_imbx, ui_imby;
	float scaledx, scaledy;
	float scale;
	int ex, ey;
	bool use_dropshadow = !is_icon && (typeflags & FILE_TYPE_IMAGE);

	BLI_assert(imb != NULL);

	ui_imbx = imb->x * UI_DPI_FAC;
	ui_imby = imb->y * UI_DPI_FAC;
	/* Unlike thumbnails, icons are not scaled up. */
	if (((ui_imbx > layout->prv_w) || (ui_imby > layout->prv_h)) ||
	    (!is_icon && ((ui_imbx < layout->prv_w) || (ui_imby < layout->prv_h))))
	{
		if (imb->x > imb->y) {
			scaledx = (float)layout->prv_w;
			scaledy = ((float)imb->y / (float)imb->x) * layout->prv_w;
			scale = scaledx / imb->x;
		}
		else {
			scaledy = (float)layout->prv_h;
			scaledx = ((float)imb->x / (float)imb->y) * layout->prv_h;
			scale = scaledy / imb->y;
		}
	}
	else {
		scaledx = ui_imbx;
		scaledy = ui_imby;
		scale = UI_DPI_FAC;
	}

	ex = (int)scaledx;
	ey = (int)scaledy;
	fx = ((float)layout->prv_w - (float)ex) / 2.0f;
	fy = ((float)layout->prv_h - (float)ey) / 2.0f;
	dx = (fx + 0.5f + layout->prv_border_x);
	dy = (fy + 0.5f - layout->prv_border_y);
	xco = sx + (int)dx;
	yco = sy - layout->prv_h + (int)dy;

	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);

	/* shadow */
	if (use_dropshadow) {
		UI_draw_box_shadow(220, (float)xco, (float)yco, (float)(xco + ex), (float)(yco + ey));
	}

	glEnable(GL_BLEND);

	/* the image */
	if (!is_icon && typeflags & FILE_TYPE_FTFONT) {
		UI_ThemeColor(TH_TEXT);
	}
	else {
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
	glaDrawPixelsTexScaled((float)xco, (float)yco, imb->x, imb->y, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, imb->rect, scale, scale);

	if (icon) {
		UI_icon_draw_aspect((float)xco, (float)yco, icon, icon_aspect, 1.0f);
	}

	/* border */
	if (use_dropshadow) {
		glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
		fdrawbox((float)xco, (float)yco, (float)(xco + ex), (float)(yco + ey));
	}

	but = uiDefBut(block, UI_BTYPE_LABEL, 0, "", xco, yco, ex, ey, NULL, 0.0, 0.0, 0, 0, NULL);
	UI_but_func_tooltip_set(but, file_draw_tooltip_func, BLI_strdup(path));

	/* dragregion */
	if (drag) {
		/* path is no more static, cannot give it directly to but... */
		UI_but_drag_set_image(but, BLI_strdup(path), icon, imb, scale, true);
	}

	glDisable(GL_BLEND);
}

static void renamebutton_cb(bContext *C, void *UNUSED(arg1), char *oldname)
{
	char newname[FILE_MAX + 12];
	char orgname[FILE_MAX + 12];
	char filename[FILE_MAX + 12];
	wmWindowManager *wm = CTX_wm_manager(C);
	SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	BLI_make_file_string(G.main->name, orgname, sfile->params->dir, oldname);
	BLI_strncpy(filename, sfile->params->renameedit, sizeof(filename));
	BLI_filename_make_safe(filename);
	BLI_make_file_string(G.main->name, newname, sfile->params->dir, filename);

	if (!STREQ(orgname, newname)) {
		if (!BLI_exists(newname)) {
			errno = 0;
			if ((BLI_rename(orgname, newname) != 0) ||
			    !BLI_exists(newname))
			{
				WM_reportf(RPT_ERROR,
				           "Could not rename: %s",
				           errno ? strerror(errno) : "unknown error");
				WM_report_banner_show();
			}

			/* to make sure we show what is on disk */
			ED_fileselect_clear(wm, sa, sfile);
		}

		ED_region_tag_redraw(ar);
	}
}


static void draw_background(FileLayout *layout, View2D *v2d)
{
	int i;
	int sy;

	UI_ThemeColorShade(TH_BACK, -7);

	/* alternating flat shade background */
	for (i = 0; (i <= layout->rows); i += 2) {
		sy = (int)v2d->cur.ymax - i * (layout->tile_h + 2 * layout->tile_border_y) - layout->tile_border_y;

		glRectf(v2d->cur.xmin, (float)sy, v2d->cur.xmax, (float)(sy + layout->tile_h + 2 * layout->tile_border_y));
		
	}
}

static void draw_dividers(FileLayout *layout, View2D *v2d)
{
	const int step = (layout->tile_w + 2 * layout->tile_border_x);
	int v1[2], v2[2];
	int sx;
	unsigned char col_hi[3], col_lo[3];

	UI_GetThemeColorShade3ubv(TH_BACK,  30, col_hi);
	UI_GetThemeColorShade3ubv(TH_BACK, -30, col_lo);

	v1[1] = v2d->cur.ymax - layout->tile_border_y;
	v2[1] = v2d->cur.ymin;

	glBegin(GL_LINES);

	/* vertical column dividers */
	sx = (int)v2d->tot.xmin;
	while (sx < v2d->cur.xmax) {
		sx += step;

		glColor3ubv(col_lo);
		v1[0] = v2[0] = sx;
		glVertex2iv(v1);
		glVertex2iv(v2);

		glColor3ubv(col_hi);
		v1[0] = v2[0] = sx + 1;
		glVertex2iv(v1);
		glVertex2iv(v2);
	}

	glEnd();
}

void file_draw_list(const bContext *C, ARegion *ar)
{
	SpaceFile *sfile = CTX_wm_space_file(C);
	FileSelectParams *params = ED_fileselect_get_params(sfile);
	FileLayout *layout = ED_fileselect_get_layout(sfile, ar);
	View2D *v2d = &ar->v2d;
	struct FileList *files = sfile->files;
	struct FileDirEntry *file;
	const char *root = filelist_dir(files);
	ImBuf *imb;
	uiBlock *block = UI_block_begin(C, ar, __func__, UI_EMBOSS);
	int numfiles;
	int numfiles_layout;
	int sx, sy;
	int offset;
	int textwidth, textheight;
	int i;
	bool is_icon;
	short align;
	bool do_drag;
	int column_space = 0.6f * UI_UNIT_X;
	const bool small_size = SMALL_SIZE_CHECK(params->thumbnail_size);
	const bool update_stat_strings = small_size != SMALL_SIZE_CHECK(layout->curr_size);
	const float thumb_icon_aspect = sqrtf(64.0f / (float)(params->thumbnail_size));

	numfiles = filelist_files_ensure(files);
	
	if (params->display != FILE_IMGDISPLAY) {

		draw_background(layout, v2d);
	
		draw_dividers(layout, v2d);
	}

	offset = ED_fileselect_layout_offset(layout, (int)ar->v2d.cur.xmin, (int)-ar->v2d.cur.ymax);
	if (offset < 0) offset = 0;

	numfiles_layout = ED_fileselect_layout_numfiles(layout, ar);

	/* adjust, so the next row is already drawn when scrolling */
	if (layout->flag & FILE_LAYOUT_HOR) {
		numfiles_layout += layout->rows;
	}
	else {
		numfiles_layout += layout->columns;
	}

	filelist_file_cache_slidingwindow_set(files, numfiles_layout);

	textwidth = (FILE_IMGDISPLAY == params->display) ? layout->tile_w : (int)layout->column_widths[COLUMN_NAME];
	textheight = (int)(layout->textheight * 3.0 / 2.0 + 0.5);

	align = (FILE_IMGDISPLAY == params->display) ? UI_STYLE_TEXT_CENTER : UI_STYLE_TEXT_LEFT;

	if (numfiles > 0) {
		const bool success = filelist_file_cache_block(files, min_ii(offset + (numfiles_layout / 2), numfiles - 1));
		BLI_assert(success);
		UNUSED_VARS_NDEBUG(success);

		filelist_cache_previews_update(files);

		/* Handle preview timer here, since it's filelist_file_cache_block() and filelist_cache_previews_update()
		 * which controlls previews task. */
		{
			const bool previews_running = filelist_cache_previews_running(files);
//			printf("%s: preview task: %d\n", __func__, previews_running);
			if (previews_running && !sfile->previews_timer) {
				sfile->previews_timer = WM_event_add_timer_notifier(CTX_wm_manager(C), CTX_wm_window(C),
				                                                    NC_SPACE | ND_SPACE_FILE_PREVIEW, 0.01);
			}
			if (!previews_running && sfile->previews_timer) {
				/* Preview is not running, no need to keep generating update events! */
//				printf("%s: Inactive preview task, sleeping!\n", __func__);
				WM_event_remove_timer_notifier(CTX_wm_manager(C), CTX_wm_window(C), sfile->previews_timer);
				sfile->previews_timer = NULL;
			}
		}
	}

	for (i = offset; (i < numfiles) && (i < offset + numfiles_layout); i++) {
		unsigned int file_selflag;
		char path[FILE_MAX_LIBEXTRA];
		ED_fileselect_layout_tilepos(layout, i, &sx, &sy);
		sx += (int)(v2d->tot.xmin + 0.1f * UI_UNIT_X);
		sy = (int)(v2d->tot.ymax - sy);

		file = filelist_file(files, i);
		file_selflag = filelist_entry_select_get(sfile->files, file, CHECK_ALL);

		BLI_join_dirfile(path, sizeof(path), root, file->relpath);

		UI_ThemeColor4(TH_TEXT);


		if (!(file_selflag & FILE_SEL_EDITING)) {
			if ((params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ||
			    (file_selflag & FILE_SEL_SELECTED))
			{
				int colorid = (file_selflag & FILE_SEL_SELECTED) ? TH_HILITE : TH_BACK;
				int shade = (params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ? 35 : 0;

				BLI_assert(i > 0 || FILENAME_IS_CURRPAR(file->relpath));

				draw_tile(sx, sy - 1, layout->tile_w + 4, sfile->layout->tile_h + layout->tile_border_y, colorid, shade);
			}
		}
		UI_draw_roundbox_corner_set(UI_CNR_NONE);

		/* don't drag parent or refresh items */
		do_drag = !(FILENAME_IS_CURRPAR(file->relpath));

		if (FILE_IMGDISPLAY == params->display) {
			const int icon = filelist_geticon(files, i, false);
			is_icon = 0;
			imb = filelist_getimage(files, i);
			if (!imb) {
				imb = filelist_geticon_image(files, i);
				is_icon = 1;
			}

			file_draw_preview(block, path, sx, sy, thumb_icon_aspect,
			                  imb, icon, layout, is_icon, file->typeflag, do_drag);
		}
		else {
			file_draw_icon(block, path, sx, sy - (UI_UNIT_Y / 6), filelist_geticon(files, i, true),
			               ICON_DEFAULT_WIDTH_SCALE, ICON_DEFAULT_HEIGHT_SCALE, do_drag);
			sx += ICON_DEFAULT_WIDTH_SCALE + 0.2f * UI_UNIT_X;
		}

		UI_ThemeColor4(TH_TEXT);

		if (file_selflag & FILE_SEL_EDITING) {
			uiBut *but;
			short width;

			if (params->display == FILE_SHORTDISPLAY) {
				width = layout->tile_w - (ICON_DEFAULT_WIDTH_SCALE + 0.2f * UI_UNIT_X);
			}
			else if (params->display == FILE_LONGDISPLAY) {
				width = layout->column_widths[COLUMN_NAME] + (column_space * 3.5f);
			}
			else {
				BLI_assert(params->display == FILE_IMGDISPLAY);
				width = textwidth;
			}

			but = uiDefBut(block, UI_BTYPE_TEXT, 1, "", sx, sy - layout->tile_h - 0.15f * UI_UNIT_X,
			               width, textheight, sfile->params->renameedit, 1.0f,
			               (float)sizeof(sfile->params->renameedit), 0, 0, "");
			UI_but_func_rename_set(but, renamebutton_cb, file);
			UI_but_flag_enable(but, UI_BUT_NO_UTF8); /* allow non utf8 names */
			UI_but_flag_disable(but, UI_BUT_UNDO);
			if (false == UI_but_active_only(C, ar, block, but)) {
				file_selflag = filelist_entry_select_set(
				                   sfile->files, file, FILE_SEL_REMOVE, FILE_SEL_EDITING, CHECK_ALL);
			}
		}

		if (!(file_selflag& FILE_SEL_EDITING)) {
			int tpos = (FILE_IMGDISPLAY == params->display) ? sy - layout->tile_h + layout->textheight : sy;
			file_draw_string(sx + 1, tpos, file->name, (float)textwidth, textheight, align);
		}

		sx += (int)layout->column_widths[COLUMN_NAME] + column_space;
		if (params->display == FILE_SHORTDISPLAY) {
			if ((file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) ||
			    !(file->typeflag & (FILE_TYPE_DIR | FILE_TYPE_BLENDERLIB)))
			{
				if ((file->entry->size_str[0] == '\0') || update_stat_strings) {
					BLI_filelist_entry_size_to_string(NULL, file->entry->size, small_size, file->entry->size_str);
				}
				file_draw_string(
				            sx, sy, file->entry->size_str, layout->column_widths[COLUMN_SIZE], layout->tile_h, align);
			}
			sx += (int)layout->column_widths[COLUMN_SIZE] + column_space;
		}
		else if (params->display == FILE_LONGDISPLAY) {
			if (!(file->typeflag & FILE_TYPE_BLENDERLIB) && !FILENAME_IS_CURRPAR(file->relpath)) {
				if ((file->entry->date_str[0] == '\0') || update_stat_strings) {
					BLI_filelist_entry_datetime_to_string(
					            NULL, file->entry->time, small_size, file->entry->time_str, file->entry->date_str);
				}
				file_draw_string(
				            sx, sy, file->entry->date_str, layout->column_widths[COLUMN_DATE], layout->tile_h, align);
				sx += (int)layout->column_widths[COLUMN_DATE] + column_space;
				file_draw_string(
				            sx, sy, file->entry->time_str, layout->column_widths[COLUMN_TIME], layout->tile_h, align);
				sx += (int)layout->column_widths[COLUMN_TIME] + column_space;
			}
			else {
				sx += (int)layout->column_widths[COLUMN_DATE] + column_space;
				sx += (int)layout->column_widths[COLUMN_TIME] + column_space;
			}

			if ((file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) ||
			    !(file->typeflag & (FILE_TYPE_DIR | FILE_TYPE_BLENDERLIB)))
			{
				if ((file->entry->size_str[0] == '\0') || update_stat_strings) {
					BLI_filelist_entry_size_to_string(NULL, file->entry->size, small_size, file->entry->size_str);
				}
				file_draw_string(
				            sx, sy, file->entry->size_str, layout->column_widths[COLUMN_SIZE], layout->tile_h, align);
			}
			sx += (int)layout->column_widths[COLUMN_SIZE] + column_space;
		}
	}

	UI_block_end(C, block);
	UI_block_draw(C, block);

	layout->curr_size = params->thumbnail_size;
}
