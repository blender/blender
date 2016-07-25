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

struct ListBase;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPDstroke;
struct Main;

/* ------------ Grease-Pencil API ------------------ */

bool free_gpencil_strokes(struct bGPDframe *gpf);
void free_gpencil_frames(struct bGPDlayer *gpl);
void free_gpencil_layers(struct ListBase *list);
void BKE_gpencil_free(struct bGPdata *gpd);

void gpencil_stroke_sync_selection(struct bGPDstroke *gps);

struct bGPDframe *gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
struct bGPDframe *gpencil_frame_addcopy(struct bGPDlayer *gpl, int cframe);
struct bGPDlayer *gpencil_layer_addnew(struct bGPdata *gpd, const char *name, bool setactive);
struct bGPdata *gpencil_data_addnew(const char name[]);

struct bGPDframe *gpencil_frame_duplicate(struct bGPDframe *src);
struct bGPDlayer *gpencil_layer_duplicate(struct bGPDlayer *src);
struct bGPdata *gpencil_data_duplicate(struct Main *bmain, struct bGPdata *gpd, bool internal_copy);

void BKE_gpencil_make_local(struct Main *bmain, struct bGPdata *gpd, const bool lib_local);

void gpencil_frame_delete_laststroke(struct bGPDlayer *gpl, struct bGPDframe *gpf);


/* Stroke and Fill - Alpha Visibility Threshold */
#define GPENCIL_ALPHA_OPACITY_THRESH 0.001f

bool gpencil_layer_is_editable(const struct bGPDlayer *gpl);

/* How gpencil_layer_getframe() should behave when there
 * is no existing GP-Frame on the frame requested.
 */
typedef enum eGP_GetFrame_Mode {
	/* Use the preceeding gp-frame (i.e. don't add anything) */
	GP_GETFRAME_USE_PREV  = 0,
	
	/* Add a new empty/blank frame */
	GP_GETFRAME_ADD_NEW   = 1,
	/* Make a copy of the active frame */
	GP_GETFRAME_ADD_COPY  = 2
} eGP_GetFrame_Mode;

struct bGPDframe *gpencil_layer_getframe(struct bGPDlayer *gpl, int cframe, eGP_GetFrame_Mode addnew);
struct bGPDframe *BKE_gpencil_layer_find_frame(struct bGPDlayer *gpl, int cframe);
bool gpencil_layer_delframe(struct bGPDlayer *gpl, struct bGPDframe *gpf);

struct bGPDlayer *gpencil_layer_getactive(struct bGPdata *gpd);
void gpencil_layer_setactive(struct bGPdata *gpd, struct bGPDlayer *active);
void gpencil_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);

#endif /*  __BKE_GPENCIL_H__ */
