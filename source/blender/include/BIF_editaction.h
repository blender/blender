/**
 * $Id$
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

#ifndef BIF_EDITACTION_H
#define BIF_EDITACTION_H

#define SET_IPO_POPUP    0
#define SET_IPO_CONSTANT 1
#define SET_IPO_LINEAR   2
#define SET_IPO_BEZIER   3

/* Key operations */
void delete_meshchannel_keys(struct Key *key);
void delete_actionchannel_keys(void);
void duplicate_meshchannel_keys(struct Key *key);
void duplicate_actionchannel_keys(void);

/* Handles */
void sethandles_meshchannel_keys(int code, struct Key *key);
void sethandles_actionchannel_keys(int code);

/* Ipo type */ 
void set_ipotype_actionchannels(int ipotype);

/* Select */
void borderselect_mesh(struct Key *key);
void borderselect_action(void);
void deselect_actionchannel_keys(bAction *act, int test);
void deselect_meshchannel_keys (Key *key, int test);

#endif

