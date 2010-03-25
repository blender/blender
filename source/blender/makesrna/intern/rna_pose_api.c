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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#include "DNA_object_types.h"

/* #include "BLO_sys_types.h" */

#ifdef RNA_RUNTIME

/* #include "DNA_anim_types.h" */
#include "DNA_action_types.h" /* bPose */


#else

void RNA_api_pose(StructRNA *srna)
{
	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */
}

void RNA_api_pose_channel(StructRNA *srna)
{
//	FunctionRNA *func;
//	PropertyRNA *parm;
}


#endif

