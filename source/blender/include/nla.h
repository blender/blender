/* nla.h   May 2001
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
 *  
 *	Use this to turn experimental options on
 *	or off with the #define flags.  Please do not
 *  put other includes, typdefs etc in this file.
 *	===========================================
 *
 *	__NLA
 *	This encompasses the new armature object, the
 *	action datablock and the action window-type.
 *
 *	__CON_IPO
 *	Support for constraint ipo keys
 *
 *	__NLA_BAKE
 *	Allow users to bake constraints into keyframes
 *
 *	__NLA_ACTION_BY_MOTION_ACTUATOR
 *	New action actuator playback type
 *
 * $Id$
 */

#ifndef NLA_H
#define NLA_H

#define __NLA			

#define __NLA_BAKE							//	Not for release: Not yet fully implemented
#define __CON_IPO							//	Not for Release: Not yet fully implemented
//#define __NLA_ACTION_BY_MOTION_ACTUATOR	//	Not for release: Not yet fully implemented

#endif

