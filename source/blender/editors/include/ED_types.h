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
#ifndef ED_TYPES_H
#define ED_TYPES_H

/* **************** GENERAL EDITOR-WIDE TYPES AND DEFINES ************************** */

/* old blender defines... should be depricated? */
#define DESELECT 0
#define SELECT	 1
#define ACTIVE	 2

/* nonstandard define, sometimes in math.h */
#ifndef MAXFLOAT
#define MAXFLOAT  ((float)3.40282347e+38)
#endif

/* buttons */
#define XIC 20
#define YIC 20

/* proposal = put scene pointers on function calls? */
#define BASACT                    (scene->basact)
#define OBACT                     (BASACT? BASACT->object: 0)



#endif /* ED_TYPES_H */

