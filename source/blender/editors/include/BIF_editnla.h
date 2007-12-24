/**
 * $Id: BIF_editnla.h 9722 2007-01-12 02:34:47Z aligorith $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_EDITNLA_H
#define BIF_EDITNLA_H

struct BWinEvent;

extern void winqreadnlaspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

/* NLA channel operations */
void delete_nlachannel_keys(void);
void duplicate_nlachannel_keys(void);
void transform_nlachannel_keys(int mode, int dummy);

/* Select */
void borderselect_nla(void);
void deselect_nlachannel_keys (int test);
void deselect_nlachannels(int test);

/* NLA Strip operations */
void shift_nlastrips_up(void);
void shift_nlastrips_down(void);
void reset_action_strips(int val);
void synchronize_action_strips(void);
void snap_action_strips(int snap_mode);
void add_nlablock(void);
void add_empty_nlablock(void);
void convert_nla(void);
void copy_action_modifiers(void);

/* Baking */
void bake_all_to_action(void);

#endif

