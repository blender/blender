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
#ifndef ED_BUTTONS_INTERN_H
#define ED_BUTTONS_INTERN_H

struct ARegion;
struct ARegionType;
struct bContext;

/* warning: the values of these defines are used in sbuts->tabs[8] */
/* buts->mainb new */
#define BCONTEXT_SCENE		0
#define BCONTEXT_WORLD		1
#define BCONTEXT_OBJECT		2
#define BCONTEXT_DATA		3
#define BCONTEXT_MATERIAL	4
#define BCONTEXT_TEXTURE	5
#define BCONTEXT_PARTICLE	6
#define BCONTEXT_PHYSICS	7
#define BCONTEXT_GAME		8

/* buts->scaflag */		
#define BUTS_SENS_SEL		1
#define BUTS_SENS_ACT		2
#define BUTS_SENS_LINK		4
#define BUTS_CONT_SEL		8
#define BUTS_CONT_ACT		16
#define BUTS_CONT_LINK		32
#define BUTS_ACT_SEL		64
#define BUTS_ACT_ACT		128
#define BUTS_ACT_LINK		256
#define BUTS_SENS_STATE		512
#define BUTS_ACT_STATE		1024

/* internal exports only */

/* image_header.c */
void buttons_header_buttons(const struct bContext *C, struct ARegion *ar);

#endif /* ED_BUTTONS_INTERN_H */

