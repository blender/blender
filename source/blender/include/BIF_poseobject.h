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

#ifndef BIF_POSEOBJECT
#define BIF_POSEOBJECT

/**
 * Activates posemode
 */
void enter_posemode(void);

/**
 * Provides the current object the opportunity to specify
 * which channels to key in the current pose (if any).
 * If an object provides its own filter, it must clear
 * then POSE_KEY flags of unwanted channels, as well as
 * setting the flags for desired channels.
 *
 * Default behaviour is to key all channels.
 */
void filter_pose_keys(void);

/**
 * Deactivates posemode
 * @param freedata 0 or 1 value indicating that posedata should be deleted
 */
void exit_posemode(int freedata);

/**
 * Removes unreferenced pose channels from an object 
 * @param ob Object to check
 */
void collect_pose_garbage(struct Object *ob);

#endif

