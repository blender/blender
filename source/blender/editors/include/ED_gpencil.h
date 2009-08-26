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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_GPENCIL_H
#define ED_GPENCIL_H

struct ListBase;
struct bScreen;
struct ScrArea;
struct ARegion;
struct View3D;
struct SpaceNode;
struct SpaceSeq;
struct bGPdata;
struct bGPDlayer;
struct bGPDframe;
struct bGPdata;
struct uiBlock;
struct ImBuf;
struct wmWindowManager;


/* ------------- Grease-Pencil Helpers -------------- */

/* Temporary 'Stroke Point' data */
typedef struct tGPspoint {
	short x, y;				/* x and y coordinates of cursor (in relative to area) */
	float pressure;			/* pressure of tablet at this point */
} tGPspoint;

/* ----------- Grease Pencil New Tools ------------- */

struct bGPdata *gpencil_data_getactive(struct bContext *C);

/* ----------- Grease Pencil Operators ------------- */

void gpencil_common_keymap(struct wmWindowManager *wm, ListBase *keymap);

void ED_operatortypes_gpencil(void);

/* ------------ Grease-Pencil Depreceated Stuff ------------------ */

//struct bGPdata *gpencil_data_getactive(struct ScrArea *sa);
short gpencil_data_setactive(struct ScrArea *sa, struct bGPdata *gpd);
struct ScrArea *gpencil_data_findowner(struct bGPdata *gpd);

/* ------------ Grease-Pencil Editing API ------------------ */

void gpencil_delete_actframe(struct bGPdata *gpd, int cfra);
void gpencil_delete_laststroke(struct bGPdata *gpd, int cfra);

void gpencil_delete_operation(int cfra, short mode);
void gpencil_delete_menu(void);

void gpencil_convert_operation(short mode);
void gpencil_convert_menu(void);

short gpencil_do_paint(struct bContext *C);

/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void draw_gpencil_2dimage(struct bContext *C, struct ImBuf *ibuf);
void draw_gpencil_2dview(struct bContext *C, short onlyv2d);
void draw_gpencil_3dview(struct bContext *C, short only3d);
void draw_gpencil_oglrender(struct bContext *C);


#endif /*  ED_GPENCIL_H */
