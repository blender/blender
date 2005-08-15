/**
 * BKE_softbody.h 
 *	
 * $Id: BKE_softbody.h 
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_SOFTBODY_H
#define BKE_SOFTBODY_H

struct Object;
struct SoftBody;

/* allocates and initializes general main data */
extern struct SoftBody	*sbNew(void);

/* frees internal data and softbody itself */
extern void				sbFree(struct SoftBody *sb);

/* do one simul step, reading and writing vertex locs from given array */
extern void				sbObjectStep(struct Object *ob, float framnr, float (*vertexCos)[3], int numVerts);

/* makes totally fresh start situation, resets time */
extern void				sbObjectToSoftbody(struct Object *ob);

#endif

