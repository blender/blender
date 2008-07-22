/**
 * $Id: BDR_gpencil.h 14444 2008-04-16 22:40:48Z aligorith $
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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BDR_GPENCIL_H
#define BDR_GPENCIL_H

struct ListBase;
struct bScreen;
struct ScrArea;
struct View3D;
struct SpaceNode;
struct SpaceSeq;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;

/* ------------ Grease-Pencil API ------------------ */

void free_gpencil_strokes(struct bGPDframe *gpf);
void free_gpencil_frames(struct bGPDlayer *gpl);
void free_gpencil_layers(struct ListBase *list);
void free_gpencil_data(struct bGPdata *gpd);

struct bGPDframe *gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
struct bGPDlayer *gpencil_layer_addnew(struct bGPdata *gpd);
struct bGPdata *gpencil_data_addnew(void);

struct bGPdata *gpencil_data_duplicate(struct bGPdata *gpd);

struct bGPdata *gpencil_data_getactive(struct ScrArea *sa);
short gpencil_data_setactive(struct ScrArea *sa, struct bGPdata *gpd);
struct bGPdata *gpencil_data_getetime(struct bScreen *sc);
void gpencil_data_setetime(struct bScreen *sc, struct bGPdata *gpd);

void gpencil_frame_delete_laststroke(struct bGPDframe *gpf);

struct bGPDframe *gpencil_layer_getframe(struct bGPDlayer *gpl, int cframe, short addnew);
void gpencil_layer_delframe(struct bGPDlayer *gpl, struct bGPDframe *gpf);
struct bGPDlayer *gpencil_layer_getactive(struct bGPdata *gpd);
void gpencil_layer_setactive(struct bGPdata *gpd, struct bGPDlayer *active);
void gpencil_layer_delactive(struct bGPdata *gpd);

void gpencil_delete_actframe(struct bGPdata *gpd);
void gpencil_delete_laststroke(struct bGPdata *gpd);

void gpencil_delete_operation(short mode);
void gpencil_delete_menu(void);

//short gpencil_paint(short mousebutton);
short gpencil_do_paint(struct ScrArea *sa);

#endif /*  BDR_GPENCIL_H */
