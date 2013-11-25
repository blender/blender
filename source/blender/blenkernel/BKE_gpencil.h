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

/* ------------ Grease-Pencil API ------------------ */

bool free_gpencil_strokes(struct bGPDframe *gpf);
void free_gpencil_frames(struct bGPDlayer *gpl);
void free_gpencil_layers(struct ListBase *list);
void BKE_gpencil_free(struct bGPdata *gpd);

struct bGPDframe *gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
struct bGPDlayer *gpencil_layer_addnew(struct bGPdata *gpd, const char *name, int setactive);
struct bGPdata *gpencil_data_addnew(const char name[]);

struct bGPDframe *gpencil_frame_duplicate(struct bGPDframe *src);
struct bGPDlayer *gpencil_layer_duplicate(struct bGPDlayer *src);
struct bGPdata *gpencil_data_duplicate(struct bGPdata *gpd);

//struct bGPdata *gpencil_data_getactive(struct ScrArea *sa);
//short gpencil_data_setactive(struct ScrArea *sa, struct bGPdata *gpd);
//struct ScrArea *gpencil_data_findowner(struct bGPdata *gpd);

void gpencil_frame_delete_laststroke(struct bGPDlayer *gpl, struct bGPDframe *gpf);

struct bGPDframe *BKE_gpencil_layer_find_frame(struct bGPDlayer *gpl, int cframe);
struct bGPDframe *gpencil_layer_getframe(struct bGPDlayer *gpl, int cframe, short addnew);
bool gpencil_layer_delframe(struct bGPDlayer *gpl, struct bGPDframe *gpf);
struct bGPDlayer *gpencil_layer_getactive(struct bGPdata *gpd);
void gpencil_layer_setactive(struct bGPdata *gpd, struct bGPDlayer *active);
void gpencil_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);

#endif /*  __BKE_GPENCIL_H__ */
