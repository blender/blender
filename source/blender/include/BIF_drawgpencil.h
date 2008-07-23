/**
 * $Id: BIF_drawgpencil.h 14444 2008-04-16 22:40:48Z aligorith $
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

#ifndef BIF_DRAWGPENCIL_H
#define BIF_DRAWGPENCIL_H

struct ScrArea;
struct View3D;
struct SpaceNode;
struct SpaceSeq;
struct bGPdata;
struct uiBlock;

short draw_gpencil_panel(struct uiBlock *block, struct bGPdata *gpd, struct ScrArea *sa); 

void draw_gpencil_2dview(struct ScrArea *sa, short onlyv2d);
void draw_gpencil_3dview(struct ScrArea *sa, short only3d);
void draw_gpencil_oglrender(struct View3D *v3d, int winx, int winy);

#endif /*  BIF_DRAWGPENCIL_H */ 
