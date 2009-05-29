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
#ifndef ED_NLA_INTERN_H
#define ED_NLA_INTERN_H

/* internal exports only */

/* -------------- NLA Channel Defines -------------- */

/* NLA channel heights */
#define NLACHANNEL_FIRST			-16
#define	NLACHANNEL_HEIGHT			24
#define NLACHANNEL_HEIGHT_HALF	12
#define	NLACHANNEL_SKIP			2
#define NLACHANNEL_STEP			(NLACHANNEL_HEIGHT + NLACHANNEL_SKIP)

/* channel widths */
#define NLACHANNEL_NAMEWIDTH		200

/* channel toggle-buttons */
#define NLACHANNEL_BUTTON_WIDTH	16


/* **************************************** */
/* nla_draw.c */

void draw_nla_channel_list(bAnimContext *ac, SpaceNla *snla, ARegion *ar);

/* **************************************** */
/* nla_header.c */

void nla_header_buttons(const bContext *C, ARegion *ar);


#endif /* ED_NLA_INTERN_H */

