/**
 * blenlib/DNA_object_types.h (mar-2001 nzc)
 *	
 * Scriptlink is hard-coded in object for some reason.
 *
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
#ifndef DNA_SCRIPTLINK_TYPES_H
#define DNA_SCRIPTLINK_TYPES_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ID;

typedef struct ScriptLink {
	struct ID **scripts;
	short *flag;
	
	short actscript, totscript;
	int pad;
} ScriptLink;

/* **************** SCRIPTLINKS ********************* */

#define SCRIPT_FRAMECHANGED	1
#define SCRIPT_ONLOAD		2
#define SCRIPT_REDRAW		4

#ifdef __cplusplus
}
#endif

#endif

