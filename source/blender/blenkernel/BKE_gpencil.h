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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_GPENCIL_H__
#define __BKE_GPENCIL_H__

/** \file BKE_gpencil.h
 *  \ingroup bke
 *  \author Joshua Leung
 */

struct ToolSettings;
struct ListBase;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct bGPDpalette;
struct bGPDpalettecolor;
struct Main;

/* ------------ Grease-Pencil API ------------------ */

void BKE_gpencil_free_stroke(struct bGPDstroke *gps);
bool BKE_gpencil_free_strokes(struct bGPDframe *gpf);
void BKE_gpencil_free_frames(struct bGPDlayer *gpl);
void BKE_gpencil_free_layers(struct ListBase *list);
void BKE_gpencil_free_brushes(struct ListBase *list);
void BKE_gpencil_free_palettes(struct ListBase *list);
void BKE_gpencil_free(struct bGPdata *gpd, bool free_palettes);

void BKE_gpencil_stroke_sync_selection(struct bGPDstroke *gps);

struct bGPDframe *BKE_gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
struct bGPDframe *BKE_gpencil_frame_addcopy(struct bGPDlayer *gpl, int cframe);
struct bGPDlayer *BKE_gpencil_layer_addnew(struct bGPdata *gpd, const char *name, bool setactive);
struct bGPdata   *BKE_gpencil_data_addnew(struct Main *bmain, const char name[]);

struct bGPDframe *BKE_gpencil_frame_duplicate(const struct bGPDframe *gpf_src);
struct bGPDlayer *BKE_gpencil_layer_duplicate(const struct bGPDlayer *gpl_src);
void BKE_gpencil_copy_data(struct Main *bmain, struct bGPdata *gpd_dst, const struct bGPdata *gpd_src, const int flag);
struct bGPdata   *BKE_gpencil_data_duplicate(struct Main *bmain, const struct bGPdata *gpd, bool internal_copy);

void BKE_gpencil_make_local(struct Main *bmain, struct bGPdata *gpd, const bool lib_local);

void BKE_gpencil_frame_delete_laststroke(struct bGPDlayer *gpl, struct bGPDframe *gpf);

struct bGPDpalette *BKE_gpencil_palette_addnew(struct bGPdata *gpd, const char *name, bool setactive);
struct bGPDpalette *BKE_gpencil_palette_duplicate(const struct bGPDpalette *palette_src);
struct bGPDpalettecolor *BKE_gpencil_palettecolor_addnew(struct bGPDpalette *palette, const char *name, bool setactive);

struct bGPDbrush *BKE_gpencil_brush_addnew(struct ToolSettings *ts, const char *name, bool setactive);
struct bGPDbrush *BKE_gpencil_brush_duplicate(const struct bGPDbrush *brush_src);
void BKE_gpencil_brush_init_presets(struct ToolSettings *ts);


/* Stroke and Fill - Alpha Visibility Threshold */
#define GPENCIL_ALPHA_OPACITY_THRESH 0.001f
#define GPENCIL_STRENGTH_MIN 0.003f

bool gpencil_layer_is_editable(const struct bGPDlayer *gpl);

/* How gpencil_layer_getframe() should behave when there
 * is no existing GP-Frame on the frame requested.
 */
typedef enum eGP_GetFrame_Mode {
	/* Use the preceding gp-frame (i.e. don't add anything) */
	GP_GETFRAME_USE_PREV  = 0,

	/* Add a new empty/blank frame */
	GP_GETFRAME_ADD_NEW   = 1,
	/* Make a copy of the active frame */
	GP_GETFRAME_ADD_COPY  = 2
} eGP_GetFrame_Mode;

struct bGPDframe *BKE_gpencil_layer_getframe(struct bGPDlayer *gpl, int cframe, eGP_GetFrame_Mode addnew);
struct bGPDframe *BKE_gpencil_layer_find_frame(struct bGPDlayer *gpl, int cframe);
bool BKE_gpencil_layer_delframe(struct bGPDlayer *gpl, struct bGPDframe *gpf);

struct bGPDlayer *BKE_gpencil_layer_getactive(struct bGPdata *gpd);
void BKE_gpencil_layer_setactive(struct bGPdata *gpd, struct bGPDlayer *active);
void BKE_gpencil_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);

struct bGPDbrush *BKE_gpencil_brush_getactive(struct ToolSettings *ts);
void BKE_gpencil_brush_setactive(struct ToolSettings *ts, struct bGPDbrush *active);
void BKE_gpencil_brush_delete(struct ToolSettings *ts, struct bGPDbrush *brush);

struct bGPDpalette *BKE_gpencil_palette_getactive(struct bGPdata *gpd);
void BKE_gpencil_palette_setactive(struct bGPdata *gpd, struct bGPDpalette *active);
void BKE_gpencil_palette_delete(struct bGPdata *gpd, struct bGPDpalette *palette);
void BKE_gpencil_palette_change_strokes(struct bGPdata *gpd);

struct bGPDpalettecolor *BKE_gpencil_palettecolor_getactive(struct bGPDpalette *palette);
void BKE_gpencil_palettecolor_setactive(struct bGPDpalette *palette, struct bGPDpalettecolor *active);
void BKE_gpencil_palettecolor_delete(struct bGPDpalette *palette, struct bGPDpalettecolor *palcolor);
struct bGPDpalettecolor *BKE_gpencil_palettecolor_getbyname(struct bGPDpalette *palette, char *name);
void BKE_gpencil_palettecolor_changename(struct bGPdata *gpd, char *oldname, const char *newname);
void BKE_gpencil_palettecolor_delete_strokes(struct bGPdata *gpd, char *name);

#endif /*  __BKE_GPENCIL_H__ */
